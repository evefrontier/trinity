// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisSystem_H
#define Tr2NoesisSystem_H

#if WITH_NOESIS

#include <string>
#include <vector>

// --------------------------------------------------------------------------------------
// Description:
//   Host services for NoesisGUI: the log, assert, error and memory callbacks that route
//   the SDK onto Carbon's equivalents, plus the library's one-time initialisation.
//
//   Noesis gets a single lifetime per process. The SDK does not support Init() after
//   Shutdown(), so we never shut it down and let process teardown take the memory.
// --------------------------------------------------------------------------------------
namespace Tr2Noesis
{

// Installs the host-service callbacks and initialises Noesis. Every entry point into the
// SDK must call this first; all calls after the first one do nothing.
void EnsureInitialized();

bool IsInitialized();

// True when /noesisLogVerbose was set at startup. Gates named-channel traces from
// the vendor and our own routine resource-creation logs.
bool IsLogVerbose();

// The version string reported from inside Noesis.dll. Initialises the library.
const char* GetVersion();

// True when this binary was built with WITH_NOESIS_STUDIO on the DX12 target.
bool IsStudioAvailable();

// Loads a ResourceDictionary from a Trinity resource path and installs it as the
// process-wide ApplicationResources. Returns false if the path is empty or the
// XAML is missing / not a ResourceDictionary. Existing resources are left in
// place on failure. RefreshDefaultStyles is called on success so live views
// pick up implicit styles and DynamicResource keys.
bool SetApplicationResources( const char* resPath );

// Replaces the process-wide font fallback list. Names are Studio-style FontFamily
// strings, for example "/ui/fonts/#ABC Favorit Mono". Copied; the caller need not
// keep them alive. An empty list clears fallbacks. A list containing an empty
// name fails and leaves the previous list in place.
bool SetFontFallbacks( const std::vector<std::string>& familyNames );

// Sets the default size, weight, stretch and style used when an element does not
// specify them. size must be positive. Returns false if size is not positive.
bool SetFontDefaultProperties( float size, int weight, int stretch, int style );

}

#endif

#endif
