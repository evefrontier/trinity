// Copyright © 2026 CCP ehf.

#pragma once
#ifndef TriStepRenderNoesis_H
#define TriStepRenderNoesis_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisView.h"
#include "RenderJob/TriRenderStep.h"

// --------------------------------------------------------------------------------------
// Description:
//   Renders one Tr2NoesisView into whatever render target the job has bound.
//
//   Size follows the current viewport. A top-level overlay therefore fills the target
//   the job already bound; Tr2Sprite2dNoesis calls SetOverrideViewport so a tree node
//   occupies its sprite rect instead.
//
//   The whole per-frame sequence lives here -- size, Update, UpdateRenderTree, the
//   offscreen phase and the onscreen draw -- because Noesis requires that exact order
//   and a Python-driven equivalent would be able to get it wrong.
// --------------------------------------------------------------------------------------

BLUE_CLASS( TriStepRenderNoesis ) :
	public TriRenderStep
{
public:
	EXPOSE_TO_BLUE();
	TriStepRenderNoesis( IRoot* lockobj = NULL );

	// IRenderStep
	TriStepResult Execute( Be::Time realTime, Be::Time simTime, Tr2RenderContext& renderContext );

	void py__init__( Tr2NoesisView* view );

	void SetView( Tr2NoesisView* view );
	Tr2NoesisView* GetView() const;

	// When set, Execute sizes the view to this rect and draws into it. Tr2Sprite2dNoesis
	// updates it from the sprite's layout each gather. The overlay path leaves it cleared
	// so the step follows the viewport the job already bound.
	void SetOverrideViewport( int x, int y, int width, int height );
	void ClearOverrideViewport();

private:
	Tr2NoesisViewPtr m_view;
	bool m_hasOverrideViewport;
	int m_overrideX;
	int m_overrideY;
	int m_overrideWidth;
	int m_overrideHeight;
};

TYPEDEF_BLUECLASS( TriStepRenderNoesis );

#endif

#endif
