//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   The front end controller. See FrontEnd.hpp for the state machine and the flow.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "FrontEnd.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Constructor 1/1
//---------------------------------------------------------------------------------------------------------------------

FrontEnd::FrontEnd ( const GameSettings& initialSettings )
{
	settings = initialSettings;

	menuState.opponentCount = settings.opponentCount;
	menuState.gridSize      = settings.gridSize;

	// The menu never shows an unplayable opponent count, whatever it was seeded with. The clamp lives here as
	// well as at the command line so the floor is a property of the menu itself rather than of one caller.

	if ( menuState.opponentCount < PLAYABLE_OPPONENT_COUNT_MINIMUM )
	{
		menuState.opponentCount = PLAYABLE_OPPONENT_COUNT_MINIMUM;
		settings.opponentCount  = PLAYABLE_OPPONENT_COUNT_MINIMUM;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::handleAction
//---------------------------------------------------------------------------------------------------------------------

bool FrontEnd::handleAction ( GameAction action, Renderer& renderer, CameraMode& cameraMode )
{
	switch ( simulationState.state () )
	{
		case AppState::Menu:

			return handleMenuAction ( action, renderer, cameraMode );

		case AppState::Playing:

			handlePlayingAction ( action, renderer, cameraMode );

			return false;

		case AppState::FreeLook:

			// In free-look, F12 returns to play and Escape drops back to the menu. The steering and zoom keys
			// are held state, handled by updateFreeLookCamera rather than as events.

			if ( action == GameAction::ToggleFreeLook )
			{
				leaveFreeLook ( renderer );
			}
			else if ( action == GameAction::Back )
			{
				// Escape out of free-look restores the player and writes the same thinned fog density the F12
				// exit does, then carries on to the menu.

				leaveFreeLook ( renderer );

				simulationState.setState ( AppState::Menu );
			}

			return false;

		case AppState::GameOver:
		case AppState::Winner:

			// Any key returns to the menu. The action itself does not matter here - what matters is that a key
			// was pressed at all, which is why unmapped keys arrive as GameAction::AnyKey rather than being
			// dropped by the window.
			//
			// Input is ignored for the first END_SCREEN_INPUT_DELAY seconds. Without that, the press already on
			// its way when the player crashed would dismiss the screen before they had read it.

			if ( endScreenSeconds >= END_SCREEN_INPUT_DELAY )
			{
				simulationState.setState ( AppState::Menu );
			}

			return false;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::enterFreeLook
//
// Description:
//
//   Enter the free-look view: take the player's pod out of play and reset the orbit camera - angles to zero and
//   distance to its maximum, so the view opens on the whole arena from outside.
//
//   The simulation keeps running. That is the entire point of the view: you watch the opponents play it out
//   without the player in the way.
//
//   The thinned fog belongs to the view itself and the renderer applies it while drawing, so there is nothing to
//   set here. Only the density written on the way OUT outlives the view, which is why leaveFreeLook takes the
//   renderer and this does not.
//
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::enterFreeLook ()
{
	simulationState.setState                 ( AppState::FreeLook );
	simulationState.setPlayerRemovedFromPlay ( true );

	freeLookCamera.pitchDegrees = 0.0f;
	freeLookCamera.yawDegrees   = 0.0f;
	freeLookCamera.distance     = ( simulationState.arenaExtent () * 0.5f ) + FREE_LOOK_DISTANCE_BASE;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::leaveFreeLook
//
// Description:
//
//   Leave the free-look view: back to play, and put the player's pod back.
//
//   The fog density written here is 0.25, NOT the 0.35 the three view projections install, and nothing restores
//   0.35 until a view key is next pressed. So the world stays a little clearer for a while after the debug view.
//   The renderer holds the density and the view keys reset it.
//
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::leaveFreeLook ( Renderer& renderer )
{
	simulationState.setState                 ( AppState::Playing );
	simulationState.setPlayerRemovedFromPlay ( false );

	renderer.setGameplayFogDensity ( FOG_DENSITY_AFTER_FREE_LOOK );

	freeLookCamera = FreeLookCamera {};
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::updateFreeLookCamera
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::updateFreeLookCamera ( const FreeLookInput& input, float frameDeltaSeconds )
{
	if ( simulationState.state () != AppState::FreeLook )
	{
		return;
	}

	const float rotationStep = FREE_LOOK_ROTATION_RATE * frameDeltaSeconds;
	const float zoomStep     = FREE_LOOK_ZOOM_RATE     * frameDeltaSeconds;

	// Pitch, about the x axis. Up decrements and down increments, and each direction wraps at its own end. The
	// underflow branch writes "360 - value" rather than "value + 360", which for a small negative value comes to
	// the same thing within a rounding step.

	if ( input.pitchUp )
	{
		freeLookCamera.pitchDegrees -= rotationStep;

		if ( freeLookCamera.pitchDegrees < 0.0f )
		{
			freeLookCamera.pitchDegrees = 360.0f - freeLookCamera.pitchDegrees;
		}
	}

	if ( input.pitchDown )
	{
		freeLookCamera.pitchDegrees += rotationStep;

		if ( freeLookCamera.pitchDegrees >= 360.0f )
		{
			freeLookCamera.pitchDegrees -= 360.0f;
		}
	}

	// Yaw, about the y axis.

	if ( input.yawLeft )
	{
		freeLookCamera.yawDegrees -= rotationStep;

		if ( freeLookCamera.yawDegrees < 0.0f )
		{
			freeLookCamera.yawDegrees = 360.0f - freeLookCamera.yawDegrees;
		}
	}

	if ( input.yawRight )
	{
		freeLookCamera.yawDegrees += rotationStep;

		if ( freeLookCamera.yawDegrees >= 360.0f )
		{
			freeLookCamera.yawDegrees -= 360.0f;
		}
	}

	// Distance. Numpad + moves in, numpad - moves out. The near clamp is zero, which puts the camera inside the
	// arena, and the far clamp is half the grid plus ten.

	const float distanceMaximum = ( simulationState.arenaExtent () * 0.5f ) + FREE_LOOK_DISTANCE_BASE;

	if ( input.moveIn )
	{
		freeLookCamera.distance -= zoomStep;

		if ( freeLookCamera.distance < 0.0f )
		{
			freeLookCamera.distance = 0.0f;
		}
	}

	if ( input.moveOut )
	{
		freeLookCamera.distance += zoomStep;

		if ( freeLookCamera.distance >= distanceMaximum )
		{
			freeLookCamera.distance = distanceMaximum;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::handleMenuAction
//
// Description:
//
//   The menu-state key dispatch. Navigation clamps at the ends rather than wrapping, and value adjustment
//   clamps to each item's own range.
//
// Arguments:
//
//   - action     : The queued input action.
//   - renderer   : Passed through to the game-start re-arms.
//   - cameraMode : Passed through to the game-start view reset.
//
// Returns:
//
//   - True when the action quits the application.
//
//---------------------------------------------------------------------------------------------------------------------

bool FrontEnd::handleMenuAction ( GameAction action, Renderer& renderer, CameraMode& cameraMode )
{
	switch ( action )
	{
		case GameAction::SteerUp:

			if ( menuState.selection > MENU_ITEM_FIRST )
			{
				menuState.selection--;
			}

			break;

		case GameAction::SteerDown:

			if ( menuState.selection < MENU_ITEM_LAST )
			{
				menuState.selection++;
			}

			break;

		case GameAction::SteerRight:

			adjustMenuValue ( 1 );

			break;

		case GameAction::SteerLeft:

			adjustMenuValue ( -1 );

			break;

		case GameAction::Activate:

			// Switch on the selection. The two value items carry no command, so Enter does nothing on them.

			if ( menuState.selection == MENU_ITEM_START_GAME )
			{
				startGame ( renderer, cameraMode );
			}
			else if ( menuState.selection == MENU_ITEM_RESET_GAME )
			{
				// Reset Game puts the menu values and the selection back to their defaults. (It also used to
				// rewrite the colour palette, clear the collision grid and respawn the
				// pods - all invisible from the menu, and all redone from scratch by the next game start, so the
				// default-constructed model reproduces everything the player could observe.)

				menuState = MenuModel {};
			}
			else if ( menuState.selection == MENU_ITEM_EXIT_GAME )
			{
				return true;                    // Exit Game ends the application
			}

			break;

		case GameAction::Back:

			// The first Escape moves the selection to Exit Game; a second one - Escape with Exit Game already
			// selected - quits. So Escape twice always gets you out, from anywhere in the menu.

			if ( menuState.selection == MENU_ITEM_EXIT_GAME )
			{
				return true;
			}

			menuState.selection = MENU_ITEM_EXIT_GAME;

			break;

		default:

			// Views, display toggles and straight-on are gameplay keys. At the menu they do nothing.

			break;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::handlePlayingAction
//
// Description:
//
//   The playing-state key dispatch: steering, the three views, the display toggles, and Escape back to the menu.
//
//   Escape from play goes straight to the menu. It does NOT stop at the GAME OVER screen - quitting a game you
//   are still playing is not losing it.
//
// Arguments:
//
//   - action     : The queued input action.
//   - renderer   : Receives the display-toggle effects.
//   - cameraMode : Updated by the view keys.
//
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::handlePlayingAction ( GameAction action, Renderer& renderer, CameraMode& cameraMode )
{
	switch ( action )
	{
		case GameAction::SteerUp:       simulationState.steerPlayer ( Turn::Up    ); break;
		case GameAction::SteerDown:     simulationState.steerPlayer ( Turn::Down  ); break;
		case GameAction::SteerLeft:     simulationState.steerPlayer ( Turn::Left  ); break;
		case GameAction::SteerRight:    simulationState.steerPlayer ( Turn::Right ); break;
		case GameAction::SteerStraight: simulationState.steerPlayer ( Turn::None  ); break;

		// Each view key installs its own projection, and every one of those three projections re-writes the fog
		// density to the gameplay 0.35 - which is the only thing that clears the 0.25 left behind by an earlier
		// trip through free-look.

		case GameAction::ViewCockpit:

			// Not purely cosmetic. The update loop reads the first person flag when it pulls the player's live
			// trail tip back to stop the wall clipping the camera, so the cockpit view really is simulation
			// state and not just a presentation choice.

			cameraMode = CameraMode::Cockpit;
			simulationState.setFirstPersonView ( true );
			renderer.setGameplayFogDensity ( FOG_DENSITY_GAMEPLAY );

			break;

		case GameAction::ViewThirdPerson:

			cameraMode = CameraMode::ThirdPerson;
			simulationState.setFirstPersonView ( false );
			renderer.setGameplayFogDensity ( FOG_DENSITY_GAMEPLAY );

			// Re-arm the own-trail flag on every switch to the third person view, so the player's full trail is
			// always visible on entering it.

			renderer.setThirdPersonTrailEnabled ( true );

			break;

		case GameAction::ViewSpotPlane:

			cameraMode = CameraMode::SpotPlane;
			simulationState.setFirstPersonView ( false );
			renderer.setGameplayFogDensity ( FOG_DENSITY_GAMEPLAY );

			break;

		case GameAction::ToggleFreeLook:

			enterFreeLook ();

			break;

		case GameAction::ToggleProximitySensor:

			renderer.toggleProximitySensor ();      // the N key

			break;

		case GameAction::ToggleThirdPersonTrail:

			renderer.toggleThirdPersonTrail ();     // the L key

			break;

		case GameAction::Back:

			simulationState.setState ( AppState::Menu );

			break;

		default:

			break;                                  // Enter does nothing in play (handler gated on the menu state)
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::updateEndScreen
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::updateEndScreen ( float frameDeltaSeconds )
{
	const AppState state = simulationState.state ();

	// Anything that is not an end screen resets the timer, so the next one starts its delay from zero.

	if ( ( state != AppState::GameOver ) && ( state != AppState::Winner ) )
	{
		endScreenSeconds = 0.0f;

		return;
	}

	endScreenSeconds += frameDeltaSeconds;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::updateMenuValueRepeat
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::updateMenuValueRepeat ( int heldDirection, float frameDeltaSeconds,
                                       float repeatDelaySeconds, float repeatIntervalSeconds )
{
	// Nothing is held, or the menu is not up. Either way the repeat is armed afresh by the next press.

	if ( ( simulationState.state () != AppState::Menu ) || ( heldDirection == 0 ) )
	{
		menuRepeatState = MenuRepeat {};

		return;
	}

	// A fresh press, or the other arrow taking the repeat over. The key-down action has already applied the first
	// step, so the timer starts from zero here and the first repeat is one full delay away.

	if ( heldDirection != menuRepeatState.direction )
	{
		menuRepeatState = MenuRepeat {};

		menuRepeatState.direction = heldDirection;

		return;
	}

	menuRepeatState.secondsSinceStep += frameDeltaSeconds;

	// Step for as long as the elapsed time covers - the delay for the first repeat, the interval for every one
	// after it. The loop rather than a single test is what keeps the count honest across a long frame; the
	// non-positive guard is what stops a pathological system setting spinning it forever.

	for ( ;; )
	{
		const float threshold = menuRepeatState.hasRepeated ? repeatIntervalSeconds : repeatDelaySeconds;

		if ( ( threshold <= 0.0f ) || ( menuRepeatState.secondsSinceStep < threshold ) )
		{
			break;
		}

		adjustMenuValue ( menuRepeatState.direction );

		menuRepeatState.secondsSinceStep -= threshold;
		menuRepeatState.hasRepeated       = true;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::adjustMenuValue
//
// Description:
//
//   Adjust the selected item's value by one step, clamped to its own range.
//
//   The command items have an empty range, so adjusting them does nothing. That is the same test the menu screen
//   uses to decide whether to draw a value at all: an item has one exactly when its minimum and maximum differ.
//
// Arguments:
//
//   - direction : +1 for Right, -1 for Left.
//
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::adjustMenuValue ( int direction )
{
	if ( menuState.selection == MENU_ITEM_OPPONENT_COUNT )
	{
		// The lower clamp is the playable floor of one opponent, NOT the zero the simulation would allow. See
		// PLAYABLE_OPPONENT_COUNT_MINIMUM in FrontEnd.hpp for why.

		menuState.opponentCount += direction;

		if ( menuState.opponentCount < PLAYABLE_OPPONENT_COUNT_MINIMUM ) { menuState.opponentCount = PLAYABLE_OPPONENT_COUNT_MINIMUM; }
		if ( menuState.opponentCount > MENU_OPPONENT_COUNT_MAXIMUM     ) { menuState.opponentCount = MENU_OPPONENT_COUNT_MAXIMUM;     }
	}
	else if ( menuState.selection == MENU_ITEM_GRID_SIZE )
	{
		menuState.gridSize += direction;

		if ( menuState.gridSize < MENU_GRID_SIZE_MINIMUM ) { menuState.gridSize = MENU_GRID_SIZE_MINIMUM; }
		if ( menuState.gridSize > MENU_GRID_SIZE_MAXIMUM ) { menuState.gridSize = MENU_GRID_SIZE_MAXIMUM; }
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: FrontEnd::startGame
//---------------------------------------------------------------------------------------------------------------------

void FrontEnd::startGame ( Renderer& renderer, CameraMode& cameraMode )
{
	settings.opponentCount = menuState.opponentCount;
	settings.gridSize      = menuState.gridSize;

	simulationState.startGame ( settings );

	// Any command line seed applies to the FIRST game only. From here on every game seeds off the clock.

	settings.randomSeed = 0;

	// Every game starts in the cockpit with all the display toggles armed on. (The P reticle toggle joins these
	// once the reticle exists.)

	cameraMode = CameraMode::Cockpit;

	simulationState.setFirstPersonView   ( true );
	renderer.setProximitySensorEnabled   ( true );
	renderer.setThirdPersonTrailEnabled  ( true );

	// Reset the fog, so a new game always starts at the gameplay density even if the last one ended in
	// free-look.

	renderer.setGameplayFogDensity ( FOG_DENSITY_GAMEPLAY );

	freeLookCamera = FreeLookCamera {};
}

}
