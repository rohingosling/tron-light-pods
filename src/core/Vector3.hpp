//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Vector Maths
//
// Description:
//
//   Three component single precision vector, and the componentwise operators the simulation is written in terms of.
//
//   Everything here stays in single precision. No operator promotes to double, and none of them reassociate. The
//   simulation has to produce identical results from run to run, so do not add a fused multiply-add path here and
//   do not build this with -ffast-math.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Vector3
//
//   Positions and directions are the same shape everywhere in the simulation, so one type covers both.
//
//---------------------------------------------------------------------------------------------------------------------

struct Vector3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

//---------------------------------------------------------------------------------------------------------------------
// Componentwise operators
//---------------------------------------------------------------------------------------------------------------------

inline Vector3 operator + ( const Vector3& a, const Vector3& b )
{
	return Vector3 { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vector3 operator - ( const Vector3& a, const Vector3& b )
{
	return Vector3 { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector3 operator * ( const Vector3& v, float scale )
{
	return Vector3 { v.x * scale, v.y * scale, v.z * scale };
}

inline Vector3 operator * ( float scale, const Vector3& v )
{
	return v * scale;
}

inline Vector3 operator - ( const Vector3& v )
{
	return Vector3 { -v.x, -v.y, -v.z };
}

inline Vector3& operator += ( Vector3& a, const Vector3& b )
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;

	return a;
}

inline bool operator == ( const Vector3& a, const Vector3& b )
{
	return ( a.x == b.x ) && ( a.y == b.y ) && ( a.z == b.z );
}

inline bool operator != ( const Vector3& a, const Vector3& b )
{
	return !( a == b );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: componentSum
//
// Description:
//
//   Sum of the three components.
//
//   We only ever call this on a basis vector, and those are always axis aligned and unit length, so the answer is
//   always exactly +1 or -1. It is used as a direction of travel sign test, nothing more.
//
// Arguments:
//
//   - v : The vector to sum.
//
// Returns:
//
//   - The sum of the three components.
//
//---------------------------------------------------------------------------------------------------------------------

inline float componentSum ( const Vector3& v )
{
	return v.x + v.y + v.z;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: isAxisAligned
//
// Description:
//
//   True when the vector lies exactly along one axis with unit length.
//
//   The whole turn model depends on this staying true of every basis vector. Keeping orientation snapped to the
//   axes is what stops it drifting no matter how long a game runs.
//
// Arguments:
//
//   - v : The vector to test.
//
// Returns:
//
//   - True if exactly one component is plus or minus one and the other two are zero.
//
//---------------------------------------------------------------------------------------------------------------------

inline bool isAxisAligned ( const Vector3& v )
{
	const int   nonZeroCount     = ( v.x != 0.0f ? 1 : 0 ) + ( v.y != 0.0f ? 1 : 0 ) + ( v.z != 0.0f ? 1 : 0 );
	const float magnitudeSquared = ( v.x * v.x ) + ( v.y * v.y ) + ( v.z * v.z );

	return ( nonZeroCount == 1 ) && ( magnitudeSquared == 1.0f );
}

}
