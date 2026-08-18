// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisFilePath_H
#define Tr2NoesisFilePath_H

#if WITH_NOESIS && WITH_NOESIS_STUDIO

#include <NsCore/String.h>
#include <NsCore/StringUtils.h>
#include <NsGui/Uri.h>

#include <string>

// --------------------------------------------------------------------------------------
// Description:
//   Joins a Studio assembly root (a filesystem folder) with a Noesis Uri path. Studio
//   hands providers the assembly directory and then asks for Uris relative to it.
// --------------------------------------------------------------------------------------
inline std::string Tr2NoesisJoinFilePath( const char* root, const Noesis::Uri& uri )
{
	Noesis::FixedString<512> path;
	uri.GetPath( path );

	if( root == nullptr || root[0] == '\0' )
	{
		return path.Str();
	}

	std::string result = root;
	if( !result.empty() && result.back() != '/' && result.back() != '\\' )
	{
		result += '/';
	}

	const char* p = path.Str();
	if( p[0] == '/' || p[0] == '\\' )
	{
		++p;
	}
	result += p;
	return result;
}

#endif

#endif
