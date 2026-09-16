// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"

BLUE_DEFINE( Tr2Sprite2dNoesis );

const Be::ClassInfo* Tr2Sprite2dNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2Sprite2dNoesis, "A sprite-tree node that renders a Tr2NoesisView at this z-order." )
		MAP_INTERFACE( ITr2SpriteObject )
		MAP_INTERFACE( Tr2Sprite2dNoesis )

		MAP_ATTRIBUTE(
			"view",
			m_view,
			"The Tr2NoesisView to render",
			Be::READWRITE )

		MAP_ATTRIBUTE(
			"host",
			m_host,
			"The Tr2NoesisHost to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( Tr2SpriteObjectBase )
}
