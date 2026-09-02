// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisView.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisFileFontProvider.h"
#include "Noesis/Tr2NoesisFileTextureProvider.h"
#include "Noesis/Tr2NoesisFileXamlProvider.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsGui/FrameworkElement.h>
#include <NsGui/Studio.h>

#include <string>

namespace Tr2Noesis
{

Tr2NoesisView* LoadStudio( Tr2NoesisView* view, const char* projectPath )
{
	if( view == nullptr )
	{
		CCP_NOESIS_LOGERR( "NoesisLoadStudio was given a null view" );
		return nullptr;
	}

	if( projectPath == nullptr || projectPath[0] == '\0' )
	{
		CCP_NOESIS_LOGERR( "NoesisLoadStudio was given an empty project path" );
		return nullptr;
	}

	if( !RequireInitialized() )
	{
		return nullptr;
	}

	if( !IsStudioAvailable() )
	{
		CCP_NOESIS_LOGERR( "NoesisLoadStudio: NoesisEditor was not loaded" );
		return nullptr;
	}

	Noesis::Studio::Options options;
	options.darkTheme = true;
	options.parentWindow = 0;

	options.GetXamlProvider = []( const char*, const char* path ) -> Noesis::Ptr<Noesis::XamlProvider>
	{
		return Noesis::MakePtr<Tr2NoesisFileXamlProvider>( path );
	};

	options.GetTextureProvider = []( const char*, const char* path ) -> Noesis::Ptr<Noesis::TextureProvider>
	{
		return Noesis::MakePtr<Tr2NoesisFileTextureProvider>( path );
	};

	options.GetFontProvider = []( const char*, const char* path ) -> Noesis::Ptr<Noesis::FontProvider>
	{
		return Noesis::MakePtr<Tr2NoesisFileFontProvider>( path );
	};

	Noesis::Ptr<Noesis::FrameworkElement> studio = Noesis::Studio::Create( projectPath, options );
	if( studio == nullptr )
	{
		CCP_NOESIS_LOGERR( "Studio::Create failed for '%s'", projectPath );
		return nullptr;
	}

	if( !view->SetContent( studio, projectPath ) )
	{
		return nullptr;
	}

	CCP_NOESIS_LOGNOTICE( "Loaded Noesis Studio from '%s'", projectPath );
	return view;
}

}

#endif
