// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisSystem_H
#define Tr2NoesisSystem_H

#if WITH_NOESIS

#include <string>
#include <vector>

namespace Noesis { class BaseComponent; }

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

// Installs the host-service callbacks and initialises Noesis. The only caller is
// trinity.NoesisInitialize, which Python's noesis.initialize() invokes after the
// Disable* flags. Subsequent calls do nothing. Do not call this from C++ entry
// points — use RequireInitialized so a missing Python init fails loudly.
void Initialize();

// True when Initialize has run. Logs, asserts, and returns false otherwise.
bool RequireInitialized();

bool IsInitialized();

// SDK Disable* forwards. Must be called before Initialize; no-ops with a warning
// after Init. Do not initialise the library.
void DisableHotReload();
void DisableInspector();
void DisableSocketInit();

// Notifies Noesis that a provider URI's bytes changed. No-op if not initialised
// or the uri is empty.
void RaiseXamlChanged( const char* uri );
void RaiseTextureChanged( const char* uri );

// True when /noesisLogVerbose was set at startup. Gates named-channel traces from
// the vendor and our own routine resource-creation logs.
bool IsLogVerbose();

// The reflected class name of a loaded object, for errors that have to say what a XAML
// file actually turned out to be. Never null; unreflected and null objects report a
// placeholder rather than failing the log call.
const char* GetTypeName( const Noesis::BaseComponent* component );

// The version string reported from inside Noesis.dll. Requires Initialize first.
const char* GetVersion();

// True when Initialize successfully loaded NoesisEditor.
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
