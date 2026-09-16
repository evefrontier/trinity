// Copyright © 2026 CCP ehf.

#pragma once
#ifndef TriStepRenderNoesis_H
#define TriStepRenderNoesis_H

#include "Noesis/Tr2NoesisHost.h"
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
	~TriStepRenderNoesis();

	// IRenderStep
	TriStepResult Execute( Be::Time realTime, Be::Time simTime, Tr2RenderContext& renderContext );

	// The view lives in the Noesis module, whose C++ types Trinity cannot name. It is
	// held as the Blue object it is, with the library's handle beside it; nhi.h says the
	// handle borrows, so the IRootPtr is what keeps it alive.

	// The view interface, retained. Null clears. Tr2Sprite2dNoesis pushes both of these
	// into the step it owns; Python sets them on the sprite, not here.
	void SetView( const nhi_view_api* view );
	void SetHost( IRoot* host );

	// When set, Execute sizes the view to this rect and draws into it. Tr2Sprite2dNoesis
	// updates it from the sprite's layout each gather. The overlay path leaves it cleared
	// so the step follows the viewport the job already bound. The rect is the layout
	// size even when it extends past the target or a parent clip; onscreen draws use
	// that full rect so the GPU clips overflow instead of scaling into the remaining
	// pixels. SetOverrideClip is the parent CarbonUI clip in render-target pixels; it
	// only applies while the override viewport is set.
	void SetOverrideViewport( int x, int y, int width, int height );
	void SetOverrideClip( int left, int top, int right, int bottom );

private:
	// Null only when no host is set; a host whose device is not built yet still counts,
	// because Execute is what builds it. Resolved per frame rather than cached, because
	// Python may rewire or rebuild the host between frames.
	Tr2NoesisHost* GetHostObject() const;

	// Retained, so it outlives whatever capsule delivered it. Released when replaced or
	// when the step goes. There is no second field to fall out of step with it.
	const nhi_view_api* m_viewApi;
	IRootPtr m_host;
	bool m_hasOverrideViewport;
	int m_overrideX;
	int m_overrideY;
	int m_overrideWidth;
	int m_overrideHeight;
	bool m_hasOverrideClip;
	int m_overrideClipLeft;
	int m_overrideClipTop;
	int m_overrideClipRight;
	int m_overrideClipBottom;
};

TYPEDEF_BLUECLASS( TriStepRenderNoesis );

#endif
