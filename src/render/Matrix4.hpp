//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   A 4x4 matrix and the handful of vector operations the renderer needs, in column-major order so the arrays
//   can be handed to glUniformMatrix4fv without transposing.
//
//   This is presentation maths and it stays out of Vector3.hpp, which belongs to the simulation. Nothing here
//   is used by the simulation and nothing here can affect it.
//
//   A core profile has no fixed-function matrix stack, no gluPerspective and no gluLookAt, so the equivalents
//   are written out here.
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
// Vector operations
//
//   The simulation never needs a dot product, a cross product or a normalisation, because its basis vectors are
//   always axis-aligned and exact. The renderer needs all three.
//---------------------------------------------------------------------------------------------------------------------

inline float dot ( const Vector3& a, const Vector3& b )
{
	return ( a.x * b.x ) + ( a.y * b.y ) + ( a.z * b.z );
}

inline Vector3 cross ( const Vector3& a, const Vector3& b )
{
	return Vector3 { ( a.y * b.z ) - ( a.z * b.y ),
	                 ( a.z * b.x ) - ( a.x * b.z ),
	                 ( a.x * b.y ) - ( a.y * b.x ) };
}

inline float length ( const Vector3& v )
{
	return std::sqrt ( dot ( v, v ) );
}

inline Vector3 normalise ( const Vector3& v )
{
	const float magnitude = length ( v );

	if ( magnitude <= 1.0e-8f )
	{
		return Vector3 { 0.0f, 0.0f, 0.0f };
	}

	return v * ( 1.0f / magnitude );
}

inline Vector3 lerp ( const Vector3& a, const Vector3& b, float t )
{
	return Vector3 { a.x + ( b.x - a.x ) * t,
	                 a.y + ( b.y - a.y ) * t,
	                 a.z + ( b.z - a.z ) * t };
}

//---------------------------------------------------------------------------------------------------------------------
// Matrix4
//
//   Column-major: element ( row, column ) lives at m [ column * 4 + row ], which is the layout OpenGL expects.
//---------------------------------------------------------------------------------------------------------------------

struct Matrix4
{
	float m [ 16 ] = { 1.0f, 0.0f, 0.0f, 0.0f,
	                   0.0f, 1.0f, 0.0f, 0.0f,
	                   0.0f, 0.0f, 1.0f, 0.0f,
	                   0.0f, 0.0f, 0.0f, 1.0f };

	//-----------------------------------------------------------------------------------------------------------------
	// Method: identity
	//-----------------------------------------------------------------------------------------------------------------

	static Matrix4 identity ()
	{
		return Matrix4 {};
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Method: perspective
	//
	// Description:
	//
	//   Equivalent of gluPerspective, producing a right-handed projection into the OpenGL clip volume.
	//
	// Arguments:
	//
	//   - verticalFieldOfView : Vertical field of view, in radians.
	//   - aspectRatio         : Width divided by height.
	//   - nearPlane, farPlane : Clip distances, both positive.
	//
	// Returns:
	//
	//   - The projection matrix.
	//
	//-----------------------------------------------------------------------------------------------------------------

	static Matrix4 perspective ( float verticalFieldOfView, float aspectRatio, float nearPlane, float farPlane )
	{
		Matrix4 result;

		const float focalLength = 1.0f / std::tan ( verticalFieldOfView * 0.5f );
		const float depthRange  = nearPlane - farPlane;

		result.m [  0 ] = focalLength / aspectRatio;
		result.m [  5 ] = focalLength;
		result.m [ 10 ] = ( farPlane + nearPlane ) / depthRange;
		result.m [ 11 ] = -1.0f;
		result.m [ 14 ] = ( 2.0f * farPlane * nearPlane ) / depthRange;
		result.m [ 15 ] = 0.0f;

		return result;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Method: lookAt
	//
	// Description:
	//
	//   Equivalent of gluLookAt.
	//
	//   The up vector is only a hint and is re-orthogonalised against the view direction, which matters here
	//   because the arena is fully three dimensional: a pod travelling straight up the y axis has a forward
	//   vector parallel to the conventional world up, and a naive construction would collapse.
	//
	// Arguments:
	//
	//   - eye       : Camera position.
	//   - target    : Point being looked at.
	//   - upHint    : Approximate up direction.
	//
	// Returns:
	//
	//   - The view matrix.
	//
	//-----------------------------------------------------------------------------------------------------------------

	static Matrix4 lookAt ( const Vector3& eye, const Vector3& target, const Vector3& upHint )
	{
		const Vector3 forward = normalise ( target - eye );
		Vector3       right   = cross ( forward, upHint );

		// Guard the degenerate case where the hint is parallel to the view direction.

		if ( length ( right ) <= 1.0e-6f )
		{
			const Vector3 fallbackHint = ( std::fabs ( forward.y ) > 0.9f ) ? Vector3 { 0.0f, 0.0f, 1.0f }
			                                                               : Vector3 { 0.0f, 1.0f, 0.0f };
			right = cross ( forward, fallbackHint );
		}

		right = normalise ( right );

		const Vector3 up = cross ( right, forward );

		Matrix4 result;

		result.m [  0 ] =  right.x;
		result.m [  4 ] =  right.y;
		result.m [  8 ] =  right.z;
		result.m [  1 ] =  up.x;
		result.m [  5 ] =  up.y;
		result.m [  9 ] =  up.z;
		result.m [  2 ] = -forward.x;
		result.m [  6 ] = -forward.y;
		result.m [ 10 ] = -forward.z;
		result.m [ 12 ] = -dot ( right,   eye );
		result.m [ 13 ] = -dot ( up,      eye );
		result.m [ 14 ] =  dot ( forward, eye );
		result.m [ 15 ] =  1.0f;

		return result;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Method: translation
	//
	// Description:
	//
	//   Equivalent of glTranslatef.
	//
	//-----------------------------------------------------------------------------------------------------------------

	static Matrix4 translation ( float x, float y, float z )
	{
		Matrix4 result;

		result.m [ 12 ] = x;
		result.m [ 13 ] = y;
		result.m [ 14 ] = z;

		return result;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Method: rotationX / rotationY
	//
	// Description:
	//
	//   Rotation about the x and y axes. Used by the free-look camera, which is built from exactly these two.
	//
	// Arguments:
	//
	//   - angleRadians : Rotation angle, counter-clockwise about the axis.
	//
	//-----------------------------------------------------------------------------------------------------------------

	static Matrix4 rotationX ( float angleRadians )
	{
		const float cosine = std::cos ( angleRadians );
		const float sine   = std::sin ( angleRadians );

		Matrix4 result;

		result.m [  5 ] =  cosine;
		result.m [  6 ] =  sine;
		result.m [  9 ] = -sine;
		result.m [ 10 ] =  cosine;

		return result;
	}

	static Matrix4 rotationY ( float angleRadians )
	{
		const float cosine = std::cos ( angleRadians );
		const float sine   = std::sin ( angleRadians );

		Matrix4 result;

		result.m [  0 ] =  cosine;
		result.m [  2 ] = -sine;
		result.m [  8 ] =  sine;
		result.m [ 10 ] =  cosine;

		return result;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Method: fromBasis
	//
	// Description:
	//
	//   Builds a model transform from an orthonormal basis, a uniform scale and a translation. Light pods carry
	//   their own exact basis, so their model matrices need no trigonometry at all.
	//
	// Arguments:
	//
	//   - right, up, forward : The orthonormal basis.
	//   - translation        : World position.
	//   - scale              : Uniform scale.
	//
	// Returns:
	//
	//   - The model matrix.
	//
	//-----------------------------------------------------------------------------------------------------------------

	static Matrix4 fromBasis ( const Vector3& right, const Vector3& up, const Vector3& forward,
	                           const Vector3& translation, float scale )
	{
		Matrix4 result;

		result.m [  0 ] = right.x   * scale;
		result.m [  1 ] = right.y   * scale;
		result.m [  2 ] = right.z   * scale;
		result.m [  3 ] = 0.0f;

		result.m [  4 ] = up.x      * scale;
		result.m [  5 ] = up.y      * scale;
		result.m [  6 ] = up.z      * scale;
		result.m [  7 ] = 0.0f;

		result.m [  8 ] = forward.x * scale;
		result.m [  9 ] = forward.y * scale;
		result.m [ 10 ] = forward.z * scale;
		result.m [ 11 ] = 0.0f;

		result.m [ 12 ] = translation.x;
		result.m [ 13 ] = translation.y;
		result.m [ 14 ] = translation.z;
		result.m [ 15 ] = 1.0f;

		return result;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// Function: operator *
//
// Description:
//
//   Matrix product, column-major throughout.
//
//---------------------------------------------------------------------------------------------------------------------

inline Matrix4 operator * ( const Matrix4& a, const Matrix4& b )
{
	Matrix4 result;

	for ( int column = 0; column < 4; column++ )
	{
		for ( int row = 0; row < 4; row++ )
		{
			float sum = 0.0f;

			for ( int k = 0; k < 4; k++ )
			{
				sum += a.m [ k * 4 + row ] * b.m [ column * 4 + k ];
			}

			result.m [ column * 4 + row ] = sum;
		}
	}

	return result;
}

}
