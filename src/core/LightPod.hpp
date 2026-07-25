//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Light Pod
//
// Description:
//
//   One light pod: its orientation basis, its motion state, and its trail.
//
//   The one field worth a second look is the target position. It sits where you would expect a "previous position"
//   to go and it reads like one, but it is not. It is the NEXT grid point the pod is heading for, and the signed
//   separation between it and the current position is what fires every turn commit and every trail corner. Read it
//   the wrong way and nothing downstream makes sense.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <vector>

#include "Constants.hpp"
#include "Vector3.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Enumerations
//---------------------------------------------------------------------------------------------------------------------

enum class PodState
{
	Alive,
	Dying
};

enum class PodKind
{
	Player,
	Opponent
};

enum class Turn
{
	None  = 0,
	Up    = 1,
	Down  = 2,
	Left  = 3,
	Right = 4
};

//---------------------------------------------------------------------------------------------------------------------
// Turn iteration order
//
//   Both halves of the opponent AI probe the turn directions in this order and take the first that lands in bounds
//   on an empty cell. No randomness in the ordering, and no lookahead past a single cell. The wander roll enters
//   the same sequence at an offset, which is why the opponents lean toward pitching up.
//---------------------------------------------------------------------------------------------------------------------

constexpr Turn TURN_PROBE_ORDER [ 4 ] = { Turn::Up, Turn::Down, Turn::Left, Turn::Right };

//---------------------------------------------------------------------------------------------------------------------
// LightPod
//---------------------------------------------------------------------------------------------------------------------

struct LightPod
{
	//-----------------------------------------------------------------------------------------------------------------
	// Trail geometry.
	//
	//   The vector always holds cornerCount () + 1 points. The committed corners sit at 0 .. cornerCount - 1, and
	//   the back of the vector is a live tip that gets rewritten to the pod's current position every frame.
	//
	//   Corners go on when a turn commits, not once per frame, so this is a corner list and not a position history.
	//   It grows as it goes, so there is no buffer to run off the end of.
	//-----------------------------------------------------------------------------------------------------------------

	std::vector<Vector3> trail;

	//-----------------------------------------------------------------------------------------------------------------
	// Identity.
	//-----------------------------------------------------------------------------------------------------------------

	PodKind kind = PodKind::Opponent;
	int     id   = 0;                           // array index + 1. The player is always id 1.

	//-----------------------------------------------------------------------------------------------------------------
	// Motion state.
	//-----------------------------------------------------------------------------------------------------------------

	PodState state          = PodState::Alive;
	Vector3  position       {};
	Vector3  targetPosition {};                 // the NEXT grid point ahead, not a history
	Vector3  forward        {};
	Vector3  up             {};

	//-----------------------------------------------------------------------------------------------------------------
	// Queued turn.
	//
	//   Steering writes the post-turn basis here and the update loop picks it up at the next cell boundary. That
	//   delay is what makes the controls feel locked to the grid rather than continuous, and it is the most
	//   important thing about how the game handles. Do not make it immediate.
	//-----------------------------------------------------------------------------------------------------------------

	Vector3 pendingForward {};
	Vector3 pendingUp      {};
	Turn    turnPending    = Turn::None;

	//-----------------------------------------------------------------------------------------------------------------
	// Presentation and scoring.
	//-----------------------------------------------------------------------------------------------------------------

	float   speed          = 0.0f;
	Vector3 colour         {};
	int     score          = 0;
	int     deathCountdown = 1;

	//-----------------------------------------------------------------------------------------------------------------
	// Value Accessor: cornerCount
	//
	// Description:
	//
	//   Number of committed trail corners, not counting the live tip.
	//
	// Returns:
	//
	//   - The committed corner count.
	//
	//-----------------------------------------------------------------------------------------------------------------

	int cornerCount () const
	{
		return static_cast<int> ( trail.size () ) - 1;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Predicate Accessor: isAlive
	//
	// Returns:
	//
	//   - True while the pod is still running.
	//
	//-----------------------------------------------------------------------------------------------------------------

	bool isAlive () const
	{
		return state == PodState::Alive;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Predicate Accessor: hasValidTip
	//
	// Description:
	//
	//   On the frame a turn commits, the newly opened tip slot still holds the sentinel the trail was filled with
	//   before the game started. It does not get a real position until the frame after. The renderer must not draw
	//   a segment out to it in that window.
	//
	// Returns:
	//
	//   - True when the live tip holds a real position rather than the sentinel.
	//
	//-----------------------------------------------------------------------------------------------------------------

	bool hasValidTip () const
	{
		return !trail.empty () && ( trail.back () != TRAIL_SENTINEL_POINT );
	}
};

}
