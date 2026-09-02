// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisFileFontProvider.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisFilePath.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsGui/Stream.h>
#include <NsGui/Uri.h>

#ifndef _WIN32
#include <dirent.h>
#include <cstring>
#include <strings.h>
#endif

Tr2NoesisFileFontProvider::Tr2NoesisFileFontProvider( const char* rootPath ) :
	m_rootPath( rootPath != nullptr ? rootPath : "" )
{
}

void Tr2NoesisFileFontProvider::ScanFolder( const Noesis::Uri& folder )
{
	const std::string directory = Tr2NoesisJoinFilePath( m_rootPath.c_str(), folder );
	ScanFolder( directory, folder, ".ttf" );
	ScanFolder( directory, folder, ".otf" );
	ScanFolder( directory, folder, ".ttc" );
}

void Tr2NoesisFileFontProvider::ScanFolder( const std::string& directory, const Noesis::Uri& folder,
											const char* extension )
{
	uint32_t registered = 0;

#ifdef _WIN32
	std::string pattern = directory;
	if( !pattern.empty() && pattern.back() != '/' && pattern.back() != '\\' )
	{
		pattern += '/';
	}
	pattern += '*';
	pattern += extension;

	const std::wstring patternW( static_cast<const wchar_t*>( CA2W( pattern.c_str() ) ) );
	WIN32_FIND_DATAW findData;
	const HANDLE handle = FindFirstFileW( patternW.c_str(), &findData );
	if( handle == INVALID_HANDLE_VALUE )
	{
		return;
	}

	do
	{
		if( ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
		{
			continue;
		}

		const std::string name( static_cast<const char*>( CW2A( findData.cFileName ) ) );
		RegisterFont( folder, name.c_str() );
		++registered;
	}
	while( FindNextFileW( handle, &findData ) );

	FindClose( handle );
#else
	DIR* dir = opendir( directory.c_str() );
	if( dir == nullptr )
	{
		return;
	}

	const size_t extensionLen = std::strlen( extension );
	while( dirent* entry = readdir( dir ) )
	{
		if( entry->d_name[0] == '.' )
		{
			continue;
		}
		const size_t nameLen = std::strlen( entry->d_name );
		if( nameLen < extensionLen )
		{
			continue;
		}
		if( strcasecmp( entry->d_name + nameLen - extensionLen, extension ) != 0 )
		{
			continue;
		}
		RegisterFont( folder, entry->d_name );
		++registered;
	}
	closedir( dir );
#endif

	if( registered > 0 && Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "FileFontProvider registered %u '%s' font file(s) from '%s'",
						registered, extension, directory.c_str() );
	}
}

Noesis::Ptr<Noesis::Stream> Tr2NoesisFileFontProvider::OpenFont( const Noesis::Uri& folder,
																const char* filename ) const
{
	std::string path = Tr2NoesisJoinFilePath( m_rootPath.c_str(), folder );
	if( !path.empty() && path.back() != '/' && path.back() != '\\' )
	{
		path += '/';
	}
	path += filename != nullptr ? filename : "";

	Noesis::Ptr<Noesis::Stream> stream = Noesis::OpenFileStream( path.c_str() );
	if( stream == nullptr )
	{
		CCP_NOESIS_LOGWARN( "FileFontProvider could not open '%s'", path.c_str() );
	}
	return stream;
}

#endif
