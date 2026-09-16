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

	const nhi_view_api* api = nullptr;
	if( capsule != Py_None )
	{
		void* pointer = PyCapsule_GetPointer( capsule, NHI_CAPSULE_VIEW_API );
		if( pointer == nullptr )
		{
			PyErr_SetString( PyExc_TypeError,
							 "expected a " NHI_CAPSULE_VIEW_API " capsule, or None" );
			return nullptr;
		}
		api = static_cast<const nhi_view_api*>( pointer );
		if( nhi_interface_usable( &api->header ) == NHI_FALSE )
		{
			PyErr_SetString( PyExc_ValueError,
							 "the view speaks an nhi ABI this Trinity cannot" );
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
			"The view to render, as the capsule its get_nhi_interface() returns. None\n"
			"clears it and the sprite draws nothing.\n"
			":param view: an " NHI_CAPSULE_VIEW_API " capsule, or None\n"
			":rtype: None" )

		MAP_ATTRIBUTE(
			"host",
			m_host,
			"The Tr2NoesisHost to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( Tr2SpriteObjectBase )
}
