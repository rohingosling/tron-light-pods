//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Front End
//
// Description:
//
//   The application front end: the menu and the state machine that drives the whole game flow - launch into the
//   menu, start a game, land on an end screen, and back to the menu.
//
//   This is an application level controller that OWNS the Simulation and steers it from outside. That is what
//   keeps menu and input logic out of the simulation classes. The simulation still knows its own state, because
//   collision and victory have to set it from inside the update, but nothing about menus, keys or views leaks
//   in there.
//
//   The flow:
//
//     - Up / Down move the menu selection, clamped at the ends.
//     - Right / Left adjust the selected item's value, clamped to its range. Only Number of Opponents and Grid
//       Size carry values. Holding either key keeps the value counting; see MenuRepeat below.
//     - Enter acts only at the menu: Start Game begins a game, Reset Game restores the defaults, Exit Game
//       quits. On the other items it does nothing.
//     - Escape from ANY state goes back to the menu. At the menu itself the first Escape moves the selection to
//       Exit Game and a second one quits.
//     - The selection starts on Start Game, so a fresh launch needs a single Enter.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include "Renderer.hpp"
#include "Simulation.hpp"
#include "Window.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Menu item indices
//
//   The two value items first, then the three commands. The selection starts on Start Game.
//---------------------------------------------------------------------------------------------------------------------

constexpr int MENU_ITEM_OPPONENT_COUNT = 0;
constexpr int MENU_ITEM_GRID_SIZE      = 1;
constexpr int MENU_ITEM_START_GAME     = 2;
constexpr int MENU_ITEM_RESET_GAME     = 3;
constexpr int MENU_ITEM_EXIT_GAME      = 4;

constexpr int MENU_ITEM_FIRST          = 0;
constexpr int MENU_ITEM_LAST           = 4;

//---------------------------------------------------------------------------------------------------------------------
// Playable opponent minimum
//
//   The simulation will happily run with zero opponents - see MENU_OPPONENT_COUNT_MINIMUM in Constants.hpp -
//   but that is not a game anyone can win. With a single pod in the arena the victory check can never fire,
//   because it needs more than one alive, so the only ending is your own crash.
//
//   So the menu clamps to one opponent rather than none. Every game the application starts has a rival in it and
//   can be both won and lost. The simulation's own range is left alone; this is the floor the front end puts on
//   top of it.
//---------------------------------------------------------------------------------------------------------------------

constexpr int PLAYABLE_OPPONENT_COUNT_MINIMUM = 1;

//---------------------------------------------------------------------------------------------------------------------
// MenuModel
//
//   The menu's whole state: the selection and the two configurable values. A default constructed model is a
//   fresh menu, which is also exactly what Reset Game restores.
//---------------------------------------------------------------------------------------------------------------------

struct MenuModel
{
	int selection     = MENU_ITEM_START_GAME;
	int opponentCount = MENU_OPPONENT_COUNT_DEFAULT;
	int gridSize      = MENU_GRID_SIZE_DEFAULT;
};

//---------------------------------------------------------------------------------------------------------------------
// End screen input delay
//
//   How long a WINNER or GAME OVER screen ignores input for, in seconds.
//
//   Any key dismisses an end screen, which is what makes it feel responsive, and also what makes this necessary.
//   A player usually dies with a hand on the steering keys, so the next press lands a fraction of a second later
//   and would throw the screen away before they had a chance to read their score.
//
//   Long enough to stop that, short enough that nobody notices they were waiting.
//---------------------------------------------------------------------------------------------------------------------

constexpr float END_SCREEN_INPUT_DELAY = 0.75f;

//---------------------------------------------------------------------------------------------------------------------
// MenuRepeat
//
//   The held-key state behind the menu's value repeat: hold Left or Right and the selected item keeps counting.
//
//   We filter auto-repeat out of the action queue, because a held steering key has to queue ONE turn and not one
//   per repeat interval. So the menu's repeat is rebuilt here from held state and driven per frame, at the
//   operating system's own delay and rate. Same cadence the user has set, without the message flood. See the
//   note in Window.hpp.
//
//   The first step is NOT counted here - it is applied by the key-down action like any other keypress - so this
//   only produces the steps that follow once the key has been held past the repeat delay.
//---------------------------------------------------------------------------------------------------------------------

struct MenuRepeat
{
	int   direction        = 0;                 // -1, 0 or +1: the direction the held key is counting
	float secondsSinceStep = 0.0f;              // time since the last step, initial press included
	bool  hasRepeated      = false;             // false until the repeat delay has been served once
};

//---------------------------------------------------------------------------------------------------------------------
// FreeLookCamera
//
//   The F12 debug view's orbit camera: pitch, yaw and distance, driven from the numpad.
//
//   These are application input state and belong here rather than in the renderer, which is simply handed them
//   to draw with.
//
//   Distance is clamped to [0, grid size * 0.5 + 10]. The lower bound puts the camera inside the arena, which is
//   allowed - it is a debug view and being able to fly into the lattice is the point.
//---------------------------------------------------------------------------------------------------------------------

constexpr float FREE_LOOK_ROTATION_RATE = 40.0f;    // degrees per second
constexpr float FREE_LOOK_ZOOM_RATE     =  2.0f;    // units per second
constexpr float FREE_LOOK_DISTANCE_BASE = 10.0f;    // the +10 in the distance clamp and the entry value

struct FreeLookCamera
{
	float pitchDegrees = 0.0f;
	float yawDegrees   = 0.0f;
	float distance     = 0.0f;
};

//*********************************************************************************************************************
// FrontEnd
//*********************************************************************************************************************

class FrontEnd
{
	//=================================================================================================================
	// Data Members
	//=================================================================================================================

	private:

		Simulation     simulationState;
		GameSettings   settings;
		MenuModel      menuState;
		MenuRepeat     menuRepeatState;
		FreeLookCamera freeLookCamera;

		// How long the current end screen has been up, in seconds. Any key dismisses an end screen, so without
		// this the screen would vanish before it could be read: a player dies mid-turn with a hand on the arrow
		// keys, and the very next press throws away the score they just earned. See END_SCREEN_INPUT_DELAY.

		float endScreenSeconds = 0.0f;

	//=================================================================================================================
	// Accessors
	//=================================================================================================================

	public:

		Simulation&           simulation ()       { return simulationState; }
		const Simulation&     simulation () const { return simulationState; }
		const MenuModel&      menu       () const { return menuState;       }
		const FreeLookCamera& freeLook   () const { return freeLookCamera;  }

	//=================================================================================================================
	// Constructors
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Constructor 1/1
		//
		// Description:
		//
		//   Start at the menu. The initial settings seed the menu's values, so command line overrides show up in
		//   the menu rather than going round it.
		//
		// Arguments:
		//
		//   - initialSettings : Opponent count, grid size, seed and options for the first game. The counts are
		//                       expected to be within the menu ranges already.
		//
		//-------------------------------------------------------------------------------------------------------------

		explicit FrontEnd ( const GameSettings& initialSettings );

	//=================================================================================================================
	// Methods
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Method: handleAction
		//
		// Description:
		//
		//   Route one input action according to the current application state.
		//
		//   The same physical keys mean different things per state - the arrows steer in play but navigate at
		//   the menu - and a key outside its state does nothing at all.
		//
		// Arguments:
		//
		//   - action     : The queued input action.
		//   - renderer   : Receives the display-toggle effects (N / L keys, and the re-arms on view switch and
		//                  game start).
		//   - cameraMode : Updated by the view keys and by game start, which always begins in the cockpit.
		//
		// Returns:
		//
		//   - True when the action quits the application (Exit Game, or Escape at the menu with Exit Game already
		//     selected).
		//
		//-------------------------------------------------------------------------------------------------------------

		bool handleAction ( GameAction action, Renderer& renderer, CameraMode& cameraMode );

		//-------------------------------------------------------------------------------------------------------------
		// Method: startGame
		//
		// Description:
		//
		//   The Enter-on-Start-Game path: begin a game with the menu's values.
		//   Starting a game resets the view to cockpit and arms every display toggle on, so both
		//   are reproduced here on every start. Public so the self test can begin a game without a keypress.
		//
		// Arguments:
		//
		//   - renderer   : Receives the display-toggle re-arms.
		//   - cameraMode : Reset to the cockpit view.
		//
		//-------------------------------------------------------------------------------------------------------------

		void startGame ( Renderer& renderer, CameraMode& cameraMode );

		//-------------------------------------------------------------------------------------------------------------
		// Method: updateFreeLookCamera
		//
		// Description:
		//
		//   Advances the free-look orbit camera from the held keys. Does nothing outside the free-look state.
		//
		//   The camera moves at a rate per frame against the real frame delta, NOT one increment per key
		//   message. Drive it off the messages and the speed depends on the operating system's key repeat
		//   settings: it stalls for half a second after the first press and then jerks along. Per frame is what
		//   the arithmetic wants and it matches the fixed timestep the rest of the loop runs on.
		//
		// Arguments:
		//
		//   - input             : Held state of the six camera keys.
		//   - frameDeltaSeconds : Real seconds since the previous frame.
		//
		//-------------------------------------------------------------------------------------------------------------

		void updateFreeLookCamera ( const FreeLookInput& input, float frameDeltaSeconds );

		//-------------------------------------------------------------------------------------------------------------
		// Method: updateEndScreen
		//
		// Description:
		//
		//   Age the current end screen, so it knows when it is old enough to accept a key.
		//
		//   The timer resets whenever the application is in any other state, so each WINNER or GAME OVER screen
		//   starts its delay from zero. Call this once per frame, whatever the state.
		//
		// Arguments:
		//
		//   - frameDeltaSeconds : Real seconds since the previous frame.
		//
		//-------------------------------------------------------------------------------------------------------------

		void updateEndScreen ( float frameDeltaSeconds );

		//-------------------------------------------------------------------------------------------------------------
		// Method: updateMenuValueRepeat
		//
		// Description:
		//
		//   Keep the selected menu item counting while Left or Right is held.
		//
		//   Does nothing outside the menu state, which is what stops a held steering key repeating during play.
		//
		//   The initial step belongs to the key-down action, not to this - so the first repeat lands one full
		//   repeat delay after the press, exactly as auto-repeat does.
		//
		// Arguments:
		//
		//   - heldDirection          : -1 for Left, +1 for Right, 0 when neither is held.
		//   - frameDeltaSeconds      : Real seconds since the previous frame.
		//   - repeatDelaySeconds     : Hold time before the first repeat.
		//   - repeatIntervalSeconds  : Time between repeats after that.
		//
		//-------------------------------------------------------------------------------------------------------------

		void updateMenuValueRepeat ( int heldDirection, float frameDeltaSeconds,
		                             float repeatDelaySeconds, float repeatIntervalSeconds );

	private:

		bool handleMenuAction     ( GameAction action, Renderer& renderer, CameraMode& cameraMode );
		void handlePlayingAction  ( GameAction action, Renderer& renderer, CameraMode& cameraMode );
		void adjustMenuValue      ( int direction );

		void enterFreeLook        ();
		void leaveFreeLook        ( Renderer& renderer );
};

}
