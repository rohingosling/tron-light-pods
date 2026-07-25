//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   Win32 window and OpenGL 3.3 core profile context, with no dependency beyond the system libraries.
//
//   A core profile context cannot be made with a plain ChoosePixelFormat and wglCreateContext. The entry point
//   that makes one, wglCreateContextAttribsARB, is itself an extension, so it can only be fetched from a context
//   that already exists. Chicken and egg.
//
//   The way round it is to stand up a throwaway window and a legacy context purely to look those functions up,
//   then throw both away and build the real window properly.
//
//   Input is collected here and surfaced as game actions rather than virtual key codes, so the application layer
//   never has to know about Win32.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include "GlApi.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// GameAction
//
//   Edge-triggered actions, queued as they arrive and drained once per frame.
//
//   These name the physical KEYS, not what they mean. The same key does different things in different
//   application states - the arrows steer in play but navigate at the menu, Enter activates a menu item and does
//   nothing elsewhere, Escape backs out of everything - and deciding which is the front end's job, not the
//   window's.
//
//   Numpad 5 is not redundant. It also resynchronises the pending basis, which is the way out of a stale up
//   vector.
//---------------------------------------------------------------------------------------------------------------------

enum class GameAction
{
	SteerUp,                                    // numpad 8 / up arrow: steer up, or menu selection up
	SteerDown,                                  // numpad 2 / down arrow: steer down, or menu selection down
	SteerLeft,                                  // numpad 4 / left arrow: steer left, or menu value down
	SteerRight,                                 // numpad 6 / right arrow: steer right, or menu value up
	SteerStraight,                              // numpad 5: straight on, and resynchronise the pending basis
	ViewCockpit,                                // F1
	ViewThirdPerson,                            // F2
	ViewSpotPlane,                              // F3
	ToggleProximitySensor,                      // N
	ToggleThirdPersonTrail,                     // L
	ToggleFreeLook,                             // F12: enter or leave the debug free-look view
	Activate,                                   // Enter: activate the selected menu item
	Back,                                       // Escape: back out; from the menu, select Exit Game then quit
	AnyKey                                      // any key that means nothing else; see the note below
};

//---------------------------------------------------------------------------------------------------------------------
// AnyKey
//
//   Queued for a key press that maps to none of the actions above, so that a "press any key" prompt can honour
//   the promise literally rather than only for keys that happen to do something else.
//
//   Modifiers on their own do NOT produce it. Shift, Control, Alt and the Windows keys are excluded, because they
//   arrive as ordinary key presses when they are only being used to build a combination - alt-tabbing away and
//   back would otherwise read as a deliberate press.
//
//   States that do not want it simply ignore it, which is every state except the end screens.
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// FreeLookInput
//
//   Held-key state for the debug free-look camera, surfaced as axes rather than key codes so the application layer
//   still never sees a Win32 virtual key.
//
//   These are held rather than edge-triggered because the camera moves continuously while a key is down. Drive
//   it from repeated WM_KEYDOWN messages instead and its speed ends up depending on the operating system's key
//   repeat settings; see the note in FrontEnd::updateFreeLookCamera.
//---------------------------------------------------------------------------------------------------------------------

struct FreeLookInput
{
	bool pitchUp   = false;                     // numpad 8 / up arrow
	bool pitchDown = false;                     // numpad 2 / down arrow
	bool yawLeft   = false;                     // numpad 4 / left arrow
	bool yawRight  = false;                     // numpad 6 / right arrow
	bool moveIn    = false;                     // numpad +
	bool moveOut   = false;                     // numpad -
};

//---------------------------------------------------------------------------------------------------------------------
// Key repeat
//
//   Ignore the WM_KEYDOWN repeat flag (bit 30 of lParam) and a held key runs its handler once per auto-repeat
//   message. At the MENU that is exactly what we want, and it is how every Windows control steps a value: hold
//   Left or Right and the selected item keeps counting until the key comes up.
//
//   We filter auto-repeat out of the edge-triggered action queue, because a held steering key has to queue ONE
//   turn and not one per repeat interval. So the menu's repeat gets rebuilt here from held state instead, driven
//   per frame against the real frame delta.
//
//   The cadence comes from the operating system's own keyboard settings rather than from a number picked here,
//   so the menu repeats at whatever rate the user has set, which is what it would do if we simply let the
//   repeats through.
//
//   Windows reports both settings as indices rather than times:
//
//     SPI_GETKEYBOARDDELAY   0 .. 3   ->  250 ms .. 1000 ms, in 250 ms steps
//     SPI_GETKEYBOARDSPEED   0 .. 31  ->  about 2.5 .. 30 repeats per second, linear
//---------------------------------------------------------------------------------------------------------------------

constexpr float KEY_REPEAT_DELAY_DEFAULT    = 0.5f;         // the Windows default, used if the query fails
constexpr float KEY_REPEAT_INTERVAL_DEFAULT = 1.0f / 30.0f;

//*********************************************************************************************************************
// Window
//*********************************************************************************************************************

class Window
{
	//=================================================================================================================
	// Data Members
	//=================================================================================================================

	private:

		HWND    windowHandle   = nullptr;
		HDC     deviceContext  = nullptr;
		HGLRC   renderContext  = nullptr;

		int     clientWidth    = 0;
		int     clientHeight   = 0;
		bool    sizeChanged    = true;
		bool    closeRequested = false;

		bool    acceleratorHeld = false;
		bool    brakeHeld       = false;

		// Which way the held menu adjust key is currently counting: -1 for Left, +1 for Right, 0 for neither.
		// A single value rather than two flags because that is how the operating system's own auto-repeat
		// behaves - pressing the second key takes the repeat over, and releasing it stops the repeat outright
		// rather than handing it back to a key that is still down.

		int     menuAdjustHeld  = 0;

		float   keyRepeatDelaySeconds    = KEY_REPEAT_DELAY_DEFAULT;
		float   keyRepeatIntervalSeconds = KEY_REPEAT_INTERVAL_DEFAULT;

		FreeLookInput freeLookHeld;

		std::vector<GameAction> pendingActions;

	//=================================================================================================================
	// Accessors
	//=================================================================================================================

	public:

		int  width           () const { return clientWidth;     }
		int  height          () const { return clientHeight;    }
		bool isClosing       () const { return closeRequested;  }
		bool isAccelerating  () const { return acceleratorHeld; }
		bool isBraking       () const { return brakeHeld;       }

		int   menuAdjustDirection () const { return menuAdjustHeld;           }
		float keyRepeatDelay      () const { return keyRepeatDelaySeconds;    }
		float keyRepeatInterval   () const { return keyRepeatIntervalSeconds; }

		const FreeLookInput& freeLookInput () const { return freeLookHeld; }

		//-------------------------------------------------------------------------------------------------------------
		// Method: consumeResize
		//
		// Description:
		//
		//   Returns true once after each size change, so the caller can update the viewport without polling.
		//
		//-------------------------------------------------------------------------------------------------------------

		bool consumeResize ()
		{
			const bool changed = sizeChanged;

			sizeChanged = false;

			return changed;
		}

		//-------------------------------------------------------------------------------------------------------------
		// Method: drainActions
		//
		// Description:
		//
		//   Hands over the actions queued since the last call and clears the queue.
		//
		//-------------------------------------------------------------------------------------------------------------

		std::vector<GameAction> drainActions ()
		{
			std::vector<GameAction> actions;

			actions.swap ( pendingActions );

			return actions;
		}

	//=================================================================================================================
	// Methods
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Method: create
		//
		// Description:
		//
		//   Creates the window and a current OpenGL 3.3 core profile context.
		//
		// Arguments:
		//
		//   - title  : Window caption.
		//   - width  : Requested client width.
		//   - height : Requested client height.
		//   - error  : Receives a description on failure.
		//
		// Returns:
		//
		//   - True on success.
		//
		//-------------------------------------------------------------------------------------------------------------

		bool create ( const char* title, int width, int height, std::string& error );

		//-------------------------------------------------------------------------------------------------------------
		// Method: destroy
		//-------------------------------------------------------------------------------------------------------------

		void destroy ();

		//-------------------------------------------------------------------------------------------------------------
		// Method: pumpMessages
		//
		// Description:
		//
		//   Drains the message queue without blocking.
		//
		// Returns:
		//
		//   - False once the window has been asked to close.
		//
		//-------------------------------------------------------------------------------------------------------------

		bool pumpMessages ();

		//-------------------------------------------------------------------------------------------------------------
		// Method: present
		//-------------------------------------------------------------------------------------------------------------

		void present ();

	private:

		static LRESULT CALLBACK windowProcedure ( HWND handle, UINT message, WPARAM wParam, LPARAM lParam );

		LRESULT handleMessage ( HWND handle, UINT message, WPARAM wParam, LPARAM lParam );

		void    queueAction        ( GameAction action );

		void    updateFreeLookKey  ( WPARAM virtualKey, bool held );

		void    updateMenuAdjustKey ( WPARAM virtualKey, bool held );

		void    clearHeldInput      ();

		void    readKeyRepeatSettings ();
};

}
