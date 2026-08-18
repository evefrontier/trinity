// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisShaders.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

// clang-format off
#define NOESIS_SHADER_STRINGIZE_( x ) #x
#define NOESIS_SHADER_STRINGIZE( x ) NOESIS_SHADER_STRINGIZE_( x )
#define NOESIS_SHADER_CODE( name ) NOESIS_SHADER_STRINGIZE( NoesisShaders/name.h )
// clang-format on

namespace
{

const uint8_t Pos_VS[] = {
#include NOESIS_SHADER_CODE( Pos_VS )
};
const uint8_t PosColor_VS[] = {
#include NOESIS_SHADER_CODE( PosColor_VS )
};
const uint8_t PosTex0_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0_VS )
};
const uint8_t PosTex0Rect_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Rect_VS )
};
const uint8_t PosTex0RectTile_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0RectTile_VS )
};
const uint8_t PosColorCoverage_VS[] = {
#include NOESIS_SHADER_CODE( PosColorCoverage_VS )
};
const uint8_t PosTex0Coverage_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Coverage_VS )
};
const uint8_t PosTex0CoverageRect_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0CoverageRect_VS )
};
const uint8_t PosTex0CoverageRectTile_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0CoverageRectTile_VS )
};
const uint8_t PosColorTex1_SDF_VS[] = {
#include NOESIS_SHADER_CODE( PosColorTex1_SDF_VS )
};
const uint8_t PosTex0Tex1_SDF_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1_SDF_VS )
};
const uint8_t PosTex0Tex1Rect_SDF_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1Rect_SDF_VS )
};
const uint8_t PosTex0Tex1RectTile_SDF_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1RectTile_SDF_VS )
};
const uint8_t PosColorTex1_VS[] = {
#include NOESIS_SHADER_CODE( PosColorTex1_VS )
};
const uint8_t PosTex0Tex1_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1_VS )
};
const uint8_t PosTex0Tex1Rect_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1Rect_VS )
};
const uint8_t PosTex0Tex1RectTile_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1RectTile_VS )
};
const uint8_t PosColorTex0Tex1_VS[] = {
#include NOESIS_SHADER_CODE( PosColorTex0Tex1_VS )
};
const uint8_t PosTex0Tex1_Downsample_VS[] = {
#include NOESIS_SHADER_CODE( PosTex0Tex1_Downsample_VS )
};
const uint8_t PosColorTex1Rect_VS[] = {
#include NOESIS_SHADER_CODE( PosColorTex1Rect_VS )
};
const uint8_t PosColorTex0RectImagePos_VS[] = {
#include NOESIS_SHADER_CODE( PosColorTex0RectImagePos_VS )
};

const uint8_t RGBA_PS[] = {
#include NOESIS_SHADER_CODE( RGBA_PS )
};
const uint8_t Mask_PS[] = {
#include NOESIS_SHADER_CODE( Mask_PS )
};
const uint8_t Clear_PS[] = {
#include NOESIS_SHADER_CODE( Clear_PS )
};
const uint8_t Path_Solid_PS[] = {
#include NOESIS_SHADER_CODE( Path_Solid_PS )
};
const uint8_t Path_Linear_PS[] = {
#include NOESIS_SHADER_CODE( Path_Linear_PS )
};
const uint8_t Path_Radial_PS[] = {
#include NOESIS_SHADER_CODE( Path_Radial_PS )
};
const uint8_t Path_Pattern_PS[] = {
#include NOESIS_SHADER_CODE( Path_Pattern_PS )
};
const uint8_t Path_Pattern_Clamp_PS[] = {
#include NOESIS_SHADER_CODE( Path_Pattern_Clamp_PS )
};
const uint8_t Path_Pattern_Repeat_PS[] = {
#include NOESIS_SHADER_CODE( Path_Pattern_Repeat_PS )
};
const uint8_t Path_Pattern_MirrorU_PS[] = {
#include NOESIS_SHADER_CODE( Path_Pattern_MirrorU_PS )
};
const uint8_t Path_Pattern_MirrorV_PS[] = {
#include NOESIS_SHADER_CODE( Path_Pattern_MirrorV_PS )
};
const uint8_t Path_Pattern_Mirror_PS[] = {
#include NOESIS_SHADER_CODE( Path_Pattern_Mirror_PS )
};
const uint8_t Path_AA_Solid_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Solid_PS )
};
const uint8_t Path_AA_Linear_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Linear_PS )
};
const uint8_t Path_AA_Radial_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Radial_PS )
};
const uint8_t Path_AA_Pattern_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Pattern_PS )
};
const uint8_t Path_AA_Pattern_Clamp_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Pattern_Clamp_PS )
};
const uint8_t Path_AA_Pattern_Repeat_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Pattern_Repeat_PS )
};
const uint8_t Path_AA_Pattern_MirrorU_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Pattern_MirrorU_PS )
};
const uint8_t Path_AA_Pattern_MirrorV_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Pattern_MirrorV_PS )
};
const uint8_t Path_AA_Pattern_Mirror_PS[] = {
#include NOESIS_SHADER_CODE( Path_AA_Pattern_Mirror_PS )
};
const uint8_t SDF_Solid_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Solid_PS )
};
const uint8_t SDF_Linear_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Linear_PS )
};
const uint8_t SDF_Radial_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Radial_PS )
};
const uint8_t SDF_Pattern_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Pattern_PS )
};
const uint8_t SDF_Pattern_Clamp_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Pattern_Clamp_PS )
};
const uint8_t SDF_Pattern_Repeat_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Pattern_Repeat_PS )
};
const uint8_t SDF_Pattern_MirrorU_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Pattern_MirrorU_PS )
};
const uint8_t SDF_Pattern_MirrorV_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Pattern_MirrorV_PS )
};
const uint8_t SDF_Pattern_Mirror_PS[] = {
#include NOESIS_SHADER_CODE( SDF_Pattern_Mirror_PS )
};
const uint8_t SDF_LCD_Solid_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Solid_PS )
};
const uint8_t SDF_LCD_Linear_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Linear_PS )
};
const uint8_t SDF_LCD_Radial_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Radial_PS )
};
const uint8_t SDF_LCD_Pattern_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Pattern_PS )
};
const uint8_t SDF_LCD_Pattern_Clamp_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Pattern_Clamp_PS )
};
const uint8_t SDF_LCD_Pattern_Repeat_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Pattern_Repeat_PS )
};
const uint8_t SDF_LCD_Pattern_MirrorU_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Pattern_MirrorU_PS )
};
const uint8_t SDF_LCD_Pattern_MirrorV_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Pattern_MirrorV_PS )
};
const uint8_t SDF_LCD_Pattern_Mirror_PS[] = {
#include NOESIS_SHADER_CODE( SDF_LCD_Pattern_Mirror_PS )
};
const uint8_t Opacity_Solid_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Solid_PS )
};
const uint8_t Opacity_Linear_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Linear_PS )
};
const uint8_t Opacity_Radial_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Radial_PS )
};
const uint8_t Opacity_Pattern_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Pattern_PS )
};
const uint8_t Opacity_Pattern_Clamp_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Pattern_Clamp_PS )
};
const uint8_t Opacity_Pattern_Repeat_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Pattern_Repeat_PS )
};
const uint8_t Opacity_Pattern_MirrorU_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Pattern_MirrorU_PS )
};
const uint8_t Opacity_Pattern_MirrorV_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Pattern_MirrorV_PS )
};
const uint8_t Opacity_Pattern_Mirror_PS[] = {
#include NOESIS_SHADER_CODE( Opacity_Pattern_Mirror_PS )
};
const uint8_t Upsample_PS[] = {
#include NOESIS_SHADER_CODE( Upsample_PS )
};
const uint8_t Downsample_PS[] = {
#include NOESIS_SHADER_CODE( Downsample_PS )
};
const uint8_t Shadow_PS[] = {
#include NOESIS_SHADER_CODE( Shadow_PS )
};
const uint8_t Blur_PS[] = {
#include NOESIS_SHADER_CODE( Blur_PS )
};

}

// clang-format off
#define NOESIS_SHADER_ENTRY( name ) { #name, name, sizeof( name ) }
// clang-format on

namespace Tr2Noesis
{

const ShaderBytecode VERTEX_SHADERS[] = {
	NOESIS_SHADER_ENTRY( Pos_VS ),
	NOESIS_SHADER_ENTRY( PosColor_VS ),
	NOESIS_SHADER_ENTRY( PosTex0_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Rect_VS ),
	NOESIS_SHADER_ENTRY( PosTex0RectTile_VS ),
	NOESIS_SHADER_ENTRY( PosColorCoverage_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Coverage_VS ),
	NOESIS_SHADER_ENTRY( PosTex0CoverageRect_VS ),
	NOESIS_SHADER_ENTRY( PosTex0CoverageRectTile_VS ),
	NOESIS_SHADER_ENTRY( PosColorTex1_SDF_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1_SDF_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1Rect_SDF_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1RectTile_SDF_VS ),
	NOESIS_SHADER_ENTRY( PosColorTex1_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1Rect_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1RectTile_VS ),
	NOESIS_SHADER_ENTRY( PosColorTex0Tex1_VS ),
	NOESIS_SHADER_ENTRY( PosTex0Tex1_Downsample_VS ),
	NOESIS_SHADER_ENTRY( PosColorTex1Rect_VS ),
	NOESIS_SHADER_ENTRY( PosColorTex0RectImagePos_VS ),
};

static_assert( sizeof( VERTEX_SHADERS ) / sizeof( VERTEX_SHADERS[ 0 ] ) == VERTEX_SHADER_COUNT,
			   "VERTEX_SHADERS must stay in lockstep with VERTEX_SHADER_COUNT" );

const ShaderBytecode PIXEL_SHADERS[] = {
	NOESIS_SHADER_ENTRY( RGBA_PS ),
	NOESIS_SHADER_ENTRY( Mask_PS ),
	NOESIS_SHADER_ENTRY( Clear_PS ),
	NOESIS_SHADER_ENTRY( Path_Solid_PS ),
	NOESIS_SHADER_ENTRY( Path_Linear_PS ),
	NOESIS_SHADER_ENTRY( Path_Radial_PS ),
	NOESIS_SHADER_ENTRY( Path_Pattern_PS ),
	NOESIS_SHADER_ENTRY( Path_Pattern_Clamp_PS ),
	NOESIS_SHADER_ENTRY( Path_Pattern_Repeat_PS ),
	NOESIS_SHADER_ENTRY( Path_Pattern_MirrorU_PS ),
	NOESIS_SHADER_ENTRY( Path_Pattern_MirrorV_PS ),
	NOESIS_SHADER_ENTRY( Path_Pattern_Mirror_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Solid_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Linear_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Radial_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Pattern_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Pattern_Clamp_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Pattern_Repeat_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Pattern_MirrorU_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Pattern_MirrorV_PS ),
	NOESIS_SHADER_ENTRY( Path_AA_Pattern_Mirror_PS ),
	NOESIS_SHADER_ENTRY( SDF_Solid_PS ),
	NOESIS_SHADER_ENTRY( SDF_Linear_PS ),
	NOESIS_SHADER_ENTRY( SDF_Radial_PS ),
	NOESIS_SHADER_ENTRY( SDF_Pattern_PS ),
	NOESIS_SHADER_ENTRY( SDF_Pattern_Clamp_PS ),
	NOESIS_SHADER_ENTRY( SDF_Pattern_Repeat_PS ),
	NOESIS_SHADER_ENTRY( SDF_Pattern_MirrorU_PS ),
	NOESIS_SHADER_ENTRY( SDF_Pattern_MirrorV_PS ),
	NOESIS_SHADER_ENTRY( SDF_Pattern_Mirror_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Solid_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Linear_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Radial_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Pattern_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Pattern_Clamp_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Pattern_Repeat_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Pattern_MirrorU_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Pattern_MirrorV_PS ),
	NOESIS_SHADER_ENTRY( SDF_LCD_Pattern_Mirror_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Solid_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Linear_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Radial_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Pattern_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Pattern_Clamp_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Pattern_Repeat_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Pattern_MirrorU_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Pattern_MirrorV_PS ),
	NOESIS_SHADER_ENTRY( Opacity_Pattern_Mirror_PS ),
	NOESIS_SHADER_ENTRY( Upsample_PS ),
	NOESIS_SHADER_ENTRY( Downsample_PS ),
	NOESIS_SHADER_ENTRY( Shadow_PS ),
	NOESIS_SHADER_ENTRY( Blur_PS ),
};

static_assert( sizeof( PIXEL_SHADERS ) / sizeof( PIXEL_SHADERS[ 0 ] ) == PIXEL_SHADER_COUNT,
			   "PIXEL_SHADERS must stay in lockstep with PIXEL_SHADER_COUNT" );

}

#endif
