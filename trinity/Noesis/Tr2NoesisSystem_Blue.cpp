// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisSystem.h"

#if WITH_NOESIS

static void NoesisInitialize()
{
	Tr2Noesis::EnsureInitialized();
}

MAP_FUNCTION_AND_WRAP( "NoesisInitialize",
					   NoesisInitialize,
					   "Initialises NoesisGUI, installing Carbon's log, assert, error and memory handlers first.\n"
					   "Does nothing if NoesisGUI is already initialised. NoesisGUI has no error channel of its\n"
					   "own during initialisation, so watch the Noesis log channel for the outcome.\n"
					   ":rtype: None" );

static bool NoesisIsInitialized()
{
	return Tr2Noesis::IsInitialized();
}

MAP_FUNCTION_AND_WRAP( "NoesisIsInitialized",
					   NoesisIsInitialized,
					   "Returns True if NoesisGUI has been initialised.\n"
					   ":rtype: bool" );

static const char* NoesisGetVersion()
{
	return Tr2Noesis::GetVersion();
}

MAP_FUNCTION_AND_WRAP( "NoesisGetVersion",
					   NoesisGetVersion,
					   "Returns the build version reported by Noesis.dll, initialising NoesisGUI if needed.\n"
					   ":rtype: str" );

static bool NoesisStudioIsAvailable()
{
	return Tr2Noesis::IsStudioAvailable();
}

MAP_FUNCTION_AND_WRAP( "NoesisStudioIsAvailable",
					   NoesisStudioIsAvailable,
					   "Returns True if this binary was built with Noesis Studio embedded "
					   "(WITH_NOESIS_STUDIO on the DX12 target).\n"
					   ":rtype: bool" );

#if TRINITY_PLATFORM == TRINITY_DIRECTX12

#include "Noesis/Tr2NoesisView.h"

static Tr2NoesisView* NoesisLoadStudio( Tr2NoesisView* view, const char* projectPath )
{
	return Tr2Noesis::LoadStudio( view, projectPath );
}

MAP_FUNCTION_AND_WRAP( "NoesisLoadStudio",
					   NoesisLoadStudio,
					   "Loads the in-process Noesis Studio editor into the given view from a filesystem\n"
					   ".noesis project path. Returns the same view on success, or None if Studio is not\n"
					   "compiled in, the path is empty, or Studio::Create fails. The parse or load error\n"
					   "is logged on the Noesis channel.\n"
					   ":param view: Tr2NoesisView to fill\n"
					   ":param projectPath: filesystem path to a .noesis project file\n"
					   ":rtype: Tr2NoesisView" );

#endif

#endif
