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

#endif
