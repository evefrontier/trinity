// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisFileXamlProvider_H
#define Tr2NoesisFileXamlProvider_H

#if WITH_NOESIS

#include <NsGui/XamlProvider.h>

#include <string>

// --------------------------------------------------------------------------------------
// Description:
//   Serves XAML from a filesystem folder. Studio's Options.GetXamlProvider callback
//   constructs one of these per assembly, rooted at the assembly path.
// --------------------------------------------------------------------------------------
class Tr2NoesisFileXamlProvider : public Noesis::XamlProvider
{
public:
	explicit Tr2NoesisFileXamlProvider( const char* rootPath );

	Noesis::Ptr<Noesis::Stream> LoadXaml( const Noesis::Uri& uri ) override;

private:
	std::string m_rootPath;
};

#endif

#endif
