// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"
#include "Noesis/Tr2NoesisNxtInterface.h"

BLUE_DEFINE( Tr2Sprite2dNoesis );

static PyObject* PySetView( PyObject* self, PyObject* args )
{
	PyObject* view = nullptr;
	if( !PyArg_ParseTuple( args, "O", &view ) )
	{
		return nullptr;
	}

	void* pointer = nullptr;
	if( !Tr2NoesisTakeNxtInterface( view, NXT_CAPSULE_VIEW, pointer ) )
	{
		return nullptr;
	}

	BluePythonCast<Tr2Sprite2dNoesis*>( self )->SetView( static_cast<const nxt_view*>( pointer ) );
	Py_RETURN_NONE;
}

const Be::ClassInfo* Tr2Sprite2dNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2Sprite2dNoesis, "A sprite-tree node that renders a Noesis view at this z-order." )
		MAP_INTERFACE( ITr2SpriteObject )
		MAP_INTERFACE( Tr2Sprite2dNoesis )

		MAP_METHOD(
			"set_view",
			PySetView,
			"The view to render. None clears it and the sprite draws nothing.\n"
			":param view: a noesis.View, or None\n"
			":rtype: None" )

		MAP_ATTRIBUTE(
			"render_device",
			m_renderDevice,
			"The Tr2NoesisRenderDevice to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( Tr2SpriteObjectBase )
}
