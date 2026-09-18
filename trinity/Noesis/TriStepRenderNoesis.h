// Copyright © 2026 CCP ehf.

#pragma once
#ifndef TriStepRenderNoesis_H
#define TriStepRenderNoesis_H

#include "Noesis/Tr2NoesisRenderDevice.h"
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

	// The view interface, retained. Null clears. Tr2Sprite2dNoesis pushes both of these
	// into the step it owns; Python sets them on the sprite, not here.
	void SetView( const nxt_view* view );
	void SetRenderDevice( Tr2NoesisRenderDevice* host );

	// When set, Execute sizes the view to this rect and draws into it. Tr2Sprite2dNoesis
	// updates it from the sprite's layout each gather. The overlay path leaves it cleared
	// so the step follows the viewport the job already bound. The rect is the layout
	// size even when it extends past the target or a parent clip; onscreen draws use
	// that full rect so the GPU clips overflow instead of scaling into the remaining
	// pixels. SetOverrideClip is the parent CarbonUI clip in render-target pixels; it
	// only applies while the override viewport is set.
	void SetOverrideViewport( int x, int y, int width, int height );
	void SetOverrideClip( int left, int top, int right, int bottom );

	// Back to following the viewport the job bound, and to no extra clip. Without this a
	// step that has served a sprite keeps drawing into that sprite's rect if it is later
	// reused as an overlay.
	void ClearOverrides();

private:
	// The state manager's managed bracket, held for the whole sequence. Scoped for the same
	// reason ScopedFrame is: Execute has several exits, and one that skipped the close
	// would leave the state manager believing a managed pass was still open.
	class ScopedManagedRendering
	{
	public:
		ScopedManagedRendering( Tr2RenderContext& renderContext, Tr2RenderContextEnum::CullMode cullMode );
		~ScopedManagedRendering();

		ScopedManagedRendering( const ScopedManagedRendering& ) = delete;
		ScopedManagedRendering& operator=( const ScopedManagedRendering& ) = delete;

	private:
		Tr2RenderContext& m_renderContext;
	};

	// Retained, so it outlives whatever capsule delivered it. Released when replaced or
	// when the step goes.
	const nxt_view* m_view;

	// Typed, so Blue rejects anything that is not a host at the point of assignment. An
	// IRootPtr here would take any object and resolve to nothing on the first frame that
	// needed it, which shows up as an empty rectangle and no error.
	Tr2NoesisRenderDevicePtr m_renderDevice;

	// Separate types because they are measured differently: a viewport is an origin and a
	// size, a scissor is four edges.
	struct ViewportOverride
	{
		bool set = false;
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	struct ClipOverride
	{
		bool set = false;
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
	};

	ViewportOverride m_viewport;
	ClipOverride m_clip;
};

TYPEDEF_BLUECLASS( TriStepRenderNoesis );

#endif
