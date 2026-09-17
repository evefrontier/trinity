// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisRenderDevice.h"
#include "Noesis/Tr2NoesisNxtInterface.h"

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisGpuDevice.h"
#include "Resources/TriTextureRes.h"
#include "Tr2RenderContext.h"

BLUE_DEFINE( Tr2NoesisRenderDevice );

namespace
{

Tr2NoesisRenderDevice& Self( void* self )
{
	return *static_cast<Tr2NoesisRenderDevice*>( self );
}

// Null between Python wiring the host and the first Execute building the device. The
// library holds the device-half vtable across that window and is entitled to call into
// it: these entry points carry no frame, so there is no RequireFrame to close it from
// that side. Device-half thunks must therefore decline rather than dereference. A failed
// resource creation is something the SDK handles; a null dereference is not.
Tr2NoesisGpuDevice* Device( void* self )
{
	Tr2NoesisGpuDevice* device = Self( self ).GetDevice();
	if( device == nullptr )
	{
		static bool s_reported = false;
		if( !s_reported )
		{
			s_reported = true;
			CCP_NOESIS_LOGERR( "The Noesis library asked the host for GPU work before the "
							   "render device was built; declining. This happens when the SDK "
							   "creates a resource outside a frame, which nothing here can "
							   "serve yet." );
		}
	}
	return device;
}

// `native` is the Python object Python passed to the video sink -- a Trinity texture
// resource. The library never looks inside it, which is why it can be anything this host
// recognises, and why unwrapping it is the host's job.
//
// Called with the GIL held: the only callers are the video element, on the thread Python
// drives.
bool NativeTexture( void* native, TriTextureResPtr& out )
{
	PyObject* object = static_cast<PyObject*>( native );
	if( object == nullptr || object == Py_None )
	{
		return false;
	}

	IRoot* root = nullptr;
	if( !BlueExtractArgument( object, root, 0 ) || root == nullptr )
	{
		PyErr_Clear();
		return false;
	}

	out = BlueCastPtr( root );
	return out != nullptr;
}

Tr2NoesisTexture* AsTexture( nxt_texture texture )
{
	return reinterpret_cast<Tr2NoesisTexture*>( texture );
}

Tr2NoesisRenderTarget* AsTarget( nxt_render_target surface )
{
	return reinterpret_cast<Tr2NoesisRenderTarget*>( surface );
}

// --------------------------------------------------------------------------------------
// Device half
// --------------------------------------------------------------------------------------

void HostGetCaps( void* self, nxt_device_caps* out )
{
	Tr2NoesisGpuDevice* device = Device( self );
	if( device == nullptr )
	{
		// Zeroed, not left undefined: the library reads these to choose how it renders,
		// and false is the conservative answer to each.
		*out = nxt_device_caps();
		return;
	}
	device->GetCaps( *out );
}

nxt_render_target HostCreateRenderTarget( void* self, const char* label, uint32_t width,
										  uint32_t height, uint32_t sampleCount,
										  nxt_bool needsStencil )
{
	Tr2NoesisGpuDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return reinterpret_cast<nxt_render_target>( device->CreateRenderTarget(
		label, width, height, sampleCount, needsStencil != NXT_FALSE ) );
}

nxt_render_target HostCloneRenderTarget( void* self, const char* label, nxt_render_target surface )
{
	Tr2NoesisGpuDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return reinterpret_cast<nxt_render_target>(
		device->CloneRenderTarget( label, AsTarget( surface ) ) );
}

void HostReleaseRenderTarget( void* /*self*/, nxt_render_target surface )
{
	// Destroys the wrapper. The AL resources it holds go with it, which is what the
	// library means by releasing a handle it created.
	delete AsTarget( surface );
}

nxt_texture HostGetRenderTargetTexture( void* /*self*/, nxt_render_target surface )
{
	Tr2NoesisRenderTarget* target = AsTarget( surface );
	return target != nullptr ? reinterpret_cast<nxt_texture>( target->GetColor() ) : nullptr;
}

nxt_texture HostCreateTexture( void* self, const char* label, uint32_t width, uint32_t height,
							   uint32_t numLevels, nxt_texture_format format, const void** data )
{
	Tr2NoesisGpuDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return reinterpret_cast<nxt_texture>(
		device->CreateTexture( label, width, height, numLevels, format, data ) );
}

void HostReleaseTexture( void* /*self*/, nxt_texture texture )
{
	// For a wrapped host texture this drops only the wrapper: the Tr2TextureAL inside is
	// a shared handle, so the resource it refers to is untouched. See release_texture.
	delete AsTexture( texture );
}

void HostGetTextureInfo( void* /*self*/, nxt_texture texture, uint32_t* width, uint32_t* height,
						 uint32_t* levels, nxt_bool* hasAlpha )
{
	Tr2NoesisTexture* t = AsTexture( texture );
	if( t == nullptr )
	{
		*width = 0;
		*height = 0;
		*levels = 0;
		*hasAlpha = NXT_FALSE;
		return;
	}

	*width = t->GetWidth();
	*height = t->GetHeight();
	*levels = t->GetLevels();
	*hasAlpha = t->HasAlpha() ? NXT_TRUE : NXT_FALSE;
}

nxt_pixel_shader HostCreatePixelShader( void* self, const char* label, uint8_t shader,
										const void* blob, uint32_t size )
{
	Tr2NoesisGpuDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return device->CreatePixelShader( label, shader, blob, size );
}

void HostReleasePixelShader( void* /*self*/, nxt_pixel_shader /*shader*/ )
{
	// Handles are 1-based indices into a vector on the device, so there is nothing to
	// free per handle. They live until the device does.
}

nxt_texture HostWrapNativeTexture( void* self, void* native, nxt_bool hasAlpha )
{
	TriTextureResPtr resource;
	if( !NativeTexture( native, resource ) )
	{
		return nullptr;
	}

	Tr2TextureAL* texture = resource->GetTexture();
	if( texture == nullptr || !texture->IsValid() )
	{
		return nullptr;
	}

	Tr2NoesisGpuDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}

	return reinterpret_cast<nxt_texture>(
		device->WrapTexture( *texture, hasAlpha != NXT_FALSE ) );
}

nxt_bool HostGetNativeTextureSize( void* /*self*/, void* native, uint32_t* width, uint32_t* height )
{
	*width = 0;
	*height = 0;

	TriTextureResPtr resource;
	if( !NativeTexture( native, resource ) )
	{
		// The one place the host can tell the caller it passed the wrong kind of object,
		// because only this side knows what the handle was supposed to be.
		return NXT_FALSE;
	}

	Tr2TextureAL* texture = resource->GetTexture();
	if( texture == nullptr || !texture->IsValid() )
	{
		return NXT_FALSE;
	}

	*width = texture->GetWidth();
	*height = texture->GetHeight();
	return NXT_TRUE;
}

// --------------------------------------------------------------------------------------
// Frame half
// --------------------------------------------------------------------------------------

void HostUpdateTexture( void* self, nxt_texture texture, uint32_t level, uint32_t x, uint32_t y,
						uint32_t width, uint32_t height, const void* data )
{
	Device( self )->UpdateTexture( AsTexture( texture ), level, x, y, width, height, data );
}

void HostBeginOffscreen( void* self ) { Device( self )->BeginOffscreenRender(); }
void HostEndOffscreen( void* self ) { Device( self )->EndOffscreenRender(); }
void HostBeginOnscreen( void* self ) { Device( self )->BeginOnscreenRender(); }
void HostEndOnscreen( void* self ) { Device( self )->EndOnscreenRender(); }

void HostSetRenderTarget( void* self, nxt_render_target surface )
{
	Device( self )->SetRenderTarget( AsTarget( surface ) );
}

void HostBeginTile( void* self, nxt_render_target surface, const nxt_tile* tile )
{
	Device( self )->BeginTile( AsTarget( surface ), *tile );
}

void HostEndTile( void* self, nxt_render_target surface )
{
	Device( self )->EndTile( AsTarget( surface ) );
}

void HostResolveRenderTarget( void* self, nxt_render_target surface, const nxt_tile* tiles,
							  uint32_t numTiles )
{
	Device( self )->ResolveRenderTarget( AsTarget( surface ), tiles, numTiles );
}

void* HostMapVertices( void* self, uint32_t bytes ) { return Device( self )->MapVertices( bytes ); }
void HostUnmapVertices( void* self ) { Device( self )->UnmapVertices(); }
void* HostMapIndices( void* self, uint32_t bytes ) { return Device( self )->MapIndices( bytes ); }
void HostUnmapIndices( void* self ) { Device( self )->UnmapIndices(); }

void HostDrawBatch( void* self, const nxt_batch* batch )
{
	Device( self )->DrawBatch( *batch );
}

// nxt retain/release. Blue's refcount is Lock/Unlock, so these are that and nothing
// more: the library holding this device host holds a Blue reference on it, and there is
// no second lifetime to keep in step.
void HostRetain( void* self )
{
	Self( self ).Lock();
}

void HostRelease( void* self )
{
	Self( self ).Unlock();
}

// `owned` false leaves retain and release null, which nxt.h defines as borrowed: valid
// for the call it was passed to and never to be stored. That is the frame host.
void FillHeader( nxt_interface_header& header, void* self, uint32_t size, bool owned )
{
	header.abi_version_major = NXT_ABI_VERSION_MAJOR;
	header.abi_version_minor = NXT_ABI_VERSION_MINOR;
	header.struct_size = size;
	header.self = self;
	header.retain = owned ? HostRetain : nullptr;
	header.release = owned ? HostRelease : nullptr;
}

}

Tr2NoesisRenderDevice::Tr2NoesisRenderDevice( IRoot* ) :
	m_shaderSource( nullptr ),
	m_deviceAttempted( false ),
	m_renderDeviceApi(),
	m_frameApi()
{
	FillVtables();
}

Tr2NoesisRenderDevice::~Tr2NoesisRenderDevice()
{
	if( m_shaderSource != nullptr && m_shaderSource->header.release != nullptr )
	{
		m_shaderSource->header.release( m_shaderSource->header.self );
	}
}

void Tr2NoesisRenderDevice::FillVtables()
{
	FillHeader( m_renderDeviceApi.header, this, sizeof( nxt_render_device ), true );
	m_renderDeviceApi.get_caps = HostGetCaps;
	m_renderDeviceApi.create_render_target = HostCreateRenderTarget;
	m_renderDeviceApi.clone_render_target = HostCloneRenderTarget;
	m_renderDeviceApi.release_render_target = HostReleaseRenderTarget;
	m_renderDeviceApi.get_render_target_texture = HostGetRenderTargetTexture;
	m_renderDeviceApi.create_texture = HostCreateTexture;
	m_renderDeviceApi.release_texture = HostReleaseTexture;
	m_renderDeviceApi.get_texture_info = HostGetTextureInfo;
	m_renderDeviceApi.create_pixel_shader = HostCreatePixelShader;
	m_renderDeviceApi.release_pixel_shader = HostReleasePixelShader;
	m_renderDeviceApi.wrap_native_texture = HostWrapNativeTexture;
	m_renderDeviceApi.get_native_texture_size = HostGetNativeTextureSize;

	FillHeader( m_frameApi.header, this, sizeof( nxt_frame_context ), false );
	m_frameApi.update_texture = HostUpdateTexture;
	m_frameApi.begin_offscreen_render = HostBeginOffscreen;
	m_frameApi.end_offscreen_render = HostEndOffscreen;
	m_frameApi.begin_onscreen_render = HostBeginOnscreen;
	m_frameApi.end_onscreen_render = HostEndOnscreen;
	m_frameApi.set_render_target = HostSetRenderTarget;
	m_frameApi.begin_tile = HostBeginTile;
	m_frameApi.end_tile = HostEndTile;
	m_frameApi.resolve_render_target = HostResolveRenderTarget;
	m_frameApi.map_vertices = HostMapVertices;
	m_frameApi.unmap_vertices = HostUnmapVertices;
	m_frameApi.map_indices = HostMapIndices;
	m_frameApi.unmap_indices = HostUnmapIndices;
	m_frameApi.draw_batch = HostDrawBatch;
}

bool Tr2NoesisRenderDevice::SetShaderSource( const nxt_shader_source* shaderSource )
{
	if( shaderSource != nullptr && nxt_interface_usable( &shaderSource->header ) == NXT_FALSE )
	{
		CCP_NOESIS_LOGERR( "The shader source speaks an nxt ABI this Trinity cannot; this "
						   "Trinity is nxt %u.%u",
						   static_cast<uint32_t>( NXT_ABI_VERSION_MAJOR ),
						   static_cast<uint32_t>( NXT_ABI_VERSION_MINOR ) );
		return false;
	}

	// Retain before releasing, so setting the same source twice is not a free-then-use.
	if( shaderSource != nullptr && shaderSource->header.retain != nullptr )
	{
		shaderSource->header.retain( shaderSource->header.self );
	}
	if( m_shaderSource != nullptr && m_shaderSource->header.release != nullptr )
	{
		m_shaderSource->header.release( m_shaderSource->header.self );
	}
	m_shaderSource = shaderSource;

	// A new source means a new device; the old one was built from the old blobs.
	m_device.reset();
	m_deviceAttempted = false;
	return m_shaderSource != nullptr;
}

bool Tr2NoesisRenderDevice::EnsureDevice()
{
	if( m_deviceAttempted )
	{
		return m_device != nullptr && m_device->IsValid();
	}

	// Latched before the check below, because Execute calls this every frame and a host
	// wired without a shader source must not log the same error forever.
	m_deviceAttempted = true;

	if( m_shaderSource == nullptr )
	{
		CCP_NOESIS_LOGERR( "No shader source is set; Python must pass the Noesis library's "
						   "shader source before anything can render" );
		return false;
	}

	// Safe here and not at Python-init time: the step calls this from Execute, which runs
	// on the render thread with the main-thread context already live.
	USE_MAIN_THREAD_RENDER_CONTEXT();

	m_device.reset( new Tr2NoesisGpuDevice( renderContext.GetPrimaryRenderContext(),
											   *m_shaderSource ) );
	if( !m_device->IsValid() )
	{
		CCP_NOESIS_LOGERR( "The Noesis render device is unusable; no Noesis rendering will "
						   "happen this session" );
		m_device.reset();
		return false;
	}

	CCP_NOESIS_LOGNOTICE( "Noesis render device ready" );
	return true;
}

bool Tr2NoesisRenderDevice::IsReady() const
{
	return m_device != nullptr && m_device->IsValid();
}

const nxt_render_device* Tr2NoesisRenderDevice::GetNxtRenderDevice()
{
	// Valid as soon as a shader source is set, not once the device is built: the library
	// wires this at startup and the device cannot exist until the first frame. Calls that
	// land in that window are the reason Device() checks before dereferencing.
	return m_shaderSource != nullptr ? &m_renderDeviceApi : nullptr;
}

const nxt_frame_context* Tr2NoesisRenderDevice::GetNsiFrameHost()
{
	return IsReady() ? &m_frameApi : nullptr;
}

void Tr2NoesisRenderDevice::BeginFrame( Tr2RenderContext& renderContext )
{
	if( m_device != nullptr )
	{
		m_device->SetRenderContext( renderContext );
	}
}

void Tr2NoesisRenderDevice::EndFrame()
{
	if( m_device != nullptr )
	{
		m_device->ClearRenderContext();
	}
}

void Tr2NoesisRenderDevice::SetHostScissor( const Tr2ScissorRect& rect )
{
	if( m_device != nullptr )
	{
		m_device->SetHostScissor( rect );
	}
}

void Tr2NoesisRenderDevice::ClearHostScissor()
{
	if( m_device != nullptr )
	{
		m_device->ClearHostScissor();
	}
}

Tr2NoesisGpuDevice* Tr2NoesisRenderDevice::GetDevice() const
{
	return m_device.get();
}

// The library retains the interface out of this capsule, so the capsule may be dropped
// the moment it has been handed over. The capsule holds a reference of its own until
// then, released by its destructor.
static void RenderDeviceCapsuleDestructor( PyObject* capsule )
{
	void* pointer = PyCapsule_GetPointer( capsule, NXT_CAPSULE_RENDER_DEVICE );
	if( pointer == nullptr )
	{
		PyErr_Clear();
		return;
	}
	const nxt_render_device* api = static_cast<const nxt_render_device*>( pointer );
	if( api->header.release != nullptr )
	{
		api->header.release( api->header.self );
	}
}

static PyObject* PyGetNxtInterface( PyObject* self, PyObject* /*args*/ )
{
	Tr2NoesisRenderDevice* host = BluePythonCast<Tr2NoesisRenderDevice*>( self );
	const nxt_render_device* api = host->GetNxtRenderDevice();
	if( api == nullptr )
	{
		// No shader source yet, so there is nothing usable to hand over.
		Py_RETURN_NONE;
	}

	if( api->header.retain != nullptr )
	{
		api->header.retain( api->header.self );
	}

	PyObject* capsule = PyCapsule_New( const_cast<nxt_render_device*>( api ),
									   NXT_CAPSULE_RENDER_DEVICE,
									   RenderDeviceCapsuleDestructor );
	if( capsule == nullptr && api->header.release != nullptr )
	{
		api->header.release( api->header.self );
	}
	return capsule;
}

static PyObject* PySetShaderSource( PyObject* self, PyObject* args )
{
	PyObject* source = nullptr;
	if( !PyArg_ParseTuple( args, "O", &source ) )
	{
		return nullptr;
	}

	void* pointer = nullptr;
	if( !Tr2NoesisTakeNxtInterface( source, NXT_CAPSULE_SHADER_SOURCE, pointer ) )
	{
		return nullptr;
	}

	Tr2NoesisRenderDevice* device = BluePythonCast<Tr2NoesisRenderDevice*>( self );
	return PyBool_FromLong(
		device->SetShaderSource( static_cast<const nxt_shader_source*>( pointer ) ) ? 1 : 0 );
}

const Be::ClassInfo* Tr2NoesisRenderDevice::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisRenderDevice,
					"Trinity's side of the Noesis boundary: the render device, wrapped in the\n"
					"vtables the Noesis library calls through. Build it with the library's shader\n"
					"source, then hand it to noesis.set_render_device." )
		MAP_INTERFACE( Tr2NoesisRenderDevice )

		MAP_METHOD(
			"set_shader_source",
			PySetShaderSource,
			"Takes the Noesis library's shader source. The device itself is built on the\n"
			"first frame, because building it needs a live render context and there is\n"
			"none while Python is still starting up.\n"
			":param shaderSource: a noesis.ShaderSource, or None\n"
			":rtype: bool" )

		MAP_METHOD(
			"_nxt_interface",
			PyGetNxtInterface,
			"The nxt_render_device interface, in an " NXT_CAPSULE_RENDER_DEVICE " capsule.\n"
			"\n"
			"Plumbing rather than API: noesis.set_render_device takes this object and calls\n"
			"this itself. None until a shader source has been set.\n"
			":rtype: PyCapsule or None" )

		MAP_PROPERTY_READONLY(
			"isReady",
			IsReady,
			"True once the device is built and usable.\n"
			":rtype: bool" )

	EXPOSURE_END()
}
