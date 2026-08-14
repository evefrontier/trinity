// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisView_H
#define Tr2NoesisView_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

// --------------------------------------------------------------------------------------
// Description:
//   A NoesisGUI view holding one XAML tree, driven by TriStepRenderNoesis.
//
//   Size is owned by the step, which is the only party that knows the render target's
//   dimensions, so it is deliberately not exposed to Python.
//
//   The renderer is initialised on the step's first Execute rather than here, because
//   IRenderer::Init alters GPU device state and so belongs inside the step's managed
//   rendering bracket. Shutdown runs from the destructor: Trinity has no render thread
//   (TriDevice::OnTick drives everything on the Carbon main thread), so the SDK's
//   "shut down on the thread that initialised" rule is satisfied by construction.
// --------------------------------------------------------------------------------------

BLUE_DECLARE( Tr2NoesisView );

class Tr2NoesisView : public IRoot
{
public:
	EXPOSE_TO_BLUE();
	Tr2NoesisView( IRoot* lockobj = NULL );
	~Tr2NoesisView();

	// Replaces any content already loaded: IView takes its content at construction and has
	// no way to swap it, so a second load rebuilds the view and re-initialises the renderer.
	bool LoadXaml( const char* resPath );
	bool LoadXamlString( const char* xaml );

	bool GetIsLoaded() const;

	// Render-thread entry points. Called by TriStepRenderNoesis, not from Python.
	bool EnsureRenderer();
	void SyncSize( uint32_t width, uint32_t height );
	Noesis::IView* GetNoesisView();

private:
	bool SetContent( Noesis::Ptr<Noesis::FrameworkElement> content, const char* source );
	void ReleaseView();

	Noesis::Ptr<Noesis::IView> m_view;
	bool m_rendererInitialized;
	uint32_t m_width;
	uint32_t m_height;
};

TYPEDEF_BLUECLASS( Tr2NoesisView );

#endif

#endif
