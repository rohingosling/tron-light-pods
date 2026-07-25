//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Renderer
//
// Description:
//
//   The OpenGL 3.3 core renderer. See Renderer.hpp for what the picture is made of.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Constants.hpp"
#include "FontGlyphs.hpp"
#include "Renderer.hpp"

namespace tron3d
{

using namespace tron3d::gl;

//---------------------------------------------------------------------------------------------------------------------
// Rendering constants
//
//   These set what the picture looks like. Change one and you change the look of the game, so treat them as
//   settled rather than as knobs.
//---------------------------------------------------------------------------------------------------------------------

namespace
{
	// Atmosphere. Black GL_EXP fog over a black clear colour. GL_EXP looks at the density and nothing else, so
	// the fog start and end are set and then ignored.
	//
	// The gameplay density and the post free-look one are shared with the application layer and live in
	// Renderer.hpp. The free-look density stays private to this file. It thins the fog right down, which is the
	// only thing that makes an outside view of the arena possible at all - at the gameplay density the lattice
	// disappears within a couple of cells.

	constexpr float FOG_DENSITY_FREE_LOOK = 0.08f;

	// The frame the game is composed for. 640 x 480, so 4:3.
	//
	// The HUD is the giveaway that this was designed to and not just defaulted to: the readouts land at 90% of the
	// frame in BOTH axes at 4:3, a uniform 10% margin, and drift inward in x at anything wider. So we letterbox to
	// 4:3 rather than filling whatever window we are given. See Renderer::resize.

	constexpr float TARGET_ASPECT_RATIO = 4.0f / 3.0f;

	// Light pod body - four sided pyramid pointing along local +X.

	constexpr float POD_SCALE        = 0.035f;
	constexpr float POD_TINT_OFFSET  = 0.2f;
	constexpr float POD_GREY         = 0.8f;

	// Light trail - square section tube. The shade offset is painted onto opposite faces on horizontal runs, and
	// left off vertical ones.

	constexpr float TRAIL_HALF_WIDTH = 0.05f;
	constexpr float TRAIL_SHADE      = 0.3f;

	// Grid node cube - edge = cell scale = 0.2, so half extent 0.1. Occupied cells brighten their occupant's
	// colour by +0.5; empty cells sit at mid grey.

	constexpr float NODE_HALF_EXTENT = 0.1f;
	constexpr float NODE_BRIGHTEN    = 0.5f;
	constexpr float NODE_EMPTY_GREY  = 0.5f;

	// Lattice opacity, NOT transparency: 0.0 is invisible and 1.0 is solid.
	//
	// - Drawing the node cubes translucent and blending them over the rest of the world lets the tunnel ahead
	//   read as structure instead of as a solid cage. Solid looks busy in the cockpit.
	//
	// - Set this to 1.0 for a solid lattice. At 1.0 the blend and the depth mask change are skipped entirely,
	//   so there is no other switch to flip.
	//
	// - Do not go hunting for a bug if a build at 1.0 does not give pixel identical output to a build with the
	//   blending code stripped out. Merely DECLARING the alpha uniform relinks the program, and GLSL promises
	//   nothing about gl_Position coming out bit identical across two program objects. The lattice is a regular
	//   grid seen down its own axes, so thousands of cube edges project onto the same pixels at the same depth;
	//   those ties get settled by GL_LESS and a one bit shift flips which edge wins. Different edge, different
	//   colour, same picture.

	constexpr float NODE_ALPHA = 0.333f;

	// Cameras. The look target sits 11 units ahead of the pod along its forward axis. The chase eye is 0.8 behind
	// and 0.4 above. The spot plane eye is offset 0.5 on every axis - that was meant to be a lift in Y alone, but
	// it looks fine as it is - then pulled back by grid size x 0.2 and lifted by 1.

	constexpr float LOOK_AHEAD       = 11.0f;
	constexpr float THIRD_BACK       = 0.8f;
	constexpr float THIRD_UP         = 0.4f;
	constexpr float SPOT_OFFSET      = 0.5f;
	constexpr float SPOT_STANDOFF_K  = 0.2f;    // multiplied by grid size
	constexpr float SPOT_UP          = 1.0f;

	// Vertical field of view per view, in degrees. F2 is deliberately wide. Near and far are shared.

	constexpr float FOV_COCKPIT      = 90.0f;
	constexpr float FOV_THIRD_PERSON = 110.0f;
	constexpr float FOV_SPOT_PLANE   = 90.0f;
	constexpr float NEAR_PLANE       = 0.01f;
	constexpr float FAR_PLANE        = 30.0f;

	// Camera basis smoothing rate per view. Each component of the smoothed basis moves toward its true value
	// LINEARLY at rate x frame delta and clamps exactly on the target. A move-towards, not an exponential ease,
	// so a 90 degree swing takes exactly 1/rate seconds.
	//
	// The smoothed vector is left short mid swing and only the view construction normalises it. That is what
	// makes the turn feel like it leans.

	constexpr float SMOOTHING_RATE_COCKPIT      = 2.2f;
	constexpr float SMOOTHING_RATE_THIRD_PERSON = 0.8f;
	constexpr float SMOOTHING_RATE_SPOT_PLANE   = 0.4f;

	// Turn the camera faster. This multiplies the per view rates above rather than replacing them, so the base
	// rates stay readable and the speed-up sits in one place. A 90 degree cockpit swing lands in 1 / (2.2 x 2) =
	// 0.227 s.

	constexpr float CAMERA_TURN_SPEED_MULTIPLIER = 2.0f;

	// Pull the F3 spot plane camera in a bit. The base offset above framed the pod a touch too far out. Scaling
	// the whole offset moves the eye in along the same ray, so the framing direction does not change.

	constexpr float SPOT_DISTANCE_MULTIPLIER = 0.7f;

	constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;

	//-----------------------------------------------------------------------------------------------------------------
	// HUD - the 2D overlay, drawn after the world pass
	//
	//   The whole overlay goes on the z = -1 plane with an identity view and the same perspective projection as the
	//   world. Since that projection is the per view one, the overlay comes out larger in the wide third person
	//   view, which is why view 2 gets its own set of "large" layout numbers.
	//-----------------------------------------------------------------------------------------------------------------

	// Uniform HUD scale per view. Views 1 and 3 are small; view 2 is large.

	constexpr float HUD_SCALE_SMALL = 0.05f;
	constexpr float HUD_SCALE_LARGE = 0.07f;

	// Turn arrow blink. The phase advances at 4.0 per second and wraps at 1.0. The arrow is filled while the phase
	// is at or below 0.5 and an outline above it.

	constexpr float HUD_BLINK_RATE      = 4.0f;
	constexpr float HUD_BLINK_THRESHOLD = 0.5f;

	// The arrow, before any transform: a triangle with base corners at +-0.8 on y = 0 and its tip at y = 1, so it
	// points up. Rotating about Z then aims it - 0 up, 270 right, 180 down, 90 left.

	constexpr float ARROW_BASE_HALF = 0.8f;
	constexpr float ARROW_TIP_Y     = 1.0f;

	// The D-pad block layout. Each of the four arrows sits on one arm of a cross, at block offsets of x in
	// { 0, +1, 0, -1 } and y in { +1, 0, -1, 0 }. The whole cross then drops to the group base below, centred in x.

	constexpr float HUD_DPAD_BASE_X = 0.0f;
	constexpr float HUD_DPAD_BASE_Y = -16.0f;

	// The segmented proximity bar. Three stacked segments, each a quad spanning x in [-0.5, +0.5]: nearest at
	// y [1.2, 1.6] in red, middle at [1.7, 2.1] in yellow, far at [2.2, 2.6] in green.
	//
	// A segment is filled only when its grid sample is occupied. The three outlines are always drawn, in green.

	constexpr float BAR_HALF_WIDTH = 0.5f;

	constexpr float BAR_SEGMENT_LOW  [ 3 ] = { 1.2f, 1.7f, 2.2f };
	constexpr float BAR_SEGMENT_HIGH [ 3 ] = { 1.6f, 2.1f, 2.6f };

	// The proximity cross. Four bars on the D-pad's cross offsets, rotated 0 / 270 / 180 / 90 to point up, right,
	// down and left, each sampling the collision grid three cells out along its direction.
	//
	// It shares the D-pad's base, offsets and scale, so the sensor is drawn AROUND the direction indicator: the
	// bar segments run from local radius 1.2 to 2.6, which starts just past the arrow tips at radius 2.0.
	//
	// Bars sample the player's own basis - up and down along the up vector, left and right along the side vector -
	// at cell distances 1, 2 and 3.

	constexpr float HUD_PROXIMITY_BASE_X = 0.0f;
	constexpr float HUD_PROXIMITY_BASE_Y = -16.0f;

	constexpr int   PROXIMITY_SAMPLE_STEPS = 3;

	//-----------------------------------------------------------------------------------------------------------------
	// Text readouts - SPEED / SCORE / opponents
	//
	//   Each readout is a label and a value, drawn with the stroke font in the z = -1 overlay plane, in green.
	//   Views 1 and 3 use the small layout and view 2 the large one; the two differ only in position and scale.
	//
	//   SPEED shows 100 times the player's speed as an integer, SCORE shows the score, and the opponents line
	//   shows the live pod count minus one. All three format as plain base 10, but only SCORE is then right
	//   justified into a fixed width field - see SCORE_FIELD_WIDTH below.
	//-----------------------------------------------------------------------------------------------------------------

	struct TextItem
	{
		float       x;
		float       y;
		const char* label;                          // nullptr for a bare value
	};

	struct ReadoutLayout
	{
		float    scaleX;
		float    scaleY;
		TextItem speedLabel;
		TextItem speedValue;
		TextItem scoreLabel;
		TextItem scoreValue;
		TextItem opponentsLabel;
		TextItem opponentsValue;
	};

	// Small layout, views 1 and 3.

	constexpr ReadoutLayout READOUT_SMALL =
	{
		0.008f, 0.005f,
		{ -1.20f, -0.9f, "SPEED"     }, { -0.95f, -0.9f, nullptr },
		{  0.75f, -0.9f, "SCORE"     }, {  1.00f, -0.9f, nullptr },
		{ -1.20f,  0.9f, "OPPONENTS" }, { -0.80f,  0.9f, nullptr }
	};

	// Large layout, view 2.

	constexpr ReadoutLayout READOUT_LARGE =
	{
		0.010f, 0.007f,
		{ -1.70f, -1.3f, "SPEED"     }, { -1.40f, -1.3f, nullptr },
		{  1.10f, -1.3f, "SCORE"     }, {  1.40f, -1.3f, nullptr },
		{ -1.70f,  1.3f, "OPPONENTS" }, { -1.20f,  1.3f, nullptr }
	};

	constexpr float SPEED_DISPLAY_SCALE = 100.0f;   // SPEED shows 100 x speed, rounded

	//-----------------------------------------------------------------------------------------------------------------
	// The SCORE field
	//
	//   SCORE is the one number we right justify, into a fixed six digit field padded with ZEROES rather than
	//   spaces. So a score of 1234 draws as "001234" and a score of 0 as "000000". It is an arcade scoreboard, and
	//   it should look like one.
	//
	//   SPEED and the opponents count are NOT padded - they go straight out as plain integers.
	//-----------------------------------------------------------------------------------------------------------------

	constexpr std::size_t SCORE_FIELD_WIDTH = 6;
	constexpr char        SCORE_FIELD_FILL  = '0';

	// The free-look label. Drawn by the same HUD text block as the opponents readout, but only while the free-look
	// view is up.

	constexpr float       TEST_MODE_X    = -0.18f;
	constexpr const char* TEST_MODE_TEXT = "TEST MODE";

	//-----------------------------------------------------------------------------------------------------------------
	// Free-look camera - the F12 debug view
	//
	//   An orbit camera around the arena centre. The transform chain is:
	//
	//     translate ( 0, 0, -distance )
	//     rotate    ( pitch, about x )
	//     rotate    ( yaw,   about y )
	//     translate ( c, c, c )           c = -( grid size - 1.0 ) * 0.5 on all three axes
	//
	//   That last term puts c at exactly minus the centre of the cell lattice, which is -4.5 on the default grid
	//   of 10, so the camera orbits the middle of the arena.
	//
	//   The view installs no projection of its own. It inherits whichever gameplay projection was up when F12 was
	//   pressed, which is why the live camera mode gets passed through.
	//-----------------------------------------------------------------------------------------------------------------

	constexpr float FREE_LOOK_CENTRE_OFFSET = 1.0f;
	constexpr float FREE_LOOK_CENTRE_SCALE  = 0.5f;

	//-----------------------------------------------------------------------------------------------------------------
	// Front end - the menu and the end screens
	//
	//   All drawn under the 2D projection - 85 degree vertical field of view, near 0.01, far 200, fog and depth
	//   test both off. It goes in on entering any front end state, and by default on a resize.
	//-----------------------------------------------------------------------------------------------------------------

	constexpr float FRONT_END_FOV       = 85.0f;
	constexpr float FRONT_END_FAR_PLANE = 200.0f;

	// The five menu items. All sit on the z = -90 plane at glyph scale 1, so the projection alone sizes them.

	struct MenuItemLayout
	{
		float       x;
		float       y;
		const char* label;
	};

	constexpr MenuItemLayout MENU_ITEM_LAYOUT [ 5 ] =
	{
		{ -64.0f,  24.0f, "Number of Opponents" },
		{ -64.0f,   8.0f, "Grid Size"           },
		{ -25.0f,  -8.0f, "Start Game"          },
		{ -25.0f, -24.0f, "Reset Game"          },
		{ -21.0f, -40.0f, "Exit Game"           }
	};

	constexpr float MENU_ITEM_Z  = -90.0f;

	// Numeric values draw at an absolute x on the item's row, NOT at an offset from the item. The item's own x
	// does not come into it.

	constexpr float MENU_VALUE_X = 40.0f;

	// Menu colours. Labels are blue and values red, both brightening on selection, with a green outline.

	constexpr Vector3 MENU_LABEL_COLOUR          { 0.3f, 0.3f, 1.0f };
	constexpr Vector3 MENU_LABEL_SELECTED_COLOUR { 0.7f, 0.7f, 1.0f };
	constexpr Vector3 MENU_VALUE_COLOUR          { 1.0f, 0.3f, 0.3f };
	constexpr Vector3 MENU_VALUE_SELECTED_COLOUR { 1.0f, 0.7f, 0.7f };
	constexpr Vector3 MENU_OUTLINE_COLOUR        { 0.0f, 1.0f, 0.0f };

	// The selection outline rectangle. Centred in x, dropped 4 below the selected item's baseline and 16 tall,
	// on the items' own z plane.

	constexpr float MENU_OUTLINE_HALF_WIDTH = 80.0f;
	constexpr float MENU_OUTLINE_DROP       = 4.0f;
	constexpr float MENU_OUTLINE_HEIGHT     = 16.0f;

	// The title backdrop. Two blocks.
	//
	// - A full screen gradient quad on z = -1, lifted 0.1, black at the top fading to dark blue at the bottom.
	//
	// - A horizon band across it, blue along the top edge and green along the bottom. Note the scale is applied
	//   BEFORE the translate, so the translate happens in scaled units. The corners work out at x +-20,
	//   y 6.7 +- 1, z -9.

	constexpr float   BACKDROP_LIFT_Y     = 0.1f;
	constexpr float   BACKDROP_HALF_X     = 2.0f;
	constexpr float   BACKDROP_HALF_Y     = 1.0f;
	constexpr float   BACKDROP_Z          = -1.0f;
	constexpr Vector3 BACKDROP_TOP_COLOUR    { 0.0f, 0.0f, 0.0f  };
	constexpr Vector3 BACKDROP_BOTTOM_COLOUR { 0.0f, 0.0f, 0.2f  };

	constexpr float   HORIZON_HALF_X      = 20.0f;  // x +-1 scaled by 20
	constexpr float   HORIZON_CENTRE_Y    = 6.7f;
	constexpr float   HORIZON_HALF_Y      = 1.0f;
	constexpr float   HORIZON_Z           = -9.0f;  // vertex z +1 through a translate of -10
	constexpr Vector3 HORIZON_TOP_COLOUR     { 0.0f, 0.0f, 0.5f  };
	constexpr Vector3 HORIZON_BOTTOM_COLOUR  { 0.0f, 0.5f, 0.0f  };

	//-----------------------------------------------------------------------------------------------------------------
	// Function: textWidth
	//
	// Description:
	//
	//   Width of a string in glyph units, for centring it.
	//
	//   Every glyph's advance carries one unit of trailing gap so that neighbouring letters do not touch. The last
	//   glyph in a string has nothing after it, so that final gap is not part of what you can see and comes off
	//   the total. Leave it in and the text sits half a unit left of centre.
	//
	// Arguments:
	//
	//   - text : The string to measure.
	//
	// Returns:
	//
	//   - The visible width, in the font's own units.
	//
	//-----------------------------------------------------------------------------------------------------------------

	constexpr float textWidth ( const char* text )
	{
		float width = 0.0f;

		for ( const char* character = text; *character != '\0'; character++ )
		{
			width += FONT_GLYPHS [ static_cast<unsigned char> ( *character ) & 0x7F ].advance;
		}

		return ( width > 0.0f ) ? ( width - 1.0f ) : 0.0f;
	}

	// The two title strings, centred on the screen.
	//
	// Both wrap their text in a scale, and the scale comes BEFORE the translate, so the origins below are the
	// translate arguments already multiplied through the scale. Get that the wrong way round and the title lands
	// somewhere else entirely.
	//
	// The x origins are computed rather than written down, so the title stays centred if the wording changes.

	constexpr const char* TITLE_TEXT   = "TRON LIGHT PODS";
	constexpr const char* VERSION_TEXT = "Version 1.9";

	// The horizontal scale is set by how much text has to fit, not by taste. The front end projection is 85
	// degrees of vertical field of view at a 4:3 aspect, so at the title's depth of 30 the frame is about 73
	// world units across. A 15 character title measures 74 glyph units, so anything much above 0.7 runs off both
	// edges. The old seven character title sat at 1.5 and filled around 70% of the frame; 0.7 puts this one at
	// the same 70%, which is why it was chosen.

	constexpr float   TITLE_SCALE_X   = 0.7f;
	constexpr float   TITLE_SCALE_Y   = 0.3f;
	constexpr float   TITLE_ORIGIN_X  = -0.5f * textWidth ( TITLE_TEXT ) * TITLE_SCALE_X;
	constexpr float   TITLE_ORIGIN_Y  = 70.0f * TITLE_SCALE_Y;
	constexpr float   TITLE_Z         = -30.0f;
	constexpr Vector3 TITLE_COLOUR       { 1.0f, 1.0f, 1.0f };

	constexpr float   VERSION_SCALE_X  = 0.15f;
	constexpr float   VERSION_SCALE_Y  = 0.08f;
	constexpr float   VERSION_ORIGIN_X = -0.5f * textWidth ( VERSION_TEXT ) * VERSION_SCALE_X;
	constexpr float   VERSION_ORIGIN_Y = -210.0f * VERSION_SCALE_Y;
	constexpr float   VERSION_Z        = -20.0f;
	constexpr Vector3 VERSION_COLOUR     { 0.0f, 0.5f, 0.5f };

	// The end screen banner. A phase advances at 90 degrees per second, and each frame draws 200 copies of the
	// text, colour ramping one channel from near black up to full, each copy rotated half a degree on from the
	// last, then one solid white copy at the final angles. It reads as a motion trail smeared through the turn.
	//
	// Each copy gets its own push and pop, so the transforms do NOT compound. Drop that and the banner winds
	// itself into a spiral.
	//
	// Both screens share the same angle formulas. They differ only in the ramped channel, the pull back, the
	// centring and the text.

	constexpr int   BANNER_COPY_COUNT   = 200;
	constexpr float BANNER_PHASE_RATE   = 90.0f;    // degrees per second
	constexpr float BANNER_COLOUR_STEP  = 1.0f / 200.0f;
	constexpr float BANNER_ANGLE_STEP   = 0.5f;
	constexpr float BANNER_ANGLE_Y_BASE = 90.0f;
	constexpr float BANNER_ANGLE_Z_BASE = 180.0f;

	constexpr const char* WINNER_TEXT    = "YOU ARE THE WINNER";
	constexpr const char* GAME_OVER_TEXT = "GAME OVER";

	constexpr float WINNER_PULL_BACK    = 80.0f;
	constexpr float WINNER_CENTRE_X     = -44.0f;
	constexpr float GAME_OVER_PULL_BACK = 40.0f;
	constexpr float GAME_OVER_CENTRE_X  = -22.0f;

	// The WINNER screen's score readout: green, in screen space at z = -1. GAME OVER has no readout. The two
	// screens are not symmetrical and are not meant to be.

	constexpr float   BANNER_SCORE_LABEL_X = -0.3f;
	constexpr float   BANNER_SCORE_VALUE_X = 0.0f;
	constexpr float   BANNER_SCORE_Y       = -0.7f;
	constexpr float   BANNER_SCORE_Z       = -1.0f;
	constexpr float   BANNER_SCORE_SCALE   = 0.01f;
	constexpr Vector3 BANNER_SCORE_COLOUR    { 0.0f, 1.0f, 0.0f };

	//-----------------------------------------------------------------------------------------------------------------
	// Function: hudPoint
	//
	// Description:
	//
	//   Place one HUD-local point into eye space on the z = -1 plane.
	//
	//   The point is rotated about Z, offset by its block and the group base, then uniformly scaled. That order
	//   matters. The result goes straight through the view's projection with an identity view, because the HUD
	//   lives in eye space already.
	//
	// Arguments:
	//
	//   - localX, localY : The element-local point (arrow or bar corner).
	//   - angleDegrees   : Rotation about Z, counter-clockwise, before translation.
	//   - blockX, blockY : The element's arm of the cross.
	//   - baseX, baseY   : The group base shared by all arms.
	//   - scale          : The uniform HUD scale for the current view.
	//
	//-----------------------------------------------------------------------------------------------------------------

	inline Vector3 hudPoint ( float localX, float localY, float angleDegrees,
	                          float blockX, float blockY, float baseX, float baseY, float scale )
	{
		const float radians = angleDegrees * DEGREES_TO_RADIANS;
		const float cosine  = std::cos ( radians );
		const float sine    = std::sin ( radians );

		const float rotatedX = ( localX * cosine ) - ( localY * sine );
		const float rotatedY = ( localX * sine   ) + ( localY * cosine );

		return Vector3
		{
			scale * ( blockX + baseX + rotatedX ),
			scale * ( blockY + baseY + rotatedY ),
			-1.0f
		};
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Function: moveTowards
	//
	// Description:
	//
	//   Step one value linearly toward a target and clamp exactly on it.
	//
	//   Applied independently to each of the six smoothed basis components. That is what gives the camera its
	//   constant speed swing with a hard, exact stop, rather than the mushy tail of an exponential ease.
	//
	// Arguments:
	//
	//   - value  : The smoothed component.
	//   - target : The true component it approaches.
	//   - step   : rate x frame delta, always non-negative.
	//
	//-----------------------------------------------------------------------------------------------------------------

	inline float moveTowards ( float value, float target, float step )
	{
		if ( value == target )
		{
			return value;
		}

		if ( target <= value )
		{
			value -= step;

			return ( value < target ) ? target : value;
		}

		value += step;

		return ( target < value ) ? target : value;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Function: clampChannel
	//
	// Description:
	//
	//   Clamp one colour channel to [0, 1].
	//
	//   The +0.5 / +-0.3 / +-0.2 colour arithmetic scattered through the geometry builders can run off either end
	//   of the unit range. Saturating is the wanted behaviour, so clamp rather than rescale.
	//
	//-----------------------------------------------------------------------------------------------------------------

	inline float clampChannel ( float value )
	{
		return ( value < 0.0f ) ? 0.0f : ( value > 1.0f ) ? 1.0f : value;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Function: clampColour
	//
	// Description:
	//
	//   Clamps every channel of a colour to [0, 1], as glColor did.
	//
	//-----------------------------------------------------------------------------------------------------------------

	inline Vector3 clampColour ( const Vector3& colour )
	{
		return Vector3 { clampChannel ( colour.x ), clampChannel ( colour.y ), clampChannel ( colour.z ) };
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Function: transformPoint
	//
	// Description:
	//
	//   Transforms a point by a column-major affine matrix. Used to bake the pod's local pyramid vertices into
	//   world space, since all geometry is drawn in world space with an identity model matrix.
	//
	//-----------------------------------------------------------------------------------------------------------------

	inline Vector3 transformPoint ( const Matrix4& matrix, const Vector3& point )
	{
		return Vector3
		{
			matrix.m [ 0 ] * point.x + matrix.m [ 4 ] * point.y + matrix.m [  8 ] * point.z + matrix.m [ 12 ],
			matrix.m [ 1 ] * point.x + matrix.m [ 5 ] * point.y + matrix.m [  9 ] * point.z + matrix.m [ 13 ],
			matrix.m [ 2 ] * point.x + matrix.m [ 6 ] * point.y + matrix.m [ 10 ] * point.z + matrix.m [ 14 ]
		};
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Function: pushVertex
	//
	// Description:
	//
	//   Appends one positioned, coloured vertex.
	//
	//-----------------------------------------------------------------------------------------------------------------

	inline void pushVertex ( std::vector<Vertex>& out, const Vector3& position, const Vector3& colour )
	{
		out.push_back ( Vertex { position.x, position.y, position.z, colour.x, colour.y, colour.z } );
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Function: formatScore
	//
	// Description:
	//
	//   Right justify a score into the six character, zero filled field. See SCORE_FIELD_WIDTH above.
	//
	//   A score of more than six digits shows its LAST six rather than being clamped or widened. The score cannot
	//   realistically get that far, but wrapping costs nothing and leaves the function total for every input.
	//
	// Arguments:
	//
	//   - score : The score to format.
	//
	// Returns:
	//
	//   The six character field.
	//
	//-----------------------------------------------------------------------------------------------------------------

	std::string formatScore ( int score )
	{
		const std::string digits = std::to_string ( score );

		// Seven digits or more: only the tail that lands inside the template buffer is ever drawn.

		if ( digits.size () >= SCORE_FIELD_WIDTH )
		{
			return digits.substr ( digits.size () - SCORE_FIELD_WIDTH );
		}

		return std::string ( SCORE_FIELD_WIDTH - digits.size (), SCORE_FIELD_FILL ) + digits;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Shader source
//
//   One program serves everything. Flat vertex colour, no lighting, and GL_EXP fog to black computed from view
//   space depth, since a core profile has no fixed function fog to lean on. gl_Position.w is the eye space
//   distance the fog is measured against.
//---------------------------------------------------------------------------------------------------------------------

static const char* VERTEX_SHADER_SOURCE = R"GLSL(
#version 330 core

layout ( location = 0 ) in vec3 aPosition;
layout ( location = 1 ) in vec3 aColour;

uniform mat4 uViewProjection;

out vec3  vColour;
out float vViewDepth;

void main ()
{
	gl_Position = uViewProjection * vec4 ( aPosition, 1.0 );
	vColour     = aColour;
	vViewDepth  = gl_Position.w;
}
)GLSL";

static const char* FRAGMENT_SHADER_SOURCE = R"GLSL(
#version 330 core

in vec3  vColour;
in float vViewDepth;

uniform vec3  uFogColour;
uniform float uFogDensity;
uniform float uAlpha;

out vec4 fragColour;

void main ()
{
	// GL_EXP fog: factor = exp ( -density * distance ), clamped, blended between the fragment and the fog colour.

	float fogFactor = clamp ( exp ( -uFogDensity * vViewDepth ), 0.0, 1.0 );

	vec3 colour = mix ( uFogColour, vColour, fogFactor );

	// uAlpha is 1.0 for every pass except the grid node lattice, which is drawn translucent (see NODE_ALPHA).
	// The fog is applied to the colour first and the alpha sits on top of it, so at 1.0 with blending off this
	// is the plain opaque path.

	fragColour = vec4 ( colour, uAlpha );
}
)GLSL";

//---------------------------------------------------------------------------------------------------------------------
// Function: compileStage
//---------------------------------------------------------------------------------------------------------------------

static unsigned int compileStage ( unsigned int stageType, const char* source, std::string& error )
{
	const unsigned int shader = glCreateShader ( stageType );

	glShaderSource  ( shader, 1, &source, nullptr );
	glCompileShader ( shader );

	int compiled = 0;

	glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

	if ( compiled == 0 )
	{
		char log [ 1024 ] = { 0 };

		glGetShaderInfoLog ( shader, sizeof ( log ) - 1, nullptr, log );

		error = std::string ( "shader compile failed: " ) + log;

		glDeleteShader ( shader );

		return 0;
	}

	return shader;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: configureVertexLayout
//
// Description:
//
//   Binds the one vertex layout every buffer shares: three floats of position at location 0, three floats of
//   colour at location 1.
//
//---------------------------------------------------------------------------------------------------------------------

static void configureVertexLayout ()
{
	glEnableVertexAttribArray ( 0 );
	glVertexAttribPointer     ( 0, 3, GL_FLOAT, GL_FALSE, sizeof ( Vertex ),
	                            reinterpret_cast<const void*> ( offsetof ( Vertex, x ) ) );

	glEnableVertexAttribArray ( 1 );
	glVertexAttribPointer     ( 1, 3, GL_FLOAT, GL_FALSE, sizeof ( Vertex ),
	                            reinterpret_cast<const void*> ( offsetof ( Vertex, r ) ) );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildShader
//---------------------------------------------------------------------------------------------------------------------

bool Renderer::buildShader ( std::string& error )
{
	const unsigned int vertexStage = compileStage ( GL_VERTEX_SHADER, VERTEX_SHADER_SOURCE, error );

	if ( vertexStage == 0 )
	{
		return false;
	}

	const unsigned int fragmentStage = compileStage ( GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SOURCE, error );

	if ( fragmentStage == 0 )
	{
		glDeleteShader ( vertexStage );

		return false;
	}

	shaderProgram = glCreateProgram ();

	glAttachShader ( shaderProgram, vertexStage );
	glAttachShader ( shaderProgram, fragmentStage );
	glLinkProgram  ( shaderProgram );

	int linked = 0;

	glGetProgramiv ( shaderProgram, GL_LINK_STATUS, &linked );

	glDeleteShader ( vertexStage );
	glDeleteShader ( fragmentStage );

	if ( linked == 0 )
	{
		char log [ 1024 ] = { 0 };

		glGetProgramInfoLog ( shaderProgram, sizeof ( log ) - 1, nullptr, log );

		error = std::string ( "shader link failed: " ) + log;

		return false;
	}

	uniformViewProjection = glGetUniformLocation ( shaderProgram, "uViewProjection" );
	uniformFogColour      = glGetUniformLocation ( shaderProgram, "uFogColour"      );
	uniformFogDensity     = glGetUniformLocation ( shaderProgram, "uFogDensity"     );
	uniformAlpha          = glGetUniformLocation ( shaderProgram, "uAlpha"          );

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::initialise
//---------------------------------------------------------------------------------------------------------------------

bool Renderer::initialise ( std::string& error )
{
	const char* missing = nullptr;

	if ( !loadFunctions ( &missing ) )
	{
		error = std::string ( "could not resolve OpenGL entry point: " ) + ( missing ? missing : "unknown" );

		return false;
	}

	if ( !buildShader ( error ) )
	{
		return false;
	}

	// One vertex array and buffer per primitive stream. Storage is grown lazily by drawStream on first use.

	glGenVertexArrays ( 1, &nodeVertexArray  );
	glGenBuffers      ( 1, &nodeVertexBuffer  );
	glBindVertexArray ( nodeVertexArray );
	glBindBuffer      ( GL_ARRAY_BUFFER, nodeVertexBuffer );
	configureVertexLayout ();

	glGenVertexArrays ( 1, &podVertexArray  );
	glGenBuffers      ( 1, &podVertexBuffer  );
	glBindVertexArray ( podVertexArray );
	glBindBuffer      ( GL_ARRAY_BUFFER, podVertexBuffer );
	configureVertexLayout ();

	glGenVertexArrays ( 1, &trailVertexArray  );
	glGenBuffers      ( 1, &trailVertexBuffer  );
	glBindVertexArray ( trailVertexArray );
	glBindBuffer      ( GL_ARRAY_BUFFER, trailVertexBuffer );
	configureVertexLayout ();

	// The HUD overlay's two streams: outlines as lines, filled arrows and bar segments as triangles.

	glGenVertexArrays ( 1, &hudLineVertexArray );
	glGenBuffers      ( 1, &hudLineVertexBuffer );
	glBindVertexArray ( hudLineVertexArray );
	glBindBuffer      ( GL_ARRAY_BUFFER, hudLineVertexBuffer );
	configureVertexLayout ();

	glGenVertexArrays ( 1, &hudTriangleVertexArray );
	glGenBuffers      ( 1, &hudTriangleVertexBuffer );
	glBindVertexArray ( hudTriangleVertexArray );
	glBindBuffer      ( GL_ARRAY_BUFFER, hudTriangleVertexBuffer );
	configureVertexLayout ();

	glBindVertexArray ( 0 );

	// Two capabilities and no more: depth test and fog. No back-face culling, and the default depth function.
	// Blending is switched on only for the translucent lattice pass and switched straight off again.

	glEnable    ( GL_DEPTH_TEST );
	glDepthFunc ( GL_LESS );
	glDisable   ( GL_BLEND );

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::describeContext
//---------------------------------------------------------------------------------------------------------------------

std::string Renderer::describeContext () const
{
	auto readString = [] ( unsigned int name ) -> std::string
	{
		const GLubyte* value = glGetString ( name );

		return ( value != nullptr ) ? std::string ( reinterpret_cast<const char*> ( value ) ) : std::string ( "?" );
	};

	return readString ( GL_VENDOR ) + " | " + readString ( GL_RENDERER ) + " | GL " + readString ( GL_VERSION );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::shutdown
//---------------------------------------------------------------------------------------------------------------------

void Renderer::shutdown ()
{
	if ( shaderProgram != 0 )
	{
		glDeleteProgram ( shaderProgram );

		shaderProgram = 0;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::resize
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::resize
//
// Description:
//
//   Fits the largest 4:3 viewport that will fit inside the client area and centres it, leaving black bars on the
//   two spare edges.
//
//   The 4:3 is not a preference. Every on-screen layout is authored against a 640 x 480 frame: the HUD readouts
//   sit at 90% of the frame in BOTH axes at 4:3, and at anything wider they hold their vertical placement but
//   drift inward horizontally, so the composition falls apart.
//
//   Letterboxing rather than pinning the window keeps the framing exact at any size, including maximised, and
//   costs nothing but the bars.
//
//   glClear is not affected by the viewport and no scissor is ever set, so the bars are cleared to the same black
//   as the arena and need no drawing of their own.
//
// Arguments:
//
//   - width, height : The new client area size, in pixels.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::resize ( int width, int height )
{
	const int clientWidth  = ( width  > 0 ) ? width  : 1;
	const int clientHeight = ( height > 0 ) ? height : 1;

	int fittedWidth  = clientWidth;
	int fittedHeight = static_cast<int> ( static_cast<float> ( clientWidth ) / TARGET_ASPECT_RATIO );

	if ( fittedHeight > clientHeight )
	{
		fittedHeight = clientHeight;
		fittedWidth  = static_cast<int> ( static_cast<float> ( clientHeight ) * TARGET_ASPECT_RATIO );
	}

	viewportWidth   = ( fittedWidth  > 0 ) ? fittedWidth  : 1;
	viewportHeight  = ( fittedHeight > 0 ) ? fittedHeight : 1;
	viewportOriginX = ( clientWidth  - viewportWidth  ) / 2;
	viewportOriginY = ( clientHeight - viewportHeight ) / 2;

	glViewport ( viewportOriginX, viewportOriginY, viewportWidth, viewportHeight );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::updateCameraBasis
//
// Description:
//
//   Move the smoothed orientation basis toward the player's true basis. Each component steps linearly at the
//   active view's rate and clamps exactly on its target.
//
//   The camera eye tracks the player's exact position, but the look direction and up come from this smoothed
//   basis, so the view swings through each 90 degree turn at a constant speed and stops dead when it lines up.
//
//   The smoothed vectors are NOT renormalised here, and that is not an oversight. Mid swing the vector drops to
//   about 0.707 of unit length, which quickens the apparent rotation through the middle of the turn and is what
//   makes it feel like it leans into the corner. Renormalise every frame and you flatten that out. Only the view
//   construction normalises.
//
// Arguments:
//
//   - player            : The pod whose orientation the camera follows.
//   - cameraMode        : Selects the smoothing rate.
//   - frameDeltaSeconds : Real seconds since the previous frame.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::updateCameraBasis ( const LightPod& player, CameraMode cameraMode, float frameDeltaSeconds )
{
	// On the first frame, or the frame after a restart, snap to the true basis so there is no swing on start-up.

	if ( !smoothingInitialised )
	{
		smoothedForward      = player.forward;
		smoothedUp           = player.up;
		smoothingInitialised = true;

		return;
	}

	const float rate = ( cameraMode == CameraMode::Cockpit )     ? SMOOTHING_RATE_COCKPIT
	                 : ( cameraMode == CameraMode::ThirdPerson )  ? SMOOTHING_RATE_THIRD_PERSON
	                                                              : SMOOTHING_RATE_SPOT_PLANE;

	const float step = rate * CAMERA_TURN_SPEED_MULTIPLIER * frameDeltaSeconds;

	smoothedForward.x = moveTowards ( smoothedForward.x, player.forward.x, step );
	smoothedForward.y = moveTowards ( smoothedForward.y, player.forward.y, step );
	smoothedForward.z = moveTowards ( smoothedForward.z, player.forward.z, step );

	smoothedUp.x      = moveTowards ( smoothedUp.x,      player.up.x,      step );
	smoothedUp.y      = moveTowards ( smoothedUp.y,      player.up.y,      step );
	smoothedUp.z      = moveTowards ( smoothedUp.z,      player.up.z,      step );

	// Defensive, and it should never fire. If a swing interrupted by a further turn ever ran the ramped forward
	// through an exact zero, the view would try to normalise a zero vector and hand us a frame full of NaNs.

	if ( dot ( smoothedForward, smoothedForward ) < 1.0e-6f )
	{
		smoothedForward = player.forward;
	}

	if ( dot ( smoothedUp, smoothedUp ) < 1.0e-6f )
	{
		smoothedUp = player.up;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildProjectionMatrix
//
// Description:
//
//   The per-view projection. Field of view differs by view; near and far are shared. The front end has its own
//   wider projection, built elsewhere.
//
// Arguments:
//
//   - cameraMode : Selects the field of view.
//
// Returns:
//
//   - The projection matrix for the view.
//
//---------------------------------------------------------------------------------------------------------------------

Matrix4 Renderer::buildProjectionMatrix ( CameraMode cameraMode ) const
{
	const float fieldOfViewDegrees = ( cameraMode == CameraMode::Cockpit )    ? FOV_COCKPIT
	                               : ( cameraMode == CameraMode::ThirdPerson ) ? FOV_THIRD_PERSON
	                                                                           : FOV_SPOT_PLANE;

	const float aspectRatio = static_cast<float> ( viewportWidth ) / static_cast<float> ( viewportHeight );

	return Matrix4::perspective ( fieldOfViewDegrees * DEGREES_TO_RADIANS, aspectRatio, NEAR_PLANE, FAR_PLANE );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildViewMatrix
//
// Description:
//
//   The per-view look-at. The eye sits at the player's exact position; the look direction and up come from the
//   smoothed basis.
//
//   All three views handle a pod travelling straight up or down, because the up vector rotates with the pod
//   instead of being pinned to world up. Pin it and the camera flips over at the poles.
//
// Arguments:
//
//   - player      : The pod the camera follows.
//   - cameraMode  : Which view to build.
//   - arenaExtent : The grid size, for the spot-plane standoff.
//
// Returns:
//
//   - The view matrix.
//
//---------------------------------------------------------------------------------------------------------------------

Matrix4 Renderer::buildViewMatrix ( const LightPod& player, CameraMode cameraMode, float arenaExtent ) const
{
	const Vector3& position = player.position;
	const Vector3& forward  = smoothedForward;
	const Vector3& up       = smoothedUp;

	switch ( cameraMode )
	{
		case CameraMode::Cockpit:
		default:
		{
			// F1: eye at the pod, looking 11 units ahead along forward.

			return Matrix4::lookAt ( position, position + forward * LOOK_AHEAD, up );
		}

		case CameraMode::ThirdPerson:
		{
			// F2: 0.8 behind and 0.4 above the pod, looking at the same point 11 units ahead.

			const Vector3 eye = position - forward * THIRD_BACK + up * THIRD_UP;

			return Matrix4::lookAt ( eye, position + forward * LOOK_AHEAD, up );
		}

		case CameraMode::SpotPlane:
		{
			// F3: offset 0.5 on every axis, pulled back by grid x 0.2 and lifted by 1, looking at the pod. The
			// whole offset is then scaled in along its own ray, so the framing direction does not change.

			const float   standoff = arenaExtent * SPOT_STANDOFF_K;
			const Vector3 offset { SPOT_OFFSET, SPOT_OFFSET, SPOT_OFFSET };
			const Vector3 eye = position + ( offset - forward * standoff + up * SPOT_UP ) * SPOT_DISTANCE_MULTIPLIER;

			return Matrix4::lookAt ( eye, position, up );
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildGridNodes
//
// Description:
//
//   Builds the arena: a wireframe cube at every integer cell of the fixed 10x10x10 lattice. Occupied cells take
//   their occupant's colour brightened by +0.5; empty cells are mid-grey. This is the whole arena - there are no
//   separate walls. The full 10x10x10 lattice is drawn whatever the menu grid size says, so a smaller arena
//   still shows the whole cage around it.
//
//   Eight corners at (+-h, +-h, +-h) and twelve edges, four along each axis.
//
// Arguments:
//
//   - simulation : Source of the occupancy grid and pod colours.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildGridNodes ( const Simulation& simulation )
{
	nodeScratch.clear ();

	const float h = NODE_HALF_EXTENT;

	const Vector3 corner [ 8 ] =
	{
		{  h,  h,  h }, { -h,  h,  h }, { -h, -h,  h }, {  h, -h,  h },
		{  h,  h, -h }, { -h,  h, -h }, { -h, -h, -h }, {  h, -h, -h }
	};

	const int edge [ 12 ] [ 2 ] =
	{
		{ 0, 1 }, { 3, 2 }, { 4, 5 }, { 7, 6 },     // the four edges along X
		{ 0, 4 }, { 1, 5 }, { 3, 7 }, { 2, 6 },     // the four edges along Z
		{ 0, 3 }, { 1, 2 }, { 4, 7 }, { 5, 6 }      // the four edges along Y
	};

	const Vector3 grey { NODE_EMPTY_GREY, NODE_EMPTY_GREY, NODE_EMPTY_GREY };

	const std::array<std::uint8_t, GRID_CELL_COUNT>& occupancy = simulation.collisionGrid ().rawOccupancy ();
	const std::vector<LightPod>&                   pods    = simulation.allPods ();

	for ( int x = 0; x < GRID_DIMENSION; x++ )
	{
		for ( int y = 0; y < GRID_DIMENSION; y++ )
		{
			for ( int z = 0; z < GRID_DIMENSION; z++ )
			{
				const std::uint8_t occupant = occupancy [ Grid::index ( x, y, z ) ];

				Vector3 colour = grey;

				if ( occupant != 0 )
				{
					const std::size_t owner = static_cast<std::size_t> ( occupant ) - 1;

					if ( owner < pods.size () )
					{
						const Vector3 brighten { NODE_BRIGHTEN, NODE_BRIGHTEN, NODE_BRIGHTEN };

						colour = clampColour ( pods [ owner ].colour + brighten );
					}
				}

				const Vector3 origin { static_cast<float> ( x ), static_cast<float> ( y ), static_cast<float> ( z ) };

				for ( const auto& pair : edge )
				{
					pushVertex ( nodeScratch, origin + corner [ pair [ 0 ] ], colour );
					pushVertex ( nodeScratch, origin + corner [ pair [ 1 ] ], colour );
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildPods
//
// Description:
//
//   Build the light pod body for every living pod: a four sided pyramid pointing along the pod's forward
//   axis, baked into world space.
//
//   Two faces are grey and two are tinted with the pod's colour, one lighter and one darker. There is no
//   lighting anywhere in this renderer, so the shading is painted straight onto the vertex colours. It reads
//   perfectly well at the speed the pods move.
//
//   The player's own pod is NOT drawn in the cockpit view. You are sitting in it.
//
// Arguments:
//
//   - simulation    : Source of the pods.
//   - drawPlayerPod : Whether to draw the player's own pod. False in the cockpit view.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildPods ( const Simulation& simulation, bool drawPlayerPod )
{
	podScratch.clear ();

	// The four triangles of the pyramid, in local space before scaling.

	const Vector3 apex { 6.0f, 0.0f, 0.0f };

	const Vector3 triangle [ 4 ] [ 3 ] =
	{
		{ {  0.0f,  2.0f,  2.0f }, apex,                     {  0.0f, -2.0f,  2.0f } },
		{ {  0.0f,  2.0f, -2.0f }, apex,                     {  0.0f, -2.0f, -2.0f } },
		{ {  0.0f,  2.0f,  2.0f }, {  0.0f,  2.0f, -2.0f },  apex                    },
		{ {  0.0f, -2.0f,  2.0f }, {  0.0f, -2.0f, -2.0f },  apex                    }
	};

	const Vector3 grey { POD_GREY, POD_GREY, POD_GREY };

	const std::vector<LightPod>& pods = simulation.allPods ();

	for ( std::size_t i = 0; i < pods.size (); i++ )
	{
		const LightPod& pod = pods [ i ];

		if ( !pod.isAlive () )
		{
			continue;
		}

		// The player is index 0. Skip its pod in the cockpit view.

		if ( ( i == 0 ) && !drawPlayerPod )
		{
			continue;
		}

		// Local +X (the pyramid's pointing axis) maps to the pod's forward; local +Y to its up. The third basis
		// vector completes a right-handed frame. No trigonometry - the pod carries its own exact basis.

		const Vector3 side  = cross ( pod.forward, pod.up );
		const Matrix4 model = Matrix4::fromBasis ( pod.forward, pod.up, side, pod.position, POD_SCALE );

		const Vector3 tint       = pod.colour;
		const Vector3 tintOffset { POD_TINT_OFFSET, POD_TINT_OFFSET, POD_TINT_OFFSET };

		const Vector3 faceColour [ 4 ] =
		{
			grey,                                                   // triangle 0: grey
			grey,                                                   // triangle 1: grey
			clampColour ( tint + tintOffset ),                      // triangle 2: pod colour, lighter
			clampColour ( tint - tintOffset )                       // triangle 3: pod colour, darker
		};

		for ( int t = 0; t < 4; t++ )
		{
			for ( int v = 0; v < 3; v++ )
			{
				pushVertex ( podScratch, transformPoint ( model, triangle [ t ] [ v ] ), faceColour [ t ] );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildTrails
//
// Description:
//
//   Build every living pod's light trail as a chain of closed square section tubes, one per segment between
//   consecutive corners.
//
//   Each tube is over extended by its own half width past both endpoints. Where two perpendicular segments meet
//   at a grid node the two tubes then overlap inside the node cube and the right angle fills in on its own, so
//   there is no need to cap the corners.
//
//   Trail segments are always axis aligned, because motion steps along the pod's basis vectors. The cross
//   section is built per run axis, and the fake shading of +-0.3 goes on opposite faces of the two horizontal
//   runs and not at all on a vertical one. That asymmetry is on purpose: a vertical run has no lit side.
//
//   Skip the live tip on the frame a turn commits. The slot has been opened but still holds the sentinel, and
//   drawing out to it would throw a tube at the corner of the world.
//
//   HIDING THE PLAYER'S NEWEST SEGMENTS. The player's trail runs two segments short in the cockpit view, and in
//   the third person view unless L is on. Opponents and the spot plane view always draw the lot.
//
//   This is not an optimisation, it is the cure for the trail flashing across the camera during a turn. At a
//   fresh corner the eye ends up inside two things at once: the just committed tube, whose end over extends half
//   a width past the corner, and the short reversed stub the first person tip pullback briefly creates. Those
//   are exactly the two segments the shortened loop leaves out. Draw them and the screen strobes on every turn.
//
// Arguments:
//
//   - simulation : Source of the trails.
//   - cameraMode : The active view, which selects the player's trail visibility rule above.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildTrails ( const Simulation& simulation, CameraMode cameraMode )
{
	trailScratch.clear ();

	const float h = TRAIL_HALF_WIDTH;

	const std::vector<LightPod>& pods = simulation.allPods ();

	for ( std::size_t podIndex = 0; podIndex < pods.size (); podIndex++ )
	{
		const LightPod& pod = pods [ podIndex ];

		if ( !pod.isAlive () )
		{
			continue;
		}

		const std::size_t pointCount = pod.hasValidTip () ? pod.trail.size ()
		                                                    : pod.trail.size () - 1;

		if ( pointCount < 2 )
		{
			continue;
		}

		// The per-view visibility rule from the header: hide the player's two newest segments in the cockpit
		// view, and in the third person view while L is off.

		const bool restrictNearPod =
			( podIndex == 0 ) &&
			( ( cameraMode == CameraMode::Cockpit ) ||
			  ( ( cameraMode == CameraMode::ThirdPerson ) && !thirdPersonFullTrailEnabled ) );

		for ( std::size_t i = 0; i + 1 < pointCount; i++ )
		{
			if ( restrictNearPod && ( static_cast<int> ( i ) >= pod.cornerCount () - 2 ) )
			{
				break;
			}
			const Vector3& from  = pod.trail [ i ];
			const Vector3& to    = pod.trail [ i + 1 ];
			const Vector3  delta = to - from;

			// Pick the run axis, the two perpendicular cross section axes, and the per face shading. Opposite
			// faces get +-0.3 on a horizontal run, and a vertical run stays flat.

			const Vector3 c        = pod.colour;
			const Vector3 lighter  = clampColour ( c + Vector3 { TRAIL_SHADE, TRAIL_SHADE, TRAIL_SHADE } );
			const Vector3 darker   = clampColour ( c - Vector3 { TRAIL_SHADE, TRAIL_SHADE, TRAIL_SHADE } );

			Vector3 runAxis  {};
			Vector3 axisU    {};
			Vector3 axisV    {};
			Vector3 faceColour [ 4 ];

			if ( ( from.x == to.x ) && ( from.y == to.y ) )
			{
				// Z-aligned run: cross-section in X and Y.

				runAxis = { 0.0f, 0.0f, 1.0f };
				axisU   = { 1.0f, 0.0f, 0.0f };
				axisV   = { 0.0f, 1.0f, 0.0f };

				faceColour [ 0 ] = c; faceColour [ 1 ] = darker; faceColour [ 2 ] = c; faceColour [ 3 ] = lighter;
			}
			else if ( ( from.x == to.x ) && ( from.z == to.z ) )
			{
				// Y-aligned run: cross-section in X and Z. No shading - all four faces flat.

				runAxis = { 0.0f, 1.0f, 0.0f };
				axisU   = { 1.0f, 0.0f, 0.0f };
				axisV   = { 0.0f, 0.0f, 1.0f };

				faceColour [ 0 ] = c; faceColour [ 1 ] = c; faceColour [ 2 ] = c; faceColour [ 3 ] = c;
			}
			else if ( ( from.y == to.y ) && ( from.z == to.z ) )
			{
				// X-aligned run: cross-section in Y and Z.

				runAxis = { 1.0f, 0.0f, 0.0f };
				axisU   = { 0.0f, 1.0f, 0.0f };
				axisV   = { 0.0f, 0.0f, 1.0f };

				faceColour [ 0 ] = lighter; faceColour [ 1 ] = c; faceColour [ 2 ] = darker; faceColour [ 3 ] = c;
			}
			else
			{
				continue;                           // not axis-aligned; cannot happen with grid-stepped motion
			}

			// Direction of travel along the run axis, used to push each end outward by the half-width. delta lies
			// along the run axis, so its component sum is the signed distance travelled.

			const Vector3 direction = ( componentSum ( delta ) >= 0.0f ) ? runAxis : -runAxis;

			const Vector3 nearCentre = from - direction * h;
			const Vector3 farCentre  = to   + direction * h;

			// The four cross-section corners, in order around the square.

			const float offsetU [ 4 ] = {  h,  h, -h, -h };
			const float offsetV [ 4 ] = {  h, -h, -h,  h };

			Vector3 nearCorner [ 4 ];
			Vector3 farCorner  [ 4 ];

			for ( int k = 0; k < 4; k++ )
			{
				nearCorner [ k ] = nearCentre + axisU * offsetU [ k ] + axisV * offsetV [ k ];
				farCorner  [ k ] = farCentre  + axisU * offsetU [ k ] + axisV * offsetV [ k ];
			}

			// Four side faces close the tube. Each quad becomes two triangles; core profile has no GL_QUADS.

			for ( int k = 0; k < 4; k++ )
			{
				const int next = ( k + 1 ) % 4;

				pushVertex ( trailScratch, nearCorner [ k ],    faceColour [ k ] );
				pushVertex ( trailScratch, nearCorner [ next ], faceColour [ k ] );
				pushVertex ( trailScratch, farCorner  [ next ], faceColour [ k ] );

				pushVertex ( trailScratch, nearCorner [ k ],    faceColour [ k ] );
				pushVertex ( trailScratch, farCorner  [ next ], faceColour [ k ] );
				pushVertex ( trailScratch, farCorner  [ k ],    faceColour [ k ] );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildTurnArrows
//
// Description:
//
//   Build the turn queue D-pad: four arrows in a cross, each pointing outward along one arm, showing which 90
//   degree turn the player has queued.
//
//   The arrow matching the pending turn blinks between filled and outline; the other three stay outlines. Turn
//   1/4/2/3 picks the arrow at rotation 0/270/180/90, which points up/right/down/left.
//
//   All arrows are green. Filled ones go to the triangle stream, outlines to the line stream as three edges.
//
// Arguments:
//
//   - player   : The pod whose queued turn drives the blink. Read-only.
//   - hudScale : The uniform HUD scale for the current view.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildTurnArrows ( const LightPod& player, float hudScale )
{
	const Vector3 green { 0.0f, 1.0f, 0.0f };

	// Filled while the blink phase is at or below the threshold, outline above it.

	const bool blinkFilled = hudBlinkPhase <= HUD_BLINK_THRESHOLD;

	struct ArrowBlock
	{
		float blockX;
		float blockY;
		float angleDegrees;
		Turn  turn;
	};

	// The four arms of the cross. The angle aims each arrow outward along its own arm.

	const ArrowBlock blocks [ 4 ] =
	{
		{  0.0f,  1.0f,   0.0f, Turn::Up    },       // top,    points up,    filled when turn 1 is queued
		{  1.0f,  0.0f, 270.0f, Turn::Right },       // right,  points right, filled when turn 4 is queued
		{  0.0f, -1.0f, 180.0f, Turn::Down  },       // bottom, points down,  filled when turn 2 is queued
		{ -1.0f,  0.0f,  90.0f, Turn::Left  }        // left,   points left,  filled when turn 3 is queued
	};

	// The arrow's three local corners: base left, base right, tip.

	const float localX [ 3 ] = { -ARROW_BASE_HALF, ARROW_BASE_HALF, 0.0f       };
	const float localY [ 3 ] = {  0.0f,            0.0f,            ARROW_TIP_Y };

	for ( const ArrowBlock& block : blocks )
	{
		Vector3 corner [ 3 ];

		for ( int i = 0; i < 3; i++ )
		{
			corner [ i ] = hudPoint ( localX [ i ], localY [ i ], block.angleDegrees,
			                          block.blockX, block.blockY, HUD_DPAD_BASE_X, HUD_DPAD_BASE_Y, hudScale );
		}

		const bool isPending  = ( player.turnPending == block.turn );
		const bool drawFilled = isPending && blinkFilled;

		if ( drawFilled )
		{
			pushVertex ( hudTriangleScratch, corner [ 0 ], green );
			pushVertex ( hudTriangleScratch, corner [ 1 ], green );
			pushVertex ( hudTriangleScratch, corner [ 2 ], green );
		}
		else
		{
			// Outline: the three edges of the triangle, as line pairs.

			for ( int i = 0; i < 3; i++ )
			{
				pushVertex ( hudLineScratch, corner [ i ],             green );
				pushVertex ( hudLineScratch, corner [ ( i + 1 ) % 3 ], green );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildProximitySensor
//
// Description:
//
//   Build the proximity sensor: four three segment bars in a cross, each sampling the collision grid outward
//   from the player along one direction.
//
//   A segment lights when a cell in its band is occupied, near/mid/far in red/yellow/green. All three outlines
//   are always drawn in green, so a clear direction shows an empty green frame rather than nothing at all.
//
//   The cross of bars sits around the direction indicator, sharing its base and block offsets. Armed at every
//   game start, toggled with N.
//
// Arguments:
//
//   - simulation : Source of the player and the collision grid. Read-only.
//   - hudScale   : The uniform HUD scale for the current view.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildProximitySensor ( const Simulation& simulation, float hudScale )
{
	const LightPod& player = simulation.player ();
	const Grid&       grid   = simulation.collisionGrid ();

	const Vector3 segmentColour [ 3 ] =
	{
		{ 1.0f, 0.0f, 0.0f },                        // A, nearest : red
		{ 1.0f, 1.0f, 0.0f },                        // B, mid     : yellow
		{ 0.0f, 1.0f, 0.0f }                         // C, far     : green
	};

	const Vector3 green { 0.0f, 1.0f, 0.0f };

	// The player's own cell and basis. Grid-stepped motion keeps the basis axis-aligned, so the up and side
	// vectors are unit steps along grid axes and round to integer cell offsets directly.

	const Vector3 side = cross ( player.forward, player.up );

	const int cellX = static_cast<int> ( std::lround ( player.position.x ) );
	const int cellY = static_cast<int> ( std::lround ( player.position.y ) );
	const int cellZ = static_cast<int> ( std::lround ( player.position.z ) );

	struct SensorBar
	{
		float   blockX;
		float   blockY;
		float   angleDegrees;
		Vector3 senseDirection;
	};

	const SensorBar bars [ 4 ] =
	{
		{  0.0f,  1.0f,   0.0f,  player.up },         // top,    senses along the player's up
		{  1.0f,  0.0f, 270.0f,  side      },         // right,  senses along the player's side
		{  0.0f, -1.0f, 180.0f, -player.up },         // bottom, senses opposite the up
		{ -1.0f,  0.0f,  90.0f, -side      }          // left,   senses opposite the side
	};

	for ( const SensorBar& bar : bars )
	{
		const int stepX = static_cast<int> ( std::lround ( bar.senseDirection.x ) );
		const int stepY = static_cast<int> ( std::lround ( bar.senseDirection.y ) );
		const int stepZ = static_cast<int> ( std::lround ( bar.senseDirection.z ) );

		for ( int segment = 0; segment < 3; segment++ )
		{
			// Segment index 0/1/2 samples the cell 1/2/3 steps out (near/mid/far).

			const int distance = segment + 1;

			const bool occupied = grid.occupantAt ( cellX + stepX * distance,
			                                         cellY + stepY * distance,
			                                         cellZ + stepZ * distance ) != 0;

			const float lowY  = BAR_SEGMENT_LOW  [ segment ];
			const float highY = BAR_SEGMENT_HIGH [ segment ];

			// Four corners of the segment rectangle, counter-clockwise.

			const Vector3 bottomLeft  = hudPoint ( -BAR_HALF_WIDTH, lowY,  bar.angleDegrees, bar.blockX, bar.blockY,
			                                        HUD_PROXIMITY_BASE_X, HUD_PROXIMITY_BASE_Y, hudScale );
			const Vector3 bottomRight = hudPoint (  BAR_HALF_WIDTH, lowY,  bar.angleDegrees, bar.blockX, bar.blockY,
			                                        HUD_PROXIMITY_BASE_X, HUD_PROXIMITY_BASE_Y, hudScale );
			const Vector3 topRight    = hudPoint (  BAR_HALF_WIDTH, highY, bar.angleDegrees, bar.blockX, bar.blockY,
			                                        HUD_PROXIMITY_BASE_X, HUD_PROXIMITY_BASE_Y, hudScale );
			const Vector3 topLeft     = hudPoint ( -BAR_HALF_WIDTH, highY, bar.angleDegrees, bar.blockX, bar.blockY,
			                                        HUD_PROXIMITY_BASE_X, HUD_PROXIMITY_BASE_Y, hudScale );

			// Filled segment, only when its cell is occupied: the quad as two triangles.

			if ( occupied )
			{
				const Vector3& fill = segmentColour [ segment ];

				pushVertex ( hudTriangleScratch, bottomLeft,  fill );
				pushVertex ( hudTriangleScratch, bottomRight, fill );
				pushVertex ( hudTriangleScratch, topRight,    fill );

				pushVertex ( hudTriangleScratch, bottomLeft,  fill );
				pushVertex ( hudTriangleScratch, topRight,    fill );
				pushVertex ( hudTriangleScratch, topLeft,     fill );
			}

			// Outline, always, in green: the four edges of the rectangle.

			pushVertex ( hudLineScratch, bottomLeft,  green ); pushVertex ( hudLineScratch, bottomRight, green );
			pushVertex ( hudLineScratch, bottomRight, green ); pushVertex ( hudLineScratch, topRight,    green );
			pushVertex ( hudLineScratch, topRight,    green ); pushVertex ( hudLineScratch, topLeft,     green );
			pushVertex ( hudLineScratch, topLeft,     green ); pushVertex ( hudLineScratch, bottomLeft,  green );
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildHud
//
// Description:
//
//   Build the whole 2D overlay for this frame into the two HUD streams: the turn arrows always, and the
//   proximity sensor only when it is switched on. Nothing here writes simulation state.
//
// Arguments:
//
//   - simulation : The state to overlay. Read-only.
//   - hudScale   : The uniform HUD scale for the current view.
//
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildText
//
// Description:
//
//   Draw a string with the stroke font into the HUD line stream, on a constant z plane.
//
//   The pen starts at the origin and advances by each glyph's own advance width. Every glyph is a run of line
//   segments in the font's coordinate space (x right, y up), scaled independently in x and y and offset to the
//   origin. One batch for the whole string rather than a call per glyph.
//
//   The character is masked to the 128 glyph table, so a high byte cannot index off the end. Everything drawn
//   here is plain ASCII anyway.
//
// Arguments:
//
//   - originX, originY : Baseline origin in the plane.
//   - originZ          : The plane's depth: -1 for the HUD overlay, deeper for menu text.
//   - scaleX, scaleY   : Independent glyph scale.
//   - colour           : Stroke colour.
//   - text             : The string to draw.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildText ( float originX, float originY, float originZ, float scaleX, float scaleY,
                           const Vector3& colour, const std::string& text )
{
	float penX = 0.0f;

	for ( const char character : text )
	{
		const FontGlyph& glyph = FONT_GLYPHS [ static_cast<unsigned char> ( character ) & 0x7F ];

		for ( int s = 0; s < glyph.segmentCount; s++ )
		{
			const FontSegment& segment = FONT_SEGMENTS [ glyph.firstSegment + s ];

			const Vector3 start
			{
				originX + ( penX + segment.x0 ) * scaleX,
				originY + segment.y0 * scaleY,
				originZ
			};

			const Vector3 end
			{
				originX + ( penX + segment.x1 ) * scaleX,
				originY + segment.y1 * scaleY,
				originZ
			};

			pushVertex ( hudLineScratch, start, colour );
			pushVertex ( hudLineScratch, end,   colour );
		}

		penX += glyph.advance;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildReadouts
//
// Description:
//
//   Draw the SPEED / SCORE / opponents text in green, at the layout for the active view. SPEED is 100 times the
//   player's speed as an integer, SCORE is the score, and the opponents line is the live pod count less the
//   player.
//
// Arguments:
//
//   - simulation : Source of the player, score and alive count. Read-only.
//   - large      : Selects the large layout (view 2) over the small one (views 1 and 3).
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildReadouts ( const Simulation& simulation, bool large )
{
	const ReadoutLayout& layout = large ? READOUT_LARGE : READOUT_SMALL;
	const LightPod&    player = simulation.player ();
	const Vector3        green { 0.0f, 1.0f, 0.0f };

	const int speedDisplay  = static_cast<int> ( player.speed * SPEED_DISPLAY_SCALE );
	const int opponentsLeft = ( simulation.alivePodCount () > 0 ) ? simulation.alivePodCount () - 1 : 0;

	// SCORE alone is right justified into the six character zero filled field. SPEED and the opponents count go
	// out unpadded.

	const std::string speedText     = std::to_string ( speedDisplay );
	const std::string scoreText     = formatScore    ( player.score );
	const std::string opponentsText = std::to_string ( opponentsLeft );

	auto drawItem = [ & ] ( const TextItem& item, const std::string& value )
	{
		const std::string& string = ( item.label != nullptr ) ? std::string ( item.label ) : value;

		buildText ( item.x, item.y, -1.0f, layout.scaleX, layout.scaleY, green, string );
	};

	drawItem ( layout.speedLabel,     speedText );
	drawItem ( layout.speedValue,     speedText );
	drawItem ( layout.scoreLabel,     scoreText );
	drawItem ( layout.scoreValue,     scoreText );
	drawItem ( layout.opponentsLabel, opponentsText );
	drawItem ( layout.opponentsValue, opponentsText );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildFreeLookText
//
// Description:
//
//   The free-look view's entire overlay: the opponents readout and the "TEST MODE" label, both green, both at
//   the small layout's position and scale. The same text block views 1 and 3 draw, plus the label they never
//   show.
//
// Arguments:
//
//   - simulation : Source of the live pod count. Read-only.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildFreeLookText ( const Simulation& simulation )
{
	const Vector3 green { 0.0f, 1.0f, 0.0f };

	const int opponentsLeft = ( simulation.alivePodCount () > 0 ) ? simulation.alivePodCount () - 1 : 0;

	buildText ( READOUT_SMALL.opponentsLabel.x, READOUT_SMALL.opponentsLabel.y, -1.0f,
	            READOUT_SMALL.scaleX, READOUT_SMALL.scaleY, green, READOUT_SMALL.opponentsLabel.label );

	buildText ( READOUT_SMALL.opponentsValue.x, READOUT_SMALL.opponentsValue.y, -1.0f,
	            READOUT_SMALL.scaleX, READOUT_SMALL.scaleY, green, std::to_string ( opponentsLeft ) );

	buildText ( TEST_MODE_X, READOUT_SMALL.opponentsLabel.y, -1.0f,
	            READOUT_SMALL.scaleX, READOUT_SMALL.scaleY, green, TEST_MODE_TEXT );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildHud
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildHud ( const Simulation& simulation, float hudScale )
{
	hudLineScratch.clear     ();
	hudTriangleScratch.clear ();

	if ( simulation.allPods ().empty () )
	{
		return;
	}

	buildTurnArrows ( simulation.player (), hudScale );

	if ( proximitySensorEnabled )
	{
		buildProximitySensor ( simulation, hudScale );
	}

	// The wide third-person view used the "large" HUD duplicates; views 1 and 3 the "small" ones.

	buildReadouts ( simulation, hudScale >= HUD_SCALE_LARGE );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::drawStream
//
// Description:
//
//   Uploads a scratch vertex list into a streaming buffer, growing it with headroom when it must, and draws it.
//
// Arguments:
//
//   - vertexArray  : The buffer's vertex array object.
//   - vertexBuffer : The buffer.
//   - capacity     : The buffer's current byte capacity. Updated when the buffer grows.
//   - vertices     : The vertices to draw.
//   - primitive    : GL_LINES or GL_TRIANGLES.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::drawStream ( unsigned int vertexArray, unsigned int vertexBuffer, std::size_t& capacity,
                            const std::vector<Vertex>& vertices, unsigned int primitive )
{
	if ( vertices.empty () )
	{
		return;
	}

	const std::size_t requiredBytes = vertices.size () * sizeof ( Vertex );

	glBindVertexArray ( vertexArray );
	glBindBuffer      ( GL_ARRAY_BUFFER, vertexBuffer );

	if ( requiredBytes > capacity )
	{
		// Grow with headroom so a long game does not reallocate the buffer every frame.

		capacity = requiredBytes * 2;

		glBufferData ( GL_ARRAY_BUFFER, static_cast<GLsizeiptr> ( capacity ), nullptr, GL_STREAM_DRAW );
	}

	glBufferSubData ( GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr> ( requiredBytes ), vertices.data () );

	glDrawArrays ( primitive, 0, static_cast<GLsizei> ( vertices.size () ) );

	glBindVertexArray ( 0 );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::drawWorldStreams
//
// Description:
//
//   Draws the three world streams. The opaque geometry - pods and trails - goes down first, writing depth as
//   usual; the grid-node lattice follows, blended over it at NODE_ALPHA.
//
//   The ordering is what makes the transparency come out right. Translucent geometry has to be composited over
//   what sits behind it, so it can only be drawn once that geometry is already in the framebuffer. With an
//   opaque lattice (NODE_ALPHA = 1.0) the order stops mattering and the nodes could go down first.
//
//   Depth WRITES are disabled for the lattice while the depth TEST stays on. The test is what keeps the lattice
//   correctly hidden behind pods and trails. Disabling the write is what stops the 12,000 cube edges occluding
//   one another - with writes on, whichever edge happened to be drawn first would reject the ones behind it and
//   the lattice would blend inconsistently depending on the x/y/z build order rather than on what is actually
//   in front.
//
//   All state is restored on the way out, so callers see the pipeline exactly as they left it.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::drawWorldStreams ()
{
	drawStream ( podVertexArray,   podVertexBuffer,   podBufferCapacity,   podScratch,   GL_TRIANGLES );
	drawStream ( trailVertexArray, trailVertexBuffer, trailBufferCapacity, trailScratch, GL_TRIANGLES );

	const bool blendNodes = ( NODE_ALPHA < 1.0f );

	if ( blendNodes )
	{
		glUniform1f ( uniformAlpha, NODE_ALPHA );
		glEnable    ( GL_BLEND );
		glBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDepthMask ( GL_FALSE );
	}

	drawStream ( nodeVertexArray, nodeVertexBuffer, nodeBufferCapacity, nodeScratch, GL_LINES );

	if ( blendNodes )
	{
		glDepthMask ( GL_TRUE );
		glDisable   ( GL_BLEND );
		glUniform1f ( uniformAlpha, 1.0f );
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::renderFrame
//---------------------------------------------------------------------------------------------------------------------

void Renderer::renderFrame ( const Simulation& simulation, CameraMode cameraMode, float frameDeltaSeconds )
{
	// Black clear colour, matching the black fog it fades into.

	glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear      ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glUseProgram ( shaderProgram );
	glUniform1f  ( uniformAlpha, 1.0f );        // every pass is opaque unless it says otherwise (see NODE_ALPHA)

	glUniform3f  ( uniformFogColour, 0.0f, 0.0f, 0.0f );
	glUniform1f  ( uniformFogDensity, gameplayFogDensity );

	// The camera needs a player. Before a game starts there is none, so there is nothing to draw.

	if ( simulation.allPods ().empty () )
	{
		return;
	}

	const LightPod& player = simulation.player ();

	updateCameraBasis ( player, cameraMode, frameDeltaSeconds );

	const Matrix4 projection     = buildProjectionMatrix ( cameraMode );
	const Matrix4 view           = buildViewMatrix ( player, cameraMode, simulation.arenaExtent () );
	const Matrix4 viewProjection = projection * view;

	glUniformMatrix4fv ( uniformViewProjection, 1, GL_FALSE, viewProjection.m );

	// Build all world geometry in world space this frame, then draw. This is the modern equivalent of the
	// drawWorldStreams owns the draw order, which matters now the lattice is translucent.

	buildGridNodes ( simulation );
	buildPods      ( simulation, cameraMode != CameraMode::Cockpit );
	buildTrails    ( simulation, cameraMode );

	drawWorldStreams ();

	// HUD pass. The overlay is drawn last, in the z = -1 plane with an identity view, so it uses the view's
	// projection alone. Fog and depth test go off - the overlay is flat and always on top - then the depth test
	// comes back for the next frame's world pass. The wide third person view takes the larger scale.

	hudBlinkPhase += frameDeltaSeconds * HUD_BLINK_RATE;

	if ( hudBlinkPhase >= 1.0f )
	{
		hudBlinkPhase = 0.0f;                       // wrap
	}

	const float hudScale = ( cameraMode == CameraMode::ThirdPerson ) ? HUD_SCALE_LARGE : HUD_SCALE_SMALL;

	buildHud ( simulation, hudScale );

	glUniformMatrix4fv ( uniformViewProjection, 1, GL_FALSE, projection.m );
	glUniform1f        ( uniformFogDensity, 0.0f );
	glDisable          ( GL_DEPTH_TEST );

	drawStream ( hudLineVertexArray,     hudLineVertexBuffer,     hudLineBufferCapacity,     hudLineScratch,
	             GL_LINES );
	drawStream ( hudTriangleVertexArray, hudTriangleVertexBuffer, hudTriangleBufferCapacity, hudTriangleScratch,
	             GL_TRIANGLES );

	glEnable ( GL_DEPTH_TEST );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::renderFreeLook
//---------------------------------------------------------------------------------------------------------------------

void Renderer::renderFreeLook ( const Simulation& simulation, CameraMode cameraMode,
                                float pitchDegrees, float yawDegrees, float distance )
{
	glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear      ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glUseProgram ( shaderProgram );
	glUniform1f  ( uniformAlpha, 1.0f );        // every pass is opaque unless it says otherwise (see NODE_ALPHA)

	// Thinned fog, written by the F12 handler itself rather than by a projection.

	glUniform3f  ( uniformFogColour, 0.0f, 0.0f, 0.0f );
	glUniform1f  ( uniformFogDensity, FOG_DENSITY_FREE_LOOK );

	if ( simulation.allPods ().empty () )
	{
		return;
	}

	// The orbit chain. OpenGL post-multiplies, so the centring translate acts on the world first and the pull
	// back acts last.

	const float centre = -( simulation.arenaExtent () - FREE_LOOK_CENTRE_OFFSET ) * FREE_LOOK_CENTRE_SCALE;

	const Matrix4 view = Matrix4::translation ( 0.0f, 0.0f, -distance )
	                   * Matrix4::rotationX   ( pitchDegrees * DEGREES_TO_RADIANS )
	                   * Matrix4::rotationY   ( yawDegrees   * DEGREES_TO_RADIANS )
	                   * Matrix4::translation ( centre, centre, centre );

	const Matrix4 projection     = buildProjectionMatrix ( cameraMode );
	const Matrix4 viewProjection = projection * view;

	glUniformMatrix4fv ( uniformViewProjection, 1, GL_FALSE, viewProjection.m );

	// The same world pass the gameplay views use. The player's pod is simply not among the living pods while
	// the view is active, so nothing here needs to know about it.

	buildGridNodes ( simulation );
	buildPods      ( simulation, true );
	buildTrails    ( simulation, cameraMode );

	drawWorldStreams ();

	// Overlay: the HUD text block only.

	hudLineScratch.clear     ();
	hudTriangleScratch.clear ();

	buildFreeLookText ( simulation );

	glUniformMatrix4fv ( uniformViewProjection, 1, GL_FALSE, projection.m );
	glUniform1f        ( uniformFogDensity, 0.0f );
	glDisable          ( GL_DEPTH_TEST );

	drawStream ( hudLineVertexArray, hudLineVertexBuffer, hudLineBufferCapacity, hudLineScratch, GL_LINES );

	glEnable ( GL_DEPTH_TEST );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildFrontEndProjection
//
// Description:
//
//   The front end 2D projection: 85 degree vertical field of view, near 0.01, far 200. The menu and both end
//   screens draw under it with an identity view. Fog and depth test are turned off at the two render entry
//   points rather than here.
//
// Returns:
//
//   - The front-end projection matrix.
//
//---------------------------------------------------------------------------------------------------------------------

Matrix4 Renderer::buildFrontEndProjection () const
{
	const float aspectRatio = static_cast<float> ( viewportWidth ) / static_cast<float> ( viewportHeight );

	return Matrix4::perspective ( FRONT_END_FOV * DEGREES_TO_RADIANS, aspectRatio, NEAR_PLANE, FRONT_END_FAR_PLANE );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildMenuScreen
//
// Description:
//
//   Build the whole menu screen into the two overlay streams: the title backdrop first, then the five items
//   with their values, then the selection outline.
//
//   The backdrop quads go into the triangle stream and everything else into the line stream. With the depth test
//   off, drawing the triangles before the lines is what layers them correctly - it is painter's order and
//   nothing else is holding it together.
//
// Arguments:
//
//   - selectedItem  : The selected menu item, 0 to 4.
//   - opponentCount : The Number of Opponents value to display.
//   - gridSize      : The Grid Size value to display.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildMenuScreen ( int selectedItem, int opponentCount, int gridSize )
{
	// Title backdrop, first block: the full screen gradient, black down to dark blue, as two triangles.

	auto pushQuad = [ & ] ( const Vector3& topLeft,    const Vector3& topRight,
	                        const Vector3& bottomRight, const Vector3& bottomLeft,
	                        const Vector3& topColour,   const Vector3& bottomColour )
	{
		pushVertex ( hudTriangleScratch, topLeft,     topColour    );
		pushVertex ( hudTriangleScratch, topRight,    topColour    );
		pushVertex ( hudTriangleScratch, bottomRight, bottomColour );

		pushVertex ( hudTriangleScratch, topLeft,     topColour    );
		pushVertex ( hudTriangleScratch, bottomRight, bottomColour );
		pushVertex ( hudTriangleScratch, bottomLeft,  bottomColour );
	};

	pushQuad ( Vector3 { -BACKDROP_HALF_X, BACKDROP_LIFT_Y + BACKDROP_HALF_Y, BACKDROP_Z },
	           Vector3 {  BACKDROP_HALF_X, BACKDROP_LIFT_Y + BACKDROP_HALF_Y, BACKDROP_Z },
	           Vector3 {  BACKDROP_HALF_X, BACKDROP_LIFT_Y - BACKDROP_HALF_Y, BACKDROP_Z },
	           Vector3 { -BACKDROP_HALF_X, BACKDROP_LIFT_Y - BACKDROP_HALF_Y, BACKDROP_Z },
	           BACKDROP_TOP_COLOUR, BACKDROP_BOTTOM_COLOUR );

	// Block B: the horizon band, blue down to green.

	pushQuad ( Vector3 { -HORIZON_HALF_X, HORIZON_CENTRE_Y + HORIZON_HALF_Y, HORIZON_Z },
	           Vector3 {  HORIZON_HALF_X, HORIZON_CENTRE_Y + HORIZON_HALF_Y, HORIZON_Z },
	           Vector3 {  HORIZON_HALF_X, HORIZON_CENTRE_Y - HORIZON_HALF_Y, HORIZON_Z },
	           Vector3 { -HORIZON_HALF_X, HORIZON_CENTRE_Y - HORIZON_HALF_Y, HORIZON_Z },
	           HORIZON_TOP_COLOUR, HORIZON_BOTTOM_COLOUR );

	// Blocks C and D: the title strings.

	buildText ( TITLE_ORIGIN_X,   TITLE_ORIGIN_Y,   TITLE_Z,   TITLE_SCALE_X,   TITLE_SCALE_Y,   TITLE_COLOUR,
	            TITLE_TEXT );
	buildText ( VERSION_ORIGIN_X, VERSION_ORIGIN_Y, VERSION_Z, VERSION_SCALE_X, VERSION_SCALE_Y, VERSION_COLOUR,
	            VERSION_TEXT );

	// The five items. Labels brighten when selected; the two configurable items also draw their value, in red, at
	// the shared value column. An item has a value exactly when its range has room to adjust, so the minimum
	// against the maximum is the whole test.

	for ( int item = 0; item < 5; item++ )
	{
		const MenuItemLayout& layout   = MENU_ITEM_LAYOUT [ item ];
		const bool            selected = ( item == selectedItem );

		buildText ( layout.x, layout.y, MENU_ITEM_Z, 1.0f, 1.0f,
		            selected ? MENU_LABEL_SELECTED_COLOUR : MENU_LABEL_COLOUR, layout.label );

		const bool hasValue = ( item == 0 ) || ( item == 1 );

		if ( hasValue )
		{
			const int value = ( item == 0 ) ? opponentCount : gridSize;

			buildText ( MENU_VALUE_X, layout.y, MENU_ITEM_Z, 1.0f, 1.0f,
			            selected ? MENU_VALUE_SELECTED_COLOUR : MENU_VALUE_COLOUR, std::to_string ( value ) );
		}
	}

	// The selection outline: a green rectangle around the selected row, centred in x.

	const float outlineBottom = MENU_ITEM_LAYOUT [ selectedItem ].y - MENU_OUTLINE_DROP;
	const float outlineTop    = outlineBottom + MENU_OUTLINE_HEIGHT;

	const Vector3 corner [ 4 ] =
	{
		Vector3 { -MENU_OUTLINE_HALF_WIDTH, outlineBottom, MENU_ITEM_Z },
		Vector3 {  MENU_OUTLINE_HALF_WIDTH, outlineBottom, MENU_ITEM_Z },
		Vector3 {  MENU_OUTLINE_HALF_WIDTH, outlineTop,    MENU_ITEM_Z },
		Vector3 { -MENU_OUTLINE_HALF_WIDTH, outlineTop,    MENU_ITEM_Z }
	};

	for ( int edge = 0; edge < 4; edge++ )
	{
		pushVertex ( hudLineScratch, corner [ edge ],           MENU_OUTLINE_COLOUR );
		pushVertex ( hudLineScratch, corner [ ( edge + 1 ) % 4 ], MENU_OUTLINE_COLOUR );
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::buildBannerText
//
// Description:
//
//   One copy of an end screen banner string, through the transform chain:
//   glTranslatef ( 0, 0, -pullBack ) . glRotatef ( angleY about Y ) . glRotatef ( angleZ about Z ) .
//   glTranslatef ( centreX, 0, 0 ), applied to the glyph-space text. OpenGL post-multiplies, so the centring
//   translate acts first - the text spins about its own centre - and the pull-back acts last.
//
// Arguments:
//
//   - text           : The banner string.
//   - angleYDegrees  : Rotation about the y axis, counter-clockwise.
//   - angleZDegrees  : Rotation about the z axis, counter-clockwise.
//   - pullBack       : Distance pushed into the screen.
//   - centreX        : The centring offset (half the text width, negated).
//   - colour         : Stroke colour for this copy.
//
//---------------------------------------------------------------------------------------------------------------------

void Renderer::buildBannerText ( const std::string& text, float angleYDegrees, float angleZDegrees,
                                 float pullBack, float centreX, const Vector3& colour )
{
	const float radiansY = angleYDegrees * DEGREES_TO_RADIANS;
	const float radiansZ = angleZDegrees * DEGREES_TO_RADIANS;

	const float cosineY  = std::cos ( radiansY );
	const float sineY    = std::sin ( radiansY );
	const float cosineZ  = std::cos ( radiansZ );
	const float sineZ    = std::sin ( radiansZ );

	auto transform = [ & ] ( float x, float y )
	{
		// Centre, rotate about z, rotate about y, pull back - the chain in application order.

		x += centreX;

		const float rotatedX = ( x * cosineZ ) - ( y * sineZ );
		const float rotatedY = ( x * sineZ )   + ( y * cosineZ );

		return Vector3
		{
			rotatedX * cosineY,
			rotatedY,
			-( rotatedX * sineY ) - pullBack
		};
	};

	float penX = 0.0f;

	for ( const char character : text )
	{
		const FontGlyph& glyph = FONT_GLYPHS [ static_cast<unsigned char> ( character ) & 0x7F ];

		for ( int s = 0; s < glyph.segmentCount; s++ )
		{
			const FontSegment& segment = FONT_SEGMENTS [ glyph.firstSegment + s ];

			pushVertex ( hudLineScratch, transform ( penX + segment.x0, segment.y0 ), colour );
			pushVertex ( hudLineScratch, transform ( penX + segment.x1, segment.y1 ), colour );
		}

		penX += glyph.advance;
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::renderMenu
//---------------------------------------------------------------------------------------------------------------------

void Renderer::renderMenu ( int selectedItem, int opponentCount, int gridSize )
{
	glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear      ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glUseProgram ( shaderProgram );
	glUniform1f  ( uniformAlpha, 1.0f );        // every pass is opaque unless it says otherwise (see NODE_ALPHA)

	// Fog and depth test are off for the whole screen; a zero density makes the shader's fog inert.

	glUniform3f  ( uniformFogColour, 0.0f, 0.0f, 0.0f );
	glUniform1f  ( uniformFogDensity, 0.0f );

	const Matrix4 projection = buildFrontEndProjection ();

	glUniformMatrix4fv ( uniformViewProjection, 1, GL_FALSE, projection.m );

	hudLineScratch.clear     ();
	hudTriangleScratch.clear ();

	buildMenuScreen ( selectedItem, opponentCount, gridSize );

	glDisable ( GL_DEPTH_TEST );

	// Triangles before lines: the backdrop under the text, in painter's order.

	drawStream ( hudTriangleVertexArray, hudTriangleVertexBuffer, hudTriangleBufferCapacity, hudTriangleScratch,
	             GL_TRIANGLES );
	drawStream ( hudLineVertexArray,     hudLineVertexBuffer,     hudLineBufferCapacity,     hudLineScratch,
	             GL_LINES );

	glEnable ( GL_DEPTH_TEST );
}

//---------------------------------------------------------------------------------------------------------------------
// Method: Renderer::renderEndScreen
//---------------------------------------------------------------------------------------------------------------------

void Renderer::renderEndScreen ( bool winnerScreen, int score, float frameDeltaSeconds )
{
	// The banner phase advances at 90 degrees per second. Wrap it at 720 rather than letting it grow without
	// bound - the phase feeds both angle formulas directly and at half rate, so 720 comes back to the same
	// angles while keeping float precision intact over a long sit on the screen.

	bannerPhase += frameDeltaSeconds * BANNER_PHASE_RATE;

	if ( bannerPhase >= 720.0f )
	{
		bannerPhase -= 720.0f;
	}

	glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear      ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glUseProgram ( shaderProgram );
	glUniform1f  ( uniformAlpha, 1.0f );        // every pass is opaque unless it says otherwise (see NODE_ALPHA)

	// Both screens turn fog and depth test off up front. The restore below is ours, because renderFrame expects
	// to find the depth test on.

	glUniform3f  ( uniformFogColour, 0.0f, 0.0f, 0.0f );
	glUniform1f  ( uniformFogDensity, 0.0f );

	const Matrix4 projection = buildFrontEndProjection ();

	glUniformMatrix4fv ( uniformViewProjection, 1, GL_FALSE, projection.m );

	hudLineScratch.clear     ();
	hudTriangleScratch.clear ();

	const std::string text     = winnerScreen ? WINNER_TEXT : GAME_OVER_TEXT;
	const float       pullBack = winnerScreen ? WINNER_PULL_BACK : GAME_OVER_PULL_BACK;
	const float       centreX  = winnerScreen ? WINNER_CENTRE_X : GAME_OVER_CENTRE_X;

	// The motion trail: 200 copies, one channel ramping from near black to full (blue for WINNER, red for GAME
	// OVER), each 0.5 degrees on from the last. Dimmest first, so later copies paint over earlier ones.

	float channel = 0.0f;

	for ( int copy = 0; copy < BANNER_COPY_COUNT; copy++ )
	{
		channel += BANNER_COLOUR_STEP;              // stepped BEFORE the colour is set

		const Vector3 colour = winnerScreen ? Vector3 { 0.0f, 0.0f, channel }
		                                    : Vector3 { channel, 0.0f, 0.0f };

		const float angleY = BANNER_ANGLE_Y_BASE + bannerPhase + ( copy * BANNER_ANGLE_STEP );
		const float angleZ = ( bannerPhase * 0.5f ) + BANNER_ANGLE_Z_BASE + ( copy * BANNER_ANGLE_STEP );

		buildBannerText ( text, angleY, angleZ, pullBack, centreX, colour );
	}

	// The solid white leading copy, at the loop's final angles.

	const float leadAngleY = BANNER_ANGLE_Y_BASE + bannerPhase + ( ( BANNER_COPY_COUNT - 1 ) * BANNER_ANGLE_STEP );
	const float leadAngleZ = ( bannerPhase * 0.5f ) + BANNER_ANGLE_Z_BASE
	                       + ( ( BANNER_COPY_COUNT - 1 ) * BANNER_ANGLE_STEP );

	buildBannerText ( text, leadAngleY, leadAngleZ, pullBack, centreX, Vector3 { 1.0f, 1.0f, 1.0f } );

	// The WINNER screen alone follows with its screen-space score readout, in green (the GAME OVER screen has
	// none). It right justifies through the same six character field as the two in-game readouts.

	if ( winnerScreen )
	{
		buildText ( BANNER_SCORE_LABEL_X, BANNER_SCORE_Y, BANNER_SCORE_Z, BANNER_SCORE_SCALE, BANNER_SCORE_SCALE,
		            BANNER_SCORE_COLOUR, "SCORE" );
		buildText ( BANNER_SCORE_VALUE_X, BANNER_SCORE_Y, BANNER_SCORE_Z, BANNER_SCORE_SCALE, BANNER_SCORE_SCALE,
		            BANNER_SCORE_COLOUR, formatScore ( score ) );
	}

	glDisable ( GL_DEPTH_TEST );

	drawStream ( hudLineVertexArray, hudLineVertexBuffer, hudLineBufferCapacity, hudLineScratch, GL_LINES );

	glEnable ( GL_DEPTH_TEST );
}

}
