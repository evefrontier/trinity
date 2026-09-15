// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2Sprite2dNoesis_h
#define Tr2Sprite2dNoesis_h

#if WITH_NOESIS

#include "Sprite2d/Tr2SpriteObject.h"
#include "Noesis/Tr2NoesisHost.h"

// --------------------------------------------------------------------------------------
// Description:
//   A sprite-tree node that renders a Tr2NoesisView at this z-order, the same way
//   Tr2Sprite2dRenderJob runs a nested job. Host Python puts it in a container's
//   children list; display, layout and picking come from Tr2SpriteObjectBase.
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
	~Tr2Sprite2dNoesis();

	// Opaque: the view belongs to the Noesis module. See TriStepRenderNoesis.
	void SetView( IRoot* view );
	IRoot* GetView() const;

	void SetHost( IRoot* host );
	IRoot* GetHost() const;

	//////////////////////////////////////////////////////////////////////////
	// ITr2SpriteObject
	unsigned int GetVertexCount();
	virtual void GatherSprites( Tr2Sprite2dScene* renderer );
	virtual ITr2SpriteObject* PickPoint( float x, float y, Tr2Sprite2dScene* renderer );

private:
	void EnsureJob();
	bool SyncOverrideViewport( Tr2Sprite2dScene* renderer );

	IRootPtr m_view;
	IRootPtr m_host;
	TriStepRenderNoesisPtr m_step;
	TriRenderJobPtr m_job;
};

TYPEDEF_BLUECLASS( Tr2Sprite2dNoesis );

#endif

#endif
