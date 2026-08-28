// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisView_H
#define Tr2NoesisView_H

#if WITH_NOESIS

#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

namespace Noesis { class Cursor; class FrameworkElement; class RenderDevice; }

BLUE_DECLARE( Tr2NoesisDataModel );

// --------------------------------------------------------------------------------------
// Description:
//   A NoesisGUI view holding one XAML tree, driven by TriStepRenderNoesis
//   (as a job step or via Tr2Sprite2dNoesis in the sprite tree).
//
//   Size is owned by the step, which follows the current viewport, so it is
//   deliberately not exposed to Python. Input is forwarded from host Python
//   (Tr2MainWindow callbacks), not hooked in C++.
//
//   The renderer is initialised on the step's first Execute rather than here, because
//   IRenderer::Init alters GPU device state and so belongs inside the step's managed
//   rendering bracket. The step supplies the RenderDevice; this class does not construct
//   it. Shutdown runs from the destructor: Trinity has no render thread
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

	bool GetLcd() const;
	void SetLcd( bool enable );

	void SetDataContext( Tr2NoesisDataModel* model );
	Tr2NoesisDataModel* GetDataContext() const;

	// C++ only. Used by LoadXaml, LoadXamlString and Tr2Noesis::LoadStudio. Not Blue-mapped.
	bool SetContent( Noesis::Ptr<Noesis::FrameworkElement> content, const char* source );

	const BlueScriptCallback& GetOnCursorChange() const;
	void SetOnCursorChange( const BlueScriptCallback& callback );
	void NotifyCursorChange( Noesis::Cursor* cursor );

	// Render-thread entry points. Called by TriStepRenderNoesis, not from Python.
	// device is the process-wide render device; Init is skipped if it is null or the
	// renderer is already up. The view does not own or construct the device.
	bool EnsureRenderer( Noesis::RenderDevice* device );
	void SyncSize( uint32_t width, uint32_t height );
	Noesis::IView* GetNoesisView();

	// Host Python forwards Tr2MainWindow input here. Coordinates are view-local
	// pixels (origin upper-left). Mouse buttons are 0-4 (left, right, middle,
	// X1, X2). Keys are Win32 virtual-key codes, as onKeyDown already delivers
	// on Windows and Mac. Event methods return whether the UI tree handled the
	// event; an unloaded view, an unmapped key, or a button outside 0-4 returns
	// False without calling Noesis. Activate is not implied by load or by the
	// first key — call it when this view should own the keyboard.
	void Activate();
	void Deactivate();
	void SetEmulateTouch( bool emulate );

	bool MouseButtonDown( int x, int y, int button );
	bool MouseButtonUp( int x, int y, int button );
	bool MouseDoubleClick( int x, int y, int button );
	bool MouseMove( int x, int y );
	bool MouseWheel( int x, int y, int delta );
	bool MouseHWheel( int x, int y, int delta );
	bool Scroll( int x, int y, float value );
	bool HScroll( int x, int y, float value );
	bool TouchDown( int x, int y, uint64_t id );
	bool TouchMove( int x, int y, uint64_t id );
	bool TouchUp( int x, int y, uint64_t id );
	bool KeyDown( int key );
	bool KeyUp( int key );
	bool Char( uint32_t ch );

private:
	void ReleaseView();
	void ApplyDataContext();
	void ApplyLcdFlag();

	Noesis::Ptr<Noesis::IView> m_view;
	Tr2NoesisDataModelPtr m_dataContext;
	BlueScriptCallback m_onCursorChange;
	bool m_rendererInitialized;
	bool m_lcd;
	uint32_t m_width;
	uint32_t m_height;
};

TYPEDEF_BLUECLASS( Tr2NoesisView );

namespace Tr2Noesis
{

// Fills view with the in-process Studio editor for the given .noesis project.
// Returns view on success, nullptr if Studio is unavailable or Create fails.
Tr2NoesisView* LoadStudio( Tr2NoesisView* view, const char* projectPath );

void InstallCursorCallback();

}

#endif

#endif
