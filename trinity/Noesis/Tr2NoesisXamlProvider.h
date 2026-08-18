// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisXamlProvider_H
#define Tr2NoesisXamlProvider_H

#if WITH_NOESIS

#include <NsCore/Ptr.h>
#include <NsGui/XamlProvider.h>

// --------------------------------------------------------------------------------------
// Description:
//   Serves XAML to NoesisGUI out of Trinity's resource system, through the Blue ResFile
//   class. Registered globally from Tr2Noesis::EnsureInitialized.
//
//   Each Uri is treated as a Trinity resource path verbatim, so callers pass full
//   'res:/...' paths. A XAML file's own dependencies arrive already combined against its
//   Uri, which is why no root prefix is configured here.
//
//   ResFile is synchronous, which is what XamlProvider::LoadXaml demands: it must return
//   a readable stream immediately, and BeResMan->GetResource returns before loading
//   completes. A cold disk read therefore stalls the calling frame (gap G6).
//
//   The owning Stream lives in Tr2NoesisResFile, shared with the font provider.
// --------------------------------------------------------------------------------------

class Tr2NoesisXamlProvider : public Noesis::XamlProvider
{
public:
	// Null when the resource cannot be opened; Noesis treats that as "not found".
	Noesis::Ptr<Noesis::Stream> LoadXaml( const Noesis::Uri& uri ) override;
};

#endif

#endif
