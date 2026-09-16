// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisHost.h"

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisRenderDevice.h"
#include "Resources/TriTextureRes.h"
#include "Tr2RenderContext.h"

BLUE_DEFINE( Tr2NoesisHost );

// Each module defines the IIDs it uses, including ones it only implements. Be::IID
// compares by name and hash rather than by address, so these match the definitions in
// frontier-noesis. Same pattern trinity.cpp uses for the Blue interfaces it consumes.
BLUE_DEFINE_INTERFACE( INhiDeviceHost );
BLUE_DEFINE_INTERFACE( INhiShaderSource );
BLUE_DEFINE_INTERFACE( INhiView );

namespace
{

Tr2NoesisHost& Self( void* self )
{
	return *static_cast<Tr2NoesisHost*>( self );
}

// Null between Python wiring the host and the first Execute building the device. The
// library holds the device-half vtable across that window and is entitled to call into
// it: these entry points carry no frame, so there is no RequireFrame to close it from
// that side. Device-half thunks must therefore decline rather than dereference. A failed
// resource creation is something the SDK handles; a null dereference is not.
Tr2NoesisRenderDevice* Device( void* self )
{
	Tr2NoesisRenderDevice* device = Self( self ).GetDevice();
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

Tr2NoesisTexture* AsTexture( nhi_texture texture )
{
	return reinterpret_cast<Tr2NoesisTexture*>( texture );
}

Tr2NoesisRenderTarget* AsTarget( nhi_render_target surface )
{
	return reinterpret_cast<Tr2NoesisRenderTarget*>( surface );
}

// --------------------------------------------------------------------------------------
// Device half
// --------------------------------------------------------------------------------------

void HostGetCaps( void* self, nhi_device_caps* out )
{
	Tr2NoesisRenderDevice* device = Device( self );
	if( device == nullptr )
	{
		// Zeroed, not left undefined: the library reads these to choose how it renders,
		// and false is the conservative answer to each.
		*out = nhi_device_caps();
		return;
	}
	device->GetCaps( *out );
}

nhi_render_target HostCreateRenderTarget( void* self, const char* label, uint32_t width,
										  uint32_t height, uint32_t sampleCount,
										  nhi_bool needsStencil )
{
	Tr2NoesisRenderDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return reinterpret_cast<nhi_render_target>( device->CreateRenderTarget(
		label, width, height, sampleCount, needsStencil != NHI_FALSE ) );
}

nhi_render_target HostCloneRenderTarget( void* self, const char* label, nhi_render_target surface )
{
	Tr2NoesisRenderDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return reinterpret_cast<nhi_render_target>(
		device->CloneRenderTarget( label, AsTarget( surface ) ) );
}

void HostReleaseRenderTarget( void* /*self*/, nhi_render_target surface )
{
	// Destroys the wrapper. The AL resources it holds go with it, which is what the
	// library means by releasing a handle it created.
	delete AsTarget( surface );
}

nhi_texture HostGetRenderTargetTexture( void* /*self*/, nhi_render_target surface )
{
	Tr2NoesisRenderTarget* target = AsTarget( surface );
	return target != nullptr ? reinterpret_cast<nhi_texture>( target->GetColor() ) : nullptr;
}

nhi_texture HostCreateTexture( void* self, const char* label, uint32_t width, uint32_t height,
							   uint32_t numLevels, nhi_texture_format format, const void** data )
{
	Tr2NoesisRenderDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return reinterpret_cast<nhi_texture>(
		device->CreateTexture( label, width, height, numLevels, format, data ) );
}

void HostReleaseTexture( void* /*self*/, nhi_texture texture )
{
	// For a wrapped host texture this drops only the wrapper: the Tr2TextureAL inside is
	// a shared handle, so the resource it refers to is untouched. See release_texture.
	delete AsTexture( texture );
}

void HostGetTextureInfo( void* /*self*/, nhi_texture texture, uint32_t* width, uint32_t* height,
						 uint32_t* levels, nhi_bool* hasAlpha )
{
	Tr2NoesisTexture* t = AsTexture( texture );
	if( t == nullptr )
	{
		*width = 0;
		*height = 0;
		*levels = 0;
		*hasAlpha = NHI_FALSE;
		return;
	}

	*width = t->GetWidth();
	*height = t->GetHeight();
	*levels = t->GetLevels();
	*hasAlpha = t->HasAlpha() ? NHI_TRUE : NHI_FALSE;
}

nhi_pixel_shader HostCreatePixelShader( void* self, const char* label, uint8_t shader,
										const void* blob, uint32_t size )
{
	Tr2NoesisRenderDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}
	return device->CreatePixelShader( label, shader, blob, size );
}

void HostReleasePixelShader( void* /*self*/, nhi_pixel_shader /*shader*/ )
{
	// Handles are 1-based indices into a vector on the device, so there is nothing to
	// free per handle. They live until the device does.
}

nhi_texture HostWrapNativeTexture( void* self, void* native, nhi_bool hasAlpha )
{
	// `native` is the Blue object Python passed to the video sink: a Trinity texture
	// resource. The library never looked inside it, which is why it can be anything the
	// host recognises.
	IRoot* object = static_cast<IRoot*>( native );
	TriTextureResPtr resource;
	resource = BlueCastPtr( object );
	if( resource == nullptr )
	{
		return nullptr;
	}

	Tr2TextureAL* texture = resource->GetTexture();
	if( texture == nullptr || !texture->IsValid() )
	{
		return nullptr;
	}

	Tr2NoesisRenderDevice* device = Device( self );
	if( device == nullptr )
	{
		return nullptr;
	}

	return reinterpret_cast<nhi_texture>(
		device->WrapTexture( *texture, hasAlpha != NHI_FALSE ) );
}

nhi_bool HostGetNativeTextureSize( void* /*self*/, void* native, uint32_t* width, uint32_t* height )
{
	*width = 0;
	*height = 0;

	IRoot* object = static_cast<IRoot*>( native );
	TriTextureResPtr resource;
	resource = BlueCastPtr( object );
	if( resource == nullptr )
	{
		// The one place the host can tell the caller it passed the wrong kind of object,
		// because only this side knows what the handle was supposed to be.
		return NHI_FALSE;
	}

	Tr2TextureAL* texture = resource->GetTexture();
	if( texture == nullptr || !texture->IsValid() )
	{
		return NHI_FALSE;
	}

	*width = texture->GetWidth();
	*height = texture->GetHeight();
	return NHI_TRUE;
}

// --------------------------------------------------------------------------------------
// Frame half
// --------------------------------------------------------------------------------------

void HostUpdateTexture( void* self, nhi_texture texture, uint32_t level, uint32_t x, uint32_t y,
						uint32_t width, uint32_t height, const void* data )
{
	Device( self )->UpdateTexture( AsTexture( texture ), level, x, y, width, height, data );
}

void HostBeginOffscreen( void* self ) { Device( self )->BeginOffscreenRender(); }
void HostEndOffscreen( void* self ) { Device( self )->EndOffscreenRender(); }
void HostBeginOnscreen( void* self ) { Device( self )->BeginOnscreenRender(); }
void HostEndOnscreen( void* self ) { Device( self )->EndOnscreenRender(); }

void HostSetRenderTarget( void* self, nhi_render_target surface )
{
	Device( self )->SetRenderTarget( AsTarget( surface ) );
}

void HostBeginTile( void* self, nhi_render_target surface, const nhi_tile* tile )
{
	Device( self )->BeginTile( AsTarget( surface ), *tile );
}

void HostEndTile( void* self, nhi_render_target surface )
{
	Device( self )->EndTile( AsTarget( surface ) );
}

void HostResolveRenderTarget( void* self, nhi_render_target surface, const nhi_tile* tiles,
							  uint32_t numTiles )
{
	Device( self )->ResolveRenderTarget( AsTarget( surface ), tiles, numTiles );
}

void* HostMapVertices( void* self, uint32_t bytes ) { return Device( self )->MapVertices( bytes ); }
void HostUnmapVertices( void* self ) { Device( self )->UnmapVertices(); }
void* HostMapIndices( void* self, uint32_t bytes ) { return Device( self )->MapIndices( bytes ); }
void HostUnmapIndices( void* self ) { Device( self )->UnmapIndices(); }

void HostDrawBatch( void* self, const nhi_batch* batch )
{
	Device( self )->DrawBatch( *batch );
}

// nhi retain/release. Blue's refcount is Lock/Unlock, so these are that and nothing
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

// `owned` false leaves retain and release null, which nhi.h defines as borrowed: valid
// for the call it was passed to and never to be stored. That is the frame host.
void FillHeader( nhi_interface_header& header, void* self, uint32_t size, bool owned )
{
	header.abi_version_major = NHI_ABI_VERSION_MAJOR;
	header.abi_version_minor = NHI_ABI_VERSION_MINOR;
	header.struct_size = size;
	header.self = self;
	header.retain = owned ? HostRetain : nullptr;
	header.release = owned ? HostRelease : nullptr;
}

}

Tr2NoesisHost::Tr2NoesisHost( IRoot* ) :
	m_shaderSource( nullptr ),
	m_deviceAttempted( false ),
	m_deviceApi(),
	m_frameApi()
{
	FillVtables();
}

Tr2NoesisHost::~Tr2NoesisHost()
{
	if( m_shaderSource != nullptr && m_shaderSource->header.release != nullptr )
	{
		m_shaderSource->header.release( m_shaderSource->header.self );
	}
}

void Tr2NoesisHost::FillVtables()
{
	FillHeader( m_deviceApi.header, this, sizeof( nhi_device_host ), true );
	m_deviceApi.get_caps = HostGetCaps;
	m_deviceApi.create_render_target = HostCreateRenderTarget;
	m_deviceApi.clone_render_target = HostCloneRenderTarget;
	m_deviceApi.release_render_target = HostReleaseRenderTarget;
	m_deviceApi.get_render_target_texture = HostGetRenderTargetTexture;
	m_deviceApi.create_texture = HostCreateTexture;
	m_deviceApi.release_texture = HostReleaseTexture;
	m_deviceApi.get_texture_info = HostGetTextureInfo;
	m_deviceApi.create_pixel_shader = HostCreatePixelShader;
	m_deviceApi.release_pixel_shader = HostReleasePixelShader;
	m_deviceApi.wrap_native_texture = HostWrapNativeTexture;
	m_deviceApi.get_native_texture_size = HostGetNativeTextureSize;

	FillHeader( m_frameApi.header, this, sizeof( nhi_frame_host ), false );
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

bool Tr2NoesisHost::SetShaderSource( const nhi_shader_source* shaderSource )
{
	if( shaderSource != nullptr && nhi_interface_usable( &shaderSource->header ) == NHI_FALSE )
	{
		CCP_NOESIS_LOGERR( "The shader source speaks an nhi ABI this Trinity cannot; this "
						   "Trinity is nhi %u.%u",
						   static_cast<uint32_t>( NHI_ABI_VERSION_MAJOR ),
						   static_cast<uint32_t>( NHI_ABI_VERSION_MINOR ) );
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

bool Tr2NoesisHost::EnsureDevice()
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

	m_device.reset( new Tr2NoesisRenderDevice( renderContext.GetPrimaryRenderContext(),
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

bool Tr2NoesisHost::IsReady() const
{
	return m_device != nullptr && m_device->IsValid();
}

const nhi_device_host* Tr2NoesisHost::GetNsiDeviceHost()
{
	// Valid as soon as a shader source is set, not once the device is built: the library
	// wires this at startup and the device cannot exist until the first frame. Calls that
	// land in that window are the reason Device() checks before dereferencing.
	return m_shaderSource != nullptr ? &m_deviceApi : nullptr;
}

const nhi_frame_host* Tr2NoesisHost::GetNsiFrameHost()
{
	return IsReady() ? &m_frameApi : nullptr;
}

void Tr2NoesisHost::BeginFrame( Tr2RenderContext& renderContext )
{
	if( m_device != nullptr )
	{
		m_device->SetRenderContext( renderContext );
	}
}

void Tr2NoesisHost::EndFrame()
{
	if( m_device != nullptr )
	{
		m_device->ClearRenderContext();
	}
}

void Tr2NoesisHost::SetHostScissor( const Tr2ScissorRect& rect )
{
	if( m_device != nullptr )
	{
		m_device->SetHostScissor( rect );
	}
}

void Tr2NoesisHost::ClearHostScissor()
{
	if( m_device != nullptr )
	{
		m_device->ClearHostScissor();
	}
}

Tr2NoesisRenderDevice* Tr2NoesisHost::GetDevice() const
{
	return m_device.get();
}

static PyObject* PySetShaderSource( PyObject* self, PyObject* args )
{
	PyObject* capsule = nullptr;
	if( !PyArg_ParseTuple( args, "O", &capsule ) )
	{
		return nullptr;
	}

	const nhi_shader_source* api = nullptr;
	if( capsule != Py_None )
	{
		// The capsule name is the first gate: a capsule from a different major version of
		// the ABI is refused here, before a single field is read.
		void* pointer = PyCapsule_GetPointer( capsule, NHI_CAPSULE_SHADER_SOURCE );
		if( pointer == nullptr )
		{
			PyErr_SetString( PyExc_TypeError,
							 "expected a " NHI_CAPSULE_SHADER_SOURCE " capsule, or None" );
			return nullptr;
		}
		api = static_cast<const nhi_shader_source*>( pointer );
	}

	Tr2NoesisHost* host = BluePythonCast<Tr2NoesisHost*>( self );
	return PyBool_FromLong( host->SetShaderSource( api ) ? 1 : 0 );
}

const Be::ClassInfo* Tr2NoesisHost::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisHost,
					"Trinity's side of the Noesis boundary: the render device, wrapped in the\n"
					"vtables the Noesis library calls through. Build it with the library's shader\n"
					"source, then hand it to noesis.set_device_host." )
		MAP_INTERFACE( Tr2NoesisHost )
		MAP_INTERFACE( INhiDeviceHost )

		MAP_METHOD(
			"SetShaderSource",
			PySetShaderSource,
			"Takes the Noesis library's shader source, as the capsule its\n"
			"get_nhi_interface() returns. The device itself is built on the first frame,\n"
			"because building it needs a live render context and there is none while Python\n"
			"is still starting up.\n"
			":param shaderSource: an " NHI_CAPSULE_SHADER_SOURCE " capsule, or None\n"
			":rtype: bool" )

		MAP_PROPERTY_READONLY(
			"isReady",
			IsReady,
			"True once the device is built and usable.\n"
			":rtype: bool" )

	EXPOSURE_END()
}
