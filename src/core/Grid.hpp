//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Collision Grid
//
// Description:
//
//   The collision grid: a fixed 10 x 10 x 10 lattice recording which pod, if any, has claimed each cell.
//
//   One byte per cell, holding the occupant id. That is all a cell needs.
//
//   Watch the axis mapping, which is the reverse of the obvious guess: x carries the largest stride and z the
//   smallest.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>

#include "Constants.hpp"

namespace tron3d
{

//*********************************************************************************************************************
// Grid
//*********************************************************************************************************************

class Grid
{
	//=================================================================================================================
	// Data Members
	//=================================================================================================================

	private:

		std::array<std::uint8_t, GRID_CELL_COUNT> occupant {};

	//=================================================================================================================
	// Accessors
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Predicate Accessor: contains
		//
		// Description:
		//
		//   True when a cell coordinate lies inside the lattice.
		//
		//   In practice this should never come back false. The arena bounds are only a tenth of a cell wider than
		//   the lattice, and a pod moves about two hundredths of a cell per frame, so the bounds test kills it
		//   long before a lookup could run off the end.
		//
		//   We check anyway. Indexing past the end of the array is undefined behaviour, and "it cannot happen" is
		//   not a good enough reason to leave that lying around.
		//
		// Arguments:
		//
		//   - x, y, z : Cell coordinates.
		//
		// Returns:
		//
		//   - True if the coordinates address a cell inside the lattice.
		//
		//-------------------------------------------------------------------------------------------------------------

		static bool contains ( int x, int y, int z )
		{
			return ( x >= 0 ) && ( x < GRID_DIMENSION ) &&
			       ( y >= 0 ) && ( y < GRID_DIMENSION ) &&
			       ( z >= 0 ) && ( z < GRID_DIMENSION );
		}

		//-------------------------------------------------------------------------------------------------------------
		// Value Accessor: index
		//
		// Description:
		//
		//   Flattens a cell coordinate. x carries the largest stride.
		//
		// Arguments:
		//
		//   - x, y, z : Cell coordinates, assumed in range.
		//
		// Returns:
		//
		//   - The flat index into the occupancy array.
		//
		//-------------------------------------------------------------------------------------------------------------

		static int index ( int x, int y, int z )
		{
			return ( x * GRID_STRIDE_X ) + ( y * GRID_STRIDE_Y ) + ( z * GRID_STRIDE_Z );
		}

		//-------------------------------------------------------------------------------------------------------------
		// Value Accessor: occupantAt
		//
		// Description:
		//
		//   The id of the pod occupying a cell, or zero if the cell is empty or outside the lattice.
		//
		// Arguments:
		//
		//   - x, y, z : Cell coordinates.
		//
		// Returns:
		//
		//   - The occupant id, where the id is the pod's array index plus one.
		//
		//-------------------------------------------------------------------------------------------------------------

		std::uint8_t occupantAt ( int x, int y, int z ) const
		{
			return contains ( x, y, z ) ? occupant [ index ( x, y, z ) ] : std::uint8_t { 0 };
		}

		//-------------------------------------------------------------------------------------------------------------
		// Predicate Accessor: isEmptyAt
		//
		// Arguments:
		//
		//   - x, y, z : Cell coordinates.
		//
		// Returns:
		//
		//   - True if no pod has claimed the cell.
		//
		//-------------------------------------------------------------------------------------------------------------

		bool isEmptyAt ( int x, int y, int z ) const
		{
			return occupantAt ( x, y, z ) == 0;
		}

		//-------------------------------------------------------------------------------------------------------------
		// Value Accessor: rawOccupancy
		//
		// Description:
		//
		//   Direct view of the occupancy array, for the renderer and for testing.
		//
		// Returns:
		//
		//   - A const reference to the occupancy array.
		//
		//-------------------------------------------------------------------------------------------------------------

		const std::array<std::uint8_t, GRID_CELL_COUNT>& rawOccupancy () const
		{
			return occupant;
		}

	//=================================================================================================================
	// Mutators
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Mutator: claim
		//
		// Description:
		//
		//   Marks a cell as owned by a pod. Out-of-range coordinates are ignored; see the note on contains.
		//
		// Arguments:
		//
		//   - x, y, z : Cell coordinates.
		//   - ownerId : The claiming pod's id.
		//
		//-------------------------------------------------------------------------------------------------------------

		void claim ( int x, int y, int z, std::uint8_t ownerId )
		{
			if ( contains ( x, y, z ) )
			{
				occupant [ index ( x, y, z ) ] = ownerId;
			}
		}

	//=================================================================================================================
	// Methods
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Method: clear
		//
		// Description:
		//
		//   Empty the whole lattice.
		//
		//-------------------------------------------------------------------------------------------------------------

		void clear ()
		{
			occupant.fill ( 0 );
		}

		//-------------------------------------------------------------------------------------------------------------
		// Method: purgeOwner
		//
		// Description:
		//
		//   Release every cell claimed by one pod.
		//
		//   Called when a pod dies, so its trail stops killing the survivors. This is a gameplay rule, not
		//   housekeeping: a wall vanishes the instant its owner does.
		//
		// Arguments:
		//
		//   - ownerId : The id whose cells are to be released.
		//
		//-------------------------------------------------------------------------------------------------------------

		void purgeOwner ( std::uint8_t ownerId )
		{
			for ( std::uint8_t& cell : occupant )
			{
				if ( cell == ownerId )
				{
					cell = 0;
				}
			}
		}
};

}
