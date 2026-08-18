// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisResFile_H
#define Tr2NoesisResFile_H

#if WITH_NOESIS

#include <NsCore/Ptr.h>
#include <NsGui/Stream.h>

// --------------------------------------------------------------------------------------
// Description:
//   Opens a Trinity resource path through the Blue ResFile class and returns a Stream
//   that owns the bytes. Noesis::MemoryStream does not own its buffer, and the SDK
//   asks for GetMemoryBase() especially when reading fonts.
//
//   ResFile is synchronous, which is what the provider interfaces demand. A cold disk
//   read therefore stalls the calling frame (gap G6).
// --------------------------------------------------------------------------------------
namespace Tr2Noesis
{

// Null when the resource cannot be opened; Noesis treats that as "not found".
// providerName is the prefix on every log line ('XamlProvider', 'FontProvider').
Noesis::Ptr<Noesis::Stream> OpenResFile( const char* path, const char* providerName );

}

#endif

#endif
