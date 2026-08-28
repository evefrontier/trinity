// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisTypes_H
#define Tr2NoesisTypes_H

#if WITH_NOESIS

enum class Tr2NoesisPropertyType
{
	Unknown,
	Bool,
	Integer,
	Float,
	String,
	Object,
	Command,
	Collection
};

Tr2NoesisPropertyType Tr2NoesisParsePropertyType( const char* name );
const char* Tr2NoesisPropertyTypeName( Tr2NoesisPropertyType type );

#endif

#endif
