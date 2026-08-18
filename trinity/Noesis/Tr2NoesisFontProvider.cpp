// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisFontProvider.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisResFile.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsCore/String.h>
#include <NsCore/StringUtils.h>
#include <NsGui/Uri.h>

#include <set>
#include <string>

namespace
{

bool IsResRoot( const std::string& path )
{
	return path == "res:" || path == "res:/" || path == "res://";
}

// Maps a Noesis font-folder Uri onto a Trinity resource path. Studio and in-engine XAML
// omit the 'res:' scheme; '/ui/fonts' and 'ui/fonts' both become 'res:/ui/fonts'. An
// already-absolute res: Uri is left alone. Empty (the "system folder") becomes empty.
std::string ToResPath( const Noesis::Uri& folder )
{
	const char* str = folder.Str();
	if( Noesis::StrIsNullOrEmpty( str ) )
	{
		return {};
	}

	Noesis::FixedString<32> scheme;
	folder.GetScheme( scheme );
	if( Noesis::StrCaseEquals( scheme.Str(), "res" ) )
	{
		return str;
	}

	Noesis::FixedString<512> path;
	folder.GetPath( path );
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

std::string JoinResPath( const std::string& folder, const char* filename )
{
	std::string path = folder;
	if( path.empty() )
	{
		return filename;
	}
	if( path.back() != '/' )
	{
		path += '/';
	}
	path += filename;
	return path;
}

bool IsFontFilename( const char* name )
{
	return Noesis::StrCaseEndsWith( name, ".ttf" ) ||
		   Noesis::StrCaseEndsWith( name, ".otf" ) ||
		   Noesis::StrCaseEndsWith( name, ".ttc" );
}

}

void Tr2NoesisFontProvider::ScanFolder( const Noesis::Uri& folder )
{
	const std::string resPath = ToResPath( folder );
	if( resPath.empty() || IsResRoot( resPath ) )
	{
		// Empty folder is the "system font" path (FontFamily="Arial"). We do not scan
		// Windows\Fonts, and we do not list the whole of res:/.
		return;
	}

	std::set<std::wstring> entries;
	const std::wstring resPathW( static_cast<const wchar_t*>( CA2W( resPath.c_str() ) ) );
	BePaths->GetDirectoryContents( resPathW.c_str(), entries );

	uint32_t registered = 0;
	for( const std::wstring& entry : entries )
	{
		const std::string raw( static_cast<const char*>( CW2A( entry.c_str() ) ) );
		const size_t slash = raw.find_last_of( "/\\" );
		const std::string name = slash == std::string::npos ? raw : raw.substr( slash + 1 );
		if( !IsFontFilename( name.c_str() ) )
		{
			continue;
		}

		// Register against the Noesis folder Uri, not the res:/ rewrite. MatchFont looks
		// up the same Uri it passed in.
		RegisterFont( folder, name.c_str() );
		++registered;
	}

	if( registered == 0 )
	{
		CCP_NOESIS_LOGWARN( "FontProvider ScanFolder found no .ttf/.otf/.ttc in '%s' (Noesis folder '%s')",
							resPath.c_str(), folder.Str() );
		return;
	}

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "FontProvider ScanFolder registered %u font file(s) from '%s'",
						registered, resPath.c_str() );
	}
}

Noesis::Ptr<Noesis::Stream> Tr2NoesisFontProvider::OpenFont( const Noesis::Uri& folder,
															const char* filename ) const
{
	const std::string resPath = JoinResPath( ToResPath( folder ), filename );
	return Tr2Noesis::OpenResFile( resPath.c_str(), "FontProvider" );
}

#endif
