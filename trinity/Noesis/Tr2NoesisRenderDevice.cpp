// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisRenderDevice.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisShaders.h"
#include "Noesis/Tr2NoesisSystem.h"
#include "Tr2RenderContext.h"

#include <NsCore/Ptr.h>

using namespace Noesis;
using namespace Tr2RenderContextEnum;

namespace
{

// Same bit layout as Noesis's D3D12RenderDevice root-signature flags.
const uint32_t VS_CB0 = 1 << 0;
const uint32_t VS_CB1 = 1 << 1;
const uint32_t PS_CB0 = 1 << 2;
const uint32_t PS_CB1 = 1 << 3;
const uint32_t PS_T0 = 1 << 4;
const uint32_t PS_T1 = 1 << 5;
const uint32_t PS_T2 = 1 << 6;
const uint32_t PS_T3 = 1 << 7;
const uint32_t PS_T4 = 1 << 8;

// Indexed by Shader::Enum. LCD and Custom_Effect stay zero: we did not build those permutations.
const uint32_t PROGRAM_FLAGS[Shader::Count] = {
	VS_CB0 | PS_CB0, // RGBA
	VS_CB0, // Mask
	VS_CB0, // Clear
	VS_CB0, // Path_Solid
	VS_CB0 | PS_CB0 | PS_T1, // Path_Linear
	VS_CB0 | PS_CB0 | PS_T1, // Path_Radial
	VS_CB0 | PS_CB0 | PS_T0, // Path_Pattern
	VS_CB0 | PS_CB0 | PS_T0, // Path_Pattern_Clamp
	VS_CB0 | PS_CB0 | PS_T0, // Path_Pattern_Repeat
	VS_CB0 | PS_CB0 | PS_T0, // Path_Pattern_MirrorU
	VS_CB0 | PS_CB0 | PS_T0, // Path_Pattern_MirrorV
	VS_CB0 | PS_CB0 | PS_T0, // Path_Pattern_Mirror
	VS_CB0, // Path_AA_Solid
	VS_CB0 | PS_CB0 | PS_T1, // Path_AA_Linear
	VS_CB0 | PS_CB0 | PS_T1, // Path_AA_Radial
	VS_CB0 | PS_CB0 | PS_T0, // Path_AA_Pattern
	VS_CB0 | PS_CB0 | PS_T0, // Path_AA_Pattern_Clamp
	VS_CB0 | PS_CB0 | PS_T0, // Path_AA_Pattern_Repeat
	VS_CB0 | PS_CB0 | PS_T0, // Path_AA_Pattern_MirrorU
	VS_CB0 | PS_CB0 | PS_T0, // Path_AA_Pattern_MirrorV
	VS_CB0 | PS_CB0 | PS_T0, // Path_AA_Pattern_Mirror
	VS_CB0 | VS_CB1 | PS_T3, // SDF_Solid
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T1 | PS_T3, // SDF_Linear
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T1 | PS_T3, // SDF_Radial
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T0 | PS_T3, // SDF_Pattern
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T0 | PS_T3, // SDF_Pattern_Clamp
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T0 | PS_T3, // SDF_Pattern_Repeat
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T0 | PS_T3, // SDF_Pattern_MirrorU
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T0 | PS_T3, // SDF_Pattern_MirrorV
	VS_CB0 | VS_CB1 | PS_CB0 | PS_T0 | PS_T3, // SDF_Pattern_Mirror
	0, 0, 0, 0, 0, 0, 0, 0, 0, // SDF_LCD_* (subpixelRendering = false)
	VS_CB0 | PS_T2, // Opacity_Solid
	VS_CB0 | PS_CB0 | PS_T1 | PS_T2, // Opacity_Linear
	VS_CB0 | PS_CB0 | PS_T1 | PS_T2, // Opacity_Radial
	VS_CB0 | PS_CB0 | PS_T0 | PS_T2, // Opacity_Pattern
	VS_CB0 | PS_CB0 | PS_T0 | PS_T2, // Opacity_Pattern_Clamp
	VS_CB0 | PS_CB0 | PS_T0 | PS_T2, // Opacity_Pattern_Repeat
	VS_CB0 | PS_CB0 | PS_T0 | PS_T2, // Opacity_Pattern_MirrorU
	VS_CB0 | PS_CB0 | PS_T0 | PS_T2, // Opacity_Pattern_MirrorV
	VS_CB0 | PS_CB0 | PS_T0 | PS_T2, // Opacity_Pattern_Mirror
	VS_CB0 | PS_T0 | PS_T2, // Upsample
	VS_CB0 | PS_T0, // Downsample
	VS_CB0 | PS_CB1 | PS_T2 | PS_T4, // Shadow
	VS_CB0 | PS_CB1 | PS_T2 | PS_T4, // Blur
	0, // Custom_Effect
};

const char* const SHADER_NAMES[Shader::Count] = {
	"RGBA",
	"Mask",
	"Clear",
	"Path_Solid",
	"Path_Linear",
	"Path_Radial",
	"Path_Pattern",
	"Path_Pattern_Clamp",
	"Path_Pattern_Repeat",
	"Path_Pattern_MirrorU",
	"Path_Pattern_MirrorV",
	"Path_Pattern_Mirror",
	"Path_AA_Solid",
	"Path_AA_Linear",
	"Path_AA_Radial",
	"Path_AA_Pattern",
	"Path_AA_Pattern_Clamp",
	"Path_AA_Pattern_Repeat",
	"Path_AA_Pattern_MirrorU",
	"Path_AA_Pattern_MirrorV",
	"Path_AA_Pattern_Mirror",
	"SDF_Solid",
	"SDF_Linear",
	"SDF_Radial",
	"SDF_Pattern",
	"SDF_Pattern_Clamp",
	"SDF_Pattern_Repeat",
	"SDF_Pattern_MirrorU",
	"SDF_Pattern_MirrorV",
	"SDF_Pattern_Mirror",
	"SDF_LCD_Solid",
	"SDF_LCD_Linear",
	"SDF_LCD_Radial",
	"SDF_LCD_Pattern",
	"SDF_LCD_Pattern_Clamp",
	"SDF_LCD_Pattern_Repeat",
	"SDF_LCD_Pattern_MirrorU",
	"SDF_LCD_Pattern_MirrorV",
	"SDF_LCD_Pattern_Mirror",
	"Opacity_Solid",
	"Opacity_Linear",
	"Opacity_Radial",
	"Opacity_Pattern",
	"Opacity_Pattern_Clamp",
	"Opacity_Pattern_Repeat",
	"Opacity_Pattern_MirrorU",
	"Opacity_Pattern_MirrorV",
	"Opacity_Pattern_Mirror",
	"Upsample",
	"Downsample",
	"Shadow",
	"Blur",
	"Custom_Effect",
};

struct VertexAttrDesc
{
	Tr2VertexDefinition::UsageCode usage;
	unsigned usageIndex;
	Tr2VertexDefinition::DataType dataType;
	Tr2ShaderPipelineInputAL::Type inputType;
	uint32_t dimension;
};

// Coverage/Rect/Tile/ImagePos land on TEXCOORD2..5 to match the fxc /D semantic renames.
const VertexAttrDesc VERTEX_ATTRS[Shader::Vertex::Format::Attr::Count] = {
	{ Tr2VertexDefinition::POSITION, 0, Tr2VertexDefinition::FLOAT32_2, Tr2ShaderPipelineInputAL::FLOAT, 2 },
	{ Tr2VertexDefinition::COLOR, 0, Tr2VertexDefinition::UBYTE_4_NORM, Tr2ShaderPipelineInputAL::FLOAT, 4 },
	{ Tr2VertexDefinition::TEXCOORD, 0, Tr2VertexDefinition::FLOAT32_2, Tr2ShaderPipelineInputAL::FLOAT, 2 },
	{ Tr2VertexDefinition::TEXCOORD, 1, Tr2VertexDefinition::FLOAT32_2, Tr2ShaderPipelineInputAL::FLOAT, 2 },
	{ Tr2VertexDefinition::TEXCOORD, 2, Tr2VertexDefinition::FLOAT32_1, Tr2ShaderPipelineInputAL::FLOAT, 1 },
	{ Tr2VertexDefinition::TEXCOORD, 3, Tr2VertexDefinition::USHORT_4_NORM, Tr2ShaderPipelineInputAL::FLOAT, 4 },
	{ Tr2VertexDefinition::TEXCOORD, 4, Tr2VertexDefinition::FLOAT32_4, Tr2ShaderPipelineInputAL::FLOAT, 4 },
	{ Tr2VertexDefinition::TEXCOORD, 5, Tr2VertexDefinition::FLOAT32_4, Tr2ShaderPipelineInputAL::FLOAT, 4 },
};

// The nine SDF_LCD_* permutations are not built (subpixelRendering = false), so every
// shader after them is nine slots ahead of its bytecode index.
const int LCD_PERMUTATION_COUNT = 9;

static_assert( Tr2Noesis::VERTEX_SHADER_COUNT == Shader::Vertex::Count, "vertex shader table must match Shader::Vertex::Enum" );
static_assert( Tr2Noesis::PIXEL_SHADER_COUNT == 43, "pixel shader table must stay at the 43 non-LCD permutations" );
static_assert( Shader::Count == 53, "Noesis shader enum changed; revisit the tables below" );
static_assert( Shader::SDF_LCD_Pattern_Mirror - Shader::SDF_LCD_Solid + 1 == LCD_PERMUTATION_COUNT,
			   "the skipped SDF_LCD block is no longer nine permutations" );
static_assert( sizeof( SamplerState ) == 1, "SamplerState is a packed uint8_t" );
static_assert( WrapMode::Count <= ( 1u << 3 ), "SamplerState.wrapMode is 3 bits" );
static_assert( MinMagFilter::Count <= ( 1u << 1 ), "SamplerState.minmagFilter is 1 bit" );
static_assert( MipFilter::Count <= ( 1u << 2 ), "SamplerState.mipFilter is 2 bits" );

int PixelBytecodeIndex( uint8_t shader )
{
	if( shader <= Shader::SDF_Pattern_Mirror )
	{
		return shader;
	}
	if( shader >= Shader::Opacity_Solid && shader <= Shader::Blur )
	{
		return shader - LCD_PERMUTATION_COUNT;
	}
	return -1;
}

PixelFormat ToPixelFormat( TextureFormat::Enum format )
{
	switch( format )
	{
	case TextureFormat::RGBA8:
	case TextureFormat::RGBX8:
		return PIXEL_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::R8:
		return PIXEL_FORMAT_R8_UNORM;
	default:
		CCP_ASSERT_M( false, "Unsupported Noesis texture format" );
		return PIXEL_FORMAT_R8G8B8A8_UNORM;
	}
}

uint32_t BytesPerPixel( TextureFormat::Enum format )
{
	switch( format )
	{
	case TextureFormat::RGBA8:
	case TextureFormat::RGBX8:
		return 4;
	case TextureFormat::R8:
		return 1;
	default:
		CCP_ASSERT_M( false, "Unsupported Noesis texture format" );
		return 4;
	}
}

void AddVertexAttributes( Tr2VertexDefinition& definition, Tr2ShaderSignatureAL* vsSignature, uint8_t attributes )
{
	uint32_t registerIndex = 0;
	for( uint32_t attr = 0; attr < Shader::Vertex::Format::Attr::Count; ++attr )
	{
		if( ( attributes & ( 1u << attr ) ) == 0 )
		{
			continue;
		}

		const VertexAttrDesc& desc = VERTEX_ATTRS[attr];
		definition.Add( desc.dataType, desc.usage, desc.usageIndex );
		if( vsSignature )
		{
			vsSignature->Add( desc.usage, desc.usageIndex, registerIndex, desc.inputType, desc.dimension );
		}
		++registerIndex;
	}
}

void ToAddressMode( WrapMode::Enum wrap, Tr2SamplerDescription& desc )
{
	switch( wrap )
	{
	case WrapMode::ClampToEdge:
		desc.m_addressU = TA_CLAMP;
		desc.m_addressV = TA_CLAMP;
		break;
	case WrapMode::ClampToZero:
		desc.m_addressU = TA_BORDER;
		desc.m_addressV = TA_BORDER;
		break;
	case WrapMode::Repeat:
		desc.m_addressU = TA_WRAP;
		desc.m_addressV = TA_WRAP;
		break;
	case WrapMode::MirrorU:
		desc.m_addressU = TA_MIRROR;
		desc.m_addressV = TA_WRAP;
		break;
	case WrapMode::MirrorV:
		desc.m_addressU = TA_WRAP;
		desc.m_addressV = TA_MIRROR;
		break;
	case WrapMode::Mirror:
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

Tr2RenderContextEnum::TextureFilter ToMinMagFilter( MinMagFilter::Enum filter )
{
	return filter == MinMagFilter::Linear ? TF_LINEAR : TF_POINT;
}

Tr2RenderContextEnum::TextureFilter ToMipFilter( MipFilter::Enum filter )
{
	switch( filter )
	{
	case MipFilter::Linear:
		return TF_LINEAR;
	case MipFilter::Nearest:
		return TF_POINT;
	case MipFilter::Disabled:
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

bool Tr2NoesisTexture::HasMipMaps() const
{
	return m_levels > 1;
}

bool Tr2NoesisTexture::IsInverted() const
{
	return false;
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

Tr2NoesisRenderTarget::Tr2NoesisRenderTarget( Ptr<Tr2NoesisTexture> color, Tr2TextureAL stencil, uint32_t width, uint32_t height ) :
	m_color( color ),
	m_stencil( stencil ),
	m_width( width ),
	m_height( height )
{
}

Texture* Tr2NoesisRenderTarget::GetTexture()
{
	return m_color;
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

bool Tr2NoesisRenderDevice::DynamicRing::Create( uint32_t stride, uint32_t size, Tr2GpuUsage::Type gpuUsage, const char* name, Tr2PrimaryRenderContextAL& primary )
{
	CCP_ASSERT_M( stride != 0 && ( size % stride ) == 0, "Noesis ring size must be a whole number of strides" );

	pageSize = size;
	pageIndex = 0;
	pos = 0;
	drawPos = 0;
	mapped = false;

	const uint32_t backBufferCount = primary.GetBackBufferCount();
	CCP_ASSERT_M( backBufferCount == 0 || backBufferCount == PAGE_COUNT,
				  "Noesis dynamic ring PAGE_COUNT must match the swap-chain buffer count" );

	const Tr2CpuUsage::Type cpuUsage = Tr2CpuUsage::WRITE_OFTEN | Tr2CpuUsage::NON_SYNCRONIZED_WRITE;
	for( uint32_t i = 0; i < PAGE_COUNT; ++i )
	{
		const ALResult result = pages[i].Create( stride, size / stride, gpuUsage, cpuUsage, nullptr, primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis dynamic buffer '%s' page %u", name, i );
			CCP_ASSERT_M( false, "Failed to create Noesis dynamic buffer" );
			return false;
		}
		char pageName[64];
		sprintf_s( pageName, "Noesis_%s_%u", name, i );
		pages[i].SetName( pageName );
	}

	SyncFrame( primary.GetCurrentBackBufferIndex() );
	return true;
}

void Tr2NoesisRenderDevice::DynamicRing::SyncFrame( uint32_t frameIndex )
{
	CCP_ASSERT_M( frameIndex < PAGE_COUNT, "Noesis ring frame index exceeds PAGE_COUNT" );
	CCP_ASSERT_M( !mapped, "Noesis ring still mapped at a frame boundary" );
	if( mapped || frameIndex >= PAGE_COUNT )
	{
		return;
	}
	if( pageIndex != frameIndex )
	{
		pageIndex = frameIndex;
		pos = 0;
	}
}

void* Tr2NoesisRenderDevice::DynamicRing::Map( uint32_t bytes, Tr2RenderContextAL& context )
{
	if( bytes > pageSize )
	{
		CCP_NOESIS_LOGERR( "Noesis Map request %u exceeds ring page size %u", bytes, pageSize );
		CCP_ASSERT_M( false, "Noesis Map request exceeds ring page size" );
		return nullptr;
	}

	CCP_ASSERT_M( !mapped, "Noesis Map without a matching Unmap" );
	if( mapped )
	{
		Unmap( context );
	}

	if( pos + bytes > pageSize )
	{
		CCP_NOESIS_LOGERR( "Noesis Map of %u bytes at pos %u exceeds frame page size %u", bytes, pos, pageSize );
		CCP_ASSERT_M( false, "Noesis dynamic ring exceeded the per-frame page budget" );
		return nullptr;
	}

	void* data = nullptr;
	const ALResult result = pages[pageIndex].MapForWriting( data, context );
	if( FAILED( result ) || data == nullptr )
	{
		CCP_ASSERT_M( false, "Failed to map Noesis dynamic buffer" );
		return nullptr;
	}

	mapped = true;
	drawPos = pos;
	pos += bytes;
	return static_cast<uint8_t*>( data ) + drawPos;
}

void Tr2NoesisRenderDevice::DynamicRing::Unmap( Tr2RenderContextAL& context )
{
	if( !mapped )
	{
		return;
	}
	pages[pageIndex].UnmapForWriting( context );
	mapped = false;
}

Tr2BufferAL& Tr2NoesisRenderDevice::DynamicRing::CurrentPage()
{
	return pages[pageIndex];
}

// --------------------------------------------------------------------------------------
// Tr2NoesisRenderDevice
// --------------------------------------------------------------------------------------

Tr2NoesisRenderDevice::Tr2NoesisRenderDevice( Tr2PrimaryRenderContextAL& primaryContext ) :
	m_primary( &primaryContext ),
	m_context( &primaryContext ),
	m_valid( true ),
	m_batchCounts{},
	m_reportedCounts{},
	m_unwiredReported( 0 ),
	m_logBatchDetail( true )
{
	Tr2Noesis::EnsureInitialized();

	m_caps.centerPixelOffset = 0.0f;
	m_caps.linearRendering = false;
	m_caps.subpixelRendering = false;
	m_caps.depthRangeZeroToOne = true;
	m_caps.clipSpaceYInverted = false;

	SetOffscreenSampleCount( 1 );

	// Default is 1024x1024. 2048x2048 is what the SDK rendering tutorial uses and keeps
	// discardedGlyphTiles at zero once CJK, emoji and several sizes share the atlas.
	SetGlyphCacheWidth( 2048 );
	SetGlyphCacheHeight( 2048 );

	CreateVertexLayouts();
	CreateShaders();
	CreateSamplers();
	CreateRings();

	if( !m_valid )
	{
		CCP_NOESIS_LOGERR( "Noesis render device construction failed; shaders, layouts, samplers or rings are invalid" );
	}
}

Tr2NoesisRenderDevice::~Tr2NoesisRenderDevice()
{
	if( m_context )
	{
		m_vertices.Unmap( *m_context );
		m_indices.Unmap( *m_context );
	}
}

bool Tr2NoesisRenderDevice::IsValid() const
{
	return m_valid;
}

void Tr2NoesisRenderDevice::SetRenderContext( Tr2RenderContextAL& renderContext )
{
	if( m_context && m_context != &renderContext )
	{
		m_vertices.Unmap( *m_context );
		m_indices.Unmap( *m_context );
	}
	m_context = &renderContext;
}

const DeviceCaps& Tr2NoesisRenderDevice::GetCaps() const
{
	return m_caps;
}

Ptr<RenderTarget> Tr2NoesisRenderDevice::CreateRenderTarget( const char* label, uint32_t width, uint32_t height,
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
		const Tr2BitmapDimensions stencilDesc( width, height, 1, PIXEL_FORMAT_D24_UNORM_S8_UINT );
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

	Ptr<Tr2NoesisTexture> color = MakePtr<Tr2NoesisTexture>( colorAL, width, height, 1, true );
	CCP_NOESIS_LOG( "RenderTarget '%s' %u x %u", SafeLabel( label, "" ), width, height );
	return MakePtr<Tr2NoesisRenderTarget>( color, stencilAL, width, height );
}

Ptr<RenderTarget> Tr2NoesisRenderDevice::CloneRenderTarget( const char* label, RenderTarget* surface_ )
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	CCP_ASSERT_M( surface_ != nullptr, "CloneRenderTarget with null surface" );

	Tr2NoesisRenderTarget* surface = static_cast<Tr2NoesisRenderTarget*>( surface_ );

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

	Ptr<Tr2NoesisTexture> color = MakePtr<Tr2NoesisTexture>( colorAL, surface->GetWidth(), surface->GetHeight(), 1, true );
	return MakePtr<Tr2NoesisRenderTarget>( color, surface->GetStencil(), surface->GetWidth(), surface->GetHeight() );
}

Ptr<Texture> Tr2NoesisRenderDevice::CreateTexture( const char* label, uint32_t width, uint32_t height,
												   uint32_t numLevels, TextureFormat::Enum format, const void** data )
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
	return MakePtr<Tr2NoesisTexture>( textureAL, width, height, numLevels, format == TextureFormat::RGBA8 );
}

void Tr2NoesisRenderDevice::UpdateTexture( Texture* texture_, uint32_t level, uint32_t x, uint32_t y,
										   uint32_t width, uint32_t height, const void* data )
{
	CCP_ASSERT_M( m_context != nullptr, "UpdateTexture without a render context" );
	CCP_ASSERT_M( texture_ != nullptr, "UpdateTexture with null texture" );
	CCP_ASSERT_M( data != nullptr, "UpdateTexture with null data" );

	Tr2NoesisTexture* texture = static_cast<Tr2NoesisTexture*>( texture_ );
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
	// barrier EndUpdatingTextures exists for, so the 19th virtual is left at the
	// SDK default. Tr2ResourceSetAL::Create is not the covering mechanism:
	// AddTransition skips when (defaultState & PIXEL_SHADER_RESOURCE) != 0.
}

void Tr2NoesisRenderDevice::BeginOffscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "BeginOffscreenRender without a render context" );
	SyncRingsToCurrentFrame();
	m_context->PushGpuMarker( "Noesis.Offscreen" );
}

void Tr2NoesisRenderDevice::EndOffscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "EndOffscreenRender without a render context" );
	m_context->PopGpuMarker();
}

void Tr2NoesisRenderDevice::BeginOnscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "BeginOnscreenRender without a render context" );
	SyncRingsToCurrentFrame();
	m_context->PushGpuMarker( "Noesis" );
}

void Tr2NoesisRenderDevice::EndOnscreenRender()
{
	CCP_ASSERT_M( m_context != nullptr, "EndOnscreenRender without a render context" );
	m_context->PopGpuMarker();

	// Last Noesis call of the frame, so this is where a frame's worth of batches is complete.
	ReportFrameBatches();
}

void Tr2NoesisRenderDevice::SetRenderTarget( RenderTarget* surface_ )
{
	CCP_ASSERT_M( m_context != nullptr, "SetRenderTarget without a render context" );
	CCP_ASSERT_M( surface_ != nullptr, "SetRenderTarget with null surface" );

	Tr2NoesisRenderTarget* surface = static_cast<Tr2NoesisRenderTarget*>( surface_ );
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

void Tr2NoesisRenderDevice::BeginTile( RenderTarget* /*surface*/, const Tile& /*tile*/ )
{
	// TODO G1: TrinityAL has no scissor rect. Noesis clears regions by drawing with the Clear
	// shader, so without scissor those draws may cover more than the intended tile.
}

void Tr2NoesisRenderDevice::EndTile( RenderTarget* /*surface*/ )
{
}

void Tr2NoesisRenderDevice::ResolveRenderTarget( RenderTarget* /*surface*/, const Tile* /*tiles*/, uint32_t /*numTiles*/ )
{
	// Sample count is 1, so there is no MSAA resolve. Color targets are created
	// RENDER_TARGET | SHADER_RESOURCE, so defaultState is PIXEL_SHADER_RESOURCE |
	// NON_PIXEL_SHADER_RESOURCE. SetRenderTarget transitions the previously bound
	// color from RENDER_TARGET back to that default when it is unbound; the render
	// step's push/pop around the offscreen phase is what unbinds the last one
	// before it is sampled. SetResourceSet does not add an SRV barrier here:
	// AddTransition skips when defaultState already includes PIXEL_SHADER_RESOURCE.
}

void* Tr2NoesisRenderDevice::MapVertices( uint32_t bytes )
{
	CCP_ASSERT_M( m_context != nullptr, "MapVertices without a render context" );
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	m_vertices.SyncFrame( m_primary->GetCurrentBackBufferIndex() );
	return m_vertices.Map( bytes, *m_context );
}

void Tr2NoesisRenderDevice::UnmapVertices()
{
	CCP_ASSERT_M( m_context != nullptr, "UnmapVertices without a render context" );
	m_vertices.Unmap( *m_context );
}

void* Tr2NoesisRenderDevice::MapIndices( uint32_t bytes )
{
	CCP_ASSERT_M( m_context != nullptr, "MapIndices without a render context" );
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	m_indices.SyncFrame( m_primary->GetCurrentBackBufferIndex() );
	return m_indices.Map( bytes, *m_context );
}

void Tr2NoesisRenderDevice::UnmapIndices()
{
	CCP_ASSERT_M( m_context != nullptr, "UnmapIndices without a render context" );
	m_indices.Unmap( *m_context );
}

void Tr2NoesisRenderDevice::DrawBatch( const Batch& batch )
{
	CCP_ASSERT_M( m_context != nullptr, "DrawBatch without a render context" );
	CCP_ASSERT_M( !batch.singlePassStereo, "Noesis sent a stereo batch; the stereo permutations are not compiled" );

	const uint8_t shader = batch.shader.v;
	if( shader >= Shader::Count )
	{
		CCP_ASSERT_M( false, "Noesis DrawBatch: shader index is out of range" );
		return;
	}

	++m_batchCounts[shader];

	const uint32_t flags = PROGRAM_FLAGS[shader];
	if( flags == 0 || !m_programs[shader].IsValid() )
	{
		ReportUnwiredShader( shader );
		return;
	}

	// Noesis filters the 256 render-state combinations itself, so a rejection here means our
	// understanding of the batch is wrong rather than that the state needs skipping.
	CCP_ASSERT_M( RenderDevice::IsValidState( batch.shader, batch.renderState ),
				  "Noesis sent a render state that its own validator rejects" );

	const uint8_t vertexShader = VertexForShader[shader];
	const uint8_t format = FormatForVertex[vertexShader];

	ApplyRenderState( batch );

	m_context->SetShaderProgram( m_programs[shader] );
	m_context->SetVertexLayout( m_vertexLayouts[format] );
	m_context->SetTopology( TOP_TRIANGLES );

	// vertexOffset is a byte offset from the current Map, matching the SDK's own D3D12 device.
	// Indices are bound at the page base and addressed through startIndex instead.
	m_context->SetStreamSource( 0, m_vertices.CurrentPage(), m_vertices.drawPos + batch.vertexOffset, SizeForFormat[format] );
	m_context->SetIndices( m_indices.CurrentPage(), 2 );

	BindUniforms( batch, flags );
	BindResources( batch, flags );

	CCP_ASSERT_M( ( batch.numIndices % 3 ) == 0, "Noesis batch index count is not a whole number of triangles" );

	const uint32_t startIndex = m_indices.drawPos / 2 + batch.startIndex;
	const ALResult result = m_context->DrawIndexedPrimitive( batch.numVertices, startIndex, batch.numIndices / 3, 0 );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "DrawIndexedPrimitive failed for shader '%s'", SHADER_NAMES[shader] );
		CCP_ASSERT_M( false, "Noesis DrawIndexedPrimitive failed" );
		return;
	}

	if( m_logBatchDetail && Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Batch '%s' state=0x%02x stencilRef=%u vertices=%u indices=%u vertexOffset=%u startIndex=%u",
						SHADER_NAMES[shader], batch.renderState.v, batch.stencilRef,
						batch.numVertices, batch.numIndices, batch.vertexOffset, startIndex );
	}
}

void Tr2NoesisRenderDevice::ReportUnwiredShader( uint8_t shader )
{
	static_assert( Shader::Count <= 64, "the unwired-shader latch is a uint64_t bitset" );

	const uint64_t bit = 1ull << shader;
	if( ( m_unwiredReported & bit ) != 0 )
	{
		return;
	}
	m_unwiredReported |= bit;

	// Once per shader: a batch-rate assert is unusable. The per-frame histogram is what shows
	// that an unwired shader is still being asked for.
	CCP_NOESIS_LOGERR( "DrawBatch: shader '%s' (%u) was never compiled. The nine SDF_LCD_* "
					   "permutations are skipped because subpixelRendering is false, and "
					   "Custom_Effect needs a shader supplied by the effect itself.",
					   SHADER_NAMES[shader], shader );
	CCP_ASSERT_M( false, "Noesis DrawBatch: shader permutation was never compiled" );
}

void Tr2NoesisRenderDevice::ReportFrameBatches()
{
	if( Tr2Noesis::IsLogVerbose() && memcmp( m_batchCounts, m_reportedCounts, sizeof( m_batchCounts ) ) != 0 )
	{
		uint32_t total = 0;
		for( uint32_t shader = 0; shader < Shader::Count; ++shader )
		{
			total += m_batchCounts[shader];
		}

		// Counted separately above so that a truncated histogram still reports the real total.
		char histogram[512] = {};
		uint32_t offset = 0;
		for( uint32_t shader = 0; shader < Shader::Count; ++shader )
		{
			if( m_batchCounts[shader] == 0 )
			{
				continue;
			}
			const int written = _snprintf_s( histogram + offset, sizeof( histogram ) - offset, _TRUNCATE,
											 "%s%s=%u", offset == 0 ? "" : ", ", SHADER_NAMES[shader], m_batchCounts[shader] );
			if( written < 0 )
			{
				break;
			}
			offset += static_cast<uint32_t>( written );
		}

		CCP_NOESIS_LOG( "Batches this frame: %u (%s)", total, total == 0 ? "none" : histogram );
		memcpy( m_reportedCounts, m_batchCounts, sizeof( m_reportedCounts ) );
	}

	memset( m_batchCounts, 0, sizeof( m_batchCounts ) );
	m_logBatchDetail = false;
}

void Tr2NoesisRenderDevice::BindUniform( Tr2ConstantBufferAL& buffer, const UniformData& uniforms,
										 Tr2RenderContextEnum::ShaderType stage, uint32_t registerIndex, const char* name )
{
	if( uniforms.values == nullptr || uniforms.numDwords == 0 )
	{
		// PROGRAM_FLAGS says the shader declares this register, so an empty block means our
		// transcription of the SDK's root-signature flags is wrong.
		CCP_ASSERT_M( false, "Noesis batch omitted a constant buffer that its shader declares" );
		return;
	}

	const uint32_t bytes = uniforms.numDwords * sizeof( uint32_t );
	if( buffer.GetSize() < bytes )
	{
		// Sized from what Noesis actually sends rather than transcribed from the SDK's cbuffer
		// layouts, so a layout change costs a reallocation instead of overrunning a buffer.
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
	buffer.Unlock( *m_context );

	// Unlock invalidates the residency token, so this uploads into the frame's ring rather
	// than reusing the address the previous batch was given.
	const ALResult result = m_context->SetConstants( buffer, stage, registerIndex );
	if( FAILED( result ) )
	{
		CCP_ASSERT_M( false, "Failed to bind a Noesis constant buffer" );
	}
}

void Tr2NoesisRenderDevice::BindUniforms( const Batch& batch, uint32_t flags )
{
	if( flags & VS_CB0 )
	{
		BindUniform( m_vertexUniforms[0], batch.vertexUniforms[0], VERTEX_SHADER, 0, "Noesis_VertexUniforms0" );
	}
	if( flags & VS_CB1 )
	{
		BindUniform( m_vertexUniforms[1], batch.vertexUniforms[1], VERTEX_SHADER, 1, "Noesis_VertexUniforms1" );
	}
	if( flags & PS_CB0 )
	{
		BindUniform( m_pixelUniforms[0], batch.pixelUniforms[0], PIXEL_SHADER, 0, "Noesis_PixelUniforms0" );
	}
	if( flags & PS_CB1 )
	{
		BindUniform( m_pixelUniforms[1], batch.pixelUniforms[1], PIXEL_SHADER, 1, "Noesis_PixelUniforms1" );
	}
}

void Tr2NoesisRenderDevice::BindResources( const Batch& batch, uint32_t flags )
{
	if( ( flags & ( PS_T0 | PS_T1 | PS_T2 | PS_T3 | PS_T4 ) ) == 0 )
	{
		// Solid fills bind nothing, so they never pay for a resource set.
		return;
	}

	const uint8_t shader = batch.shader.v;
	const struct
	{
		uint32_t flag;
		uint32_t registerIndex;
		Texture* texture;
		SamplerState sampler;
	} bindings[] = {
		{ PS_T0, 0, batch.pattern, batch.patternSampler },
		{ PS_T1, 1, batch.ramps, batch.rampsSampler },
		{ PS_T2, 2, batch.image, batch.imageSampler },
		{ PS_T3, 3, batch.glyphs, batch.glyphsSampler },
		{ PS_T4, 4, batch.shadow, batch.shadowSampler },
	};

	Tr2ResourceSetDescriptionAL description( m_programs[shader] );
	for( const auto& binding : bindings )
	{
		if( ( flags & binding.flag ) == 0 )
		{
			continue;
		}
		if( binding.texture == nullptr )
		{
			CCP_ASSERT_M( false, "Noesis batch omitted a texture that its shader declares" );
			continue;
		}

		Tr2NoesisTexture* texture = static_cast<Tr2NoesisTexture*>( binding.texture );
		// linearRendering is false, so textures are sampled raw rather than sRGB-converted.
		// A rejection means the register is absent from the program's map, which would mean
		// PROGRAM_FLAGS and the signature we built from it disagree.
		const bool srvSet = description.SetSrv( PIXEL_SHADER, binding.registerIndex, texture->GetAL() );
		CCP_ASSERT_M( srvSet, "Noesis shader program has no SRV at the register PROGRAM_FLAGS claims" );

		CCP_ASSERT_M( binding.sampler.v < std::size( m_samplers ), "Noesis sampler index out of range" );
		const bool samplerSet = description.SetSampler( PIXEL_SHADER, binding.registerIndex, m_samplers[binding.sampler.v] );
		CCP_ASSERT_M( samplerSet, "Noesis shader program has no sampler at the register PROGRAM_FLAGS claims" );
	}

	const uint64_t key = ( static_cast<uint64_t>( shader ) << 32 ) | description.ComputeHash();
	ResourceSetEntry& entry = m_resourceSets[key];
	if( !entry.set.IsValid() || !( entry.description == description ) )
	{
		Tr2ResourceSetAL set;
		const ALResult result = set.Create( description, m_programs[shader], *m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create a Noesis resource set for shader '%s'", SHADER_NAMES[shader] );
			CCP_ASSERT_M( false, "Failed to create a Noesis resource set" );
			return;
		}
		set.SetName( SHADER_NAMES[shader] );
		entry.description = description;
		entry.set = set;
	}

	const ALResult result = m_context->SetResourceSet( entry.set );
	if( FAILED( result ) )
	{
		CCP_ASSERT_M( false, "Failed to bind a Noesis resource set" );
	}
}

void Tr2NoesisRenderDevice::CreateVertexLayouts()
{
	for( uint32_t format = 0; format < Shader::Vertex::Format::Count; ++format )
	{
		Tr2VertexDefinition definition;
		AddVertexAttributes( definition, nullptr, AttributesForFormat[format] );
		const ALResult result = m_vertexLayouts[format].Create( definition, *m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis vertex layout %u", format );
			CCP_ASSERT_M( false, "Failed to create Noesis vertex layout" );
			m_valid = false;
		}
	}
}

void Tr2NoesisRenderDevice::CreateShaders()
{
	for( uint32_t vs = 0; vs < Shader::Vertex::Count; ++vs )
	{
		Tr2ShaderSignatureAL signature;
		Tr2VertexDefinition unused;
		AddVertexAttributes( unused, &signature, AttributesForFormat[FormatForVertex[vs]] );
		signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0 );
		if( vs >= Shader::Vertex::PosColorTex1_SDF && vs <= Shader::Vertex::PosTex0Tex1RectTile_SDF )
		{
			signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 1 );
		}

		const Tr2Noesis::ShaderBytecode& bytecode = Tr2Noesis::VERTEX_SHADERS[vs];
		const ALResult result = m_vertexShaders[vs].Create(
			VERTEX_SHADER,
			Tr2ShaderBytecodeAL( bytecode.code, bytecode.size ),
			signature,
			bytecode.name,
			*m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis vertex shader '%s'", bytecode.name );
			CCP_ASSERT_M( false, "Failed to create Noesis vertex shader" );
			m_valid = false;
		}
		else
		{
			m_vertexShaders[vs].SetName( bytecode.name );
		}
	}

	for( uint32_t shader = 0; shader < Shader::Count; ++shader )
	{
		const int bytecodeIndex = PixelBytecodeIndex( static_cast<uint8_t>( shader ) );
		if( bytecodeIndex < 0 )
		{
			continue;
		}

		const uint32_t flags = PROGRAM_FLAGS[shader];
		Tr2ShaderSignatureAL signature;
		if( flags & PS_CB0 )
		{
			signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0 );
		}
		if( flags & PS_CB1 )
		{
			signature.Add( Tr2ShaderRegisterAL::CONSTANT_BUFFER, 1 );
		}
		if( flags & PS_T0 )
		{
			signature.Add( Tr2ShaderRegisterAL::SRV_TEXTURE2D, 0 );
			signature.Add( Tr2ShaderRegisterAL::SAMPLER, 0 );
		}
		if( flags & PS_T1 )
		{
			signature.Add( Tr2ShaderRegisterAL::SRV_TEXTURE2D, 1 );
			signature.Add( Tr2ShaderRegisterAL::SAMPLER, 1 );
		}
		if( flags & PS_T2 )
		{
			signature.Add( Tr2ShaderRegisterAL::SRV_TEXTURE2D, 2 );
			signature.Add( Tr2ShaderRegisterAL::SAMPLER, 2 );
		}
		if( flags & PS_T3 )
		{
			signature.Add( Tr2ShaderRegisterAL::SRV_TEXTURE2D, 3 );
			signature.Add( Tr2ShaderRegisterAL::SAMPLER, 3 );
		}
		if( flags & PS_T4 )
		{
			signature.Add( Tr2ShaderRegisterAL::SRV_TEXTURE2D, 4 );
			signature.Add( Tr2ShaderRegisterAL::SAMPLER, 4 );
		}

		const Tr2Noesis::ShaderBytecode& bytecode = Tr2Noesis::PIXEL_SHADERS[bytecodeIndex];
		const ALResult result = m_pixelShaders[shader].Create(
			PIXEL_SHADER,
			Tr2ShaderBytecodeAL( bytecode.code, bytecode.size ),
			signature,
			bytecode.name,
			*m_primary );
		if( FAILED( result ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis pixel shader '%s'", bytecode.name );
			CCP_ASSERT_M( false, "Failed to create Noesis pixel shader" );
			m_valid = false;
			continue;
		}
		m_pixelShaders[shader].SetName( bytecode.name );

		const uint8_t vsIndex = VertexForShader[shader];
		Tr2ShaderAL stages[] = { m_vertexShaders[vsIndex], m_pixelShaders[shader] };
		const ALResult programResult = m_programs[shader].Create( stages, 2, *m_primary );
		if( FAILED( programResult ) )
		{
			CCP_NOESIS_LOGERR( "Failed to create Noesis shader program '%s'", bytecode.name );
			CCP_ASSERT_M( false, "Failed to create Noesis shader program" );
			m_valid = false;
		}
		else
		{
			m_programs[shader].SetName( bytecode.name );
		}
	}
}

void Tr2NoesisRenderDevice::CreateSamplers()
{
	// SamplerState::v packs wrapMode:3, minmagFilter:1, mipFilter:2; unused:2 must stay 0
	// or the index lands past the 64 slots those six bits address.
	static_assert( sizeof( m_samplers ) / sizeof( m_samplers[0] ) == ( 1u << 6 ),
				   "m_samplers must cover every 6-bit SamplerState value" );

	for( uint8_t wrap = 0; wrap < WrapMode::Count; ++wrap )
	{
		for( uint8_t minmag = 0; minmag < MinMagFilter::Count; ++minmag )
		{
			for( uint8_t mip = 0; mip < MipFilter::Count; ++mip )
			{
				SamplerState state = { { wrap, minmag, mip } };

				const WrapMode::Enum wrapMode = static_cast<WrapMode::Enum>( wrap );
				const MinMagFilter::Enum minMagFilter = static_cast<MinMagFilter::Enum>( minmag );
				const MipFilter::Enum mipFilter = static_cast<MipFilter::Enum>( mip );

				Tr2SamplerDescription desc;
				desc.m_minFilter = ToMinMagFilter( minMagFilter );
				desc.m_magFilter = ToMinMagFilter( minMagFilter );
				desc.m_mipFilter = ToMipFilter( mipFilter );
				ToAddressMode( wrapMode, desc );
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

				CCP_ASSERT_M( state.v < std::size( m_samplers ), "Noesis sampler index out of range" );
				const ALResult result = m_samplers[state.v].Create( desc, *m_primary );
				if( FAILED( result ) )
				{
					CCP_NOESIS_LOGERR( "Failed to create Noesis sampler %u", state.v );
					CCP_ASSERT_M( false, "Failed to create Noesis sampler" );
					m_valid = false;
				}
			}
		}
	}
}

void Tr2NoesisRenderDevice::CreateRings()
{
	if( !m_vertices.Create( 1, DYNAMIC_VB_SIZE, Tr2GpuUsage::VERTEX_BUFFER, "Vertices", *m_primary ) )
	{
		m_valid = false;
	}
	// Noesis writes 16-bit indices, and the AL reads the index format off the buffer's stride.
	if( !m_indices.Create( 2, DYNAMIC_IB_SIZE, Tr2GpuUsage::INDEX_BUFFER, "Indices", *m_primary ) )
	{
		m_valid = false;
	}
}

void Tr2NoesisRenderDevice::SyncRingsToCurrentFrame()
{
	CCP_ASSERT_M( m_primary != nullptr, "Noesis render device has no primary context" );
	const uint32_t frameIndex = m_primary->GetCurrentBackBufferIndex();
	m_vertices.SyncFrame( frameIndex );
	m_indices.SyncFrame( frameIndex );
}

void Tr2NoesisRenderDevice::ApplyRenderState( const Batch& batch )
{
	// Emitted in full for every batch, never as a delta: on DX12 any state we leave out
	// survives in the sticky PSO description from whichever render step ran before us.
	CCP_ASSERT_M( m_context != nullptr, "ApplyRenderState without a render context" );

	const Noesis::RenderState state = batch.renderState;
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
	add( RS_FILLMODE, state.f.wireframe ? FM_WIREFRAME : FM_SOLID );
	add( RS_DEPTHBIAS, 0 );
	add( RS_SLOPESCALEDEPTHBIAS, 0 );
	add( RS_DEPTH_CLIP_ENABLE, 1 );
	add( RS_ZWRITEENABLE, 0 );
	add( RS_COLORWRITEENABLE, state.f.colorEnable ? ( COLORWRITEENABLE_RED | COLORWRITEENABLE_GREEN | COLORWRITEENABLE_BLUE | COLORWRITEENABLE_ALPHA ) : 0 );
	add( RS_SRGBWRITEENABLE, 0 );
	add( RS_ALPHATESTENABLE, 0 );

	if( state.f.colorEnable && state.f.blendMode != Noesis::BlendMode::Src )
	{
		add( RS_ALPHABLENDENABLE, 1 );
		add( RS_SEPARATEALPHABLENDENABLE, 1 );
		add( RS_BLENDOP, BO_ADD );
		add( RS_BLENDOPALPHA, BO_ADD );
		add( RS_SRCBLENDALPHA, BM_ONE );
		add( RS_DESTBLENDALPHA, BM_INVSRCALPHA );

		switch( state.f.blendMode )
		{
		case Noesis::BlendMode::SrcOver:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRCALPHA );
			break;
		case Noesis::BlendMode::SrcOver_Multiply:
			add( RS_SRCBLEND, BM_DESTCOLOR );
			add( RS_DESTBLEND, BM_INVSRCALPHA );
			break;
		case Noesis::BlendMode::SrcOver_Screen:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRCCOLOR );
			break;
		case Noesis::BlendMode::SrcOver_Additive:
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_ONE );
			break;
		case Noesis::BlendMode::SrcOver_Dual:
			CCP_ASSERT_M( false, "SrcOver_Dual requires dual-source blending, which is off (no LCD text)" );
			add( RS_SRCBLEND, BM_ONE );
			add( RS_DESTBLEND, BM_INVSRCALPHA );
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

	const bool zTest = state.f.stencilMode == StencilMode::Disabled_ZTest || state.f.stencilMode == StencilMode::Equal_Keep_ZTest;
	add( RS_ZENABLE, zTest ? 1 : 0 );
	add( RS_ZFUNC, CMP_GREATEREQUAL );

	bool stencilEnable = false;
	uint32_t stencilFunc = CMP_EQUAL;
	uint32_t stencilPass = STENCILOP_KEEP;
	switch( state.f.stencilMode )
	{
	case StencilMode::Disabled:
	case StencilMode::Disabled_ZTest:
		break;
	case StencilMode::Equal_Keep:
	case StencilMode::Equal_Keep_ZTest:
		stencilEnable = true;
		break;
	case StencilMode::Equal_Incr:
		stencilEnable = true;
		stencilPass = STENCILOP_INCR;
		break;
	case StencilMode::Equal_Decr:
		stencilEnable = true;
		stencilPass = STENCILOP_DECR;
		break;
	case StencilMode::Clear:
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
	add( RS_STENCILREF, batch.stencilRef );
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

// --------------------------------------------------------------------------------------
// The process-wide device
// --------------------------------------------------------------------------------------

namespace Tr2Noesis
{

Tr2NoesisRenderDevice* GetRenderDevice()
{
	// Intentionally leaked; see the declaration.
	static Tr2NoesisRenderDevice* s_device = nullptr;
	static bool s_attempted = false;

	if( s_attempted )
	{
		return s_device;
	}
	s_attempted = true;

	USE_MAIN_THREAD_RENDER_CONTEXT();

	// Plain new: BaseObject overrides operator new to reach Noesis's memory manager, which our
	// callbacks point back at Carbon, so this is still tagged and counted in the noesisMem stat.
	Tr2NoesisRenderDevice* device = new Tr2NoesisRenderDevice( renderContext.GetPrimaryRenderContext() );
	if( !device->IsValid() )
	{
		// One attempt only. A device that failed to build its shaders will not build them on
		// the next frame either, and retrying would repeat the whole assert storm every frame.
		CCP_NOESIS_LOGERR( "Noesis render device is unusable; no Noesis rendering will happen this session" );
		delete device;
		return nullptr;
	}

	CCP_NOESIS_LOGNOTICE( "Noesis render device ready: %u vertex shaders, %u pixel shaders",
						  static_cast<uint32_t>( Shader::Vertex::Count ),
						  static_cast<uint32_t>( Tr2Noesis::PIXEL_SHADER_COUNT ) );

	s_device = device;
	return s_device;
}

}

#endif
