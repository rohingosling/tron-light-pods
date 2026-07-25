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
//   The simulation itself: world setup, motion, turning, collision, scoring and the opponent AI.
//
//   See Simulation.hpp for the interface and for the two standing warnings about the speed model and the timestep.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include <ctime>

#include "Math.hpp"
#include "Simulation.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// World setup
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::startGame
//
// Description:
//
//   Clear the world and place every pod.
//
//   Four details here are easy to miss and all four matter:
//
//     1. The trail starts with one committed corner, not zero. The spawn position is written as corner zero.
//     2. Speed starts at zero, so every pod accelerates from a standstill.
//     3. The death countdown starts at 1, which is why the decrement, which only ever runs inside the alive
//        branch, still correctly drops the alive count on the death frame.
//     4. The target position is seeded EQUAL to the spawn position, so the boundary test fires on the very first
//        frame and bootstraps the first real target. It is a neat trick and completely invisible until you look
//        for why the first frame behaves differently.
//
//   Three random draws are consumed per pod regardless of how many the chosen face actually uses. Skipping the
//   unused ones would desynchronise the generator and move every subsequent spawn.
//
// Arguments:
//
//   - settings : Opponent count, grid size and seed.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::startGame ( const GameSettings& settings )
{
	// World initialise.

	grid.clear ();

	arenaSize = static_cast<float> ( settings.gridSize );
	cellScale = CELL_SCALE;

	// Seed. The clock is the normal source; a caller supplied seed is what makes a run repeatable.

	if ( settings.randomSeed != 0 )
	{
		random.seedWith ( settings.randomSeed );
	}
	else
	{
		random.seedWith ( static_cast<std::uint32_t> ( std::time ( nullptr ) ) );
	}

	firstPersonView = true;

	grid.clear ();

	int podCount = settings.opponentCount + 1;

	if ( podCount > POD_MAX ) { podCount = POD_MAX; }
	if ( podCount < 1         ) { podCount = 1;         }

	pods.assign ( static_cast<std::size_t> ( podCount ), LightPod {} );

	const float farWall = arenaSize - 1.0f;
	int         face    = 0;

	for ( int i = 0; i < podCount; i++ )
	{
		LightPod& pod = pods [ static_cast<std::size_t> ( i ) ];

		pod.kind = ( i == 0 ) ? PodKind::Player : PodKind::Opponent;

		// All three draws are always taken. See the note in the description.

		float randomX = static_cast<float> ( random.below ( floatToInteger ( arenaSize ) ) );
		float randomY = static_cast<float> ( random.below ( floatToInteger ( arenaSize ) ) );
		float randomZ = static_cast<float> ( random.below ( floatToInteger ( arenaSize ) ) );

		// The opt-in interior clamp; see the note on GameSettings. Only the landing coordinates are adjusted -
		// the three draws above always happen, so the generator stays in step whether the clamp is on or off
		// and a given seed still identifies one game.

		if ( settings.spawnAwayFromEdges )
		{
			const float interiorMinimum = 1.0f;
			const float interiorMaximum = arenaSize - 2.0f;

			auto clampToInterior = [ = ] ( float value )
			{
				return ( value < interiorMinimum ) ? interiorMinimum
				     : ( value > interiorMaximum ) ? interiorMaximum
				                                   : value;
			};

			randomX = clampToInterior ( randomX );
			randomY = clampToInterior ( randomY );
			randomZ = clampToInterior ( randomZ );
		}

		switch ( face )
		{
			case 0:                             // Near face on z, heading toward positive z.

				pod.position = Vector3 { randomX, randomY, 0.0f };
				pod.forward  = Vector3 { 0.0f, 0.0f, 1.0f };
				pod.up       = Vector3 { 0.0f, 1.0f, 0.0f };
				pod.colour   = POD_PALETTE [ 1 ];
				break;

			case 1:                             // Far face on z, heading toward negative z.

				pod.position = Vector3 { randomX, randomY, farWall };
				pod.forward  = Vector3 { 0.0f, 0.0f, -1.0f };
				pod.up       = Vector3 { 0.0f, 1.0f,  0.0f };
				pod.colour   = POD_PALETTE [ 0 ];
				break;

			case 2:                             // Near face on x, heading toward positive x.

				pod.position = Vector3 { 0.0f, randomY, randomZ };
				pod.forward  = Vector3 { 1.0f, 0.0f, 0.0f };
				pod.up       = Vector3 { 0.0f, 1.0f, 0.0f };
				pod.colour   = POD_PALETTE [ 2 ];
				break;

			case 3:                             // Far face on x, heading toward negative x.

				pod.position = Vector3 { farWall, randomY, randomZ };
				pod.forward  = Vector3 { -1.0f, 0.0f, 0.0f };
				pod.up       = Vector3 {  0.0f, 1.0f, 0.0f };
				pod.colour   = POD_PALETTE [ 3 ];
				break;

			case 4:                             // Near face on y, heading toward positive y.

				pod.position = Vector3 { randomX, 0.0f, randomZ };
				pod.forward  = Vector3 { 0.0f, 1.0f, 0.0f };
				pod.up       = Vector3 { 0.0f, 0.0f, 1.0f };
				pod.colour   = POD_PALETTE [ 1 ];
				break;

			case 5:                             // Far face on y, heading toward negative y.

				pod.position = Vector3 { randomX, farWall, randomZ };
				pod.forward  = Vector3 { 0.0f, -1.0f, 0.0f };
				pod.up       = Vector3 { 0.0f,  0.0f, 1.0f };
				pod.colour   = POD_PALETTE [ 0 ];
				break;

			default:
				break;
		}

		// The queued basis starts equal to the live basis, and the spawn point is the first committed corner.
		// The trailing sentinel is the live tip slot, which the first frame of motion will overwrite.

		pod.pendingForward = pod.forward;
		pod.pendingUp      = pod.up;
		pod.targetPosition = pod.position;

		pod.trail.clear ();
		pod.trail.push_back ( pod.position );
		pod.trail.push_back ( TRAIL_SENTINEL_POINT );

		face++;

		if ( face >= SPAWN_FACE_COUNT )
		{
			face = 0;
		}

		pod.state          = PodState::Alive;
		pod.id             = i + 1;
		pod.turnPending    = Turn::None;
		pod.speed          = 0.0f;
		pod.score          = 0;
		pod.deathCountdown = 1;
	}

	// Hoisted out of the spawn loop above. Every iteration wrote the same values and the loop always runs at
	// least once, so there is nothing to be gained by doing it per pod.

	aliveCount         = podCount;
	playerAccelerating = false;
	playerBraking      = false;
	steerCallCount     = 0;
	applicationState   = AppState::Playing;
}

//---------------------------------------------------------------------------------------------------------------------
// Steering
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::steerPod
//
// Description:
//
//   Queue a turn by writing the post-turn basis into the pending fields. Nothing is applied here. The update
//   loop picks it up at the next cell boundary, which is what makes the controls feel locked to the grid rather
//   than continuous.
//
//   The basis vectors are always axis aligned unit vectors, so every turn is exact and orientation never picks up
//   any floating point drift. A game can run as long as it likes without the pods going crooked.
//
//   The yaw cases write the pending forward and leave the pending up alone, even though the update loop copies
//   both. That is not an oversight. pendingUp == up holds after every turn, because a pitch writes both and a yaw
//   does not change up at all, so the store would be redundant.
//
// Arguments:
//
//   - pod     : The pod to steer.
//   - direction : The turn to queue.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::steerPod ( LightPod& pod, Turn direction )
{
	steerCallCount++;

	const float forwardX = pod.forward.x;
	const float forwardY = pod.forward.y;
	const float forwardZ = pod.forward.z;
	const float upX      = pod.up.x;
	const float upY      = pod.up.y;
	const float upZ      = pod.up.z;

	switch ( direction )
	{
		case Turn::None:

			pod.pendingForward = pod.forward;
			pod.pendingUp      = pod.up;
			pod.turnPending    = Turn::None;
			break;

		case Turn::Up:

			pod.pendingForward = pod.up;
			pod.pendingUp      = -pod.forward;
			pod.turnPending    = Turn::Up;
			break;

		case Turn::Down:

			pod.pendingForward = -pod.up;
			pod.pendingUp      = pod.forward;
			pod.turnPending    = Turn::Down;
			break;

		case Turn::Left:

			// Yaw about whichever axis the up vector currently occupies.

			if ( ( upX == 1.0f ) || ( upX == -1.0f ) )
			{
				pod.pendingForward = Vector3 { 0.0f, -forwardZ * upX, forwardY * upX };
			}

			if ( ( upY == 1.0f ) || ( upY == -1.0f ) )
			{
				pod.pendingForward = Vector3 { forwardZ * upY, 0.0f, -forwardX * upY };
			}

			if ( ( upZ == 1.0f ) || ( upZ == -1.0f ) )
			{
				pod.pendingForward = Vector3 { -forwardY * upZ, forwardX * upZ, 0.0f };
			}

			pod.turnPending = Turn::Left;
			break;

		case Turn::Right:

			if ( ( upX == 1.0f ) || ( upX == -1.0f ) )
			{
				pod.pendingForward = Vector3 { 0.0f, forwardZ * upX, -forwardY * upX };
			}

			if ( ( upY == 1.0f ) || ( upY == -1.0f ) )
			{
				pod.pendingForward = Vector3 { -forwardZ * upY, 0.0f, forwardX * upY };
			}

			if ( ( upZ == 1.0f ) || ( upZ == -1.0f ) )
			{
				pod.pendingForward = Vector3 { forwardY * upZ, -forwardX * upZ, 0.0f };
			}

			pod.turnPending = Turn::Right;
			break;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::steerPlayer
//---------------------------------------------------------------------------------------------------------------------

void Simulation::steerPlayer ( Turn direction )
{
	if ( !pods.empty () )
	{
		steerPod ( pods.front (), direction );
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::probeTurn
//
// Description:
//
//   Answer the question the opponent AI keeps asking: which cell would I enter if I committed this turn right
//   now. Nothing is mutated.
//
//   The offsets applied here are the same post-turn forward vectors steerPod would install, added to the
//   snapped cell ahead, so probe and steer agree by construction.
//
//   The yaw cases fall through to a zeroed result if no component of up is exactly plus or minus one. That cannot
//   happen while the basis stays axis aligned, which it always does, but leaving the result undefined in the
//   unreachable branch is asking for trouble later, so it is set explicitly.
//
// Arguments:
//
//   - pod     : The pod to probe from. Not modified.
//   - direction : The turn to evaluate.
//
// Returns:
//
//   - The cell the pod would enter.
//
//---------------------------------------------------------------------------------------------------------------------

Vector3 Simulation::probeTurn ( const LightPod& pod, Turn direction ) const
{
	const float forwardX = pod.forward.x;
	const float forwardY = pod.forward.y;
	const float forwardZ = pod.forward.z;
	const float upX      = pod.up.x;
	const float upY      = pod.up.y;
	const float upZ      = pod.up.z;

	const Vector3 snapped = snapAgainstTravel ( pod.position + pod.forward, pod.forward );

	Vector3 result {};

	switch ( direction )
	{
		case Turn::Up:

			result = snapped + pod.up;
			break;

		case Turn::Down:

			result = snapped - pod.up;
			break;

		case Turn::Left:

			if ( ( upX == 1.0f ) || ( upX == -1.0f ) )
			{
				result = Vector3 { snapped.x, snapped.y - forwardZ * upX, snapped.z + forwardY * upX };
			}

			if ( ( upY == 1.0f ) || ( upY == -1.0f ) )
			{
				result = Vector3 { snapped.x + forwardZ * upY, snapped.y, snapped.z - forwardX * upY };
			}

			if ( ( upZ == 1.0f ) || ( upZ == -1.0f ) )
			{
				result = Vector3 { snapped.x - forwardY * upZ, snapped.y + forwardX * upZ, snapped.z };
			}

			break;

		case Turn::Right:

			if ( ( upX == 1.0f ) || ( upX == -1.0f ) )
			{
				result = Vector3 { snapped.x, snapped.y + forwardZ * upX, snapped.z - forwardY * upX };
			}

			if ( ( upY == 1.0f ) || ( upY == -1.0f ) )
			{
				result = Vector3 { snapped.x - forwardZ * upY, snapped.y, snapped.z + forwardX * upY };
			}

			if ( ( upZ == 1.0f ) || ( upZ == -1.0f ) )
			{
				result = Vector3 { snapped.x + forwardY * upZ, snapped.y - forwardX * upZ, snapped.z };
			}

			break;

		case Turn::None:
		default:
			break;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
// Collision
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::killPod
//---------------------------------------------------------------------------------------------------------------------

void Simulation::killPod ( LightPod& pod )
{
	pod.state = PodState::Dying;

	grid.purgeOwner ( static_cast<std::uint8_t> ( pod.id ) );

	if ( pod.id == 1 )
	{
		applicationState = AppState::GameOver;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::checkCollision
//
// Description:
//
//   Test the pod against the arena bounds, and then against the collision grid.
//
//   The two tests are independent and BOTH always run. We do not skip the grid test after a bounds death, so a
//   pod that leaves the arena can still be recorded as a trail kill for whoever owns the cell it snapped to.
//   Leave it that way.
//
//   The grid hit is gated on the separation being non-negative, so it only counts at a cell boundary. A trail
//   kill credits the OWNER of the occupied cell, which is how the scoring rewards laying a wall someone else runs
//   into; a pod that runs into its own trail therefore credits itself.
//
// Arguments:
//
//   - pod : The pod to test. Killed in place if it has collided.
//
// Returns:
//
//   - True if the pod died during this call.
//
//---------------------------------------------------------------------------------------------------------------------

bool Simulation::checkCollision ( LightPod& pod )
{
	bool collided = false;

	const float boundLow  = -cellScale * 0.5f;
	const float boundHigh = ( cellScale * 0.5f + arenaSize ) - 1.0f;

	// Arena bounds.

	if ( ( pod.position.x < boundLow ) || ( pod.position.x > boundHigh ) ||
	     ( pod.position.y < boundLow ) || ( pod.position.y > boundHigh ) ||
	     ( pod.position.z < boundLow ) || ( pod.position.z > boundHigh ) )
	{
		killPod ( pod );

		collided = true;
	}

	// Collision grid.

	const float   distance = separation ( pod.position, pod.targetPosition, pod.forward );
	const Vector3 snapped  = snapAgainstTravel ( pod.position, pod.forward );

	const std::uint8_t occupant = grid.occupantAt ( floatToInteger ( snapped.x ),
	                                                floatToInteger ( snapped.y ),
	                                                floatToInteger ( snapped.z ) );

	if ( ( occupant != 0 ) && ( distance >= 0.0f ) )
	{
		killPod ( pod );

		collided = true;

		// Credit the owner of the wall that did the killing.

		pods [ static_cast<std::size_t> ( occupant - 1 ) ].score += SCORE_PER_KILL;
	}

	return collided;
}

//---------------------------------------------------------------------------------------------------------------------
// Opponent AI
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::isCellInPlayableRange
//
// Description:
//
//   MIND THE MINUS ONE. The bound the opponent AI tests against is arenaSize - 1, not arenaSize.
//
//   Drop the - 1 and the AI notices a wall exactly one cell too late, which is late enough that the queued turn
//   cannot commit before the pod crosses the death boundary. The game still plays, the opponents still look
//   like they are dodging, and the behaviour is completely different. It is not the sort of mistake you catch by
//   watching it.
//
// Arguments:
//
//   - cell : The cell coordinate to test.
//
// Returns:
//
//   - True if the cell lies within the range the AI treats as playable.
//
//---------------------------------------------------------------------------------------------------------------------

bool Simulation::isCellInPlayableRange ( const Vector3& cell ) const
{
	const float boundLow  = 0.0f;
	const float boundHigh = arenaSize - 1.0f;

	return ( cell.x >= boundLow ) && ( cell.x <= boundHigh ) &&
	       ( cell.y >= boundLow ) && ( cell.y <= boundHigh ) &&
	       ( cell.z >= boundLow ) && ( cell.z <= boundHigh );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::avoidObstacle
//
// Description:
//
//   The reactive half of the opponent AI, and entirely deterministic.
//
//   Two independent passes, one for the arena wall and one for an occupied cell. Each tries the four turn
//   directions in fixed order and takes the first that lands in bounds on an empty cell. No randomness, and no
//   lookahead past a single cell. That is the whole of an opponent's survival instinct.
//
//   The wall pass falls through into the occupancy pass even after committing a turn, so both always run.
//
// Arguments:
//
//   - pod : The pod to steer.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::avoidObstacle ( LightPod& pod )
{
	// The AI snaps ALONG the direction of travel, landing one cell further than the motion code does. That is
	// lookahead, not a defect: the AI needs the cell it is about to enter, the motion code needs the grid point
	// it has just left.

	const Vector3 cellAhead = snapAlongTravel ( pod.position + pod.forward, pod.forward );

	// Pass one. Steer away from the arena wall.

	if ( !isCellInPlayableRange ( cellAhead ) )
	{
		for ( const Turn direction : TURN_PROBE_ORDER )
		{
			const Vector3 probe = probeTurn ( pod, direction );

			if ( isCellInPlayableRange ( probe ) &&
			     grid.isEmptyAt ( floatToInteger ( probe.x ),
			                      floatToInteger ( probe.y ),
			                      floatToInteger ( probe.z ) ) )
			{
				steerPod ( pod, direction );
				break;
			}
		}
	}

	// Pass two. Steer away from an occupied cell directly ahead.
	//
	// Range check the cell before the grid lookup. This matters more than it looks.
	//
	// Pass two fires whenever pass one just did, because the thing that fires pass one is precisely that the cell
	// ahead is outside the arena. Index the grid with that coordinate unchecked and the read either falls off the
	// end of the array, or - when only one axis is out by one - wraps into a completely unrelated cell. Either
	// way it can come back non-zero and drive a second steer.
	//
	// That second steer would be redundant anyway. Pass two probes the same four directions in the same order,
	// from a pod whose basis pass one did not touch (steering writes only the pending fields), against a grid
	// that has not changed. It picks the direction pass one already picked and writes the same pending basis
	// again. So the check costs nothing and removes an out of bounds read.

	if ( !grid.isEmptyAt ( floatToInteger ( cellAhead.x ),
	                       floatToInteger ( cellAhead.y ),
	                       floatToInteger ( cellAhead.z ) ) )
	{
		for ( const Turn direction : TURN_PROBE_ORDER )
		{
			const Vector3 probe = probeTurn ( pod, direction );

			if ( isCellInPlayableRange ( probe ) &&
			     grid.isEmptyAt ( floatToInteger ( probe.x ),
			                      floatToInteger ( probe.y ),
			                      floatToInteger ( probe.z ) ) )
			{
				steerPod ( pod, direction );
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::wander
//
// Description:
//
//   The exploratory half of the opponent AI.
//
//   Runs only when the way ahead is clear, so it can never override avoidance. A roll of rand () % 500 selects a
//   starting direction, and rolls of 4 and above do nothing at all. That gives each opponent roughly a four in
//   five hundred chance per frame of attempting a turn, biased toward pitching up, with the cascade falling
//   through to the remaining directions when the preferred one is blocked.
//
// Arguments:
//
//   - pod : The pod to steer.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::wander ( LightPod& pod )
{
	const Vector3 cellAhead = snapAlongTravel ( pod.position + pod.forward, pod.forward );

	// Defer to avoidance whenever the way ahead is not clear.

	if ( !isCellInPlayableRange ( cellAhead ) )
	{
		return;
	}

	if ( !grid.isEmptyAt ( floatToInteger ( cellAhead.x ),
	                       floatToInteger ( cellAhead.y ),
	                       floatToInteger ( cellAhead.z ) ) )
	{
		return;
	}

	// Roll for a turn.

	const int roll = random.below ( WANDER_ROLL_RANGE );

	if ( roll > 3 )
	{
		return;
	}

	// The roll selects where in the fixed probe order the cascade begins, which is what biases the AI toward
	// pitching up: a roll of zero starts at Turn::Up, and only a roll of three starts at Turn::Right.

	for ( int i = roll; i < 4; i++ )
	{
		const Turn    direction = TURN_PROBE_ORDER [ i ];
		const Vector3 probe     = probeTurn ( pod, direction );

		if ( isCellInPlayableRange ( probe ) &&
		     grid.isEmptyAt ( floatToInteger ( probe.x ),
		                      floatToInteger ( probe.y ),
		                      floatToInteger ( probe.z ) ) )
		{
			steerPod ( pod, direction );
			return;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Motion
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::updateSpeed
//
// Description:
//
//   The player coasts between a floor and a ceiling and responds to the accelerate and brake keys; opponents
//   simply ramp toward the maximum.
//
//   The deceleration constant is NEGATIVE, so the first branch bleeds speed rather than building it.
//
//   When the player's speed drops below the floor it gets REFLECTED about the floor rather than clamped to it.
//   That is odd, but it gives the pod a small kick back up off the bottom instead of pinning it there, so leave
//   it alone.
//
// Arguments:
//
//   - pod     : The pod to update.
//   - index     : Its index. Only index zero is the player.
//   - deltaTime : Seconds elapsed.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::updateSpeed ( LightPod& pod, int index, float deltaTime )
{
	if ( index == 0 )
	{
		if ( pod.speed >= SPEED_MINIMUM )
		{
			pod.speed += deltaTime * SPEED_DECELERATION;

			if ( pod.speed < SPEED_MINIMUM )
			{
				pod.speed = ( SPEED_MINIMUM * 2.0f ) - pod.speed;
			}
		}
		else
		{
			pod.speed += deltaTime * SPEED_ACCELERATION;
		}

		if ( playerAccelerating )
		{
			pod.speed += deltaTime * SPEED_ACCELERATION * 2.0f;

			if ( pod.speed > SPEED_MAXIMUM )
			{
				pod.speed = SPEED_MAXIMUM;
			}
		}

		if ( playerBraking )
		{
			pod.speed += deltaTime * SPEED_DECELERATION * 3.0f;

			if ( pod.speed < SPEED_MINIMUM )
			{
				pod.speed = SPEED_MINIMUM;
			}
		}
	}
	else
	{
		pod.speed += deltaTime * SPEED_ACCELERATION;

		if ( pod.speed > SPEED_MAXIMUM )
		{
			pod.speed = SPEED_MAXIMUM;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::advancePod
//
// Description:
//
//   One living pod, one frame. The order of operations is integrate, record the trail tip, test collision,
//   claim the grid cell, then commit any queued turn at the cell boundary.
//
//   The step distance is computed from the speed as it stood on ENTRY, before the speed update, which means every
//   frame moves the pod at last frame's speed. That one frame of lag is deliberate - the handling is tuned around
//   this ordering, so do not hoist the speed update above it.
//
// Arguments:
//
//   - index     : The pod's index.
//   - deltaTime : Seconds elapsed.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::advancePod ( int index, float deltaTime )
{
	LightPod& pod = pods [ static_cast<std::size_t> ( index ) ];

	const float stepDistance = deltaTime * pod.speed;

	updateSpeed ( pod, index, deltaTime );

	// Integrate.

	pod.position += pod.forward * stepDistance;

	// Move the live trail tip to the current position. In the first person view the player's tip is pulled back
	// along the forward axis so the wall does not clip through the camera.

	if ( ( index == 0 ) && firstPersonView )
	{
		pod.trail.back () = pod.position - ( pod.forward * FIRST_PERSON_TRAIL_PULLBACK );
	}
	else
	{
		pod.trail.back () = pod.position;
	}

	// Collision.

	checkCollision ( pod );

	// Claim the grid cell this pod now occupies. Snapped, never rounded: marking on a naively rounded position
	// claims the next cell at the halfway point and the pod then collides with itself on arrival.

	if ( pod.state == PodState::Alive )
	{
		const Vector3 snapped = snapAgainstTravel ( pod.position, pod.forward );

		grid.claim ( floatToInteger ( snapped.x ),
		             floatToInteger ( snapped.y ),
		             floatToInteger ( snapped.z ),
		             static_cast<std::uint8_t> ( index + 1 ) );
	}

	// Commit a queued turn at the cell boundary. The separation stays negative until the pod reaches the grid
	// point it is heading for, so this fires exactly once per cell.

	const float distance = separation ( pod.position, pod.targetPosition, pod.forward );

	if ( distance >= 0.0f )
	{
		pod.score += SCORE_PER_STEP;

		if ( pod.turnPending != Turn::None )
		{
			// Land exactly on the grid point before turning. Writing the snapped position back over the live one
			// here is what stops sub-cell drift accumulating across a run. Note this uses the OLD forward vector,
			// before the pending basis is installed.

			pod.position = snapAgainstTravel ( pod.position, pod.forward );

			pod.forward = pod.pendingForward;
			pod.up      = pod.pendingUp;

			pod.turnPending = Turn::None;

			// The turn adjustment is negative, so a turn actually COSTS speed, and costs opponents twice as
			// much. That is deliberate, not a sign error.

			pod.speed += ( index == 0 ) ? SPEED_TURN_BONUS : ( SPEED_TURN_BONUS * 2.0f );

			if ( pod.speed < SPEED_MINIMUM )
			{
				pod.speed = SPEED_MINIMUM;
			}

			// The corner is a permanent trail vertex, so commit the tip and open a fresh one. The new tip holds
			// the sentinel until the next frame's motion overwrites it.

			pod.trail.back () = pod.position;
			pod.trail.push_back ( TRAIL_SENTINEL_POINT );
		}

		// Aim at the next grid point along the new heading.
		//
		// The target is snapped, not raw. Taking position plus forward without snapping leaves the sub-cell
		// offset from the previous step baked into the target, so the boundary test fires a frame later than the
		// grid marking crosses the same integer. That one frame of disagreement is enough for a pod to read the
		// cell it claimed itself and die on an empty grid.

		pod.targetPosition = snapAgainstTravel ( pod.position + pod.forward, pod.forward );
	}

	// Retire a dying opponent once its countdown expires.

	if ( ( pod.state == PodState::Dying ) && ( index > 0 ) )
	{
		pod.deathCountdown--;

		if ( pod.deathCountdown < 1 )
		{
			aliveCount--;
		}
	}

	// Last one standing wins.

	if ( ( aliveCount < 2 ) && ( pods.size () > 1 ) )
	{
		pods.front ().state = PodState::Dying;

		grid.purgeOwner ( 1 );

		applicationState = AppState::Winner;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Simulation::update
//
// Description:
//
//   Advance the whole world by one frame.
//
//   The opponent AI runs for every pod above index zero whether or not it is still alive. Skipping the dead
//   ones is the obvious tidy-up and it would desynchronise the generator, because a dead opponent still burns a
//   wander roll every frame. Leave them in.
//
// Arguments:
//
//   - deltaTime : Seconds elapsed.
//
//---------------------------------------------------------------------------------------------------------------------

void Simulation::update ( float deltaTime )
{
	const int podCount = static_cast<int> ( pods.size () );

	for ( int i = 0; i < podCount; i++ )
	{
		if ( pods [ static_cast<std::size_t> ( i ) ].state == PodState::Alive )
		{
			advancePod ( i, deltaTime );
		}

		// Opponent behaviour. Avoidance first, so wander can never override it.

		if ( i > 0 )
		{
			LightPod& pod = pods [ static_cast<std::size_t> ( i ) ];

			avoidObstacle ( pod );
			wander        ( pod );
		}
	}
}

}
