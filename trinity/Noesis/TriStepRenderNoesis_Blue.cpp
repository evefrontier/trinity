// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/TriStepRenderNoesis.h"
#include "Noesis/Tr2NoesisNxtInterface.h"

BLUE_DEFINE( TriStepRenderNoesis );

const Be::ClassInfo* TriStepRenderNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( TriStepRenderNoesis, "" )
		MAP_INTERFACE( TriStepRenderNoesis )
		MAP_INTERFACE( TriRenderStep )

		MAP_METHOD(
			"set_view",
			Tr2NoesisPySetView<TriStepRenderNoesis>,
			"The view to render. None clears it and the step draws nothing.\n"
			"\n"
			"Raises TypeError if the object is not a view, and NoesisAbiMismatchError\n"
			"if it speaks an nxt ABI this Trinity cannot.\n"
			":param view: a noesis.View, or None\n"
			":rtype: None" )

		MAP_ATTRIBUTE(
			"render_device",
			m_renderDevice,
			"The Tr2NoesisRenderDevice to render through",
			Be::READWRITE )

	EXPOSURE_CHAINTO( TriRenderStep )
}
