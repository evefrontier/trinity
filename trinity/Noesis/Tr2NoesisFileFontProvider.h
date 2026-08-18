// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisFileFontProvider_H
#define Tr2NoesisFileFontProvider_H

#if WITH_NOESIS && WITH_NOESIS_STUDIO

#include <NsGui/CachedFontProvider.h>

#include <string>

// --------------------------------------------------------------------------------------
// Description:
//   Serves fonts from a filesystem folder. Studio's Options.GetFontProvider callback
//   constructs one of these per assembly, rooted at the assembly path.
// --------------------------------------------------------------------------------------
class Tr2NoesisFileFontProvider : public Noesis::CachedFontProvider
{
public:
	explicit Tr2NoesisFileFontProvider( const char* rootPath );

protected:
	void ScanFolder( const Noesis::Uri& folder ) override;
	Noesis::Ptr<Noesis::Stream> OpenFont( const Noesis::Uri& folder, const char* filename ) const override;

private:
	void ScanFolder( const std::string& directory, const Noesis::Uri& folder, const char* extension );

	std::string m_rootPath;
};

#endif

#endif
