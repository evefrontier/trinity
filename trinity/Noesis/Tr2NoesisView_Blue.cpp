// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisView.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisDataModel.h"

BLUE_DEFINE( Tr2NoesisView );

const Be::ClassInfo* Tr2NoesisView::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisView, "A NoesisGUI view holding one XAML tree. Render it with TriStepRenderNoesis or Tr2Sprite2dNoesis." )
		MAP_INTERFACE( Tr2NoesisView )

		MAP_METHOD_AND_WRAP(
			"LoadXaml",
			LoadXaml,
			"Loads XAML from a Trinity resource path, replacing any content already loaded.\n"
			"A missing resource, unparseable XAML or a root that cannot be a view's content is\n"
			"reported on the Noesis log channel, and the previously loaded content stays as it\n"
			"was. XAML that logs errors but still parses is installed; check isLoaded for whether\n"
			"there is content, and read the log for anything more specific.\n"
			":param resPath: full resource path, for example 'res:/UI/Noesis/Test.xaml'\n"
			":rtype: None" )

		MAP_METHOD_AND_WRAP(
			"LoadXamlString",
			LoadXamlString,
			"Parses XAML from a string, replacing any content already loaded. Needs no resource\n"
			"provider, so it separates a render problem from a resource problem. Reports failure\n"
			"the same way LoadXaml does, on the Noesis log channel.\n"
			":param xaml: XAML markup\n"
			":rtype: None" )

		MAP_PROPERTY_READONLY(
			"isLoaded",
			GetIsLoaded,
			"True once XAML has been loaded successfully. A load that fails leaves this as it was,\n"
			"so it reports whether there is content rather than whether the last load worked." )

		MAP_PROPERTY(
			"lcd",
			GetLcd,
			SetLcd,
			"LCD subpixel text rendering (Noesis RenderFlags_LCD)." )

		MAP_PROPERTY(
			"dataContext",
			GetDataContext,
			SetDataContext,
			"Observable model installed as the XAML tree DataContext. Re-applied after LoadXaml." )

		MAP_PROPERTY(
			"onCursorChange",
			GetOnCursorChange,
			SetOnCursorChange,
			"Callable(cursorType, filename) invoked when this view's mouse cursor should change.\n"
			"cursorType is Noesis CursorType: 0 None, 1 No, 2 Arrow, 3 AppStarting, 4 Cross, 5 Help,\n"
			"6 IBeam, 7 SizeAll, 8 SizeNESW, 9 SizeNS, 10 SizeNWSE, 11 SizeWE, 12 UpArrow, 13 Wait,\n"
			"14 Hand, 15 Pen, 16 ScrollNS, 17 ScrollWE, 18 ScrollAll, 19 ScrollN, 20 ScrollS,\n"
			"21 ScrollW, 22 ScrollE, 23 ScrollNW, 24 ScrollNE, 25 ScrollSW, 26 ScrollSE,\n"
			"27 ArrowCD, 28 Custom. filename is empty except for Custom, where it is the cursor URI." )

		MAP_METHOD_AND_WRAP(
			"Activate",
			Activate,
			"Gives this view keyboard focus. LoadXaml does not call this; call it when this\n"
			"view should own the keyboard. A second LoadXaml drops activation.\n"
			":rtype: None" )

		MAP_METHOD_AND_WRAP(
			"Deactivate",
			Deactivate,
			"Removes keyboard focus from this view.\n"
			":rtype: None" )

		MAP_METHOD_AND_WRAP(
			"SetEmulateTouch",
			SetEmulateTouch,
			"When True, mouse events also generate touch events. Dropped by a second LoadXaml.\n"
			":param emulate: True to emulate touch from the mouse\n"
			":rtype: None" )

		MAP_METHOD_AND_WRAP(
			"MouseButtonDown",
			MouseButtonDown,
			"Notifies that a mouse button was pressed. Origin is the upper-left of the view.\n"
			"Returns True if the UI handled the event. Button outside 0-4 returns False.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param button: 0 left, 1 right, 2 middle, 3 X1, 4 X2\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"MouseButtonUp",
			MouseButtonUp,
			"Notifies that a mouse button was released. Origin is the upper-left of the view.\n"
			"Returns True if the UI handled the event. Button outside 0-4 returns False.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param button: 0 left, 1 right, 2 middle, 3 X1, 4 X2\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"MouseDoubleClick",
			MouseDoubleClick,
			"Notifies a double-click. Expected sequence is Down, Up, DoubleClick, Up.\n"
			"Returns True if the UI handled the event. Button outside 0-4 returns False.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param button: 0 left, 1 right, 2 middle, 3 X1, 4 X2\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"MouseMove",
			MouseMove,
			"Notifies that the mouse moved. Origin is the upper-left of the view.\n"
			"Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"MouseWheel",
			MouseWheel,
			"Notifies vertical wheel rotation. delta is in multiples of 120 per notch;\n"
			"positive is away from the user. Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param delta: wheel rotation\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"MouseHWheel",
			MouseHWheel,
			"Notifies horizontal wheel rotation. delta is in multiples of 120 per notch;\n"
			"positive is to the right. Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param delta: wheel rotation\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"Scroll",
			Scroll,
			"Notifies a vertical scroll on the element under (x, y). value is -1..+1, typically\n"
			"from a gamepad stick. Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param value: scroll amount in -1..+1\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"HScroll",
			HScroll,
			"Notifies a horizontal scroll on the element under (x, y). value is -1..+1, typically\n"
			"from a gamepad stick. Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param value: scroll amount in -1..+1\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"TouchDown",
			TouchDown,
			"Notifies that a finger touched the view. Origin is the upper-left of the view.\n"
			"Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param id: touch id\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"TouchMove",
			TouchMove,
			"Notifies that a finger moved on the view. Origin is the upper-left of the view.\n"
			"Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param id: touch id\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"TouchUp",
			TouchUp,
			"Notifies that a finger left the view. Origin is the upper-left of the view.\n"
			"Returns True if the UI handled the event.\n"
			":param x: view-local x in pixels\n"
			":param y: view-local y in pixels\n"
			":param id: touch id\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"KeyDown",
			KeyDown,
			"Notifies that a key was pressed. key is a Win32 virtual-key code, as Tr2MainWindow\n"
			"onKeyDown already delivers on Windows and Mac. Returns True if the UI handled the\n"
			"event; an unmapped key returns False.\n"
			":param key: Win32 virtual-key code\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"KeyUp",
			KeyUp,
			"Notifies that a key was released. key is a Win32 virtual-key code, as Tr2MainWindow\n"
			"onKeyUp already delivers on Windows and Mac. Returns True if the UI handled the\n"
			"event; an unmapped key returns False.\n"
			":param key: Win32 virtual-key code\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"Char",
			Char,
			"Notifies a translated unicode character. Send between the matching KeyDown and KeyUp.\n"
			"Returns True if the UI handled the event.\n"
			":param ch: unicode code point\n"
			":rtype: bool" )

	EXPOSURE_END()
}

#endif
