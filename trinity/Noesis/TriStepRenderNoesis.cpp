// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/TriStepRenderNoesis.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisRenderDevice.h"
#include "Noesis/Tr2NoesisSystem.h"
#include "Tr2RenderContext.h"

#include <NsGui/IRenderer.h>

TriStepRenderNoesis::TriStepRenderNoesis( IRoot* lockobj ) :
	TriRenderStep( lockobj )
{
}

TriStepResult TriStepRenderNoesis::Execute( Be::Time realTime, Be::Time /*simTime*/, Tr2RenderContext& renderContext )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_view || !m_view->GetIsLoaded() )
	{
		return RS_OK;
	}

	// Noesis touches render state directly rather than through an effect, so everything below
	// runs inside the managed bracket: it resets the state manager's shadow copy on entry and
	// tells it not to trust its cache afterwards. CULLMODE_NONE matches what Noesis expects,
	// though ApplyRenderState sets it per batch as well.
	renderContext.m_esm.BeginManagedRendering( Tr2RenderContextEnum::CULLMODE_NONE );

	// IRenderer::Init creates GPU resources and is therefore inside the bracket too.
	if( !m_view->EnsureRenderer() )
	{
		renderContext.m_esm.EndManagedRendering();
		return RS_OK;
	}

	// The step owns sizing: it is the only party that knows what the job has bound.
	m_view->SyncSize( renderContext.m_esm.GetRenderTargetWidth(), renderContext.m_esm.GetRenderTargetHeight() );

	Noesis::IView* view = m_view->GetNoesisView();
	Noesis::IRenderer* renderer = view->GetRenderer();

	Tr2NoesisRenderDevice* device = Tr2Noesis::GetRenderDevice();
	device->SetRenderContext( renderContext );

	// Absolute seconds since an arbitrary origin, not a delta. Be::Time counts 100ns ticks.
	view->Update( static_cast<double>( realTime ) / 10000000.0 );

	// Everything below is ordered as the SDK requires: the render tree is only safe to read
	// after UpdateRenderTree, and the offscreen phase must finish before the onscreen draw
	// because the onscreen pass samples what it produced.
	renderer->UpdateRenderTree();

	// Bracketed unconditionally. RenderOffscreen's return value says whether it drew anything,
	// but the target has to be saved before the call either way, so it is only good for logging.
	renderContext.m_esm.PushViewport();
	renderContext.m_esm.PushRenderTarget();
	const bool pushedDepthStencil = renderContext.m_esm.PushDepthStencilBuffer();

	const bool renderedOffscreen = renderer->RenderOffscreen();

	if( pushedDepthStencil )
	{
		renderContext.m_esm.PopDepthStencilBuffer();
	}
	renderContext.m_esm.PopRenderTarget();
	renderContext.m_esm.PopViewport();

	// flipY is false because clipSpaceYInverted is false; clear is false because the job has
	// already put something in the target and Noesis composites over it.
	renderer->Render( false, false );

	// Noesis leaves its own vertex buffer and program bound, and the state manager's cache no
	// longer describes the device, so hand back something neutral.
	renderContext.SetStreamSource( 0, Tr2BufferAL(), 0, 0 );
	renderContext.SetShaderProgram( Tr2ShaderProgramAL() );

	renderContext.m_esm.EndManagedRendering();

	if( renderedOffscreen && Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Rendered an offscreen phase; render targets and depth-stencil were restored" );
	}

	return RS_OK;
}

void TriStepRenderNoesis::py__init__( Tr2NoesisView* view )
{
	m_view = view;
}

#endif
