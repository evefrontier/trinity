// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisView.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisRenderDevice.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsGui/FrameworkElement.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/Uri.h>

Tr2NoesisView::Tr2NoesisView( IRoot* ) :
	m_rendererInitialized( false ),
	m_width( 0 ),
	m_height( 0 )
{
}

Tr2NoesisView::~Tr2NoesisView()
{
	ReleaseView();
}

bool Tr2NoesisView::LoadXaml( const char* resPath )
{
	if( resPath == nullptr || resPath[0] == '\0' )
	{
		CCP_NOESIS_LOGERR( "LoadXaml was given an empty path" );
		return false;
	}

	Tr2Noesis::EnsureInitialized();

	// Goes through Tr2NoesisXamlProvider, which treats the Uri as a Trinity resource path.
	Noesis::Ptr<Noesis::FrameworkElement> content =
		Noesis::GUI::LoadXaml<Noesis::FrameworkElement>( Noesis::Uri( resPath ) );
	if( content == nullptr )
	{
		CCP_NOESIS_LOGERR( "LoadXaml failed for '%s'. Either the resource is missing or the XAML "
						   "did not parse into a FrameworkElement; the parse error is logged above.",
						   resPath );
		return false;
	}

	return SetContent( content, resPath );
}

bool Tr2NoesisView::LoadXamlString( const char* xaml )
{
	if( xaml == nullptr || xaml[0] == '\0' )
	{
		CCP_NOESIS_LOGERR( "LoadXamlString was given an empty string" );
		return false;
	}

	Tr2Noesis::EnsureInitialized();

	// No provider involved, so this reaches pixels without the resource system in the picture.
	Noesis::Ptr<Noesis::FrameworkElement> content = Noesis::GUI::ParseXaml<Noesis::FrameworkElement>( xaml );
	if( content == nullptr )
	{
		CCP_NOESIS_LOGERR( "LoadXamlString failed to parse the XAML into a FrameworkElement; "
						   "the parse error is logged above" );
		return false;
	}

	return SetContent( content, "<string>" );
}

bool Tr2NoesisView::SetContent( Noesis::Ptr<Noesis::FrameworkElement> content, const char* source )
{
	ReleaseView();

	m_view = Noesis::GUI::CreateView( content );
	if( m_view == nullptr )
	{
		CCP_NOESIS_LOGERR( "CreateView failed for '%s'", source );
		return false;
	}

	// PPAA exercises the COVERAGE semantic rename. LCD enables the SDF_LCD_* shaders and
	// SrcOver_Dual dual-source blending, which is how Noesis does subpixel text on RGB LCDs.
	m_view->SetFlags( Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD );

	if( m_width != 0 && m_height != 0 )
	{
		m_view->SetSize( m_width, m_height );
	}

	CCP_NOESIS_LOGNOTICE( "Loaded XAML from '%s'", source );
	return true;
}

void Tr2NoesisView::ReleaseView()
{
	if( m_view == nullptr )
	{
		return;
	}

	if( m_rendererInitialized )
	{
		// Mandatory before releasing the view, per the SDK guide, and there is no way to ask
		// whether a renderer was initialised, hence the flag.
		m_view->GetRenderer()->Shutdown();
		m_rendererInitialized = false;
	}

	m_view.Reset();
}

bool Tr2NoesisView::GetIsLoaded() const
{
	return m_view != nullptr;
}

bool Tr2NoesisView::EnsureRenderer()
{
	if( m_view == nullptr )
	{
		return false;
	}
	if( m_rendererInitialized )
	{
		return true;
	}

	Tr2NoesisRenderDevice* device = Tr2Noesis::GetRenderDevice();
	if( device == nullptr )
	{
		return false;
	}

	m_view->GetRenderer()->Init( device );
	m_rendererInitialized = true;
	return true;
}

void Tr2NoesisView::SyncSize( uint32_t width, uint32_t height )
{
	if( width == 0 || height == 0 || ( width == m_width && height == m_height ) )
	{
		return;
	}

	m_width = width;
	m_height = height;

	if( m_view != nullptr )
	{
		m_view->SetSize( width, height );
	}
}

Noesis::IView* Tr2NoesisView::GetNoesisView()
{
	return m_view;
}

#if !WITH_NOESIS_STUDIO

namespace Tr2Noesis
{

Tr2NoesisView* LoadStudio( Tr2NoesisView* /*view*/, const char* /*projectPath*/ )
{
	CCP_NOESIS_LOGERR( "NoesisLoadStudio: this build was not configured with WITH_NOESIS_STUDIO" );
	return nullptr;
}

}

#endif

#endif
