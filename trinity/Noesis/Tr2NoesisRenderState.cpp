// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisRenderState.h"

#include "Tr2RenderContext.h"

using namespace Tr2RenderContextEnum;

uint32_t Tr2NoesisBuildRenderStates( const nxt_batch& batch,
									 uint32_t ( &pairs )[TR2_NOESIS_RENDER_STATE_ENTRIES] )
{
	// Unpacked with nxt.h's own helpers rather than by re-deriving the shifts from the
	// comment on nxt_render_state.
	const nxt_render_state state = batch.render_state;
	const nxt_blend_mode blendMode = nxt_render_state_blend_mode( state );
	const bool colorEnable = nxt_render_state_color_enable( state ) != NXT_FALSE;
	const bool wireframe = nxt_render_state_wireframe( state ) != NXT_FALSE;
	uint32_t count = 0;

	auto add = [&]( Tr2RenderContextEnum::RenderState rs, uint32_t value ) {
		if( count + 2 > TR2_NOESIS_RENDER_STATE_ENTRIES )
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

	return count;
}
