// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisSystem.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisFontProvider.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisTextureProvider.h"
#include "Noesis/Tr2NoesisXamlProvider.h"

#include <NoesisLicense.h>
#include <NsCore/Error.h>
#include <NsCore/Init.h>
#include <NsCore/Log.h>
#include <NsCore/Memory.h>
#include <NsCore/Version.h>
#include <NsGui/FontProperties.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/ResourceDictionary.h>
#include <NsGui/Uri.h>

#include <string>
#include <vector>

CCP_STATS_DECLARE( noesisMem, "Trinity/NoesisMemory", false, CST_MEMORY, "Memory used by NoesisGUI" );

namespace
{

// Only ever written by Initialize, which Blue calls from the Python thread.
bool s_initialized = false;

// Noesis's own launcher drops everything below warning that came from a named channel, on the
// grounds that per-channel traces are overwhelming. Lifted with the /noesisLogVerbose startup
// argument (Src/Packages/App/Launcher/Src/Launcher.cpp:218).
bool s_logVerbose = false;

// Held for the process lifetime, like everything else Noesis owns. Registered after Init,
// which is when providers may be installed.
Noesis::Ptr<Tr2NoesisXamlProvider> s_xamlProvider;
Noesis::Ptr<Tr2NoesisFontProvider> s_fontProvider;
Noesis::Ptr<Tr2NoesisTextureProvider> s_textureProvider;

// Owned copies for SetFontFallbacks: Noesis keeps the char* pointers it is given.
std::vector<std::string> s_fontFallbackNames;
std::vector<const char*> s_fontFallbackPtrs;

void NoesisLogHandler( const char* /*file*/, uint32_t /*line*/, uint32_t level, const char* channel, const char* message )
{
	const bool named = channel != nullptr && channel[0] != '\0';

	if( !s_logVerbose && named && level < NS_LOG_LEVEL_WARNING )
	{
		return;
	}

	// Carbon log channels are compile-time statics, so all of Noesis shares one and its own
	// channel name has to ride in the message text instead.
	const char* open = named ? "[" : "";
	const char* name = named ? channel : "";
	const char* close = named ? "] " : "";

	switch( level )
	{
	case NS_LOG_LEVEL_ERROR:
		CCP_NOESIS_LOGERR( "%s%s%s%s", open, name, close, message );
		break;

	case NS_LOG_LEVEL_WARNING:
		CCP_NOESIS_LOGWARN( "%s%s%s%s", open, name, close, message );
		break;

	default:
		// Carbon has nothing below info, so trace, debug and info all land here.
		CCP_NOESIS_LOG( "%s%s%s%s", open, name, close, message );
		break;
	}
}

bool NoesisAssertHandler( const char* /*file*/, uint32_t /*line*/, const char* expr )
{
	CCP_ASSERT_M( false, expr );

	// The return value asks the caller to break. Carbon has already broken if it wanted to.
	return false;
}

void NoesisErrorHandler( const char* file, uint32_t line, const char* message, bool fatal )
{
	CCP_NOESIS_LOGERR( "%s(%u): %s", file, line, message );

	if( fatal )
	{
		// NS_FATAL breaks and calls abort() the moment we return, so this is the only chance
		// of getting a Carbon-side report out of it.
		CCP_ASSERT_M( false, message );
	}
}

void* NoesisAlloc( void* /*user*/, Noesis::SizeT size )
{
	void* block = CCP_MALLOC( "Noesis", size );
	if( block != nullptr )
	{
		CCP_STATS_ADD( noesisMem, CCP_MSIZE( block ) );
	}
	return block;
}

void* NoesisRealloc( void* /*user*/, void* ptr, Noesis::SizeT size )
{
	if( ptr != nullptr )
	{
		CCP_STATS_ADD( noesisMem, -static_cast<int64_t>( CCP_MSIZE( ptr ) ) );
	}

	void* block = CCP_REALLOC( "Noesis", ptr, size );
	if( block != nullptr )
	{
		CCP_STATS_ADD( noesisMem, CCP_MSIZE( block ) );
	}
	return block;
}

void NoesisDealloc( void* /*user*/, void* ptr )
{
	if( ptr != nullptr )
	{
		CCP_STATS_ADD( noesisMem, -static_cast<int64_t>( CCP_MSIZE( ptr ) ) );
	}
	CCP_FREE( ptr );
}

Noesis::SizeT NoesisAllocSize( void* /*user*/, void* ptr )
{
	return ptr != nullptr ? CCP_MSIZE( ptr ) : 0;
}

}

namespace Tr2Noesis
{

void Initialize()
{
	if( s_initialized )
	{
		return;
	}
	s_initialized = true;

	const auto logVerboseArg = BeOS->GetStartupArgValue( L"noesisLogVerbose" );
	if( !logVerboseArg.empty() )
	{
		s_logVerbose = logVerboseArg != L"0";
	}

	// Every handler has to be in place before Init; see NsCore/Init.h.
	Noesis::SetLogHandler( NoesisLogHandler );
	Noesis::SetAssertHandler( NoesisAssertHandler );
	Noesis::SetErrorHandler( NoesisErrorHandler );

	Noesis::MemoryCallbacks callbacks = {};
	callbacks.user = nullptr;
	callbacks.alloc = NoesisAlloc;
	callbacks.realloc = NoesisRealloc;
	callbacks.dealloc = NoesisDealloc;
	callbacks.allocSize = NoesisAllocSize;
	// dumpLeaks only ever runs from a Shutdown we never call.
	callbacks.dumpLeaks = nullptr;
	Noesis::SetMemoryCallbacks( callbacks );

	// Both default to "" in NoesisLicense.h and are overridden by the NOESIS_LICENSE_NAME and
	// NOESIS_LICENSE_KEY CMake cache variables, so no key ever lands in the source tree.
	Noesis::SetLicense( NS_LICENSE_NAME, NS_LICENSE_KEY );

	// DisableHotReload / DisableInspector / DisableSocketInit are Python's job, via
	// noesis.initialize(), and must already have been applied. Calling them here would
	// override that policy.

	Noesis::Init();

	// Providers go in after Init, unlike the handlers above. One global provider rather than a
	// scheme-scoped one: a XAML file's merged dictionaries arrive as Uris combined against the
	// parent's, and a provider bound to the 'res' scheme would never be asked for those. Font
	// folders and textures are the same global provider, with 'res:/' prefixed onto the
	// Studio-style path.
	s_xamlProvider = Noesis::MakePtr<Tr2NoesisXamlProvider>();
	Noesis::GUI::SetXamlProvider( s_xamlProvider );

	s_fontProvider = Noesis::MakePtr<Tr2NoesisFontProvider>();
	Noesis::GUI::SetFontProvider( s_fontProvider );

	s_textureProvider = Noesis::MakePtr<Tr2NoesisTextureProvider>();
	Noesis::GUI::SetTextureProvider( s_textureProvider );

	CCP_NOESIS_LOGNOTICE( "NoesisGUI %s initialised, %u allocations through Carbon's allocator",
						  Noesis::GetBuildVersion(),
						  Noesis::GetAllocationsCount() );

	if( NS_LICENSE_NAME[0] == '\0' )
	{
		CCP_NOESIS_LOGWARN( "NoesisGUI is running unlicensed. Evaluation builds stop working ten "
							"minutes after initialisation; a UI that dies mid-session is the "
							"licence expiring, not a bug. Set NOESIS_LICENSE_NAME and "
							"NOESIS_LICENSE_KEY at CMake configure time to lift the limit." );
	}
}

bool RequireInitialized()
{
	if( s_initialized )
	{
		return true;
	}
	CCP_NOESIS_LOGERR( "NoesisGUI is not initialised; Python must call noesis.initialize() first" );
	CCP_ASSERT_M( false, "NoesisGUI is not initialised; Python must call noesis.initialize() first" );
	return false;
}

bool IsInitialized()
{
	return s_initialized;
}

void DisableHotReload()
{
	if( s_initialized )
	{
		CCP_NOESIS_LOGWARN( "NoesisDisableHotReload is a no-op after NoesisGUI has been initialised" );
		return;
	}
	Noesis::GUI::DisableHotReload();
}

void DisableInspector()
{
	if( s_initialized )
	{
		CCP_NOESIS_LOGWARN( "NoesisDisableInspector is a no-op after NoesisGUI has been initialised" );
		return;
	}
	Noesis::GUI::DisableInspector();
}

void DisableSocketInit()
{
	if( s_initialized )
	{
		CCP_NOESIS_LOGWARN( "NoesisDisableSocketInit is a no-op after NoesisGUI has been initialised" );
		return;
	}
	Noesis::GUI::DisableSocketInit();
}

void RaiseXamlChanged( const char* uri )
{
	if( !RequireInitialized() )
	{
		return;
	}
	if( uri == nullptr || uri[0] == '\0' )
	{
		CCP_NOESIS_LOGERR( "NoesisRaiseXamlChanged was given an empty uri" );
		return;
	}
	if( s_xamlProvider != nullptr )
	{
		s_xamlProvider->RaiseXamlChanged( Noesis::Uri( uri ) );
	}
}

void RaiseTextureChanged( const char* uri )
{
	if( !RequireInitialized() )
	{
		return;
	}
	if( uri == nullptr || uri[0] == '\0' )
	{
		CCP_NOESIS_LOGERR( "NoesisRaiseTextureChanged was given an empty uri" );
		return;
	}
	if( s_textureProvider != nullptr )
	{
		s_textureProvider->RaiseTextureChanged( Noesis::Uri( uri ) );
	}
}

bool IsLogVerbose()
{
	return s_logVerbose;
}

const char* GetVersion()
{
	if( !RequireInitialized() )
	{
		return "";
	}

	return Noesis::GetBuildVersion();
}

bool IsStudioAvailable()
{
#if WITH_NOESIS_STUDIO
	return true;
#else
	return false;
#endif
}

bool SetApplicationResources( const char* resPath )
{
	if( !RequireInitialized() )
	{
		return false;
	}

	if( resPath == nullptr || resPath[0] == '\0' )
	{
		CCP_NOESIS_LOGERR( "SetApplicationResources was given an empty path" );
		return false;
	}

	const Noesis::Uri uri( resPath );
	Noesis::Ptr<Noesis::ResourceDictionary> resources =
		Noesis::GUI::LoadXaml<Noesis::ResourceDictionary>( uri );
	if( resources == nullptr )
	{
		CCP_NOESIS_LOGERR( "SetApplicationResources failed for '%s'. Either the resource is missing "
						   "or the XAML did not parse into a ResourceDictionary; the parse error is "
						   "logged above.",
						   resPath );
		return false;
	}

	Noesis::GUI::SetApplicationResources( resources );
	Noesis::GUI::RefreshDefaultStyles();
	CCP_NOESIS_LOGNOTICE( "Set application resources from '%s'", resPath );
	return true;
}

bool SetFontFallbacks( const std::vector<std::string>& familyNames )
{
	if( !RequireInitialized() )
	{
		return false;
	}

	for( const std::string& name : familyNames )
	{
		if( name.empty() )
		{
			CCP_NOESIS_LOGERR( "SetFontFallbacks was given an empty family name" );
			return false;
		}
	}

	// Noesis keeps the char* pointers; own the bytes for the process lifetime.
	s_fontFallbackNames = familyNames;
	s_fontFallbackPtrs.clear();
	s_fontFallbackPtrs.reserve( s_fontFallbackNames.size() );
	for( const std::string& name : s_fontFallbackNames )
	{
		s_fontFallbackPtrs.push_back( name.c_str() );
	}

	Noesis::GUI::SetFontFallbacks(
		s_fontFallbackPtrs.empty() ? nullptr : s_fontFallbackPtrs.data(),
		static_cast<uint32_t>( s_fontFallbackPtrs.size() ) );

	CCP_NOESIS_LOGNOTICE( "Set %u font fallback(s)", static_cast<uint32_t>( s_fontFallbackPtrs.size() ) );
	return true;
}

bool SetFontDefaultProperties( float size, int weight, int stretch, int style )
{
	if( !RequireInitialized() )
	{
		return false;
	}

	if( !( size > 0.0f ) )
	{
		CCP_NOESIS_LOGERR( "SetFontDefaultProperties size must be positive, got %f", size );
		return false;
	}
	if( weight <= 0 || stretch < 1 || stretch > 9 || style < 0 || style > 2 )
	{
		CCP_NOESIS_LOGERR( "SetFontDefaultProperties got out-of-range weight=%d stretch=%d style=%d",
						   weight, stretch, style );
		return false;
	}

	Noesis::GUI::SetFontDefaultProperties(
		size,
		static_cast<Noesis::FontWeight>( weight ),
		static_cast<Noesis::FontStretch>( stretch ),
		static_cast<Noesis::FontStyle>( style ) );

	CCP_NOESIS_LOGNOTICE( "Set font defaults size=%g weight=%d stretch=%d style=%d",
						  size, weight, stretch, style );
	return true;
}

}

#endif
