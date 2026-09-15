// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisHost.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisRenderDevice.h"
#include "Resources/TriTextureRes.h"
#include "Tr2RenderContext.h"

BLUE_DEFINE( Tr2NoesisHost );

// Each module defines the IIDs it uses, including ones it only implements. Be::IID
// compares by name and hash rather than by address, so these match the definitions in
// frontier-noesis. Same pattern trinity.cpp uses for the Blue interfaces it consumes.
BLUE_DEFINE_INTERFACE( INsiDeviceHost );
BLUE_DEFINE_INTERFACE( INsiShaderSource );
BLUE_DEFINE_INTERFACE( INsiView );

namespace
{

Tr2NoesisHost& Self( void* self )
{
	return *static_cast<Tr2NoesisHost*>( self );
}

Tr2NoesisRenderDevice* Device( void* self )
{
	return Self( self ).GetDevice();
}

Tr2NoesisTexture* AsTexture( nsi_texture texture )
{
	return reinterpret_cast<Tr2NoesisTexture*>( texture );
}

Tr2NoesisRenderTarget* AsTarget( nsi_render_target surface )
{
	return reinterpret_cast<Tr2NoesisRenderTarget*>( surface );
}

// --------------------------------------------------------------------------------------
// Device half
// --------------------------------------------------------------------------------------

void HostGetCaps( void* self, nsi_device_caps* out )
{
	Device( self )->GetCaps( *out );
}

nsi_render_target HostCreateRenderTarget( void* self, const char* label, uint32_t width,
										  uint32_t height, uint32_t sampleCount,
										  nsi_bool needsStencil )
{
	return reinterpret_cast<nsi_render_target>( Device( self )->CreateRenderTarget(
		label, width, height, sampleCount, needsStencil != NSI_FALSE ) );
}

nsi_render_target HostCloneRenderTarget( void* self, const char* label, nsi_render_target surface )
{
	return reinterpret_cast<nsi_render_target>(
		Device( self )->CloneRenderTarget( label, AsTarget( surface ) ) );
}

void HostReleaseRenderTarget( void* /*self*/, nsi_render_target surface )
{
	// Destroys the wrapper. The AL resources it holds go with it, which is what the
	// library means by releasing a handle it created.
	delete AsTarget( surface );
}

nsi_texture HostGetRenderTargetTexture( void* /*self*/, nsi_render_target surface )
{
	Tr2NoesisRenderTarget* target = AsTarget( surface );
	return target != nullptr ? reinterpret_cast<nsi_texture>( target->GetTexture() ) : nullptr;
}

nsi_texture HostCreateTexture( void* self, const char* label, uint32_t width, uint32_t height,
							   uint32_t numLevels, nsi_texture_format format, const void** data )
{
	return reinterpret_cast<nsi_texture>(
		Device( self )->CreateTexture( label, width, height, numLevels, format, data ) );
}

void HostReleaseTexture( void* /*self*/, nsi_texture texture )
{
	// For a wrapped host texture this drops only the wrapper: the Tr2TextureAL inside is
	// a shared handle, so the resource it refers to is untouched. See release_texture.
	delete AsTexture( texture );
}

void HostGetTextureInfo( void* /*self*/, nsi_texture texture, uint32_t* width, uint32_t* height,
						 uint32_t* levels, nsi_bool* hasAlpha )
{
	Tr2NoesisTexture* t = AsTexture( texture );
	if( t == nullptr )
	{
		*width = 0;
		*height = 0;
		*levels = 0;
		*hasAlpha = NSI_FALSE;
		return;
	}

	*width = t->GetWidth();
	*height = t->GetHeight();
	*levels = t->HasMipMaps() ? 2 : 1;
	*hasAlpha = t->HasAlpha() ? NSI_TRUE : NSI_FALSE;
}

nsi_pixel_shader HostCreatePixelShader( void* self, const char* label, uint8_t shader,
										const void* blob, uint32_t size )
{
	return Device( self )->CreatePixelShader( label, shader, blob, size );
}

void HostReleasePixelShader( void* /*self*/, nsi_pixel_shader /*shader*/ )
{
	// Custom shaders live in a vector on the device and are handed out as 1-based
	// indices, so there is nothing per-handle to free. ClearPixelShaders drops them all
	// when the device goes.
}

nsi_texture HostWrapNativeTexture( void* self, void* native, nsi_bool hasAlpha )
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

	return reinterpret_cast<nsi_texture>(
		Device( self )->WrapTexture( *texture, hasAlpha != NSI_FALSE ) );
}

nsi_bool HostGetNativeTextureSize( void* /*self*/, void* native, uint32_t* width, uint32_t* height )
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
		return NSI_FALSE;
	}

	Tr2TextureAL* texture = resource->GetTexture();
	if( texture == nullptr || !texture->IsValid() )
	{
		return NSI_FALSE;
	}

	*width = texture->GetWidth();
	*height = texture->GetHeight();
	return NSI_TRUE;
}

// --------------------------------------------------------------------------------------
// Frame half
// --------------------------------------------------------------------------------------

void HostUpdateTexture( void* self, nsi_texture texture, uint32_t level, uint32_t x, uint32_t y,
						uint32_t width, uint32_t height, const void* data )
{
	Device( self )->UpdateTexture( AsTexture( texture ), level, x, y, width, height, data );
}

void HostBeginOffscreen( void* self ) { Device( self )->BeginOffscreenRender(); }
void HostEndOffscreen( void* self ) { Device( self )->EndOffscreenRender(); }
void HostBeginOnscreen( void* self ) { Device( self )->BeginOnscreenRender(); }
void HostEndOnscreen( void* self ) { Device( self )->EndOnscreenRender(); }

void HostSetRenderTarget( void* self, nsi_render_target surface )
{
	Device( self )->SetRenderTarget( AsTarget( surface ) );
}

void HostBeginTile( void* self, nsi_render_target surface, const nsi_tile* tile )
{
	Device( self )->BeginTile( AsTarget( surface ), *tile );
}

void HostEndTile( void* self, nsi_render_target surface )
{
	Device( self )->EndTile( AsTarget( surface ) );
}

void HostResolveRenderTarget( void* self, nsi_render_target surface, const nsi_tile* tiles,
							  uint32_t numTiles )
{
	Device( self )->ResolveRenderTarget( AsTarget( surface ), tiles, numTiles );
}

void* HostMapVertices( void* self, uint32_t bytes ) { return Device( self )->MapVertices( bytes ); }
void HostUnmapVertices( void* self ) { Device( self )->UnmapVertices(); }
void* HostMapIndices( void* self, uint32_t bytes ) { return Device( self )->MapIndices( bytes ); }
void HostUnmapIndices( void* self ) { Device( self )->UnmapIndices(); }

void HostDrawBatch( void* self, const nsi_batch* batch )
{
	Device( self )->DrawBatch( *batch );
}

void FillHeader( nsi_interface_header& header, void* self, uint32_t size )
{
	header.abi_version_major = NSI_ABI_VERSION_MAJOR;
	header.abi_version_minor = NSI_ABI_VERSION_MINOR;
	header.struct_size = size;
	header.self = self;
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
}

void Tr2NoesisHost::FillVtables()
{
	FillHeader( m_deviceApi.header, this, sizeof( nsi_device_host ) );
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

	FillHeader( m_frameApi.header, this, sizeof( nsi_frame_host ) );
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

bool Tr2NoesisHost::SetShaderSource( IRoot* shaderSource )
{
	m_shaderSourceObject = shaderSource;
	m_shaderSource = Nsi::QueryShaderSource( shaderSource );

	if( shaderSource != nullptr && m_shaderSource == nullptr )
	{
		CCP_NOESIS_LOGERR( "The object given as a shader source is not one, or speaks an "
						   "nsi ABI this Trinity cannot; this Trinity is nsi %u.%u",
						   static_cast<uint32_t>( NSI_ABI_VERSION_MAJOR ),
						   static_cast<uint32_t>( NSI_ABI_VERSION_MINOR ) );
		return false;
	}

	// A new source means a new device; the old one was built from the old blobs.
	m_device.reset();
	m_deviceAttempted = false;
	return m_shaderSource != nullptr;
}

IRoot* Tr2NoesisHost::GetShaderSource() const
{
	return m_shaderSourceObject;
}

bool Tr2NoesisHost::EnsureDevice()
{
	if( m_deviceAttempted )
	{
		return m_device != nullptr && m_device->IsValid();
	}

	if( m_shaderSource == nullptr )
	{
		CCP_NOESIS_LOGERR( "No shader source is set; Python must pass the Noesis library's "
						   "shader source before anything can render" );
		return false;
	}
	m_deviceAttempted = true;

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

const nsi_device_host* Tr2NoesisHost::GetNsiDeviceHost()
{
	// Valid as soon as a shader source is set, not once the device is built: the library
	// wires this at startup and the device cannot exist until the first frame. Every call
	// through it happens inside a frame, by which point EnsureDevice has run.
	return m_shaderSource != nullptr ? &m_deviceApi : nullptr;
}

const nsi_frame_host* Tr2NoesisHost::GetNsiFrameHost()
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

const Be::ClassInfo* Tr2NoesisHost::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisHost,
					"Trinity's side of the Noesis boundary: the render device, wrapped in the\n"
					"vtables the Noesis library calls through. Build it with the library's shader\n"
					"source, then hand it to noesis.set_device_host." )
		MAP_INTERFACE( Tr2NoesisHost )
		MAP_INTERFACE( INsiDeviceHost )

		MAP_METHOD_AND_WRAP(
			"SetShaderSource",
			SetShaderSource,
			"Takes the Noesis library's shader source. The device itself is built on the first\n"
			"frame, because building it needs a live render context and there is none while\n"
			"Python is still starting up.\n"
			":param shaderSource: an NsiShaderSource from the Noesis library\n"
			":rtype: bool" )

		MAP_PROPERTY_READONLY(
			"isReady",
			IsReady,
			"True once the device is built and usable.\n"
			":rtype: bool" )

	EXPOSURE_END()
}

#endif
