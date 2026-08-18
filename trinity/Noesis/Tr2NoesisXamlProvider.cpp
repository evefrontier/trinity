// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisXamlProvider.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisResFile.h"

#include <NsGui/Uri.h>

Noesis::Ptr<Noesis::Stream> Tr2NoesisXamlProvider::LoadXaml( const Noesis::Uri& uri )
{
	return Tr2Noesis::OpenResFile( uri.Str(), "XamlProvider" );
}

#endif
