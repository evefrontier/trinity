// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisSystem_H
#define Tr2NoesisSystem_H

#if WITH_NOESIS

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

// The version string reported from inside Noesis.dll. Initialises the library.
const char* GetVersion();

}

#endif

#endif
