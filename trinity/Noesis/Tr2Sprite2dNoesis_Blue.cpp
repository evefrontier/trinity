// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"

BLUE_DEFINE( Tr2Sprite2dNoesis );

static PyObject* PySetView( PyObject* self, PyObject* args )
{
	PyObject* capsule = nullptr;
	if( !PyArg_ParseTuple( args, "O", &capsule ) )
	{
		return nullptr;
	}

	const nxt_view* api = nullptr;
	if( capsule != Py_None )
	{
		void* pointer = PyCapsule_GetPointer( capsule, NXT_CAPSULE_VIEW );
		if( pointer == nullptr )
		{
			PyErr_SetString( PyExc_TypeError,
							 "expected a " NXT_CAPSULE_VIEW " capsule, or None" );
			return nullptr;
		}
		api = static_cast<const nxt_view*>( pointer );
		if( nxt_interface_usable( &api->header ) == NXT_FALSE )
		{
			PyErr_SetString( PyExc_ValueError,
							 "the view speaks an nxt ABI this Trinity cannot" );
			return nullptr;
		}
	}

	BluePythonCast<Tr2Sprite2dNoesis*>( self )->SetView( api );
	Py_RETURN_NONE;
}

const Be::ClassInfo* Tr2Sprite2dNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2Sprite2dNoesis, "A sprite-tree node that renders a Tr2NoesisView at this z-order." )
		MAP_INTERFACE( ITr2SpriteObject )
		MAP_INTERFACE( Tr2Sprite2dNoesis )

		MAP_METHOD(
			"set_view",
			PySetView,
			"The view to render, as the capsule its get_nxt_interface() returns. None\n"
			"clears it and the sprite draws nothing.\n"
			":param view: an " NXT_CAPSULE_VIEW " capsule, or None\n"
			":rtype: None" )

		MAP_ATTRIBUTE(
			"host",
			m_host,
			"The Tr2NoesisHost to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( Tr2SpriteObjectBase )
}
