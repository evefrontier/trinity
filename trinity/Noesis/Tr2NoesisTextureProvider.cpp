// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisTextureProvider.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisTextureDecode.h"

#include <NsCore/String.h>
#include <NsCore/StringUtils.h>
#include <NsGui/Uri.h>

#include <string>

namespace
{

// Maps a Noesis texture Uri onto a Trinity resource path. Studio and in-engine XAML
// omit the 'res:' scheme; '/ui/images/icon.png' becomes 'res:/ui/images/icon.png'. An
// already-absolute res: Uri is left alone.
std::string ToResPath( const Noesis::Uri& uri )
{
	const char* str = uri.Str();
	if( Noesis::StrIsNullOrEmpty( str ) )
	{
		return {};
	}

	Noesis::FixedString<32> scheme;
	uri.GetScheme( scheme );
	if( Noesis::StrCaseEquals( scheme.Str(), "res" ) )
	{
		return str;
	}

	Noesis::FixedString<512> path;
	uri.GetPath( path );
	const char* p = path.Empty() ? str : path.Str();
	if( Noesis::StrIsNullOrEmpty( p ) )
	{
		return {};
	}

	if( p[0] == '/' )
	{
		return std::string( "res:" ) + p;
	}
	return std::string( "res:/" ) + p;
}

IResFilePtr OpenTextureFile( const Noesis::Uri& uri, std::string& path )
{
	path = ToResPath( uri );
	if( path.empty() )
	{
		return nullptr;
	}

	Be::Clsid resFileClsid( "blue", "ResFile" );
	IResFilePtr file( resFileClsid );
	if( !file->Open( path.c_str(), true ) )
	{
		CCP_NOESIS_LOGWARN( "TextureProvider could not open '%s'", path.c_str() );
		return nullptr;
	}
	return file;
}

}

Noesis::TextureInfo Tr2NoesisTextureProvider::GetTextureInfo( const Noesis::Uri& uri )
{
	std::string path;
	IResFilePtr file = OpenTextureFile( uri, path );
	if( !file )
	{
		return {};
	}
	ON_BLOCK_EXIT( [&] { file->Close(); } );
	return Tr2Noesis::GetTextureInfoFromStream( *file, path.c_str(), "TextureProvider" );
}

Noesis::Ptr<Noesis::Texture> Tr2NoesisTextureProvider::LoadTexture( const Noesis::Uri& uri,
																	Noesis::RenderDevice* device )
{
	std::string path;
	IResFilePtr file = OpenTextureFile( uri, path );
	if( !file )
	{
		return nullptr;
	}
	ON_BLOCK_EXIT( [&] { file->Close(); } );
	return Tr2Noesis::CreateTextureFromStream( *file, path.c_str(), "TextureProvider", device );
}

#endif
