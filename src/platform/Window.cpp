//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   Implementation of the Win32 window and the OpenGL 3.3 core profile context.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "Window.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// WGL extension entry points
//
//   Declared here rather than pulled in from wglext.h. There are only three of them and a dozen enumerants, and
//   writing them out keeps this file free of any header-ordering interaction with glcorearb.h.
//---------------------------------------------------------------------------------------------------------------------

typedef HGLRC ( WINAPI *PFN_wglCreateContextAttribsARB ) ( HDC, HGLRC, const int* );
typedef BOOL  ( WINAPI *PFN_wglChoosePixelFormatARB    ) ( HDC, const int*, const FLOAT*, UINT, int*, UINT* );
typedef BOOL  ( WINAPI *PFN_wglSwapIntervalEXT         ) ( int );

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_COLOR_BITS_ARB                      0x2014
#define WGL_DEPTH_BITS_ARB                      0x2022
#define WGL_STENCIL_BITS_ARB                    0x2023
#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_SAMPLE_BUFFERS_ARB                  0x2041
#define WGL_SAMPLES_ARB                         0x2042

#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x00000002

#define GL_MULTISAMPLE_ENUM                     0x809D

namespace
{
	const char* WINDOW_CLASS_NAME = "Window3D";

	PFN_wglCreateContextAttribsARB wglCreateContextAttribsARB = nullptr;
	PFN_wglChoosePixelFormatARB    wglChoosePixelFormatARB    = nullptr;
	PFN_wglSwapIntervalEXT         wglSwapIntervalEXT         = nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: loadWglExtensions
//
// Description:
//
//   Stands up a throwaway window and legacy context solely so the modern context-creation entry points can be
//   looked up, then tears both down.
//
//   The dummy window is genuinely necessary rather than merely conventional. A pixel format can be set on a
//   device context exactly once and can never be changed afterwards, so the window whose format is chosen with
//   the basic ChoosePixelFormat cannot then be given the multisampled format that wglChoosePixelFormatARB
//   selects. The window used for the lookup therefore has to be thrown away.
//
// Returns:
//
//   - True if both required entry points resolved.
//
//---------------------------------------------------------------------------------------------------------------------

static bool loadWglExtensions ()
{
	if ( wglCreateContextAttribsARB != nullptr )
	{
		return true;
	}

	WNDCLASSA dummyClass  = {};

	dummyClass.style         = CS_OWNDC;
	dummyClass.lpfnWndProc   = DefWindowProcA;
	dummyClass.hInstance     = GetModuleHandleA ( nullptr );
	dummyClass.lpszClassName = "Tron3DDummyGlClass";

	if ( RegisterClassA ( &dummyClass ) == 0 )
	{
		return false;
	}

	HWND dummyWindow = CreateWindowExA ( 0, dummyClass.lpszClassName, "dummy", WS_OVERLAPPEDWINDOW,
	                                     CW_USEDEFAULT, CW_USEDEFAULT, 64, 64,
	                                     nullptr, nullptr, dummyClass.hInstance, nullptr );

	if ( dummyWindow == nullptr )
	{
		UnregisterClassA ( dummyClass.lpszClassName, dummyClass.hInstance );

		return false;
	}

	HDC dummyDeviceContext = GetDC ( dummyWindow );

	PIXELFORMATDESCRIPTOR descriptor = {};

	descriptor.nSize        = sizeof ( descriptor );
	descriptor.nVersion     = 1;
	descriptor.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	descriptor.iPixelType   = PFD_TYPE_RGBA;
	descriptor.cColorBits   = 32;
	descriptor.cDepthBits   = 24;
	descriptor.cStencilBits = 8;

	const int dummyFormat = ChoosePixelFormat ( dummyDeviceContext, &descriptor );

	SetPixelFormat ( dummyDeviceContext, dummyFormat, &descriptor );

	HGLRC dummyContext = wglCreateContext ( dummyDeviceContext );

	wglMakeCurrent ( dummyDeviceContext, dummyContext );

	// Each cast goes through void*. PROC is declared as a no-argument function pointer, so converting it
	// straight to a typed entry point is a cast between incompatible function types; routing it through an
	// object pointer is the conventional way to state that the conversion is deliberate.

	wglCreateContextAttribsARB = reinterpret_cast<PFN_wglCreateContextAttribsARB> (
		reinterpret_cast<void*> ( wglGetProcAddress ( "wglCreateContextAttribsARB" ) ) );

	wglChoosePixelFormatARB = reinterpret_cast<PFN_wglChoosePixelFormatARB> (
		reinterpret_cast<void*> ( wglGetProcAddress ( "wglChoosePixelFormatARB" ) ) );

	wglSwapIntervalEXT = reinterpret_cast<PFN_wglSwapIntervalEXT> (
		reinterpret_cast<void*> ( wglGetProcAddress ( "wglSwapIntervalEXT" ) ) );

	wglMakeCurrent   ( nullptr, nullptr );
	wglDeleteContext ( dummyContext );
	ReleaseDC        ( dummyWindow, dummyDeviceContext );
	DestroyWindow    ( dummyWindow );
	UnregisterClassA ( dummyClass.lpszClassName, dummyClass.hInstance );

	return ( wglCreateContextAttribsARB != nullptr ) && ( wglChoosePixelFormatARB != nullptr );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::windowProcedure
//---------------------------------------------------------------------------------------------------------------------

LRESULT CALLBACK Window::windowProcedure ( HWND handle, UINT message, WPARAM wParam, LPARAM lParam )
{
	Window* window = reinterpret_cast<Window*> ( GetWindowLongPtrA ( handle, GWLP_USERDATA ) );

	if ( message == WM_NCCREATE )
	{
		const CREATESTRUCTA* create = reinterpret_cast<const CREATESTRUCTA*> ( lParam );

		SetWindowLongPtrA ( handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR> ( create->lpCreateParams ) );
	}

	if ( window != nullptr )
	{
		return window->handleMessage ( handle, message, wParam, lParam );
	}

	return DefWindowProcA ( handle, message, wParam, lParam );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::queueAction
//---------------------------------------------------------------------------------------------------------------------

void Window::queueAction ( GameAction action )
{
	pendingActions.push_back ( action );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::updateFreeLookKey
//
// Description:
//
//   Record the held state of one free-look camera key. The same numpad and arrow keys that steer during play
//   orbit the camera here, and numpad + / - move it in and out.
//
// Arguments:
//
//   - virtualKey : The Win32 virtual key code from the message.
//   - held       : True on key down, false on key up.
//
//---------------------------------------------------------------------------------------------------------------------

void Window::updateFreeLookKey ( WPARAM virtualKey, bool held )
{
	switch ( virtualKey )
	{
		case VK_NUMPAD8: case VK_UP:      freeLookHeld.pitchUp   = held; break;
		case VK_NUMPAD2: case VK_DOWN:    freeLookHeld.pitchDown = held; break;
		case VK_NUMPAD4: case VK_LEFT:    freeLookHeld.yawLeft   = held; break;
		case VK_NUMPAD6: case VK_RIGHT:   freeLookHeld.yawRight  = held; break;
		case VK_ADD:                      freeLookHeld.moveIn    = held; break;
		case VK_SUBTRACT:                 freeLookHeld.moveOut   = held; break;
		default:                                                         break;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::updateMenuAdjustKey
//
// Description:
//
//   Record which way the held menu adjust key is counting, so value items keep stepping while the key is down.
//
//   Left and Right own the direction between them: the most recently pressed key takes it and its release
//   clears it, which is the same rule the operating system's own auto-repeat follows. See the note in
//   Window.hpp. Bound to the left and right arrows and to numpad 4 / 6.
//
// Arguments:
//
//   - virtualKey : The Win32 virtual key code from the message.
//   - held       : True on key down, false on key up.
//
//---------------------------------------------------------------------------------------------------------------------

void Window::updateMenuAdjustKey ( WPARAM virtualKey, bool held )
{
	int direction = 0;

	switch ( virtualKey )
	{
		case VK_NUMPAD4: case VK_LEFT:    direction = -1; break;
		case VK_NUMPAD6: case VK_RIGHT:   direction = +1; break;
		default:                          return;
	}

	if ( held )
	{
		menuAdjustHeld = direction;
	}
	else if ( menuAdjustHeld == direction )
	{
		menuAdjustHeld = 0;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::clearHeldInput
//
// Description:
//
//   Drops every held-key latch. Called on focus loss, because a key released while another window has the focus
//   sends its WM_KEYUP there - so without this a value the player was stepping would carry on stepping on the way
//   back, and the throttle would stay stuck on.
//
//---------------------------------------------------------------------------------------------------------------------

void Window::clearHeldInput ()
{
	acceleratorHeld = false;
	brakeHeld       = false;
	menuAdjustHeld  = 0;
	freeLookHeld    = FreeLookInput {};
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::readKeyRepeatSettings
//
// Description:
//
//   Read the operating system's keyboard auto-repeat delay and rate, converting both from the index Windows
//   reports into seconds. See the note in Window.hpp for why the menu's repeat comes from here rather than from
//   a number chosen on the spot.
//
//---------------------------------------------------------------------------------------------------------------------

void Window::readKeyRepeatSettings ()
{
	// The defaults stand if either query fails, so an unusual system cannot leave the menu unable to repeat.

	int delayIndex = 1;                                     // 0..3  -> 250 ms .. 1000 ms
	int speedIndex = 31;                                    // 0..31 -> about 2.5 .. 30 repeats per second

	if ( SystemParametersInfoA ( SPI_GETKEYBOARDDELAY, 0, &delayIndex, 0 ) != FALSE )
	{
		keyRepeatDelaySeconds = ( static_cast<float> ( delayIndex ) + 1.0f ) * 0.25f;
	}

	if ( SystemParametersInfoA ( SPI_GETKEYBOARDSPEED, 0, &speedIndex, 0 ) != FALSE )
	{
		const float repeatsPerSecond = 2.5f + ( static_cast<float> ( speedIndex ) * ( 27.5f / 31.0f ) );

		keyRepeatIntervalSeconds = 1.0f / repeatsPerSecond;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::handleMessage
//---------------------------------------------------------------------------------------------------------------------

LRESULT Window::handleMessage ( HWND handle, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_CLOSE:
		case WM_DESTROY:

			closeRequested = true;
			return 0;

		case WM_SIZE:
		{
			// Clamp before anything downstream divides by the height. Minimising hands us a zero sized client
			// rect, and the aspect ratio must never be computed from a zero.

			const int newWidth  = LOWORD ( lParam );
			const int newHeight = HIWORD ( lParam );

			if ( ( newWidth > 0 ) && ( newHeight > 0 ) )
			{
				clientWidth  = newWidth;
				clientHeight = newHeight;
				sizeChanged  = true;
			}

			return 0;
		}

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			// Ignore auto-repeat: a held steering key should queue one turn, not one per repeat interval.

			const bool isRepeat = ( ( lParam & ( 1 << 30 ) ) != 0 );

			if ( !isRepeat )
			{
				switch ( wParam )
				{
					case VK_NUMPAD8: case VK_UP:     queueAction ( GameAction::SteerUp         ); break;
					case VK_NUMPAD2: case VK_DOWN:   queueAction ( GameAction::SteerDown       ); break;
					case VK_NUMPAD4: case VK_LEFT:   queueAction ( GameAction::SteerLeft       ); break;
					case VK_NUMPAD6: case VK_RIGHT:  queueAction ( GameAction::SteerRight      ); break;
					case VK_NUMPAD5: case VK_CLEAR:  queueAction ( GameAction::SteerStraight   ); break;
					case VK_F1:                      queueAction ( GameAction::ViewCockpit     ); break;
					case VK_F2:                      queueAction ( GameAction::ViewThirdPerson ); break;
					case VK_F3:                      queueAction ( GameAction::ViewSpotPlane   ); break;
					case 'N':                        queueAction ( GameAction::ToggleProximitySensor  ); break;
					case 'L':                        queueAction ( GameAction::ToggleThirdPersonTrail ); break;
					case VK_F12:                     queueAction ( GameAction::ToggleFreeLook   ); break;
					case VK_RETURN:                  queueAction ( GameAction::Activate        ); break;
					case VK_ESCAPE:                  queueAction ( GameAction::Back            ); break;

					// Anything else is still a key press. Queue it as such so a "press any key" prompt can
					// honour the promise, and let the states that do not care ignore it.
					//
					// Modifiers are left out on purpose - see the note on AnyKey in Window.hpp.

					case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
					case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
					case VK_MENU:    case VK_LMENU:    case VK_RMENU:
					case VK_LWIN:    case VK_RWIN:                                                break;

					default:                         queueAction ( GameAction::AnyKey          ); break;
				}
			}

			if ( ( wParam == VK_SHIFT ) || ( wParam == VK_LSHIFT ) || ( wParam == VK_RSHIFT ) )
			{
				acceleratorHeld = true;
			}

			if ( ( wParam == VK_CONTROL ) || ( wParam == VK_LCONTROL ) || ( wParam == VK_RCONTROL ) )
			{
				brakeHeld = true;
			}

			// Held state for the free-look camera, which moves continuously while a key is down, and for the menu's
			// value repeat. Auto-repeat is irrelevant to both - the flag is simply on from the first press to the
			// release - so these sit outside the repeat filter above.

			updateFreeLookKey   ( wParam, true );
			updateMenuAdjustKey ( wParam, true );

			return 0;
		}

		case WM_KEYUP:
		case WM_SYSKEYUP:

			if ( ( wParam == VK_SHIFT ) || ( wParam == VK_LSHIFT ) || ( wParam == VK_RSHIFT ) )
			{
				acceleratorHeld = false;
			}

			if ( ( wParam == VK_CONTROL ) || ( wParam == VK_LCONTROL ) || ( wParam == VK_RCONTROL ) )
			{
				brakeHeld = false;
			}

			updateFreeLookKey   ( wParam, false );
			updateMenuAdjustKey ( wParam, false );

			return 0;

		case WM_KILLFOCUS:

			// The key-up for anything held at this moment is delivered to whichever window takes the focus, so
			// every latch has to be dropped here or it stays on until the key is pressed and released again.

			clearHeldInput ();

			return 0;

		default:
			break;
	}

	return DefWindowProcA ( handle, message, wParam, lParam );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::create
//---------------------------------------------------------------------------------------------------------------------

bool Window::create ( const char* title, int width, int height, std::string& error )
{
	readKeyRepeatSettings ();

	if ( !loadWglExtensions () )
	{
		error = "this system does not provide wglCreateContextAttribsARB, so no OpenGL 3.3 core context "
		        "can be created";

		return false;
	}

	const HINSTANCE instance = GetModuleHandleA ( nullptr );

	WNDCLASSA windowClass = {};

	windowClass.style         = CS_OWNDC;
	windowClass.lpfnWndProc   = &Window::windowProcedure;
	windowClass.hInstance     = instance;
	windowClass.hCursor       = LoadCursorA ( nullptr, IDC_ARROW );
	windowClass.lpszClassName = WINDOW_CLASS_NAME;

	if ( RegisterClassA ( &windowClass ) == 0 )
	{
		error = "RegisterClass failed";

		return false;
	}

	RECT desired = { 0, 0, width, height };

	AdjustWindowRect ( &desired, WS_OVERLAPPEDWINDOW, FALSE );

	windowHandle = CreateWindowExA ( 0, WINDOW_CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
	                                 CW_USEDEFAULT, CW_USEDEFAULT,
	                                 desired.right - desired.left, desired.bottom - desired.top,
	                                 nullptr, nullptr, instance, this );

	if ( windowHandle == nullptr )
	{
		error = "CreateWindowEx failed";

		return false;
	}

	deviceContext = GetDC ( windowHandle );

	// Choose a multisampled format. The arena is almost entirely thin lines and edges, which alias badly without
	// it. If the driver cannot provide it, fall back rather than fail.

	const int formatAttributes [] =
	{
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
		WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB,     32,
		WGL_DEPTH_BITS_ARB,     24,
		WGL_STENCIL_BITS_ARB,   8,
		WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
		WGL_SAMPLES_ARB,        4,
		0
	};

	int   pixelFormat  = 0;
	UINT  formatCount  = 0;

	if ( ( wglChoosePixelFormatARB ( deviceContext, formatAttributes, nullptr, 1, &pixelFormat, &formatCount ) == FALSE )
	     || ( formatCount == 0 ) )
	{
		const int fallbackAttributes [] =
		{
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,     32,
			WGL_DEPTH_BITS_ARB,     24,
			0
		};

		if ( ( wglChoosePixelFormatARB ( deviceContext, fallbackAttributes, nullptr, 1,
		                                 &pixelFormat, &formatCount ) == FALSE ) || ( formatCount == 0 ) )
		{
			error = "no suitable pixel format";

			return false;
		}
	}

	PIXELFORMATDESCRIPTOR descriptor = {};

	DescribePixelFormat ( deviceContext, pixelFormat, sizeof ( descriptor ), &descriptor );

	if ( SetPixelFormat ( deviceContext, pixelFormat, &descriptor ) == FALSE )
	{
		error = "SetPixelFormat failed";

		return false;
	}

	const int contextAttributes [] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		0
	};

	renderContext = wglCreateContextAttribsARB ( deviceContext, nullptr, contextAttributes );

	if ( renderContext == nullptr )
	{
		error = "could not create an OpenGL 3.3 core profile context";

		return false;
	}

	wglMakeCurrent ( deviceContext, renderContext );

	// Vertical sync. The simulation runs on its own fixed timestep accumulator, so this only paces presentation
	// and cannot affect behaviour.

	if ( wglSwapIntervalEXT != nullptr )
	{
		wglSwapIntervalEXT ( 1 );
	}

	RECT clientRect = {};

	GetClientRect ( windowHandle, &clientRect );

	clientWidth  = clientRect.right  - clientRect.left;
	clientHeight = clientRect.bottom - clientRect.top;
	sizeChanged  = true;

	ShowWindow   ( windowHandle, SW_SHOW );
	SetForegroundWindow ( windowHandle );
	SetFocus     ( windowHandle );

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::pumpMessages
//---------------------------------------------------------------------------------------------------------------------

bool Window::pumpMessages ()
{
	MSG message;

	while ( PeekMessageA ( &message, nullptr, 0, 0, PM_REMOVE ) )
	{
		if ( message.message == WM_QUIT )
		{
			closeRequested = true;
		}

		TranslateMessage ( &message );
		DispatchMessageA ( &message );
	}

	return !closeRequested;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::present
//---------------------------------------------------------------------------------------------------------------------

void Window::present ()
{
	SwapBuffers ( deviceContext );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Window::destroy
//---------------------------------------------------------------------------------------------------------------------

void Window::destroy ()
{
	if ( renderContext != nullptr )
	{
		wglMakeCurrent   ( nullptr, nullptr );
		wglDeleteContext ( renderContext );

		renderContext = nullptr;
	}

	if ( ( deviceContext != nullptr ) && ( windowHandle != nullptr ) )
	{
		ReleaseDC ( windowHandle, deviceContext );

		deviceContext = nullptr;
	}

	if ( windowHandle != nullptr )
	{
		DestroyWindow ( windowHandle );

		windowHandle = nullptr;
	}

	UnregisterClassA ( WINDOW_CLASS_NAME, GetModuleHandleA ( nullptr ) );
}

}
