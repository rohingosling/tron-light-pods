//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Maths
//
// Description:
//
//   The small arithmetic helpers the simulation is built on.
//
//   These are not general purpose utilities and they should not be tidied into general purpose utilities. Each one
//   makes a specific choice that looks wrong until you know why it is not, and the game stops behaving itself the
//   moment one of them gets "improved".
//
//   Three of those choices carry real weight:
//
//     1. Magnitude sums in float and takes the square root in double. The sum of three float products is a float,
//        which then gets promoted for sqrt and truncated on the way back. Calling the float overload of sqrt
//        instead would skip that intermediate rounding and give different answers.
//
//     2. There are two snapping conventions and they are exact opposites. Motion snaps AGAINST the direction of
//        travel, onto the grid point just reached, because that is where a trail corner belongs. The opponent AI
//        snaps ALONG the direction of travel, onto the cell it is about to enter, because that is what it needs to
//        test. Swap them and the opponents notice walls a cell too late.
//
//     3. Rounding to integer is round to nearest, not truncation.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cmath>

#include "Vector3.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Function: roundUp / roundDown
//
// Description:
//
//   Ceiling and floor.
//
//   The trip through double is intentional. It costs nothing, because the result is always integral and exactly
//   representable, and it keeps these in step with everything else that rounds.
//
// Arguments:
//
//   - value : The value to round.
//
// Returns:
//
//   - The value rounded toward plus infinity (roundUp) or minus infinity (roundDown).
//
//---------------------------------------------------------------------------------------------------------------------

inline float roundUp ( float value )
{
	return static_cast<float> ( std::ceil ( static_cast<double> ( value ) ) );
}

inline float roundDown ( float value )
{
	return static_cast<float> ( std::floor ( static_cast<double> ( value ) ) );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: floatToInteger
//
// Description:
//
//   Round to nearest and return an integer. Nearest, not truncation.
//
// Arguments:
//
//   - value : The value to convert.
//
// Returns:
//
//   - The value rounded to the nearest integer.
//
//---------------------------------------------------------------------------------------------------------------------

inline int floatToInteger ( float value )
{
	return static_cast<int> ( ( value < 0.0f ) ? ( value - 0.5f ) : ( value + 0.5f ) );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: magnitude
//
// Description:
//
//   Euclidean length. See note 1 in the file header before touching the sqrt call.
//
// Arguments:
//
//   - v : The vector to measure.
//
// Returns:
//
//   - The length of the vector.
//
//---------------------------------------------------------------------------------------------------------------------

inline float magnitude ( const Vector3& v )
{
	const float sumOfSquares = ( v.x * v.x ) + ( v.y * v.y ) + ( v.z * v.z );

	return static_cast<float> ( std::sqrt ( static_cast<double> ( sumOfSquares ) ) );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: separation
//
// Description:
//
//   Signed distance between two points, taken along a pod's direction of travel.
//
//   The magnitude is negated whenever any component of the difference opposes the matching component of forward.
//   Forward is always axis aligned, so two of the three products are exactly zero and only the axis of travel gets
//   a say in the sign.
//
//   This is the cell boundary test the whole motion model turns on. Callers pass a pod's current position and
//   its target position, which is the next grid point ahead. The result stays negative while the pod is still
//   short of that point and goes non-negative the moment it arrives, and that is when a queued turn commits and a
//   trail corner goes down.
//
// Arguments:
//
//   - a       : First point, in practice the pod's current position.
//   - b       : Second point, in practice the pod's target position.
//   - forward : The direction of travel supplying the sign.
//
// Returns:
//
//   - Distance between the points, negated when the displacement opposes travel.
//
//---------------------------------------------------------------------------------------------------------------------

inline float separation ( const Vector3& a, const Vector3& b, const Vector3& forward )
{
	const Vector3 difference = a - b;
	const float   distance   = magnitude ( difference );

	const bool opposesTravel = ( ( difference.x * forward.x ) < 0.0f ) ||
	                           ( ( difference.y * forward.y ) < 0.0f ) ||
	                           ( ( difference.z * forward.z ) < 0.0f );

	return opposesTravel ? -distance : distance;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: snapAgainstTravel
//
// Description:
//
//   Snap a position backwards along the direction of travel, onto the grid point most recently reached. The motion
//   code, the collision test and target selection all use this one.
//
//   Marking the collision grid on a naively rounded position instead claims the next cell as soon as the pod is
//   past halfway, which drops the pod inside a cell it has already claimed and makes it collide with itself on
//   arrival. Snapping backwards means a pod only ever claims ground it has actually covered.
//
// Arguments:
//
//   - position : The position to snap.
//   - forward  : The direction of travel, which picks the rounding direction.
//
// Returns:
//
//   - The grid point just behind the position along the direction of travel.
//
//---------------------------------------------------------------------------------------------------------------------

inline Vector3 snapAgainstTravel ( const Vector3& position, const Vector3& forward )
{
	if ( componentSum ( forward ) <= 0.0f )
	{
		return Vector3 { roundUp ( position.x ), roundUp ( position.y ), roundUp ( position.z ) };
	}

	return Vector3 { roundDown ( position.x ), roundDown ( position.y ), roundDown ( position.z ) };
}

//---------------------------------------------------------------------------------------------------------------------
// Function: snapAlongTravel
//
// Description:
//
//   Snap a position forwards along the direction of travel. The rounding test is the exact inverse of
//   snapAgainstTravel, so the result lands one cell further on.
//
//   Only the opponent AI uses this. It is lookahead, not a mistake: the AI wants the cell it is about to enter,
//   where the motion code wants the grid point it has just left.
//
// Arguments:
//
//   - position : The position to snap.
//   - forward  : The direction of travel, which picks the rounding direction.
//
// Returns:
//
//   - The grid point just ahead of the position along the direction of travel.
//
//---------------------------------------------------------------------------------------------------------------------

inline Vector3 snapAlongTravel ( const Vector3& position, const Vector3& forward )
{
	if ( componentSum ( forward ) >= 0.0f )
	{
		return Vector3 { roundUp ( position.x ), roundUp ( position.y ), roundUp ( position.z ) };
	}

	return Vector3 { roundDown ( position.x ), roundDown ( position.y ), roundDown ( position.z ) };
}

}
