// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2Sprite2dNoesis_H
#define Tr2Sprite2dNoesis_H

#include "Sprite2d/Tr2SpriteObject.h"
#include "Noesis/Tr2NoesisRenderDevice.h"

// --------------------------------------------------------------------------------------
// Description:
//   A sprite-tree node that renders a Tr2NoesisView at this z-order, the same way
//   Tr2Sprite2dRenderJob runs a nested job. Host Python puts it in a container's
//   children list; display and layout come from Tr2SpriteObjectBase. Picking is the
//   base's rectangle test followed by the view's own hit test, so a pick on a part of
//   the view with no hit-testable element falls through to the sprites beneath.
//
//   Internally this owns a TriRenderJob whose only step is TriStepRenderNoesis, so
//   GatherSprites can go through Tr2Sprite2dScene::RunJob (flush, leave sprite
//   managed mode, restore) and display-list capture keeps working. Parent
//   clipChildren is applied as an onscreen scissor; the layout viewport stays the
//   sprite rect so scrolling does not reflow the XAML.
// --------------------------------------------------------------------------------------

BLUE_DECLARE( TriRenderJob );
BLUE_DECLARE( TriStepRenderNoesis );
BLUE_DECLARE( Tr2Sprite2dNoesis );

class Tr2Sprite2dNoesis : public Tr2SpriteObjectBase
{
public:
	EXPOSE_TO_BLUE();

	Tr2Sprite2dNoesis( IRoot* lockobj = NULL );

	// The view interface, retained. None clears it.
	void SetView( const pynr_view* view );
	~Tr2Sprite2dNoesis();

	//////////////////////////////////////////////////////////////////////////
	// ITr2SpriteObject
	unsigned int GetVertexCount();
	virtual void GatherSprites( Tr2Sprite2dScene* renderer );
	virtual ITr2SpriteObject* PickPoint( float x, float y, Tr2Sprite2dScene* renderer );

private:
	void EnsureJob();
	bool SyncOverrideViewport( Tr2Sprite2dScene* renderer );

	// Whether the view claims a point in the sprite's own pixels, origin at its top-left.
	// True without a view, or with one too old to be asked, so the sprite then picks as
	// an opaque rectangle the way every other sprite does.
	bool ViewHasPoint( const Vector2& point ) const;

	// Both are pushed to the step every gather, because the step is created lazily and
	// Python has already set these by the time it exists.
	const pynr_view* m_view;

	// Typed, so Blue rejects anything that is not a host at the point of assignment.
	Tr2NoesisRenderDevicePtr m_renderDevice;
	TriStepRenderNoesisPtr m_step;
	TriRenderJobPtr m_job;
};

TYPEDEF_BLUECLASS( Tr2Sprite2dNoesis );

#endif
