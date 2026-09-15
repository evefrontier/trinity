// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/TriStepRenderNoesis.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisHost.h"

#include "Tr2RenderContext.h"

TriStepRenderNoesis::TriStepRenderNoesis( IRoot* lockobj ) :
	TriRenderStep( lockobj ),
	m_hasOverrideViewport( false ),
	m_overrideX( 0 ),
	m_overrideY( 0 ),
	m_overrideWidth( 0 ),
	m_overrideHeight( 0 ),
	m_viewApi( nullptr ),
	m_hasOverrideClip( false ),
	m_overrideClipLeft( 0 ),
	m_overrideClipTop( 0 ),
	m_overrideClipRight( 0 ),
	m_overrideClipBottom( 0 )
{
}

TriStepResult TriStepRenderNoesis::Execute( Be::Time realTime, Be::Time /*simTime*/, Tr2RenderContext& renderContext )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Nothing wired, or nothing loaded: the normal state for a client with no Noesis UI.
	Tr2NoesisHost* host = GetHostObject();
	if( host == nullptr || m_viewApi == nullptr ||
		!m_viewApi->is_loaded( m_viewApi->header.self ) )
	{
		return RS_OK;
	}

	// First frame builds the device. Here rather than when Python wired the host, because
	// this runs on the render thread with a live context and that does not.
	if( !host->EnsureDevice() )
	{
		return RS_OK;
	}

	const nsi_frame_host* frame = host->GetNsiFrameHost();
	if( frame == nullptr )
	{
		return RS_OK;
	}

	// Noesis touches render state directly rather than through an effect, so everything below
	// runs inside the managed bracket: it resets the state manager's shadow copy on entry and
	// tells it not to trust its cache afterwards. CULLMODE_NONE matches what Noesis expects,
	// though ApplyRenderState sets it per batch as well.
	renderContext.m_esm.BeginManagedRendering( Tr2RenderContextEnum::CULLMODE_NONE );

	// The device is Trinity-owned and reaches the library through the nsi_render_host
	// vtable. Bringing up the renderer creates GPU resources, so it belongs inside the
	// bracket rather than at load time.
	host->BeginFrame( renderContext );

	if( !m_viewApi->ensure_renderer( m_viewApi->header.self, frame ) )
	{
		host->EndFrame();
		renderContext.m_esm.EndManagedRendering();
		return RS_OK;
	}

	// Size follows the viewport: the overlay path uses whatever the job already bound,
	// and Tr2Sprite2dNoesis overrides it to the sprite rect before RunJob.
	if( m_hasOverrideViewport )
	{
		renderContext.m_esm.SetViewport( m_overrideWidth, m_overrideHeight, m_overrideX, m_overrideY, 0.0f, 1.0f );
	}

	const TriViewport& vp = renderContext.m_esm.GetViewport();
	if( vp.width <= 0 || vp.height <= 0 )
	{
		renderContext.m_esm.EndManagedRendering();
		return RS_OK;
	}

	m_viewApi->sync_size( m_viewApi->header.self, static_cast<uint32_t>( vp.width ),
						  static_cast<uint32_t>( vp.height ) );

	// Absolute seconds since an arbitrary origin, not a delta. Be::Time counts 100ns ticks.
	m_viewApi->update( m_viewApi->header.self, static_cast<double>( realTime ) / 10000000.0 );

	// Everything below is ordered as the SDK requires: the render tree is only safe to read
	// after UpdateRenderTree, and the offscreen phase must finish before the onscreen draw
	// because the onscreen pass samples what it produced.
	m_viewApi->update_render_tree( m_viewApi->header.self, frame );

	// Bracketed unconditionally. RenderOffscreen's return value says whether it drew anything,
	// but the target has to be saved before the call either way, so it is only good for logging.
	renderContext.m_esm.PushViewport();
	renderContext.m_esm.PushRenderTarget();
	const bool pushedDepthStencil = renderContext.m_esm.PushDepthStencilBuffer();

	const bool renderedOffscreen =
		m_viewApi->render_offscreen( m_viewApi->header.self, frame ) != NSI_FALSE;

	if( pushedDepthStencil )
	{
		renderContext.m_esm.PopDepthStencilBuffer();
	}
	renderContext.m_esm.PopRenderTarget();
	renderContext.m_esm.PopViewport();

	// PopRenderTarget rebinds the colour target through the AL, which resets the device
	// viewport to the full target. PopViewport restores the esm copy; apply the override
	// again so onscreen Noesis draws into the sprite rect rather than the whole target.
	if( m_hasOverrideViewport )
	{
		renderContext.m_esm.SetViewport( m_overrideWidth, m_overrideHeight, m_overrideX, m_overrideY, 0.0f, 1.0f );
	}

	// SetupViewport clips a rect that extends past the render target. 3D recovers with
	// viewport2projectionAdjustment; Noesis owns its projection, so that clip would
	// squash the UI into the remaining pixels. Put the logical rect on the device
	// instead. BeginOnscreenRender scissors to that rect (clamped to the target and
	// the parent CarbonUI clip) so overflow is clipped rather than scaled, and binds
	// a stencil for ClipToBounds. Restore the esm's clipped copy afterwards so later
	// draws still match what SetupViewport recorded.
	Tr2Viewport logicalVp;
	renderContext.m_esm.GetViewport().ConvertToTr2Viewport( logicalVp );
	renderContext.SetViewport( logicalVp );

	// Only meaningful alongside the override viewport: the overlay path draws into the
	// rect the job bound and has no parent sprite to clip against.
	if( m_hasOverrideViewport && m_hasOverrideClip )
	{
		Tr2ScissorRect clip;
		clip.m_left = m_overrideClipLeft;
		clip.m_top = m_overrideClipTop;
		clip.m_right = m_overrideClipRight;
		clip.m_bottom = m_overrideClipBottom;
		// Host-side state: begin_onscreen_render applies it when the library calls in, so
		// the clip never crosses the ABI.
		host->SetHostScissor( clip );
	}

	// flipY is false because clipSpaceYInverted is false; clear is false because the job has
	// already put something in the target and Noesis composites over it.
	m_viewApi->render( m_viewApi->header.self, frame, NSI_FALSE, NSI_FALSE );

	host->ClearHostScissor();

	renderContext.SetViewport( renderContext.m_esm.GetDeviceViewport() );

	// Noesis leaves its own vertex buffer and program bound, and the state manager's cache no
	// longer describes the device, so hand back something neutral.
	renderContext.SetStreamSource( 0, Tr2BufferAL(), 0, 0 );
	renderContext.SetShaderProgram( Tr2ShaderProgramAL() );

	host->EndFrame();
	renderContext.m_esm.EndManagedRendering();

	if( renderedOffscreen && Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Rendered an offscreen phase; render targets and depth-stencil were restored" );
	}

	return RS_OK;
}

void TriStepRenderNoesis::py__init__( IRoot* view )
{
	SetView( view );
}

void TriStepRenderNoesis::SetView( IRoot* view )
{
	m_view = view;

	// Asks the object whether it is a view rather than assuming. A wrong object leaves
	// the api null and the step simply draws nothing.
	m_viewApi = Nsi::QueryViewApi( view );
	if( view != nullptr && m_viewApi == nullptr )
	{
		CCP_NOESIS_LOGERR( "TriStepRenderNoesis was given an object that is not a Noesis "
						   "view, or one speaking an incompatible nsi ABI; it will render "
						   "nothing" );
	}
}

void TriStepRenderNoesis::SetHost( IRoot* host )
{
	m_host = host;
}

IRoot* TriStepRenderNoesis::GetHost() const
{
	return m_host;
}

Tr2NoesisHost* TriStepRenderNoesis::GetHostObject() const
{
	if( m_host == nullptr )
	{
		return nullptr;
	}

	// Readiness is not checked here: the device is built on the first Execute, so a host
	// that is merely not-yet-built must still be returned.
	Tr2NoesisHostPtr host;
	host = BlueCastPtr( static_cast<IRoot*>( m_host ) );
	return host;
}

IRoot* TriStepRenderNoesis::GetView() const
{
	return m_view;
}

void TriStepRenderNoesis::SetOverrideViewport( int x, int y, int width, int height )
{
	m_hasOverrideViewport = true;
	m_overrideX = x;
	m_overrideY = y;
	m_overrideWidth = width;
	m_overrideHeight = height;
}

void TriStepRenderNoesis::ClearOverrideViewport()
{
	m_hasOverrideViewport = false;
	m_hasOverrideClip = false;
}

void TriStepRenderNoesis::SetOverrideClip( int left, int top, int right, int bottom )
{
	m_hasOverrideClip = true;
	m_overrideClipLeft = left;
	m_overrideClipTop = top;
	m_overrideClipRight = right;
	m_overrideClipBottom = bottom;
}

#endif
