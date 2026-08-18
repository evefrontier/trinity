// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisFileXamlProvider.h"

#if WITH_NOESIS && WITH_NOESIS_STUDIO

#include "Noesis/Tr2NoesisFilePath.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsGui/Stream.h>

Tr2NoesisFileXamlProvider::Tr2NoesisFileXamlProvider( const char* rootPath ) :
	m_rootPath( rootPath != nullptr ? rootPath : "" )
{
}

Noesis::Ptr<Noesis::Stream> Tr2NoesisFileXamlProvider::LoadXaml( const Noesis::Uri& uri )
{
	const std::string filename = Tr2NoesisJoinFilePath( m_rootPath.c_str(), uri );
	Noesis::Ptr<Noesis::Stream> stream = Noesis::OpenFileStream( filename.c_str() );
	if( stream == nullptr )
	{
		CCP_NOESIS_LOGWARN( "FileXamlProvider could not open '%s'", filename.c_str() );
		return nullptr;
	}

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "FileXamlProvider served '%s'", filename.c_str() );
	}

	return stream;
}

#endif
