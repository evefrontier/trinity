// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/TriStepRenderNoesis.h"
#include "Noesis/Tr2NoesisNxtInterface.h"

BLUE_DEFINE( TriStepRenderNoesis );

static PyObject* PySetView( PyObject* self, PyObject* args )
{
	PyObject* view = nullptr;
	if( !PyArg_ParseTuple( args, "O", &view ) )
	{
		return nullptr;
	}

	void* pointer = nullptr;
	if( !Tr2NoesisTakeNxtInterface( view, NXT_CAPSULE_VIEW,
								   sizeof( nxt_view ), pointer ) )
	{
		return nullptr;
	}

	BluePythonCast<TriStepRenderNoesis*>( self )->SetView( static_cast<const nxt_view*>( pointer ) );
	Py_RETURN_NONE;
}

const Be::ClassInfo* TriStepRenderNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( TriStepRenderNoesis, "" )
		MAP_INTERFACE( TriStepRenderNoesis )
		MAP_INTERFACE( TriRenderStep )

		MAP_METHOD(
			"set_view",
			PySetView,
			"The view to render. None clears it and the step draws nothing.\n"
			":param view: a noesis.View, or None\n"
			":rtype: None" )

		MAP_ATTRIBUTE(
			"render_device",
			m_renderDevice,
			"The Tr2NoesisRenderDevice to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( TriRenderStep )
}
