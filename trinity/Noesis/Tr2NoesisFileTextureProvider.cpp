// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisFileTextureProvider.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisFilePath.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisTextureDecode.h"

#include <NsGui/Uri.h>

namespace
{

bool OpenBitmapFile( const std::string& rootPath, const Noesis::Uri& uri, IBlueStreamPtr& stream,
					 std::string& filename )
{
	filename = Tr2NoesisJoinFilePath( rootPath.c_str(), uri );
	if( filename.empty() )
	{
		return false;
	}

	const std::wstring filenameW( static_cast<const wchar_t*>( CA2W( filename.c_str() ) ) );
	if( !BeIsSuccess( BePaths->GetFileContentsWithYield( filenameW.c_str(), &stream ) ) || !stream )
	{
		CCP_NOESIS_LOGWARN( "FileTextureProvider could not open '%s'", filename.c_str() );
		return false;
	}

	return true;
}

}

Tr2NoesisFileTextureProvider::Tr2NoesisFileTextureProvider( const char* rootPath ) :
	m_rootPath( rootPath != nullptr ? rootPath : "" )
{
}

Noesis::TextureInfo Tr2NoesisFileTextureProvider::GetTextureInfo( const Noesis::Uri& uri )
{
	IBlueStreamPtr stream;
	std::string filename;
	if( !OpenBitmapFile( m_rootPath, uri, stream, filename ) )
	{
		return {};
	}

	return Tr2Noesis::GetTextureInfoFromStream( *stream, filename.c_str(), "FileTextureProvider" );
}

Noesis::Ptr<Noesis::Texture> Tr2NoesisFileTextureProvider::LoadTexture( const Noesis::Uri& uri,
																		Noesis::RenderDevice* device )
{
	IBlueStreamPtr stream;
	std::string filename;
	if( !OpenBitmapFile( m_rootPath, uri, stream, filename ) )
	{
		return nullptr;
	}

	return Tr2Noesis::CreateTextureFromStream( *stream, filename.c_str(), "FileTextureProvider",
											   device );
}

#endif
