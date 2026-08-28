// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisView.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisDataModel.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisObject.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsGui/Cursor.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/InputEnums.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/Uri.h>
#include <NsRender/RenderDevice.h>

#include <unordered_map>

namespace
{

const Noesis::Key* VirtualKeyTable()
{
	// Transcribed from the SDK's Win32Display::FillKeyTable. Unmapped entries stay
	// Key_None (zero). VK_SHIFT / VK_CONTROL / VK_MENU collapse to the left-side
	// keys, matching both that table and Tr2MainWindow's Mac path.
	static Noesis::Key table[256];
	static bool ready = false;
	if( !ready )
	{
		table[VK_BACK] = Noesis::Key_Back;
		table[VK_TAB] = Noesis::Key_Tab;
		table[VK_CLEAR] = Noesis::Key_Clear;
		table[VK_RETURN] = Noesis::Key_Return;
		table[VK_PAUSE] = Noesis::Key_Pause;

		table[VK_SHIFT] = Noesis::Key_LeftShift;
		table[VK_LSHIFT] = Noesis::Key_LeftShift;
		table[VK_RSHIFT] = Noesis::Key_RightShift;
		table[VK_CONTROL] = Noesis::Key_LeftCtrl;
		table[VK_LCONTROL] = Noesis::Key_LeftCtrl;
		table[VK_RCONTROL] = Noesis::Key_RightCtrl;
		table[VK_MENU] = Noesis::Key_LeftAlt;
		table[VK_LMENU] = Noesis::Key_LeftAlt;
		table[VK_RMENU] = Noesis::Key_RightAlt;
		table[VK_LWIN] = Noesis::Key_LWin;
		table[VK_RWIN] = Noesis::Key_RWin;
		table[VK_ESCAPE] = Noesis::Key_Escape;

		table[VK_SPACE] = Noesis::Key_Space;
		table[VK_PRIOR] = Noesis::Key_Prior;
		table[VK_NEXT] = Noesis::Key_Next;
		table[VK_END] = Noesis::Key_End;
		table[VK_HOME] = Noesis::Key_Home;
		table[VK_LEFT] = Noesis::Key_Left;
		table[VK_UP] = Noesis::Key_Up;
		table[VK_RIGHT] = Noesis::Key_Right;
		table[VK_DOWN] = Noesis::Key_Down;
		table[VK_SELECT] = Noesis::Key_Select;
		table[VK_PRINT] = Noesis::Key_Print;
		table[VK_EXECUTE] = Noesis::Key_Execute;
		table[VK_SNAPSHOT] = Noesis::Key_Snapshot;
		table[VK_INSERT] = Noesis::Key_Insert;
		table[VK_DELETE] = Noesis::Key_Delete;
		table[VK_HELP] = Noesis::Key_Help;

		table['0'] = Noesis::Key_D0;
		table['1'] = Noesis::Key_D1;
		table['2'] = Noesis::Key_D2;
		table['3'] = Noesis::Key_D3;
		table['4'] = Noesis::Key_D4;
		table['5'] = Noesis::Key_D5;
		table['6'] = Noesis::Key_D6;
		table['7'] = Noesis::Key_D7;
		table['8'] = Noesis::Key_D8;
		table['9'] = Noesis::Key_D9;

		table[VK_NUMPAD0] = Noesis::Key_NumPad0;
		table[VK_NUMPAD1] = Noesis::Key_NumPad1;
		table[VK_NUMPAD2] = Noesis::Key_NumPad2;
		table[VK_NUMPAD3] = Noesis::Key_NumPad3;
		table[VK_NUMPAD4] = Noesis::Key_NumPad4;
		table[VK_NUMPAD5] = Noesis::Key_NumPad5;
		table[VK_NUMPAD6] = Noesis::Key_NumPad6;
		table[VK_NUMPAD7] = Noesis::Key_NumPad7;
		table[VK_NUMPAD8] = Noesis::Key_NumPad8;
		table[VK_NUMPAD9] = Noesis::Key_NumPad9;

		table[VK_MULTIPLY] = Noesis::Key_Multiply;
		table[VK_ADD] = Noesis::Key_Add;
		table[VK_SEPARATOR] = Noesis::Key_Separator;
		table[VK_SUBTRACT] = Noesis::Key_Subtract;
		table[VK_DECIMAL] = Noesis::Key_Decimal;
		table[VK_DIVIDE] = Noesis::Key_Divide;

		table['A'] = Noesis::Key_A;
		table['B'] = Noesis::Key_B;
		table['C'] = Noesis::Key_C;
		table['D'] = Noesis::Key_D;
		table['E'] = Noesis::Key_E;
		table['F'] = Noesis::Key_F;
		table['G'] = Noesis::Key_G;
		table['H'] = Noesis::Key_H;
		table['I'] = Noesis::Key_I;
		table['J'] = Noesis::Key_J;
		table['K'] = Noesis::Key_K;
		table['L'] = Noesis::Key_L;
		table['M'] = Noesis::Key_M;
		table['N'] = Noesis::Key_N;
		table['O'] = Noesis::Key_O;
		table['P'] = Noesis::Key_P;
		table['Q'] = Noesis::Key_Q;
		table['R'] = Noesis::Key_R;
		table['S'] = Noesis::Key_S;
		table['T'] = Noesis::Key_T;
		table['U'] = Noesis::Key_U;
		table['V'] = Noesis::Key_V;
		table['W'] = Noesis::Key_W;
		table['X'] = Noesis::Key_X;
		table['Y'] = Noesis::Key_Y;
		table['Z'] = Noesis::Key_Z;

		table[VK_F1] = Noesis::Key_F1;
		table[VK_F2] = Noesis::Key_F2;
		table[VK_F3] = Noesis::Key_F3;
		table[VK_F4] = Noesis::Key_F4;
		table[VK_F5] = Noesis::Key_F5;
		table[VK_F6] = Noesis::Key_F6;
		table[VK_F7] = Noesis::Key_F7;
		table[VK_F8] = Noesis::Key_F8;
		table[VK_F9] = Noesis::Key_F9;
		table[VK_F10] = Noesis::Key_F10;
		table[VK_F11] = Noesis::Key_F11;
		table[VK_F12] = Noesis::Key_F12;
		table[VK_F13] = Noesis::Key_F13;
		table[VK_F14] = Noesis::Key_F14;
		table[VK_F15] = Noesis::Key_F15;
		table[VK_F16] = Noesis::Key_F16;
		table[VK_F17] = Noesis::Key_F17;
		table[VK_F18] = Noesis::Key_F18;
		table[VK_F19] = Noesis::Key_F19;
		table[VK_F20] = Noesis::Key_F20;
		table[VK_F21] = Noesis::Key_F21;
		table[VK_F22] = Noesis::Key_F22;
		table[VK_F23] = Noesis::Key_F23;
		table[VK_F24] = Noesis::Key_F24;

		table[VK_NUMLOCK] = Noesis::Key_NumLock;
		table[VK_SCROLL] = Noesis::Key_Scroll;

		table[VK_OEM_1] = Noesis::Key_Oem1;
		table[VK_OEM_PLUS] = Noesis::Key_OemPlus;
		table[VK_OEM_COMMA] = Noesis::Key_OemComma;
		table[VK_OEM_MINUS] = Noesis::Key_OemMinus;
		table[VK_OEM_PERIOD] = Noesis::Key_OemPeriod;
		table[VK_OEM_2] = Noesis::Key_Oem2;
		table[VK_OEM_3] = Noesis::Key_Oem3;
		table[VK_OEM_4] = Noesis::Key_Oem4;
		table[VK_OEM_5] = Noesis::Key_Oem5;
		table[VK_OEM_6] = Noesis::Key_Oem6;
		table[VK_OEM_7] = Noesis::Key_Oem7;
		table[VK_OEM_8] = Noesis::Key_Oem8;
		table[VK_OEM_102] = Noesis::Key_Oem102;

		ready = true;
	}
	return table;
}

Noesis::Key KeyFromVirtualKey( int vk )
{
	if( vk < 0 || vk >= 256 )
	{
		return Noesis::Key_None;
	}
	return VirtualKeyTable()[vk];
}

bool TryMouseButton( int button, Noesis::MouseButton& mouseButton )
{
	if( button < 0 || button >= static_cast<int>( Noesis::MouseButton_Count ) )
	{
		return false;
	}
	mouseButton = static_cast<Noesis::MouseButton>( button );
	return true;
}

std::unordered_map<Noesis::IView*, Tr2NoesisView*> s_views;

void OnNoesisCursor( void* /*user*/, Noesis::IView* view, Noesis::Cursor* cursor )
{
	if( view == nullptr )
	{
		return;
	}
	const auto it = s_views.find( view );
	if( it == s_views.end() )
	{
		return;
	}
	it->second->NotifyCursorChange( cursor );
}

}

Tr2NoesisView::Tr2NoesisView( IRoot* ) :
	m_rendererInitialized( false ),
	m_lcd( false ),
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

	if( !Tr2Noesis::RequireInitialized() )
	{
		return false;
	}

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

	if( !Tr2Noesis::RequireInitialized() )
	{
		return false;
	}

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

	m_view->SetFlags( Noesis::RenderFlags_PPAA );
	ApplyLcdFlag();

	if( m_width != 0 && m_height != 0 )
	{
		m_view->SetSize( m_width, m_height );
	}

	s_views[m_view.GetPtr()] = this;

	CCP_NOESIS_LOGNOTICE( "Loaded XAML from '%s'", source );
	ApplyDataContext();
	return true;
}

void Tr2NoesisView::SetDataContext( Tr2NoesisDataModel* model )
{
	m_dataContext = model;
	ApplyDataContext();
}

Tr2NoesisDataModel* Tr2NoesisView::GetDataContext() const
{
	return m_dataContext;
}

void Tr2NoesisView::SetOnCursorChange( const BlueScriptCallback& callback )
{
	m_onCursorChange = callback;
}

const BlueScriptCallback& Tr2NoesisView::GetOnCursorChange() const
{
	return m_onCursorChange;
}

void Tr2NoesisView::NotifyCursorChange( Noesis::Cursor* cursor )
{
	if( !m_onCursorChange )
	{
		return;
	}

	const int type = cursor != nullptr ? static_cast<int>( cursor->Type() ) : static_cast<int>( Noesis::CursorType_Arrow );
	const char* filename = "";
	if( cursor != nullptr )
	{
		filename = cursor->Filename().Str();
		if( filename == nullptr )
		{
			filename = "";
		}
	}

	if( !m_onCursorChange.CallVoid( type, filename ) )
	{
		CCP_NOESIS_LOGERR( "onCursorChange callback failed" );
#if BLUE_WITH_PYTHON
		PyOS->PyFlushError( "Tr2NoesisView: onCursorChange callback failed" );
#endif
	}
}

void Tr2NoesisView::ApplyDataContext()
{
	if( m_view == nullptr )
	{
		return;
	}
	Noesis::FrameworkElement* root = m_view->GetContent();
	if( root == nullptr )
	{
		return;
	}
	if( m_dataContext != nullptr )
	{
		root->SetDataContext( m_dataContext->GetNative() );
	}
}

void Tr2NoesisView::ReleaseView()
{
	if( m_view == nullptr )
	{
		return;
	}

	s_views.erase( m_view.GetPtr() );

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

bool Tr2NoesisView::GetLcd() const
{
	return m_lcd;
}

void Tr2NoesisView::SetLcd( bool enable )
{
	m_lcd = enable;
	ApplyLcdFlag();
}

void Tr2NoesisView::ApplyLcdFlag()
{
	if( m_view == nullptr )
	{
		return;
	}

	uint32_t flags = m_view->GetFlags();
	if( m_lcd )
	{
		flags |= Noesis::RenderFlags_LCD;
	}
	else
	{
		flags &= ~static_cast<uint32_t>( Noesis::RenderFlags_LCD );
	}
	m_view->SetFlags( flags );
}

bool Tr2NoesisView::EnsureRenderer( Noesis::RenderDevice* device )
{
	if( m_view == nullptr || device == nullptr )
	{
		return false;
	}
	if( m_rendererInitialized )
	{
		return true;
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

void Tr2NoesisView::Activate()
{
	if( m_view != nullptr )
	{
		m_view->Activate();
	}
}

void Tr2NoesisView::Deactivate()
{
	if( m_view != nullptr )
	{
		m_view->Deactivate();
	}
}

void Tr2NoesisView::SetEmulateTouch( bool emulate )
{
	if( m_view != nullptr )
	{
		m_view->SetEmulateTouch( emulate );
	}
}

bool Tr2NoesisView::MouseButtonDown( int x, int y, int button )
{
	Noesis::MouseButton mouseButton;
	if( m_view == nullptr || !TryMouseButton( button, mouseButton ) )
	{
		return false;
	}
	return m_view->MouseButtonDown( x, y, mouseButton );
}

bool Tr2NoesisView::MouseButtonUp( int x, int y, int button )
{
	Noesis::MouseButton mouseButton;
	if( m_view == nullptr || !TryMouseButton( button, mouseButton ) )
	{
		return false;
	}
	return m_view->MouseButtonUp( x, y, mouseButton );
}

bool Tr2NoesisView::MouseDoubleClick( int x, int y, int button )
{
	Noesis::MouseButton mouseButton;
	if( m_view == nullptr || !TryMouseButton( button, mouseButton ) )
	{
		return false;
	}
	return m_view->MouseDoubleClick( x, y, mouseButton );
}

bool Tr2NoesisView::MouseMove( int x, int y )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->MouseMove( x, y );
}

bool Tr2NoesisView::MouseWheel( int x, int y, int delta )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->MouseWheel( x, y, delta );
}

bool Tr2NoesisView::MouseHWheel( int x, int y, int delta )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->MouseHWheel( x, y, delta );
}

bool Tr2NoesisView::Scroll( int x, int y, float value )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->Scroll( x, y, value );
}

bool Tr2NoesisView::HScroll( int x, int y, float value )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->HScroll( x, y, value );
}

bool Tr2NoesisView::TouchDown( int x, int y, uint64_t id )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->TouchDown( x, y, id );
}

bool Tr2NoesisView::TouchMove( int x, int y, uint64_t id )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->TouchMove( x, y, id );
}

bool Tr2NoesisView::TouchUp( int x, int y, uint64_t id )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->TouchUp( x, y, id );
}

bool Tr2NoesisView::KeyDown( int key )
{
	if( m_view == nullptr )
	{
		return false;
	}
	const Noesis::Key noesisKey = KeyFromVirtualKey( key );
	if( noesisKey == Noesis::Key_None )
	{
		return false;
	}
	return m_view->KeyDown( noesisKey );
}

bool Tr2NoesisView::KeyUp( int key )
{
	if( m_view == nullptr )
	{
		return false;
	}
	const Noesis::Key noesisKey = KeyFromVirtualKey( key );
	if( noesisKey == Noesis::Key_None )
	{
		return false;
	}
	return m_view->KeyUp( noesisKey );
}

bool Tr2NoesisView::Char( uint32_t ch )
{
	if( m_view == nullptr )
	{
		return false;
	}
	return m_view->Char( ch );
}

namespace Tr2Noesis
{

void InstallCursorCallback()
{
	Noesis::GUI::SetCursorCallback( nullptr, OnNoesisCursor );
}

}

#endif
