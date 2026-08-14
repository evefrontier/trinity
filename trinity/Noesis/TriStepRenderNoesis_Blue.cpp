// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/TriStepRenderNoesis.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

BLUE_DEFINE( TriStepRenderNoesis );

const Be::ClassInfo* TriStepRenderNoesis::ExposeToBlue()
{
	EXPOSURE_BEGIN( TriStepRenderNoesis, "" )
		MAP_INTERFACE( TriStepRenderNoesis )
		MAP_INTERFACE( TriRenderStep )

		MAP_ATTRIBUTE(
			"view",
			m_view,
			"The Tr2NoesisView to render",
			Be::READWRITE )

		MAP_METHOD_AND_WRAP_OPTIONAL_ARGS(
			"__init__",
			py__init__,
			1,
			"Create a step that renders a NoesisGUI view into the target the job has bound.\n"
			":param view: Tr2NoesisView" )

	EXPOSURE_CHAINTO( TriRenderStep )
}

#endif
