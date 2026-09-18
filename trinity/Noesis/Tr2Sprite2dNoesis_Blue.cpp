// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"
#include "Noesis/Tr2NoesisNxtInterface.h"

BLUE_DEFINE( Tr2Sprite2dNoesis );

const Be::ClassInfo* Tr2Sprite2dNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2Sprite2dNoesis, "A sprite-tree node that renders a Noesis view at this z-order." )
		MAP_INTERFACE( ITr2SpriteObject )
		MAP_INTERFACE( Tr2Sprite2dNoesis )

		MAP_METHOD(
			"set_view",
			Tr2NoesisPySetView<Tr2Sprite2dNoesis>,
			"The view to render. None clears it and the sprite draws nothing.\n"
			"\n"
			"Raises TypeError if the object is not a view, and ValueError if it speaks\n"
			"an nxt ABI this Trinity cannot.\n"
			":param view: a noesis.View, or None\n"
			":rtype: None" )

		MAP_ATTRIBUTE(
			"render_device",
			m_renderDevice,
			"The Tr2NoesisRenderDevice to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( Tr2SpriteObjectBase )
}
