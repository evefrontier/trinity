// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisView.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

BLUE_DEFINE( Tr2NoesisView );

const Be::ClassInfo* Tr2NoesisView::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisView, "A NoesisGUI view holding one XAML tree. Render it with a TriStepRenderNoesis step." )
		MAP_INTERFACE( Tr2NoesisView )

		MAP_METHOD_AND_WRAP(
			"LoadXaml",
			LoadXaml,
			"Loads XAML from a Trinity resource path, replacing any content already loaded.\n"
			"Returns False and logs the path if the resource is missing or does not parse.\n"
			":param resPath: full resource path, for example 'res:/UI/Noesis/Test.xaml'\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"LoadXamlString",
			LoadXamlString,
			"Parses XAML from a string, replacing any content already loaded. Needs no resource\n"
			"provider, so it separates a render problem from a resource problem.\n"
			":param xaml: XAML markup\n"
			":rtype: bool" )

		MAP_PROPERTY_READONLY(
			"isLoaded",
			GetIsLoaded,
			"True once XAML has been loaded successfully." )

	EXPOSURE_END()
}

#endif
