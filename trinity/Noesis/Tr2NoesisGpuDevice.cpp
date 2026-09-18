// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisGpuDevice.h"

#include "Noesis/Tr2NoesisLog.h"
#include "Tr2RenderContext.h"


#include <cmath>
#include <string>
#include <utility>

using namespace Tr2RenderContextEnum;

// The largest single Map the SDK will ask for, from its own NOESIS_DYNAMIC_VB_SIZE and
// NOESIS_DYNAMIC_IB_SIZE defaults. They size a chunk of each ring; the rings grow past them by
// appending chunks, so these are a starting point rather than a cap.
const uint32_t NOESIS_DYNAMIC_VB_SIZE = 512 * 1024;
const uint32_t NOESIS_DYNAMIC_IB_SIZE = 128 * 1024;

namespace
{

// Maps one attribute the library described onto the AL's vertex vocabulary. Semantic and
// index cross the ABI exactly as the bytecode was compiled with them, so the fxc semantic
// renames are the library's business and nothing here has to restate them.
bool ToVertexUsage( const nxt_vertex_attribute& attr, Tr2VertexDefinition::UsageCode& usage )
{
	if( attr.semantic == nullptr )
	{
		return false;
	}
	if( strcmp( attr.semantic, "POSITION" ) == 0 )
	{
		usage = Tr2VertexDefinition::POSITION;
		return true;
	}
	if( strcmp( attr.semantic, "COLOR" ) == 0 )
	{
		usage = Tr2VertexDefinition::COLOR;
		return true;
	}
	if( strcmp( attr.semantic, "TEXCOORD" ) == 0 )
	{
		usage = Tr2VertexDefinition::TEXCOORD;
		return true;
	}
	return false;
}

uint32_t VertexAttrSize( nxt_vertex_attr_type type )
{
	switch( type )
	{
	case NXT_VERTEX_ATTR_FLOAT:
		return 4;
	case NXT_VERTEX_ATTR_FLOAT2:
		return 8;
	case NXT_VERTEX_ATTR_FLOAT4:
		return 16;
	case NXT_VERTEX_ATTR_UBYTE4_NORM:
		return 4;
	case NXT_VERTEX_ATTR_USHORT4_NORM:
		return 8;
	}
	return 0;
}

bool ToVertexDataType( nxt_vertex_attr_type type, Tr2VertexDefinition::DataType& dataType,
					   uint32_t& dimension )
{
	switch( type )
	{
	case NXT_VERTEX_ATTR_FLOAT:
		dataType = Tr2VertexDefinition::FLOAT32_1;
		dimension = 1;
		return true;
	case NXT_VERTEX_ATTR_FLOAT2:
		dataType = Tr2VertexDefinition::FLOAT32_2;
		dimension = 2;
		return true;
	case NXT_VERTEX_ATTR_FLOAT4:
		dataType = Tr2VertexDefinition::FLOAT32_4;
		dimension = 4;
		return true;
	case NXT_VERTEX_ATTR_UBYTE4_NORM:
		dataType = Tr2VertexDefinition::UBYTE_4_NORM;
		dimension = 4;
		return true;
	case NXT_VERTEX_ATTR_USHORT4_NORM:
		dataType = Tr2VertexDefinition::USHORT_4_NORM;
		dimension = 4;
		return true;
	}
	return false;
}

// The bit layout is nxt.h's guarantee, and the library asserts its own agreement with
// the SDK at its end. Here it is enough that a byte addresses 64 slots.
static_assert( sizeof( nxt_sampler_state ) == 1, "nxt_sampler_state is a packed byte" );

// Only the six bits nxt.h defines. The top two are documented as unused, but this value
// arrives over the ABI and the table it indexes has exactly 64 entries, so it is masked
// rather than trusted -- an assert would compile out in the builds that ship.
const nxt_sampler_state SAMPLER_INDEX_MASK = 0x3f;

// Stock programs are keyed by shader id, custom ones by their index into m_customShaders.
// The tag keeps the two apart in the resource-set cache's key space.
const uint64_t CUSTOM_PROGRAM_ID_TAG = 0x80000000ull;

// The five pixel texture slots, in register order.
//
// nxt_batch names each texture and its sampler as its own field, and three separate things
// here have to walk the same five: the signature a shader declares, the signature a batch
// supplies, and the binding itself. One table, so a sixth slot is one row rather than
// three edits in three functions.
struct TextureSlot
{
	uint32_t flag;
	uint32_t registerIndex;
	nxt_texture nxt_batch::*texture;
	nxt_sampler_state nxt_batch::*sampler;
};

const TextureSlot TEXTURE_SLOTS[] = {
	{ NXT_SHADER_USES_PS_T0, 0, &nxt_batch::pattern, &nxt_batch::pattern_sampler },
	{ NXT_SHADER_USES_PS_T1, 1, &nxt_batch::ramps, &nxt_batch::ramps_sampler },
	{ NXT_SHADER_USES_PS_T2, 2, &nxt_batch::image, &nxt_batch::image_sampler },
	{ NXT_SHADER_USES_PS_T3, 3, &nxt_batch::glyphs, &nxt_batch::glyphs_sampler },
	{ NXT_SHADER_USES_PS_T4, 4, &nxt_batch::shadow, &nxt_batch::shadow_sampler },
};

void FillPixelSignature( Tr2ShaderSignatureAL& signature, uint32_t flags )
{
	if( flags & NXT_SHADER_USES_PS_CB0 )
	{
		signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0 );
	}
	if( flags & NXT_SHADER_USES_PS_CB1 )
	{
		signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 1 );
	}

	for( const TextureSlot& slot : TEXTURE_SLOTS )
	{
		if( flags & slot.flag )
		{
			signature.Add( Tr2ShaderRegisterAL::SRV_TEXTURE2D, slot.registerIndex );
			signature.Add( Tr2ShaderRegisterAL::SAMPLER, slot.registerIndex );
		}
	}
}

uint32_t GetBatchSignature( const nxt_batch& batch )
{
	uint32_t signature = 0;

	for( const TextureSlot& slot : TEXTURE_SLOTS )
	{
		if( batch.*slot.texture != nullptr )
		{
			signature |= slot.flag;
		}
	}

	if( batch.vertex_uniforms[0].values )
	{
		signature |= NXT_SHADER_USES_VS_CB0;
	}
	if( batch.vertex_uniforms[1].values )
	{
		signature |= NXT_SHADER_USES_VS_CB1;
	}
	if( batch.pixel_uniforms[0].values )
	{
		signature |= NXT_SHADER_USES_PS_CB0;
	}
	if( batch.pixel_uniforms[1].values )
	{
		signature |= NXT_SHADER_USES_PS_CB1;
	}
	return signature;
}

PixelFormat NoesisStencilFormat()
{
#if TRINITY_PLATFORM == TRINITY_METAL
	// MetalUtils maps D24S8 to Depth32Float with no stencil plane on Apple Silicon.
	return PIXEL_FORMAT_D32_FLOAT_S8X24_UINT;
#else
	return PIXEL_FORMAT_D24_UNORM_S8_UINT;
#endif
}

PixelFormat ToPixelFormat( nxt_texture_format format )
{
	switch( format )
	{
	case NXT_TEXTURE_FORMAT_RGBA8:
	case NXT_TEXTURE_FORMAT_RGBX8:
		return PIXEL_FORMAT_R8G8B8A8_UNORM;
	case NXT_TEXTURE_FORMAT_R8:
		return PIXEL_FORMAT_R8_UNORM;
	default:
		CCP_ASSERT_M( false, "Unsupported Noesis texture format" );
		return PIXEL_FORMAT_R8G8B8A8_UNORM;
	}
}

uint32_t BytesPerPixel( nxt_texture_format format )
{
	switch( format )
	{
	case NXT_TEXTURE_FORMAT_RGBA8:
	case NXT_TEXTURE_FORMAT_RGBX8:
		return 4;
	case NXT_TEXTURE_FORMAT_R8:
		return 1;
	default:
		CCP_ASSERT_M( false, "Unsupported Noesis texture format" );
		return 4;
	}
}

bool AddVertexAttributes( Tr2VertexDefinition& definition, Tr2ShaderSignatureAL* vsSignature,
						  const std::vector<nxt_vertex_attribute>& attributes )
{
	uint32_t registerIndex = 0;
	for( const nxt_vertex_attribute& attr : attributes )
	{
		Tr2VertexDefinition::UsageCode usage = Tr2VertexDefinition::POSITION;
		Tr2VertexDefinition::DataType dataType = Tr2VertexDefinition::FLOAT32_4;
		uint32_t dimension = 4;

		if( !ToVertexUsage( attr, usage ) || !ToVertexDataType( attr.type, dataType, dimension ) )
		{
			CCP_NOESIS_LOGERR( "The Noesis library described a vertex attribute this "
							   "Trinity cannot express (semantic '%s', type %d)",
							   attr.semantic != nullptr ? attr.semantic : "?",
							   static_cast<int>( attr.type ) );
			return false;
		}

		definition.Add( dataType, usage, attr.semantic_index );
		if( vsSignature )
		{
			vsSignature->Add( usage, attr.semantic_index, registerIndex,
							  Tr2ShaderPipelineInputAL::FLOAT, dimension );
		}
		++registerIndex;
	}
	return true;
}

void ToAddressMode( nxt_wrap_mode wrap, Tr2SamplerDescription& desc )
{
	switch( wrap )
	{
	case NXT_WRAP_CLAMP_TO_EDGE:
		desc.m_addressU = TA_CLAMP;
		desc.m_addressV = TA_CLAMP;
		break;
	case NXT_WRAP_CLAMP_TO_ZERO:
		desc.m_addressU = TA_BORDER;
		desc.m_addressV = TA_BORDER;
		break;
	case NXT_WRAP_REPEAT:
		desc.m_addressU = TA_WRAP;
		desc.m_addressV = TA_WRAP;
		break;
	case NXT_WRAP_MIRROR_U:
		desc.m_addressU = TA_MIRROR;
		desc.m_addressV = TA_WRAP;
		break;
	case NXT_WRAP_MIRROR_V:
		desc.m_addressU = TA_WRAP;
		desc.m_addressV = TA_MIRROR;
		break;
	case NXT_WRAP_MIRROR:
		desc.m_addressU = TA_MIRROR;
		desc.m_addressV = TA_MIRROR;
		break;
	default:
		CCP_ASSERT_M( false, "Unknown Noesis wrap mode" );
		desc.m_addressU = TA_CLAMP;
		desc.m_addressV = TA_CLAMP;
		break;
	}
}

Tr2RenderContextEnum::TextureFilter ToMinMagFilter( nxt_minmag_filter filter )
{
	return filter == NXT_MINMAG_LINEAR ? TF_LINEAR : TF_POINT;
}

Tr2RenderContextEnum::TextureFilter ToMipFilter( nxt_mip_filter filter )
{
	switch( filter )
	{
	case NXT_MIP_LINEAR:
		return TF_LINEAR;
	case NXT_MIP_NEAREST:
		return TF_POINT;
	case NXT_MIP_DISABLED:
		return TF_NONE;
	default:
		CCP_ASSERT_M( false, "Unknown Noesis mip filter" );
		return TF_POINT;
	}
}

const char* SafeLabel( const char* label, const char* fallback )
{
	return label ? label : fallback;
}

// sprintf_s aborts on overflow; Noesis labels have no length bound, so truncate into the
// 128-byte debug-name buffer instead.
const char* FormatDebugName( char ( &out )[128], const char* label, const char* fallback, const char* suffix = "" )
{
	_snprintf_s( out, _TRUNCATE, "Noesis_%s%s", SafeLabel( label, fallback ), suffix );
	return out;
}

// IRenderer::Render uses the host viewport and scissor. D3D12 scissor is always
// on, and SetDepthStencil resets it to the full target, so the rect has to match
// the viewport (clamped to the target — D3D12 rejects a scissor that extends past it).
Tr2ScissorRect ScissorForViewport( const Tr2Viewport& vp, uint32_t rtWidth, uint32_t rtHeight )
{
	const int rtW = int( rtWidth );
	const int rtH = int( rtHeight );
	int left = int( floorf( vp.m_x ) );
	int top = int( floorf( vp.m_y ) );
	int right = int( ceilf( vp.m_x + vp.m_width ) );
	int bottom = int( ceilf( vp.m_y + vp.m_height ) );
	if( left < 0 )
	{
		left = 0;
	}
	if( top < 0 )
	{
		top = 0;
	}
	if( right > rtW )
	{
		right = rtW;
	}
	if( bottom > rtH )
	{
		bottom = rtH;
	}
	if( right < left )
	{
		right = left;
	}
	if( bottom < top )
	{
		bottom = top;
	}
	Tr2ScissorRect rect;
	rect.m_left = left;
	rect.m_top = top;
	rect.m_right = right;
	rect.m_bottom = bottom;
	return rect;
}

Tr2ScissorRect IntersectScissor( const Tr2ScissorRect& a, const Tr2ScissorRect& b )
{
	Tr2ScissorRect rect;
	rect.m_left = a.m_left > b.m_left ? a.m_left : b.m_left;
	rect.m_top = a.m_top > b.m_top ? a.m_top : b.m_top;
	rect.m_right = a.m_right < b.m_right ? a.m_right : b.m_right;
	rect.m_bottom = a.m_bottom < b.m_bottom ? a.m_bottom : b.m_bottom;
	if( rect.m_right < rect.m_left )
	{
		rect.m_right = rect.m_left;
	}
	if( rect.m_bottom < rect.m_top )
	{
		rect.m_bottom = rect.m_top;
	}
	return rect;
}

}

// --------------------------------------------------------------------------------------
// Tr2NoesisTexture
// --------------------------------------------------------------------------------------

Tr2NoesisTexture::Tr2NoesisTexture( Tr2TextureAL texture, uint32_t width, uint32_t height, uint32_t levels, bool hasAlpha ) :
	m_texture( texture ),
	m_width( width ),
	m_height( height ),
	m_levels( levels ),
	m_hasAlpha( hasAlpha )
{
}

uint32_t Tr2NoesisTexture::GetWidth() const
{
	return m_width;
}

uint32_t Tr2NoesisTexture::GetHeight() const
{
	return m_height;
}

uint32_t Tr2NoesisTexture::GetLevels() const
{
	return m_levels;
}

bool Tr2NoesisTexture::HasMipMaps() const
{
	return m_levels > 1;
}

bool Tr2NoesisTexture::HasAlpha() const
{
	return m_hasAlpha;
}

Tr2TextureAL& Tr2NoesisTexture::GetAL()
{
	return m_texture;
}

const Tr2TextureAL& Tr2NoesisTexture::GetAL() const
{
	return m_texture;
}

// --------------------------------------------------------------------------------------
// Tr2NoesisRenderTarget
// --------------------------------------------------------------------------------------

Tr2NoesisRenderTarget::Tr2NoesisRenderTarget( Tr2NoesisTexture* color, Tr2TextureAL stencil, uint32_t width, uint32_t height ) :
	m_color( color ),
	m_stencil( stencil ),
	m_width( width ),
	m_height( height )
{
}

Tr2NoesisTexture* Tr2NoesisRenderTarget::GetColor()
{
	return m_color;
}

Tr2TextureAL& Tr2NoesisRenderTarget::GetStencil()
{
	return m_stencil;
}

bool Tr2NoesisRenderTarget::HasStencil() const
{
	return m_stencil.IsValid();
}

uint32_t Tr2NoesisRenderTarget::GetWidth() const
{
	return m_width;
}

uint32_t Tr2NoesisRenderTarget::GetHeight() const
{
	return m_height;
}

// --------------------------------------------------------------------------------------
// DynamicRing
// --------------------------------------------------------------------------------------

bool Tr2NoesisGpuDevice::DynamicRing::Create( uint32_t bufferStride, uint32_t bytesPerChunk, Tr2GpuUsage::Type usage, const char* ringName, Tr2PrimaryRenderContextAL& primary )
{
	CCP_ASSERT_M( bufferStride != 0 && ( bytesPerChunk % bufferStride ) == 0, "Noesis ring chunk size must be a whole number of strides" );

	chunks.clear();
	stride = bufferStride;
	chunkSize = bytesPerChunk;
	gpuUsage = usage;
	name = ringName != nullptr ? ringName : "Ring";
	chunkIndex = INVALID_CHUNK;
	pos = 0;
	drawPos = 0;
	frame = primary.GetRecordingFrameNumber();
	mapped = false;
	growthCapped = false;
	frameBytes = 0;
	reportedPeak = 0;

	// One chunk up front, so a device that cannot get GPU memory reports it at construction
	// rather than on the first heavy frame. AppendChunk leaves it current and stamped with
	// this frame, which is what the frame's first Map wants anyway.
	return AppendChunk( primary );
}

// --------------------------------------------------------------------------------------
// Description:
//   Adds a chunk to the ring and makes it current. Fails once the ring is at its cap.
// Arguments:
//   primary - main-thread primary render context
// Return value:
//   true If a chunk was created
// --------------------------------------------------------------------------------------
bool Tr2NoesisGpuDevice::DynamicRing::AppendChunk( Tr2PrimaryRenderContextAL& primary )
{
	if( chunks.size() >= MAX_CHUNKS )
	{
		if( !growthCapped )
		{
			const uint32_t cap = MAX_CHUNKS;
			CCP_NOESIS_LOGERR( "Noesis ring '%s' is at its cap of %u chunks (%u bytes); dropping UI geometry this frame",
							   name.c_str(), cap, cap * chunkSize );
			growthCapped = true;
		}
		return false;
	}

	const uint32_t index = static_cast<uint32_t>( chunks.size() );
	const Tr2CpuUsage::Type cpuUsage = Tr2CpuUsage::WRITE_OFTEN | Tr2CpuUsage::NON_SYNCRONIZED_WRITE;

	Chunk chunk;
	const ALResult result = chunk.buffer.Create( stride, chunkSize / stride, gpuUsage, cpuUsage, nullptr, primary );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis dynamic buffer '%s' chunk %u", name.c_str(), index );
		CCP_ASSERT_M( false, "Failed to create Noesis dynamic buffer" );
		return false;
	}

	char chunkName[64];
	sprintf_s( chunkName, "Noesis_%s_%u", name.c_str(), index );
	chunk.buffer.SetName( chunkName );
	chunk.lastUsedFrame = frame;

	chunks.push_back( chunk );
	chunkIndex = index;
	pos = 0;
	return true;
}

// --------------------------------------------------------------------------------------
// Description:
//   Makes a chunk the GPU has finished with current, growing the ring if they are all
//   still in flight.
// Arguments:
//   primary - main-thread primary render context, for the rendered frame number
// Return value:
//   true If there is a chunk to write into
// --------------------------------------------------------------------------------------
bool Tr2NoesisGpuDevice::DynamicRing::AcquireChunk( Tr2PrimaryRenderContextAL& primary )
{
	// Chunks already filled this frame carry the recording frame number, which is always
	// ahead of the rendered one, so they are never handed out twice within a frame.
	const uint64_t rendered = primary.GetRenderedFrameNumber();
	for( uint32_t i = 0; i < static_cast<uint32_t>( chunks.size() ); ++i )
	{
		if( chunks[i].lastUsedFrame != frame && chunks[i].lastUsedFrame <= rendered )
		{
			chunks[i].lastUsedFrame = frame;
			chunkIndex = i;
			pos = 0;
			return true;
		}
	}

	return AppendChunk( primary );
}

// --------------------------------------------------------------------------------------
// Description:
//   Starts a new frame's allocations if the recording frame has moved on.
// Arguments:
//   primary - main-thread primary render context, for the recording frame number
// --------------------------------------------------------------------------------------
void Tr2NoesisGpuDevice::DynamicRing::SyncFrame( Tr2PrimaryRenderContextAL& primary )
{
	// Keyed on the frame number rather than an index into a fixed set of pages: every view
	// on this device shares the frame's allocations, and a frame Noesis sat out must not
	// leave the bump offset where it was.
	const uint64_t recording = primary.GetRecordingFrameNumber();
	if( recording == frame )
	{
		return;
	}

	CCP_ASSERT_M( !mapped, "Noesis ring still mapped at a frame boundary" );
	if( mapped )
	{
		return;
	}

	if( frameBytes > reportedPeak )
	{
		reportedPeak = frameBytes;
		if( Tr2Noesis::IsLogVerbose() )
		{
			CCP_NOESIS_LOG( "Noesis ring '%s' peak %u bytes in one frame, %u chunk(s) of %u allocated",
							name.c_str(), frameBytes, static_cast<uint32_t>( chunks.size() ), chunkSize );
		}
	}

	frame = recording;
	chunkIndex = INVALID_CHUNK;
	pos = 0;
	frameBytes = 0;
	growthCapped = false;
}

void* Tr2NoesisGpuDevice::DynamicRing::Map( uint32_t bytes, Tr2RenderContextAL& context, Tr2PrimaryRenderContextAL& primary )
{
	if( bytes > chunkSize )
	{
		// Only reachable if a ring is created with chunks below the SDK's per-Map cap.
		CCP_NOESIS_LOGERR( "Noesis Map request %u exceeds ring '%s' chunk size %u", bytes, name.c_str(), chunkSize );
		CCP_ASSERT_M( false, "Noesis Map request exceeds ring chunk size" );
		return nullptr;
	}

	CCP_ASSERT_M( !mapped, "Noesis Map without a matching Unmap" );
	if( mapped )
	{
		Unmap( context );
	}

	if( ( chunkIndex == INVALID_CHUNK || pos + bytes > chunkSize ) && !AcquireChunk( primary ) )
	{
		return nullptr;
	}

	void* data = nullptr;
	const ALResult result = chunks[chunkIndex].buffer.MapForWriting( data, context );
	if( FAILED( result ) || data == nullptr )
	{
		CCP_ASSERT_M( false, "Failed to map Noesis dynamic buffer" );
		return nullptr;
	}

	mapped = true;
	drawPos = pos;
	pos += bytes;
	frameBytes += bytes;
	return static_cast<uint8_t*>( data ) + drawPos;
}

void Tr2NoesisGpuDevice::DynamicRing::Unmap( Tr2RenderContextAL& context )
{
	if( !mapped )
	{
		return;
	}
	CCP_ASSERT_M( chunkIndex < chunks.size(), "Noesis ring is mapped without a current chunk" );
	if( chunkIndex < chunks.size() )
	{
		chunks[chunkIndex].buffer.UnmapForWriting( context );
	}
	mapped = false;
}

Tr2BufferAL& Tr2NoesisGpuDevice::DynamicRing::CurrentChunk()
{
	if( chunkIndex >= chunks.size() )
	{
		CCP_ASSERT_M( false, "Noesis ring has no current chunk" );
		return fallback;
	}
	return chunks[chunkIndex].buffer;
}

// --------------------------------------------------------------------------------------
// Tr2NoesisGpuDevice
// --------------------------------------------------------------------------------------

Tr2NoesisGpuDevice::Tr2NoesisGpuDevice( Tr2PrimaryRenderContextAL& primaryContext,
											  const nxt_shader_source& shaders ) :
	m_primary( &primaryContext ),
	m_context( &primaryContext ),
	m_valid( true ),
	m_pushedOnscreenStencil( false ),
	m_hasHostScissor( false ),
	m_logBatchDetail( true )
{
	if( !ReadShaderSource( shaders ) )
	{
		m_valid = false;
		return;
	}

	m_caps.linear_rendering = NXT_FALSE;
	m_caps.subpixel_rendering = NXT_TRUE;
	m_caps.depth_range_zero_to_one = NXT_TRUE;
	m_caps.clip_space_y_inverted = NXT_FALSE;

	CreateVertexLayouts();
	CreateShaders();
	CreateSamplers();
	CreateRings();

	if( !m_valid )
	{
		CCP_NOESIS_LOGERR( "Noesis render device construction failed; shaders, layouts, samplers or rings are invalid" );
	}
}

Tr2NoesisGpuDevice::~Tr2NoesisGpuDevice()
{
	if( m_context )
	{
		m_vertices.Unmap( *m_context );
		m_indices.Unmap( *m_context );
	}
}

bool Tr2NoesisGpuDevice::IsValid() const
{
	return m_valid;
}

void Tr2NoesisGpuDevice::SetRenderContext( Tr2RenderContextAL& renderContext )
{
	if( m_context && m_context != &renderContext )
	{
		m_vertices.Unmap( *m_context );
		m_indices.Unmap( *m_context );
	}
	m_context = &renderContext;
}

void Tr2NoesisGpuDevice::ClearRenderContext()
{
	if( m_context != nullptr )
	{
		// A ring left mapped across a frame boundary is a bug SyncFrame already asserts on,
		// but unmapping here means the buffer is not left locked against a context nobody
		// will touch again.
		m_vertices.Unmap( *m_context );
		m_indices.Unmap( *m_context );
	}
	m_context = nullptr;
}

void Tr2NoesisGpuDevice::SetHostScissor( const Tr2ScissorRect& rect )
{
	m_hasHostScissor = true;
	m_hostScissor = rect;
}

void Tr2NoesisGpuDevice::ClearHostScissor()
{
	m_hasHostScissor = false;
}

void Tr2NoesisGpuDevice::GetCaps( nxt_device_caps& out ) const
{
	out = m_caps;
}

Tr2NoesisRenderTarget* Tr2NoesisGpuDevice::CreateRenderTarget( const char* label, uint32_t width, uint32_t height,
															uint32_t sampleCount, bool needsStencil )
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );

	if( sampleCount != 1 )
	{
		CCP_NOESIS_LOGWARN( "CreateRenderTarget '%s' requested sampleCount=%u; offscreen sample count is 1, ignoring MSAA",
							SafeLabel( label, "" ), sampleCount );
	}

	Tr2TextureAL colorAL;
	const Tr2BitmapDimensions colorDesc( width, height, 1, PIXEL_FORMAT_R8G8B8A8_UNORM );
	const ALResult colorResult = colorAL.Create( colorDesc, Tr2GpuUsage::RENDER_TARGET | Tr2GpuUsage::SHADER_RESOURCE, *m_primary );
	if( FAILED( colorResult ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis render target '%s'", SafeLabel( label, "" ) );
		CCP_ASSERT_M( false, "Failed to create Noesis render target" );
		return nullptr;
	}

	char colorName[128];
	colorAL.SetName( FormatDebugName( colorName, label, "RT" ) );

	Tr2TextureAL stencilAL;
	if( needsStencil )
	{
		const Tr2BitmapDimensions stencilDesc( width, height, 1, NoesisStencilFormat() );
		const ALResult stencilResult = stencilAL.Create( stencilDesc, Tr2GpuUsage::DEPTH_STENCIL, *m_primary );
		if( FAILED( stencilResult ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis stencil '%s'", SafeLabel( label, "" ) );
			CCP_ASSERT_M( false, "Failed to create Noesis stencil" );
			return nullptr;
		}
		char stencilName[128];
		stencilAL.SetName( FormatDebugName( stencilName, label, "RT", "_Stencil" ) );
	}

	Tr2NoesisTexture* color = new Tr2NoesisTexture( colorAL, width, height, 1, true );
	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "RenderTarget '%s' %u x %u", SafeLabel( label, "" ), width, height );
	}
	return new Tr2NoesisRenderTarget( color, stencilAL, width, height );
}

Tr2NoesisRenderTarget* Tr2NoesisGpuDevice::CloneRenderTarget( const char* label, Tr2NoesisRenderTarget* surface )
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	CCP_ASSERT_M( surface != nullptr, "CloneRenderTarget with null surface" );


	Tr2TextureAL colorAL;
	const Tr2BitmapDimensions colorDesc( surface->GetWidth(), surface->GetHeight(), 1, PIXEL_FORMAT_R8G8B8A8_UNORM );
	const ALResult colorResult = colorAL.Create( colorDesc, Tr2GpuUsage::RENDER_TARGET | Tr2GpuUsage::SHADER_RESOURCE, *m_primary );
	if( FAILED( colorResult ) )
	{
		CCP_NOESIS_LOGERR( "Failed to clone Noesis render target '%s'", SafeLabel( label, "" ) );
		CCP_ASSERT_M( false, "Failed to clone Noesis render target" );
		return nullptr;
	}

	char colorName[128];
	colorAL.SetName( FormatDebugName( colorName, label, "RT" ) );

	Tr2NoesisTexture* color = new Tr2NoesisTexture( colorAL, surface->GetWidth(), surface->GetHeight(), 1, true );
	return new Tr2NoesisRenderTarget( color, surface->GetStencil(), surface->GetWidth(), surface->GetHeight() );
}

Tr2NoesisTexture* Tr2NoesisGpuDevice::CreateTexture( const char* label, uint32_t width, uint32_t height,
												   uint32_t numLevels, nxt_texture_format format, const void** data )
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	CCP_ASSERT_M( numLevels > 0, "CreateTexture with zero mip levels" );

	const PixelFormat pixelFormat = ToPixelFormat( format );
	const Tr2BitmapDimensions desc( width, height, numLevels, pixelFormat );
	Tr2TextureAL textureAL;

	ALResult result;
	if( data != nullptr )
	{
		const uint32_t bpp = BytesPerPixel( format );
		std::vector<Tr2SubresourceData> initialData( numLevels );
		uint32_t mipWidth = width;
		uint32_t mipHeight = height;
		for( uint32_t level = 0; level < numLevels; ++level )
		{
			const uint32_t pitch = mipWidth * bpp;
			initialData[level].m_sysMem = data[level];
			initialData[level].m_sysMemPitch = pitch;
			initialData[level].m_sysMemSlicePitch = pitch * std::max( mipHeight, 1u );
			mipWidth = std::max( mipWidth / 2, 1u );
			mipHeight = std::max( mipHeight / 2, 1u );
		}
		result = textureAL.Create( desc, Tr2GpuUsage::SHADER_RESOURCE, initialData.data(), *m_primary );
	}
	else
	{
		// data == nullptr means Noesis will call UpdateTexture. DX12 UpdateSubresource is
		// implemented as MapForWriting, which requires Tr2CpuUsage::WRITE; COPY_DESTINATION
		// alone is enough for the GPU-copy gate but not for the CPU upload path.
		result = textureAL.Create( desc, Tr2GpuUsage::SHADER_RESOURCE | Tr2GpuUsage::COPY_DESTINATION,
								   Tr2CpuUsage::WRITE, *m_primary );
	}

	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis texture '%s'", SafeLabel( label, "" ) );
		CCP_ASSERT_M( false, "Failed to create Noesis texture" );
		return nullptr;
	}

	char textureName[128];
	textureAL.SetName( FormatDebugName( textureName, label, "Texture" ) );

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Texture '%s' %u x %u x %u", SafeLabel( label, "" ), width, height, numLevels );
	}
	return new Tr2NoesisTexture( textureAL, width, height, numLevels, format == NXT_TEXTURE_FORMAT_RGBA8 );
}

Tr2NoesisTexture* Tr2NoesisGpuDevice::WrapTexture( const Tr2TextureAL& texture, bool hasAlpha )
{
	CCP_ASSERT_M( texture.IsValid(), "WrapTexture with an invalid Trinity texture" );
	if( !texture.IsValid() )
	{
		return nullptr;
	}

	return new Tr2NoesisTexture( texture, texture.GetWidth(), texture.GetHeight(),
									  texture.GetMipCount(), hasAlpha );
}

void* Tr2NoesisGpuDevice::CreatePixelShader( const char* label, uint8_t shader, const void* hlsl, uint32_t size )
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	CCP_ASSERT_M( shader < m_pixelShaders.size(), "CreatePixelShader with an out-of-range shader enum" );
	CCP_ASSERT_M( hlsl != nullptr, "CreatePixelShader with null bytecode" );
	CCP_ASSERT_M( size > sizeof( uint32_t ), "CreatePixelShader blob is too small for the signature prefix" );

	if( shader >= m_pixelShaders.size() || hlsl == nullptr || size <= sizeof( uint32_t ) )
	{
		return nullptr;
	}

	// ShaderCompiler blobs start with the same root-signature flags the stock permutations
	// carry in nxt_shader_blob::resource_flags, then the backend bytecode (DXBC on D3D,
	// AIR/metallib on Metal). Skip the same 4 bytes on every AL.
	uint32_t flags = 0;
	memcpy( &flags, hlsl, sizeof( flags ) );
	const uint8_t* dxbc = static_cast<const uint8_t*>( hlsl ) + sizeof( flags );
	const uint32_t dxbcSize = size - sizeof( flags );

	const uint8_t vsIndex = m_shaderInfo[shader].vertexShader;
	CCP_ASSERT_M( vsIndex < m_vertexShaders.size(), "CreatePixelShader vertex shader index is out of range" );
	if( vsIndex >= m_vertexShaders.size() || !m_vertexShaders[vsIndex].IsValid() )
	{
		CCP_NOESIS_LOGERR( "CreatePixelShader '%s': stock vertex shader %u is missing",
						   SafeLabel( label, "" ), vsIndex );
		CCP_ASSERT_M( false, "CreatePixelShader is missing the stock vertex shader" );
		return nullptr;
	}

	Tr2ShaderSignatureAL signature;
	FillPixelSignature( signature, flags );

	CustomProgram custom;
	custom.flags = flags;
	custom.vertexFormat = m_shaderInfo[shader].vertexFormat;

	const char* name = SafeLabel( label, "Custom" );
	const ALResult result = custom.pixelShader.Create(
		PIXEL_SHADER,
		Tr2ShaderBytecodeAL( dxbc, dxbcSize ),
		signature,
		name,
		*m_primary );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis custom pixel shader '%s'", name );
		CCP_ASSERT_M( false, "Failed to create Noesis custom pixel shader" );
		return nullptr;
	}
	custom.pixelShader.SetName( name );

	Tr2ShaderAL stages[] = { m_vertexShaders[vsIndex], custom.pixelShader };
	const ALResult programResult = custom.program.Create( stages, 2, *m_primary );
	if( FAILED( programResult ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis custom shader program '%s'", name );
		CCP_ASSERT_M( false, "Failed to create Noesis custom shader program" );
		return nullptr;
	}
	custom.program.SetName( name );

	m_customShaders.push_back( std::move( custom ) );
	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Custom pixel shader '%s' shader=%u flags=0x%x handle=%zu",
						name, shader, flags, m_customShaders.size() );
	}
	return reinterpret_cast<void*>( m_customShaders.size() );
}

void Tr2NoesisGpuDevice::UpdateTexture( Tr2NoesisTexture* texture, uint32_t level, uint32_t x, uint32_t y,
										   uint32_t width, uint32_t height, const void* data )
{
	CCP_ASSERT_M( m_context != nullptr, "UpdateTexture without a render context" );
	CCP_ASSERT_M( texture != nullptr, "UpdateTexture with null texture" );
	CCP_ASSERT_M( data != nullptr, "UpdateTexture with null data" );

	const uint32_t bpp = GetBytesPerPixel( texture->GetAL().GetFormat() );

	Tr2TextureSubresource region( level );
	region.SetRect( x, y, x + width, y + height );

	const uint32_t pitch = width * bpp;
	const ALResult result = texture->GetAL().UpdateSubresource( region, data, pitch, pitch * height, *m_context );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "Failed to update Noesis texture %ux%u at (%u,%u) mip %u fmt %u: %08x",
						   width, height, x, y, level, texture->GetAL().GetFormat(), result.GetResult() );
		CCP_ASSERT_M( false, "Failed to update Noesis texture" );
		return;
	}

	// UnmapForWriting (called by UpdateSubresource) transitions COPY_DEST back to
	// the texture's defaultState (PIXEL_SHADER_RESOURCE | NON_PIXEL_SHADER_RESOURCE
	// for anything created with SHADER_RESOURCE), including the dynamic glyph atlas
	// (SHADER_RESOURCE | COPY_DESTINATION). That is the copy-write -> shader-read
	// barrier, so the library has none of its own left to issue. Tr2ResourceSetAL::Create
	// does not cover it: AddTransition skips when
	// (defaultState & PIXEL_SHADER_RESOURCE) != 0.
}

void Tr2NoesisGpuDevice::BeginOffscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "BeginOffscreenRender without a render context" );
	SyncRingsToCurrentFrame();
	m_context->PushGpuMarker( "Noesis.Offscreen" );
}

void Tr2NoesisGpuDevice::EndOffscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "EndOffscreenRender without a render context" );
	m_context->PopGpuMarker();
}

void Tr2NoesisGpuDevice::BeginOnscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "BeginOnscreenRender without a render context" );
	SyncRingsToCurrentFrame();
	m_context->PushGpuMarker( "Noesis" );

	Tr2Viewport vp;
	m_context->GetViewport( vp );

	uint32_t rtWidth = 0;
	uint32_t rtHeight = 0;
	if( FAILED( m_context->GetRenderTargetSize( rtWidth, rtHeight ) ) || rtWidth == 0 || rtHeight == 0 )
	{
		return;
	}

	// ClipToBounds is a stencil mask. Transform3D enables Z-test (GREATEREQUAL,
	// reverse-Z) but never writes depth, so the far plane has to be 0. The
	// sprite 2D path unbinds DS and the scene's D32F has no stencil, so the
	// host buffer is unusable -- this private depth-stencil is not the scene's
	// pre-cleared depth and would otherwise stay undefined, rejecting
	// fragments at random and punching holes in the colour target.
	// DX12 SetDepthStencil resets viewport and scissor to the full target;
	// put the caller's rect back.
	if( EnsureOnscreenStencil( rtWidth, rtHeight ) )
	{
		m_context->PushDepthStencil();
		if( SUCCEEDED( m_context->SetDepthStencil( m_onscreenStencil ) ) )
		{
			m_pushedOnscreenStencil = true;
			m_context->Clear( CLEARFLAGS_ZBUFFER | CLEARFLAGS_STENCIL, 0, 0.0f, 0 );
		}
		else
		{
			m_context->PopDepthStencil();
		}
	}

	m_context->SetViewport( vp );
	// Viewport clamped to the target, then the sprite-tree clipChildren rect if any.
	Tr2ScissorRect scissor = ScissorForViewport( vp, rtWidth, rtHeight );
	if( m_hasHostScissor )
	{
		scissor = IntersectScissor( scissor, m_hostScissor );
	}
	m_context->SetScissorRect( scissor );
}

void Tr2NoesisGpuDevice::EndOnscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "EndOnscreenRender without a render context" );

	if( m_pushedOnscreenStencil )
	{
		m_context->PopDepthStencil();
		m_pushedOnscreenStencil = false;
	}

	// DX11 does not reset scissor when only the DS changes. Sprite 2D after
	// this job would otherwise stay clipped to the Noesis viewport.
	uint32_t rtWidth = 0;
	uint32_t rtHeight = 0;
	if( SUCCEEDED( m_context->GetRenderTargetSize( rtWidth, rtHeight ) ) )
	{
		m_context->SetScissorRect( Tr2ScissorRect( rtWidth, rtHeight ) );
	}

	m_context->PopGpuMarker();

	// Last Noesis call of the frame, so this is where a frame's worth of batches is complete.
	ReportFrameBatches();
}

void Tr2NoesisGpuDevice::SetRenderTarget( Tr2NoesisRenderTarget* surface )
{
	CCP_ASSERT_M( m_context != nullptr, "SetRenderTarget without a render context" );
	CCP_ASSERT_M( surface != nullptr, "SetRenderTarget with null surface" );

	m_context->SetRenderTarget( surface->GetColor()->GetAL() );
	if( surface->HasStencil() )
	{
		m_context->SetDepthStencil( surface->GetStencil() );
	}
	else
	{
		m_context->SetDepthStencil( Tr2TextureAL() );
	}
	m_context->SetViewport( Tr2Viewport( surface->GetWidth(), surface->GetHeight() ) );
}

void Tr2NoesisGpuDevice::BeginTile( Tr2NoesisRenderTarget* surface, const nxt_tile& tile )
{
	CCP_ASSERT_M( m_context != nullptr, "BeginTile without a render context" );
	CCP_ASSERT_M( surface != nullptr, "BeginTile with null surface" );

	Tr2ScissorRect rect;
	rect.m_left = int32_t( tile.x );
	rect.m_top = int32_t( surface->GetHeight() - ( tile.y + tile.height ) );
	rect.m_right = int32_t( tile.x + tile.width );
	rect.m_bottom = int32_t( surface->GetHeight() - tile.y );
	m_context->SetScissorRect( rect );
}

void Tr2NoesisGpuDevice::EndTile( Tr2NoesisRenderTarget* /*surface*/ )
{
	// Empty, matching D3D12RenderDevice. The next SetRenderTarget - or the
	// step's PopRenderTarget after the offscreen phase - resets scissor to
	// the full target.
}

void Tr2NoesisGpuDevice::ResolveRenderTarget( Tr2NoesisRenderTarget* /*surface*/, const nxt_tile* /*tiles*/, uint32_t /*numTiles*/ )
{
	// Sample count is 1, so there is no MSAA resolve. Color targets are created
	// RENDER_TARGET | SHADER_RESOURCE, so defaultState is PIXEL_SHADER_RESOURCE |
	// NON_PIXEL_SHADER_RESOURCE. SetRenderTarget transitions the previously bound
	// color from RENDER_TARGET back to that default when it is unbound; the render
	// step's push/pop around the offscreen phase is what unbinds the last one
	// before it is sampled. SetResourceSet does not add an SRV barrier here:
	// AddTransition skips when defaultState already includes PIXEL_SHADER_RESOURCE.
}

void* Tr2NoesisGpuDevice::MapVertices( uint32_t bytes )
{
	CCP_ASSERT_M( m_context != nullptr, "MapVertices without a render context" );
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	m_vertices.SyncFrame( *m_primary );
	return m_vertices.Map( bytes, *m_context, *m_primary );
}

void Tr2NoesisGpuDevice::UnmapVertices()
{
	CCP_ASSERT_M( m_context != nullptr, "UnmapVertices without a render context" );
	m_vertices.Unmap( *m_context );
}

void* Tr2NoesisGpuDevice::MapIndices( uint32_t bytes )
{
	CCP_ASSERT_M( m_context != nullptr, "MapIndices without a render context" );
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	m_indices.SyncFrame( *m_primary );
	return m_indices.Map( bytes, *m_context, *m_primary );
}

void Tr2NoesisGpuDevice::UnmapIndices()
{
	CCP_ASSERT_M( m_context != nullptr, "UnmapIndices without a render context" );
	m_indices.Unmap( *m_context );
}

void Tr2NoesisGpuDevice::DrawBatch( const nxt_batch& batch )
{
	CCP_ASSERT_M( m_context != nullptr, "DrawBatch without a render context" );
	CCP_ASSERT_M( !batch.single_pass_stereo, "Noesis sent a stereo batch; the stereo permutations are not compiled" );

	const uint8_t shader = batch.shader;
	if( shader >= m_pixelShaders.size() )
	{
		CCP_ASSERT_M( false, "Noesis DrawBatch: shader index is out of range" );
		return;
	}

	++m_batchCounts[shader];

	ResolvedProgram resolved;
	if( !ResolveProgram( batch, resolved ) )
	{
		return;
	}

	// The library filters the render-state combinations it sends, so anything asserted
	// here would repeat that check with less information than it had.

	ApplyRenderState( batch );

	m_context->SetShaderProgram( *resolved.program );
	m_context->SetVertexLayout( m_vertexLayouts[resolved.vertexFormat] );
	m_context->SetTopology( TOP_TRIANGLES );

	// vertexOffset is a byte offset from the current Map, matching the SDK's own D3D12 device.
	// Indices are bound at the chunk base and addressed through startIndex instead.
	m_context->SetStreamSource( 0, m_vertices.CurrentChunk(),
								m_vertices.drawPos + batch.vertex_offset, m_vertexStrides[resolved.vertexFormat] );
	m_context->SetIndices( m_indices.CurrentChunk(), m_indices.stride );

	BindUniforms( batch, resolved.flags );
	BindResources( batch, resolved.flags, *resolved.program, resolved.programId );

	CCP_ASSERT_M( ( batch.num_indices % 3 ) == 0, "Noesis batch index count is not a whole number of triangles" );

	const uint32_t startIndex = m_indices.drawPos / m_indices.stride + batch.start_index;
	const ALResult result = m_context->DrawIndexedPrimitive( batch.num_vertices, startIndex, batch.num_indices / 3, 0 );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "DrawIndexedPrimitive failed for shader '%s'", m_shaderInfo[shader].name );
		CCP_ASSERT_M( false, "Noesis DrawIndexedPrimitive failed" );
		return;
	}

	if( m_logBatchDetail && Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Batch '%s' state=0x%02x stencilRef=%u vertices=%u indices=%u vertexOffset=%u startIndex=%u",
						m_shaderInfo[shader].name, batch.render_state, batch.stencil_ref,
						batch.num_vertices, batch.num_indices, batch.vertex_offset, startIndex );
	}
}

bool Tr2NoesisGpuDevice::ResolveProgram( const nxt_batch& batch, ResolvedProgram& out )
{
	const uint8_t shader = batch.shader;

	if( batch.pixel_shader == nullptr )
	{
		out.flags = m_shaderInfo[shader].resourceFlags;
		if( out.flags == 0 || !m_programs[shader].IsValid() )
		{
			ReportUnwiredShader( shader );
			return false;
		}

		out.program = &m_programs[shader];
		out.vertexFormat = m_shaderInfo[shader].vertexFormat;
		out.programId = shader;
		return true;
	}

	const uintptr_t index = reinterpret_cast<uintptr_t>( batch.pixel_shader );
	if( index == 0 || index > m_customShaders.size() )
	{
		CCP_ASSERT_M( false, "Noesis DrawBatch: custom pixel shader handle is invalid" );
		return false;
	}

	CustomProgram& custom = m_customShaders[index - 1];
	if( !custom.program.IsValid() )
	{
		ReportUnwiredShader( shader );
		return false;
	}

	// Same skip D3D12RenderDevice uses: the custom permutation declared registers this
	// batch did not bind.
	if( ( custom.flags & GetBatchSignature( batch ) ) != custom.flags )
	{
		return false;
	}

	out.program = &custom.program;
	out.flags = custom.flags;
	out.vertexFormat = custom.vertexFormat;
	out.programId = CUSTOM_PROGRAM_ID_TAG | ( index - 1 );
	return true;
}

void Tr2NoesisGpuDevice::ReportUnwiredShader( uint8_t shader )
{
	if( shader >= m_unwiredReported.size() || m_unwiredReported[shader] )
	{
		return;
	}
	m_unwiredReported[shader] = true;

	// Once per shader: a batch-rate assert is unusable. The per-frame histogram is what shows
	// that an unwired shader is still being asked for.
	CCP_NOESIS_LOGERR( "DrawBatch: shader '%s' (%u) has no program. Custom_Effect and BrushShader "
					   "permutations need CreatePixelShader plus SetPixelShader on the effect.",
					   m_shaderInfo[shader].name, shader );
	CCP_ASSERT_M( false, "Noesis DrawBatch: shader permutation was never compiled" );
}

void Tr2NoesisGpuDevice::ReportFrameBatches()
{
	if( Tr2Noesis::IsLogVerbose() && m_batchCounts != m_reportedCounts )
	{
		uint32_t total = 0;
		for( uint32_t shader = 0; shader < m_pixelShaders.size(); ++shader )
		{
			total += m_batchCounts[shader];
		}

		std::string histogram;
		for( uint32_t shader = 0; shader < m_pixelShaders.size(); ++shader )
		{
			if( m_batchCounts[shader] == 0 )
			{
				continue;
			}
			if( !histogram.empty() )
			{
				histogram += ", ";
			}
			histogram += m_shaderInfo[shader].name;
			histogram += "=";
			histogram += std::to_string( m_batchCounts[shader] );
		}

		CCP_NOESIS_LOG( "Batches this frame: %u (%s)", total, total == 0 ? "none" : histogram.c_str() );
		m_reportedCounts = m_batchCounts;
	}

	std::fill( m_batchCounts.begin(), m_batchCounts.end(), 0u );
	m_logBatchDetail = false;
}

void Tr2NoesisGpuDevice::BindUniform( Tr2ConstantBufferAL& buffer, const nxt_uniform_data& uniforms,
										 Tr2RenderContextEnum::ShaderType stage, uint32_t registerIndex, const char* name )
{
	if( uniforms.values == nullptr || uniforms.num_dwords == 0 )
	{
		// The shader's resource flags say it declares this register, so an empty block means our
		// transcription of the SDK's root-signature flags is wrong.
		CCP_ASSERT_M( false, "Noesis batch omitted a constant buffer that its shader declares" );
		return;
	}

	const uint32_t bytes = uniforms.num_dwords * sizeof( uint32_t );
	if( buffer.GetSize() < bytes )
	{
		// Sized from what Noesis actually sends, so a cbuffer layout change costs a
		// reallocation instead of overrunning a buffer.
		const uint32_t size = ( bytes + 255 ) & ~255u;
		Tr2ConstantBufferAL grown;
		if( FAILED( grown.Create( size, *m_primary ) ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis constant buffer '%s' of %u bytes", name, size );
			CCP_ASSERT_M( false, "Failed to create Noesis constant buffer" );
			return;
		}
		grown.SetName( name );

		// The AL uploads the whole buffer, not just the part we write, so clear the tail once.
		void* zero = nullptr;
		if( SUCCEEDED( grown.Lock( &zero, *m_context ) ) && zero != nullptr )
		{
			memset( zero, 0, size );
			grown.Unlock( *m_context );
		}

		buffer = grown;
	}

	void* mapped = nullptr;
	if( FAILED( buffer.Lock( &mapped, *m_context ) ) || mapped == nullptr )
	{
		CCP_ASSERT_M( false, "Failed to lock a Noesis constant buffer" );
		return;
	}
	memcpy( mapped, uniforms.values, bytes );
	if( buffer.GetSize() > bytes )
	{
		// DX11 Map WRITE_DISCARD leaves the rest of the buffer undefined, and SetConstants
		// binds the whole allocation. DX12 uploads the whole buffer too; keep the tail zero.
		memset( static_cast<uint8_t*>( mapped ) + bytes, 0, buffer.GetSize() - bytes );
	}
	buffer.Unlock( *m_context );

	// Unlock invalidates the residency token on DX12, so this uploads into the frame's ring
	// rather than reusing the address the previous batch was given. DX11 DISCARD-maps.
	const ALResult result = m_context->SetConstants( buffer, stage, registerIndex );
	if( FAILED( result ) )
	{
		CCP_ASSERT_M( false, "Failed to bind a Noesis constant buffer" );
	}
}

void Tr2NoesisGpuDevice::BindUniforms( const nxt_batch& batch, uint32_t flags )
{
	if( flags & NXT_SHADER_USES_VS_CB0 )
	{
		BindUniform( m_vertexUniforms[0], batch.vertex_uniforms[0], VERTEX_SHADER, 0, "Noesis_VertexUniforms0" );
	}
	if( flags & NXT_SHADER_USES_VS_CB1 )
	{
		BindUniform( m_vertexUniforms[1], batch.vertex_uniforms[1], VERTEX_SHADER, 1, "Noesis_VertexUniforms1" );
	}
	if( flags & NXT_SHADER_USES_PS_CB0 )
	{
		BindUniform( m_pixelUniforms[0], batch.pixel_uniforms[0], PIXEL_SHADER, 0, "Noesis_PixelUniforms0" );
	}
	if( flags & NXT_SHADER_USES_PS_CB1 )
	{
		BindUniform( m_pixelUniforms[1], batch.pixel_uniforms[1], PIXEL_SHADER, 1, "Noesis_PixelUniforms1" );
	}
}

void Tr2NoesisGpuDevice::BindResources( const nxt_batch& batch, uint32_t flags, Tr2ShaderProgramAL& program, uint64_t programId )
{
	if( ( flags & ( NXT_SHADER_USES_PS_T0 | NXT_SHADER_USES_PS_T1 | NXT_SHADER_USES_PS_T2 | NXT_SHADER_USES_PS_T3 | NXT_SHADER_USES_PS_T4 ) ) == 0 )
	{
		// Solid fills bind nothing, so they never pay for a resource set.
		return;
	}

	Tr2ResourceSetDescriptionAL description( program );
	for( const TextureSlot& slot : TEXTURE_SLOTS )
	{
		if( ( flags & slot.flag ) == 0 )
		{
			continue;
		}
		if( batch.*slot.texture == nullptr )
		{
			CCP_ASSERT_M( false, "Noesis batch omitted a texture that its shader declares" );
			continue;
		}

		Tr2NoesisTexture* texture = reinterpret_cast<Tr2NoesisTexture*>( batch.*slot.texture );
		// linearRendering is false, so textures are sampled raw rather than sRGB-converted.
		// A rejection means the register is absent from the program's map, which would mean
		// the resource flags and the signature we built from them disagree.
		const bool srvSet = description.SetSrv( PIXEL_SHADER, slot.registerIndex, texture->GetAL() );
		CCP_ASSERT_M( srvSet, "Noesis shader program has no SRV at the register the resource flags claim" );

		// Masked, not asserted: see SAMPLER_INDEX_MASK.
		const nxt_sampler_state raw = batch.*slot.sampler;
		CCP_ASSERT_M( ( raw & ~SAMPLER_INDEX_MASK ) == 0,
					  "Noesis sampler state used a bit nxt.h reserves" );
		const bool samplerSet = description.SetSampler( PIXEL_SHADER, slot.registerIndex,
														m_samplers[raw & SAMPLER_INDEX_MASK] );
		CCP_ASSERT_M( samplerSet, "Noesis shader program has no sampler at the register the resource flags claim" );
	}

	const uint64_t key = ( programId << 32 ) | description.ComputeHash();
	ResourceSetEntry& entry = m_resourceSets[key];
	if( !entry.set.IsValid() || !( entry.description == description ) )
	{
		Tr2ResourceSetAL set;
		const ALResult result = set.Create( description, program, *m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create a Noesis resource set for shader '%s'", m_shaderInfo[batch.shader].name );
			CCP_ASSERT_M( false, "Failed to create a Noesis resource set" );
			return;
		}
		set.SetName( m_shaderInfo[batch.shader].name );
		entry.description = description;
		entry.set = set;
	}

	const ALResult result = m_context->SetResourceSet( entry.set );
	if( FAILED( result ) )
	{
		CCP_ASSERT_M( false, "Failed to bind a Noesis resource set" );
	}
}

void Tr2NoesisGpuDevice::CreateVertexLayouts()
{
	for( uint32_t format = 0; format < m_vertexLayouts.size(); ++format )
	{
		Tr2VertexDefinition definition;
		if( !AddVertexAttributes( definition, nullptr, m_vertexFormats[format] ) )
		{
			m_valid = false;
			continue;
		}
		const ALResult result = m_vertexLayouts[format].Create( definition, *m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis vertex layout %u", format );
			CCP_ASSERT_M( false, "Failed to create Noesis vertex layout" );
			m_valid = false;
		}
	}
}

bool Tr2NoesisGpuDevice::ReadShaderSource( const nxt_shader_source& shaders )
{
	// Everything the host needs to build pipelines comes from here. Read once and cached,
	// because a blob is stable for the life of the process and asking per batch would put
	// a C call on the hot path for an answer that never changes.
	const uint32_t formatCount = shaders.get_vertex_format_count( shaders.header.self );
	if( formatCount == 0 )
	{
		CCP_NOESIS_LOGERR( "The Noesis library reported no vertex formats" );
		return false;
	}

	m_vertexFormats.resize( formatCount );
	for( uint32_t format = 0; format < formatCount; ++format )
	{
		const uint32_t needed = shaders.get_vertex_format( shaders.header.self, format, nullptr, 0 );
		if( needed == 0 )
		{
			CCP_NOESIS_LOGERR( "Vertex format %u has no attributes", format );
			return false;
		}

		m_vertexFormats[format].resize( needed );
		if( shaders.get_vertex_format( shaders.header.self, format,
									   m_vertexFormats[format].data(), needed ) != needed )
		{
			CCP_NOESIS_LOGERR( "Vertex format %u would not describe itself", format );
			return false;
		}
	}

	const uint32_t blobCount = shaders.get_count( shaders.header.self );
	for( uint32_t i = 0; i < blobCount; ++i )
	{
		nxt_shader_blob blob = {};
		if( shaders.get_blob( shaders.header.self, i, &blob ) != NXT_OK )
		{
			CCP_NOESIS_LOGERR( "Shader blob %u could not be read", i );
			return false;
		}

		// Checked here rather than per batch: every later use indexes a vector with this,
		// so a blob naming a format we were not given would read off the end long after
		// the bad value arrived.
		if( blob.vertex_format >= formatCount )
		{
			CCP_NOESIS_LOGERR( "Shader blob %u ('%s') names vertex format %u, but the library "
							   "described only %u",
							   i, blob.name != nullptr ? blob.name : "?",
							   static_cast<uint32_t>( blob.vertex_format ), formatCount );
			return false;
		}

		ShaderInfo info;
		info.vertexShader = blob.vertex_shader;
		info.vertexFormat = blob.vertex_format;
		info.resourceFlags = blob.resource_flags;
		info.bytecode = blob.bytecode;
		info.bytecodeSize = blob.size;
		info.name = blob.name;

		const bool isVertex = blob.stage == NXT_SHADER_STAGE_VERTEX;
		std::vector<ShaderInfo>& table = isVertex ? m_vertexInfo : m_shaderInfo;
		if( table.size() <= blob.id )
		{
			table.resize( blob.id + 1 );
		}
		table[blob.id] = info;
	}

	// One AL shader slot per described blob, ids being contiguous per stage.
	m_vertexShaders.resize( m_vertexInfo.size() );
	m_pixelShaders.resize( m_shaderInfo.size() );

	if( m_vertexShaders.empty() || m_pixelShaders.empty() )
	{
		CCP_NOESIS_LOGERR( "The Noesis library reported %u vertex and %u pixel shaders",
						   static_cast<uint32_t>( m_vertexShaders.size() ),
						   static_cast<uint32_t>( m_pixelShaders.size() ) );
		return false;
	}

	// Pixel blobs pair with a stock vertex shader by id, and CreateShaders indexes with
	// that id; reject a pairing we cannot satisfy before it gets there.
	for( uint32_t shader = 0; shader < m_shaderInfo.size(); ++shader )
	{
		if( m_shaderInfo[shader].bytecode != nullptr &&
			m_shaderInfo[shader].vertexShader >= m_vertexShaders.size() )
		{
			CCP_NOESIS_LOGERR( "Pixel shader '%s' names vertex shader %u, but the library "
							   "supplied only %u",
							   m_shaderInfo[shader].name,
							   static_cast<uint32_t>( m_shaderInfo[shader].vertexShader ),
							   static_cast<uint32_t>( m_vertexShaders.size() ) );
			return false;
		}
	}

	// The stride of a format is the sum of its attributes, so it is derived rather than
	// asked for: another entry point would be one more thing that could disagree.
	m_vertexStrides.assign( formatCount, 0 );
	for( uint32_t format = 0; format < formatCount; ++format )
	{
		uint32_t stride = 0;
		for( const nxt_vertex_attribute& attr : m_vertexFormats[format] )
		{
			stride += VertexAttrSize( attr.type );
		}
		m_vertexStrides[format] = stride;
	}

	m_programs.resize( m_pixelShaders.size() );
	m_vertexLayouts.resize( formatCount );
	m_batchCounts.assign( m_pixelShaders.size(), 0 );
	m_reportedCounts.assign( m_pixelShaders.size(), 0 );
	m_unwiredReported.assign( m_pixelShaders.size(), false );

	CCP_NOESIS_LOGNOTICE( "Noesis shaders: %u vertex, %u pixel, %u vertex formats",
						  static_cast<uint32_t>( m_vertexShaders.size() ),
						  static_cast<uint32_t>( m_pixelShaders.size() ), formatCount );
	return true;
}

void Tr2NoesisGpuDevice::CreateShaders()
{
	for( uint32_t vs = 0; vs < m_vertexShaders.size(); ++vs )
	{
		Tr2ShaderSignatureAL signature;
		Tr2VertexDefinition unused;
		if( !AddVertexAttributes( unused, &signature, m_vertexFormats[m_vertexInfo[vs].vertexFormat] ) )
		{
			m_valid = false;
			continue;
		}
		if( ( m_vertexInfo[vs].resourceFlags & NXT_SHADER_USES_VS_CB0 ) != 0 )
		{
			signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0 );
		}
		// Which constant buffers a vertex shader binds arrives on the blob, so which
		// permutations are SDF is not a fact this side has to know.
		if( ( m_vertexInfo[vs].resourceFlags & NXT_SHADER_USES_VS_CB1 ) != 0 )
		{
			signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 1 );
		}

		const ShaderInfo& info = m_vertexInfo[vs];
		const ALResult result = m_vertexShaders[vs].Create(
			VERTEX_SHADER,
			Tr2ShaderBytecodeAL( info.bytecode, info.bytecodeSize ),
			signature,
			info.name,
			*m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis vertex shader '%s'", info.name );
			CCP_ASSERT_M( false, "Failed to create Noesis vertex shader" );
			m_valid = false;
		}
		else
		{
			m_vertexShaders[vs].SetName( info.name );
		}
	}

	for( uint32_t shader = 0; shader < m_pixelShaders.size(); ++shader )
	{
		const ShaderInfo& info = m_shaderInfo[shader];
		if( info.bytecode == nullptr )
		{
			// A slot the library did not supply: the custom-effect shader comes from the
			// effect at draw time, not from here.
			continue;
		}

		// The flags arrive on the blob, so this signature matches the bytecode by
		// construction.
		Tr2ShaderSignatureAL signature;
		FillPixelSignature( signature, info.resourceFlags );

		const ALResult result = m_pixelShaders[shader].Create(
			PIXEL_SHADER,
			Tr2ShaderBytecodeAL( info.bytecode, info.bytecodeSize ),
			signature,
			info.name,
			*m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis pixel shader '%s'", info.name );
			CCP_ASSERT_M( false, "Failed to create Noesis pixel shader" );
			m_valid = false;
			continue;
		}
		m_pixelShaders[shader].SetName( info.name );

		const uint8_t vsIndex = m_shaderInfo[shader].vertexShader;
		Tr2ShaderAL stages[] = { m_vertexShaders[vsIndex], m_pixelShaders[shader] };
		const ALResult programResult = m_programs[shader].Create( stages, 2, *m_primary );
		if( FAILED( programResult ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis shader program '%s'", info.name );
			CCP_ASSERT_M( false, "Failed to create Noesis shader program" );
			m_valid = false;
		}
		else
		{
			m_programs[shader].SetName( info.name );
		}
	}
}

void Tr2NoesisGpuDevice::CreateSamplers()
{
	// nxt_sampler_state packs wrapMode:3, minmagFilter:1, mipFilter:2; unused:2 stays 0
	// or the index lands past the 64 slots those six bits address.
	static_assert( sizeof( m_samplers ) / sizeof( m_samplers[0] ) == ( 1u << 6 ),
				   "m_samplers must cover every 6-bit sampler value" );

	for( uint8_t wrap = 0; wrap <= NXT_WRAP_MIRROR; ++wrap )
	{
		for( uint8_t minmag = 0; minmag <= NXT_MINMAG_LINEAR; ++minmag )
		{
			for( uint8_t mip = 0; mip <= NXT_MIP_LINEAR; ++mip )
			{
				// Packed the way nxt.h documents, then unpacked with its own helpers, so
				// the index a batch arrives with and the slot built here cannot disagree
				// about where the bits are.
				const nxt_sampler_state state = static_cast<nxt_sampler_state>(
					( wrap & 0x7 ) | ( ( minmag & 0x1 ) << 3 ) | ( ( mip & 0x3 ) << 4 ) );

				Tr2SamplerDescription desc;
				desc.m_minFilter = ToMinMagFilter( nxt_sampler_minmag_filter( state ) );
				desc.m_magFilter = ToMinMagFilter( nxt_sampler_minmag_filter( state ) );
				desc.m_mipFilter = ToMipFilter( nxt_sampler_mip_filter( state ) );
				ToAddressMode( nxt_sampler_wrap_mode( state ), desc );
				desc.m_addressW = TA_CLAMP;
				desc.m_mipLODBias = -0.75f;
				desc.m_maxAnisotropy = 1;
				desc.m_comparisonFunc = CMP_NEVER;
				desc.m_minLOD = 0.0f;
				desc.m_maxLOD = std::numeric_limits<float>::max();
				desc.m_borderColor[0] = 0.0f;
				desc.m_borderColor[1] = 0.0f;
				desc.m_borderColor[2] = 0.0f;
				desc.m_borderColor[3] = 0.0f;

				const ALResult result = m_samplers[state].Create( desc, *m_primary );
				if( FAILED( result ) )
				{
					CCP_NOESIS_LOGERR( "Failed to create Noesis sampler %u", state );
					CCP_ASSERT_M( false, "Failed to create Noesis sampler" );
					m_valid = false;
				}
			}
		}
	}
}

void Tr2NoesisGpuDevice::CreateRings()
{
	// Chunks are sized at the SDK's per-Map cap, the smallest size that guarantees any
	// single legal Map fits in a fresh chunk. A frame's total comes from the chunk count.
	if( !m_vertices.Create( 1, NOESIS_DYNAMIC_VB_SIZE, Tr2GpuUsage::VERTEX_BUFFER, "Vertices", *m_primary ) )
	{
		m_valid = false;
	}
	// Noesis writes 16-bit indices, and the AL reads the index format off the buffer's stride.
	if( !m_indices.Create( 2, NOESIS_DYNAMIC_IB_SIZE, Tr2GpuUsage::INDEX_BUFFER, "Indices", *m_primary ) )
	{
		m_valid = false;
	}
}

void Tr2NoesisGpuDevice::SyncRingsToCurrentFrame()
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	m_vertices.SyncFrame( *m_primary );
	m_indices.SyncFrame( *m_primary );
}

bool Tr2NoesisGpuDevice::EnsureOnscreenStencil( uint32_t width, uint32_t height )
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );

	if( m_onscreenStencil.IsValid() &&
		m_onscreenStencil.GetWidth() == width &&
		m_onscreenStencil.GetHeight() == height )
	{
		return true;
	}

	m_onscreenStencil = Tr2TextureAL();
	const Tr2BitmapDimensions desc( width, height, 1, NoesisStencilFormat() );
	const ALResult result = m_onscreenStencil.Create( desc, Tr2GpuUsage::DEPTH_STENCIL, *m_primary );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis onscreen stencil %u x %u", width, height );
		m_onscreenStencil = Tr2TextureAL();
		return false;
	}

	m_onscreenStencil.SetName( "Noesis_OnscreenStencil" );
	return true;
}

void Tr2NoesisGpuDevice::ApplyRenderState( const nxt_batch& batch )
{
	// Emitted in full for every batch, never as a delta: any state we leave out can
	// survive from whichever render step ran before us.
	CCP_ASSERT_M( m_context != nullptr, "ApplyRenderState without a render context" );

	// Unpacked with nxt.h's own helpers rather than by re-deriving the shifts from the
	// comment on nxt_render_state.
	const nxt_render_state state = batch.render_state;
	const nxt_blend_mode blendMode = nxt_render_state_blend_mode( state );
	const bool colorEnable = nxt_render_state_color_enable( state ) != NXT_FALSE;
	const bool wireframe = nxt_render_state_wireframe( state ) != NXT_FALSE;
	// Two entries per state, and the count below is the ceiling for any one batch.
	uint32_t pairs[2 * 32];
	const uint32_t capacity = static_cast<uint32_t>( std::size( pairs ) );
	uint32_t count = 0;

	auto add = [&]( Tr2RenderContextEnum::RenderState rs, uint32_t value ) {
		if( count + 2 > capacity )
		{
			CCP_ASSERT_M( false, "Noesis render-state pair overflow" );
			return;
		}
		pairs[count++] = rs;
		pairs[count++] = value;
	};

	add( RS_CULLMODE, CULLMODE_NONE );
	add( RS_FILLMODE, wireframe ? FM_WIREFRAME : FM_SOLID );
	add( RS_DEPTHBIAS, 0 );
	add( RS_SLOPESCALEDEPTHBIAS, 0 );
	add( RS_DEPTH_CLIP_ENABLE, 1 );
	add( RS_ZWRITEENABLE, 0 );
	add( RS_COLORWRITEENABLE, colorEnable ? ( COLORWRITEENABLE_RED | COLORWRITEENABLE_GREEN | COLORWRITEENABLE_BLUE | COLORWRITEENABLE_ALPHA ) : 0 );
	add( RS_SRGBWRITEENABLE, 0 );
	add( RS_ALPHATESTENABLE, 0 );

	if( colorEnable && blendMode != NXT_BLEND_SRC )
	{
		add( RS_ALPHABLENDENABLE, 1 );
		add( RS_SEPARATEALPHABLENDENABLE, 1 );
		add( RS_BLENDOP, BO_ADD );
		add( RS_BLENDOPALPHA, BO_ADD );
		add( RS_SRCBLENDALPHA, BM_ONE );
		add( RS_DESTBLENDALPHA, BM_INVSRCALPHA );

		switch( blendMode )
		{
		case NXT_BLEND_SRC_OVER:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRCALPHA );
			break;
		case NXT_BLEND_SRC_OVER_MULTIPLY:
			add( RS_SRCBLEND, BM_DESTCOLOR );
			add( RS_DESTBLEND, BM_INVSRCALPHA );
			break;
		case NXT_BLEND_SRC_OVER_SCREEN:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRCCOLOR );
			break;
		case NXT_BLEND_SRC_OVER_ADDITIVE:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_ONE );
			break;
		case NXT_BLEND_SRC_OVER_DUAL:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRC1COLOR );
			add( RS_DESTBLENDALPHA, BM_INVSRC1ALPHA );
			break;
		default:
			CCP_ASSERT_M( false, "Unknown Noesis blend mode" );
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRCALPHA );
			break;
		}
	}
	else
	{
		add( RS_ALPHABLENDENABLE, 0 );
		add( RS_SEPARATEALPHABLENDENABLE, 0 );
	}

	const nxt_stencil_mode stencilMode = nxt_render_state_stencil_mode( state );
	const bool zTest = stencilMode == NXT_STENCIL_DISABLED_ZTEST ||
					   stencilMode == NXT_STENCIL_EQUAL_KEEP_ZTEST;
	add( RS_ZENABLE, zTest ? 1 : 0 );
	add( RS_ZFUNC, CMP_GREATEREQUAL );

	bool stencilEnable = false;
	uint32_t stencilFunc = CMP_EQUAL;
	uint32_t stencilPass = STENCILOP_KEEP;
	switch( stencilMode )
	{
	case NXT_STENCIL_DISABLED:
	case NXT_STENCIL_DISABLED_ZTEST:
		break;
	case NXT_STENCIL_EQUAL_KEEP:
	case NXT_STENCIL_EQUAL_KEEP_ZTEST:
		stencilEnable = true;
		break;
	case NXT_STENCIL_EQUAL_INCR:
		stencilEnable = true;
		stencilPass = STENCILOP_INCR;
		break;
	case NXT_STENCIL_EQUAL_DECR:
		stencilEnable = true;
		stencilPass = STENCILOP_DECR;
		break;
	case NXT_STENCIL_CLEAR:
		stencilEnable = true;
		stencilFunc = CMP_ALWAYS;
		stencilPass = STENCILOP_ZERO;
		break;
	default:
		CCP_ASSERT_M( false, "Unknown Noesis stencil mode" );
		break;
	}

	add( RS_STENCILENABLE, stencilEnable ? 1 : 0 );
	add( RS_STENCILMASK, 0xff );
	add( RS_STENCILFUNC, stencilFunc );
	add( RS_STENCILPASS, stencilPass );
	add( RS_STENCILFAIL, STENCILOP_KEEP );
	add( RS_STENCILZFAIL, STENCILOP_KEEP );
	add( RS_STENCILREF, batch.stencil_ref );
	// The AL applies both faces from these CW states, so front and back are identical
	// by construction, matching the SDK. Do not issue RS_TWOSIDEDSTENCILMODE or the
	// RS_CCW_* states: they are unimplemented on DX12, and leaving them unset is also
	// the right choice if DX11 starts honouring two-sided stencil.

	// SetRenderStates counts pairs, not array entries.
	const ALResult result = m_context->SetRenderStates( pairs, count / 2 );
	if( FAILED( result ) )
	{
		CCP_ASSERT_M( false, "Noesis render state not implemented by the AL" );
	}
}
