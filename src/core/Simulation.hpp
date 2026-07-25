//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Simulation
//
// Description:
//
//   The whole game simulation as one self-contained object. It owns the arena, the pods and the collision grid,
//   and it knows nothing about windows, input or drawing.
//
//   Two things in the speed model look like sign errors and are not. Coasting BLEEDS speed rather than building
//   it, and turning COSTS speed rather than granting it, at double the rate for opponents. Leave both alone.
//
//   TIMING. Drive this on a fixed timestep. Do not feed it a wall clock delta measured per frame: the tick counter
//   has a resolution of about fifteen milliseconds, so on a fast machine several frames land in the same tick, and
//   anything that reuses the last delta when the measurement comes back zero will run the game at a ludicrous
//   speed. The application owns an accumulator for exactly this reason.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>

#include "RandomGenerator.hpp"
#include "Constants.hpp"
#include "Grid.hpp"
#include "LightPod.hpp"
#include "Vector3.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// AppState
//
//   The simulation runs in the playing state and nowhere else. That gate matters: let the update run on past a
//   game over and the victory check will overwrite it, handing the player a win they never had.
//---------------------------------------------------------------------------------------------------------------------

enum class AppState
{
	Menu,
	Playing,
	FreeLook,
	GameOver,
	Winner
};

//---------------------------------------------------------------------------------------------------------------------
// GameSettings
//
//   Everything the menu can configure. The defaults are the game as it plays out of the box.
//---------------------------------------------------------------------------------------------------------------------

struct GameSettings
{
	int           opponentCount = MENU_OPPONENT_COUNT_DEFAULT;
	int           gridSize      = MENU_GRID_SIZE_DEFAULT;
	std::uint32_t randomSeed    = 0;            // zero seeds from the clock

	// Optional, and off by default. Each spawn rolls its two in-face coordinates over the full 0 .. grid-1 range,
	// so a pod can land on the outermost row of its face and end up running flush along the arena wall with no
	// grid at all on one side of it. Set this and the rolled coordinates are clamped to 1 .. grid-2 instead, which
	// gives every spawn turning room on both sides.
	//
	// The draws themselves are untouched, only the landing spot is clamped, so the generator stays in step either
	// way. The heading needs no help: it is always the exact inward face normal, straight into the grid.

	bool spawnAwayFromEdges = false;
};

//*********************************************************************************************************************
// Simulation
//*********************************************************************************************************************

class Simulation
{
	//=================================================================================================================
	// Data Members
	//=================================================================================================================

	private:

		Grid                  grid;
		std::vector<LightPod> pods;
		RandomGenerator       random;

		float    arenaSize          = 0.0f;     // an extent, not a cell count
		float    cellScale          = CELL_SCALE;
		int      aliveCount         = 0;
		AppState applicationState   = AppState::Menu;

		bool     playerAccelerating = false;
		bool     playerBraking      = false;
		bool     firstPersonView    = true;

		// Diagnostic only. Counting steer decisions is a cheap way to see whether two runs are making the same
		// choices, long before anything visible diverges.

		long long steerCallCount = 0;

	//=================================================================================================================
	// Accessors
	//=================================================================================================================

	public:

		const std::vector<LightPod>& allPods        () const { return pods; }
		const Grid&                  collisionGrid  () const { return grid; }
		float                        arenaExtent    () const { return arenaSize; }
		float                        arenaCellScale () const { return cellScale; }
		int                          alivePodCount  () const { return aliveCount; }
		AppState                     state          () const { return applicationState; }
		long long                    steerCount     () const { return steerCallCount; }
		bool                         isFirstPerson  () const { return firstPersonView; }

		//-------------------------------------------------------------------------------------------------------------
		// Value Accessor: player
		//
		// Description:
		//
		//   The player's pod, which is always index 0. Only valid once a game has started.
		//
		// Returns:
		//
		//   - A const reference to the player's pod.
		//
		//-------------------------------------------------------------------------------------------------------------

		const LightPod& player () const
		{
			return pods.front ();
		}

		//-------------------------------------------------------------------------------------------------------------
		// Predicate Accessor: isPlaying
		//
		// Returns:
		//
		//   - True while a game is in progress.
		//
		//-------------------------------------------------------------------------------------------------------------

		bool isPlaying () const
		{
			return applicationState == AppState::Playing;
		}

	//=================================================================================================================
	// Mutators
	//=================================================================================================================

	public:

		void setPlayerAccelerating ( bool value ) { playerAccelerating = value; }
		void setPlayerBraking      ( bool value ) { playerBraking      = value; }
		void setFirstPersonView    ( bool value ) { firstPersonView    = value; }
		void setState              ( AppState s ) { applicationState   = s;     }

		//-------------------------------------------------------------------------------------------------------------
		// Mutator: setPlayerRemovedFromPlay
		//
		// Description:
		//
		//   Take the player's pod out of the simulation, or put it back. This is what the free-look view needs:
		//   the player stops moving, stops laying trail and cannot collide, while every opponent carries on. That
		//   is the whole point of the view.
		//
		//   Note what this does NOT touch: the alive count. Leave it alone. If it were decremented here the
		//   victory test would fire the moment the player stepped out, and the count would be wrong when they
		//   stepped back in.
		//
		// Arguments:
		//
		//   - removed : True to take the player out of play, false to put them back.
		//
		//-------------------------------------------------------------------------------------------------------------

		void setPlayerRemovedFromPlay ( bool removed )
		{
			if ( pods.empty () )
			{
				return;
			}

			pods.front ().state = removed ? PodState::Dying : PodState::Alive;
		}

	//=================================================================================================================
	// Methods
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Method: startGame
		//
		// Description:
		//
		//   Clear the arena and place every pod.
		//
		//   Pods go on the six faces of the arena cube in rotation, each heading inward along the face normal, at
		//   a position randomised on the two axes lying in that face. With more than six pods the rotation wraps
		//   and faces get reused.
		//
		// Arguments:
		//
		//   - settings : Opponent count, grid size and seed.
		//
		//-------------------------------------------------------------------------------------------------------------

		void startGame ( const GameSettings& settings );

		//-------------------------------------------------------------------------------------------------------------
		// Method: update
		//
		// Description:
		//
		//   Advance every pod by one frame.
		//
		//   Do not call this outside the playing state. Running on past a game over lets the victory check
		//   overwrite the result.
		//
		// Arguments:
		//
		//   - deltaTime : Seconds elapsed. Use a fixed step; see the note in the file header.
		//
		//-------------------------------------------------------------------------------------------------------------

		void update ( float deltaTime );

		//-------------------------------------------------------------------------------------------------------------
		// Method: steerPlayer
		//
		// Description:
		//
		//   Queue a turn for the player. Nothing is applied here. The update loop picks the turn up at the next
		//   cell boundary.
		//
		// Arguments:
		//
		//   - direction : The turn to queue.
		//
		//-------------------------------------------------------------------------------------------------------------

		void steerPlayer ( Turn direction );

	private:

		void    steerPod       ( LightPod& pod, Turn direction );
		Vector3 probeTurn      ( const LightPod& pod, Turn direction ) const;
		bool    checkCollision ( LightPod& pod );
		void    avoidObstacle  ( LightPod& pod );
		void    wander         ( LightPod& pod );

		void    advancePod     ( int index, float deltaTime );
		void    updateSpeed    ( LightPod& pod, int index, float deltaTime );
		void    killPod        ( LightPod& pod );

		bool    isCellInPlayableRange ( const Vector3& cell ) const;
};

}
