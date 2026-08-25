// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisFontProvider_H
#define Tr2NoesisFontProvider_H

#if WITH_NOESIS

#include <NsGui/CachedFontProvider.h>

// --------------------------------------------------------------------------------------
// Description:
//   Serves fonts to NoesisGUI out of Trinity's resource tree. Registered globally from
//   Tr2Noesis::Initialize.
//
//   XAML uses Studio-style folder URIs with no 'res:' scheme, for example
//   FontFamily="/ui/fonts/#Arial Unicode MS". ScanFolder and OpenFont prefix 'res:/' so
//   that folder maps onto res:/ui/fonts. An empty folder (FontFamily="Arial") is a miss;
//   there is no Windows-font fallback.
//
//   ScanFolder lists the requested folder through BePaths->GetDirectoryContents, which
//   unions every registered Blue filesystem, then RegisterFont for each .ttf/.otf/.ttc.
//   OpenFont reads the file through ResFile.
// --------------------------------------------------------------------------------------

class Tr2NoesisFontProvider : public Noesis::CachedFontProvider
{
private:
	void ScanFolder( const Noesis::Uri& folder ) override;
	Noesis::Ptr<Noesis::Stream> OpenFont( const Noesis::Uri& folder, const char* filename ) const override;
};

#endif

#endif
