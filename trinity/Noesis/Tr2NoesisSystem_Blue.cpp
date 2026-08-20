// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisSystem.h"

#if WITH_NOESIS

static void NoesisInitialize()
{
	Tr2Noesis::EnsureInitialized();
}

MAP_FUNCTION_AND_WRAP( "NoesisInitialize",
					   NoesisInitialize,
					   "Initialises NoesisGUI, installing Carbon's log, assert, error and memory handlers first.\n"
					   "Does nothing if NoesisGUI is already initialised. NoesisGUI has no error channel of its\n"
					   "own during initialisation, so watch the Noesis log channel for the outcome.\n"
					   ":rtype: None" );

static bool NoesisIsInitialized()
{
	return Tr2Noesis::IsInitialized();
}

MAP_FUNCTION_AND_WRAP( "NoesisIsInitialized",
					   NoesisIsInitialized,
					   "Returns True if NoesisGUI has been initialised.\n"
					   ":rtype: bool" );

static const char* NoesisGetVersion()
{
	return Tr2Noesis::GetVersion();
}

MAP_FUNCTION_AND_WRAP( "NoesisGetVersion",
					   NoesisGetVersion,
					   "Returns the build version reported by Noesis.dll, initialising NoesisGUI if needed.\n"
					   ":rtype: str" );

static bool NoesisStudioIsAvailable()
{
	return Tr2Noesis::IsStudioAvailable();
}

MAP_FUNCTION_AND_WRAP( "NoesisStudioIsAvailable",
					   NoesisStudioIsAvailable,
					   "Returns True if this binary was built with Noesis Studio embedded "
					   "(WITH_NOESIS_STUDIO on the DX12 target).\n"
					   ":rtype: bool" );

static bool NoesisSetApplicationResources( const char* resPath )
{
	return Tr2Noesis::SetApplicationResources( resPath );
}

MAP_FUNCTION_AND_WRAP(
	"NoesisSetApplicationResources",
	NoesisSetApplicationResources,
	"Loads a ResourceDictionary from a Trinity resource path and installs it as the process-wide\n"
	"ApplicationResources (the Noesis equivalent of Application.Resources). Implicit control\n"
	"styles come from here. Call before LoadXaml so new trees pick up the theme at construction;\n"
	"calling after is allowed and restyles live views. Returns False and leaves the previous\n"
	"dictionary in place if the resource is missing or is not a ResourceDictionary.\n"
	":param resPath: full resource path, for example 'res:/ui/noesis/theme/NoesisTheme.DarkBlue.xaml'\n"
	":rtype: bool" );

#if BLUE_WITH_PYTHON

static bool AppendFontFallback( PyObject* item, std::vector<std::string>& names )
{
	if( !PyUnicode_Check( item ) )
	{
		PyErr_SetString( PyExc_TypeError, "font fallback names must be strings" );
		return false;
	}
	const char* utf8 = PyUnicode_AsUTF8( item );
	if( utf8 == nullptr )
	{
		return false;
	}
	names.emplace_back( utf8 );
	return true;
}

static PyObject* PyNoesisSetFontFallbacks( PyObject* /*self*/, PyObject* args )
{
	PyObject* arg = nullptr;
	if( !PyArg_ParseTuple( args, "O", &arg ) )
	{
		return nullptr;
	}

	std::vector<std::string> names;
	if( PyUnicode_Check( arg ) )
	{
		if( !AppendFontFallback( arg, names ) )
		{
			return nullptr;
		}
	}
	else
	{
		PyObject* seq = PySequence_Fast( arg, "expected a string or a sequence of strings" );
		if( seq == nullptr )
		{
			return nullptr;
		}
		const Py_ssize_t count = PySequence_Fast_GET_SIZE( seq );
		names.reserve( static_cast<size_t>( count ) );
		for( Py_ssize_t i = 0; i < count; ++i )
		{
			if( !AppendFontFallback( PySequence_Fast_GET_ITEM( seq, i ), names ) )
			{
				Py_DECREF( seq );
				return nullptr;
			}
		}
		Py_DECREF( seq );
	}

	if( !Tr2Noesis::SetFontFallbacks( names ) )
	{
		PyErr_SetString( PyExc_ValueError, "font fallback names must be non-empty strings" );
		return nullptr;
	}
	Py_RETURN_TRUE;
}

MAP_FUNCTION(
	"NoesisSetFontFallbacks",
	PyNoesisSetFontFallbacks,
	"Sets the process-wide font fallback list used when a FontFamily is missing glyphs, and as\n"
	"the default family when an element does not specify one. Names are Studio-style FontFamily\n"
	"strings. Call before LoadXaml. An empty sequence clears fallbacks. A single string is\n"
	"accepted as a one-entry list.\n"
	":param familyNames: a string, or a sequence of strings, for example\n"
	"    '/ui/fonts/#ABC Favorit Mono' or ['/ui/fonts/#ABC Favorit Mono', '/ui/fonts/#Arial Unicode MS']\n"
	":rtype: bool" );

static bool ParseFontEnum( PyObject* value, const char* what, const char* const* names, int nameCount,
						   const int* numbers, int numberCount, int* out )
{
	if( value == nullptr || value == Py_None )
	{
		return true;
	}
	if( PyLong_Check( value ) )
	{
		const long n = PyLong_AsLong( value );
		if( n == -1 && PyErr_Occurred() )
		{
			return false;
		}
		for( int i = 0; i < numberCount; ++i )
		{
			if( numbers[i] == static_cast<int>( n ) )
			{
				*out = static_cast<int>( n );
				return true;
			}
		}
		PyErr_Format( PyExc_ValueError, "unknown font %s value %ld", what, n );
		return false;
	}
	if( PyUnicode_Check( value ) )
	{
		const char* utf8 = PyUnicode_AsUTF8( value );
		if( utf8 == nullptr )
		{
			return false;
		}
		for( int i = 0; i < nameCount; ++i )
		{
			if( _stricmp( utf8, names[i] ) == 0 )
			{
				*out = numbers[i];
				return true;
			}
		}
		PyErr_Format( PyExc_ValueError, "unknown font %s name '%s'", what, utf8 );
		return false;
	}
	PyErr_Format( PyExc_TypeError, "font %s must be a string or int", what );
	return false;
}

static PyObject* PyNoesisSetFontDefaultProperties( PyObject* /*self*/, PyObject* args )
{
	float size = 0.0f;
	PyObject* weightObj = nullptr;
	PyObject* stretchObj = nullptr;
	PyObject* styleObj = nullptr;
	if( !PyArg_ParseTuple( args, "f|OOO", &size, &weightObj, &stretchObj, &styleObj ) )
	{
		return nullptr;
	}

	static const char* weightNames[] = {
		"Thin", "ExtraLight", "UltraLight", "Light", "SemiLight", "Normal", "Regular",
		"Medium", "DemiBold", "SemiBold", "Bold", "ExtraBold", "UltraBold", "Black",
		"Heavy", "ExtraBlack", "UltraBlack"
	};
	static const int weightValues[] = {
		100, 200, 200, 300, 350, 400, 400,
		500, 600, 600, 700, 800, 800, 900,
		900, 950, 950
	};
	static const char* stretchNames[] = {
		"UltraCondensed", "ExtraCondensed", "Condensed", "SemiCondensed", "Normal",
		"Medium", "SemiExpanded", "Expanded", "ExtraExpanded", "UltraExpanded"
	};
	static const int stretchValues[] = { 1, 2, 3, 4, 5, 5, 6, 7, 8, 9 };
	static const char* styleNames[] = { "Normal", "Oblique", "Italic" };
	static const int styleValues[] = { 0, 1, 2 };

	int weight = 400;
	int stretch = 5;
	int style = 0;
	if( !ParseFontEnum( weightObj, "weight", weightNames, 17, weightValues, 17, &weight ) ||
		!ParseFontEnum( stretchObj, "stretch", stretchNames, 10, stretchValues, 10, &stretch ) ||
		!ParseFontEnum( styleObj, "style", styleNames, 3, styleValues, 3, &style ) )
	{
		return nullptr;
	}

	if( !Tr2Noesis::SetFontDefaultProperties( size, weight, stretch, style ) )
	{
		PyErr_SetString( PyExc_ValueError, "font default size must be positive" );
		return nullptr;
	}
	Py_RETURN_TRUE;
}

MAP_FUNCTION(
	"NoesisSetFontDefaultProperties",
	PyNoesisSetFontDefaultProperties,
	"Sets the default font size, weight, stretch and style used when an element does not specify\n"
	"them. Call before LoadXaml. Omitted weight, stretch and style default to Normal.\n"
	":param size: font size in pixels, must be positive\n"
	":param weight: optional FontWeight name or int, for example 'Normal' or 400\n"
	":param stretch: optional FontStretch name or int, for example 'Normal' or 5\n"
	":param style: optional FontStyle name or int: 'Normal', 'Oblique', or 'Italic'\n"
	":rtype: bool" );

#endif

#if TRINITY_PLATFORM == TRINITY_DIRECTX12

#include "Noesis/Tr2NoesisView.h"

static Tr2NoesisView* NoesisLoadStudio( Tr2NoesisView* view, const char* projectPath )
{
	return Tr2Noesis::LoadStudio( view, projectPath );
}

MAP_FUNCTION_AND_WRAP( "NoesisLoadStudio",
					   NoesisLoadStudio,
					   "Loads the in-process Noesis Studio editor into the given view from a filesystem\n"
					   ".noesis project path. Returns the same view on success, or None if Studio is not\n"
					   "compiled in, the path is empty, or Studio::Create fails. The parse or load error\n"
					   "is logged on the Noesis channel.\n"
					   ":param view: Tr2NoesisView to fill\n"
					   ":param projectPath: filesystem path to a .noesis project file\n"
					   ":rtype: Tr2NoesisView" );

#endif

#endif
