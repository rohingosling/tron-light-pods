//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Random Numbers
//
// Description:
//
//   The C runtime's linear congruential generator, written out in full so the sequence is ours and not the
//   library's.
//
//   Every spawn position and every opponent wander roll comes out of here. Seeding the same way but generating
//   differently would put the pods somewhere else entirely, so it is worth pinning down.
//
//   Careful with the multiplier. It looks like it ought to be 0x015A4E35, which is the well known 32-bit constant,
//   and that is what you get if you read the two loaded halves as adjacent 16-bit words. It is not that. The high
//   limb sits 32 bits up rather than 16, so the multiplier is 0x0000015A00004E35. Change it and every spawn in the
//   game moves.
//
//   Note also that we return the HIGH dword of the state, not the usual 16-bit middle slice.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

namespace tron3d
{

//*********************************************************************************************************************
// RandomGenerator
//
//   The state is 64 bits. It is easier to keep it in one uint64_t than as two halves.
//
//*********************************************************************************************************************

class RandomGenerator
{
	//=================================================================================================================
	// Data Members
	//=================================================================================================================

	private:

		std::uint64_t state = 0;

	//=================================================================================================================
	// Constants
	//=================================================================================================================

	public:

		static constexpr std::uint64_t MULTIPLIER = 0x0000015A00004E35ULL;
		static constexpr std::uint64_t INCREMENT  = 1ULL;

	//=================================================================================================================
	// Mutators
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Mutator: seedWith
		//
		// Description:
		//
		//   Set the low half of the state to the seed and zero the high half.
		//
		//   Watch out when picking test seeds. For any seed below about 189 the first product stays under 2^32, so
		//   the high dword is zero and the first draw comes back 0. Harmless in the game, which seeds off the clock,
		//   but it does mean every small seed opens on the same spawn coordinate. Use seeds in the thousands if you
		//   want them to tell each other apart.
		//
		// Arguments:
		//
		//   - seed : The seed value.
		//
		//-------------------------------------------------------------------------------------------------------------

		void seedWith ( std::uint32_t seed )
		{
			state = static_cast<std::uint64_t> ( seed );
		}

	//=================================================================================================================
	// Methods
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Method: next
		//
		// Description:
		//
		//   Advance the state and return the high dword with the sign bit masked off.
		//
		// Returns:
		//
		//   - A pseudorandom value in the range 0 .. 0x7FFFFFFF.
		//
		//-------------------------------------------------------------------------------------------------------------

		int next ()
		{
			state = ( state * MULTIPLIER ) + INCREMENT;

			return static_cast<int> ( static_cast<std::uint32_t> ( state >> 32 ) & 0x7FFFFFFFu );
		}

		//-------------------------------------------------------------------------------------------------------------
		// Method: below
		//
		// Description:
		//
		//   Return next () modulo the range, guarding against a zero range.
		//
		// Arguments:
		//
		//   - range : Exclusive upper bound.
		//
		// Returns:
		//
		//   - A pseudorandom value in the range 0 .. range - 1, or zero if the range is zero.
		//
		//-------------------------------------------------------------------------------------------------------------

		int below ( int range )
		{
			if ( range == 0 )
			{
				return 0;
			}

			return next () % range;
		}
};

}
