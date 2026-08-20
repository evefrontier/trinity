// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisTypes.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include <string.h>

Tr2NoesisPropertyType Tr2NoesisParsePropertyType( const char* name )
{
	if( name == nullptr || name[0] == '\0' )
	{
		return Tr2NoesisPropertyType::Unknown;
	}
	if( _stricmp( name, "bool" ) == 0 )
	{
		return Tr2NoesisPropertyType::Bool;
	}
	if( _stricmp( name, "int" ) == 0 || _stricmp( name, "integer" ) == 0 )
	{
		return Tr2NoesisPropertyType::Integer;
	}
	if( _stricmp( name, "float" ) == 0 || _stricmp( name, "double" ) == 0 )
	{
		return Tr2NoesisPropertyType::Float;
	}
	if( _stricmp( name, "string" ) == 0 || _stricmp( name, "str" ) == 0 )
	{
		return Tr2NoesisPropertyType::String;
	}
	if( _stricmp( name, "object" ) == 0 || _stricmp( name, "model" ) == 0 )
	{
		return Tr2NoesisPropertyType::Object;
	}
	if( _stricmp( name, "command" ) == 0 )
	{
		return Tr2NoesisPropertyType::Command;
	}
	if( _stricmp( name, "collection" ) == 0 || _stricmp( name, "list" ) == 0 )
	{
		return Tr2NoesisPropertyType::Collection;
	}
	return Tr2NoesisPropertyType::Unknown;
}

const char* Tr2NoesisPropertyTypeName( Tr2NoesisPropertyType type )
{
	switch( type )
	{
	case Tr2NoesisPropertyType::Bool:
		return "bool";
	case Tr2NoesisPropertyType::Integer:
		return "int";
	case Tr2NoesisPropertyType::Float:
		return "float";
	case Tr2NoesisPropertyType::String:
		return "string";
	case Tr2NoesisPropertyType::Object:
		return "object";
	case Tr2NoesisPropertyType::Command:
		return "command";
	case Tr2NoesisPropertyType::Collection:
		return "collection";
	default:
		return "unknown";
	}
}

#endif
