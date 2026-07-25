//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Module:  Simulation Constants
//
// Description:
//
//   Fixed limits and tuning values for the simulation, gathered in one place.
//
//   These are not knobs to fiddle with. Each one changes how the game plays, and several of them are relied on by
//   more than one part of the code. If a future version wants different handling, put a settings layer on top
//   rather than editing these.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include "Vector3.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Arena geometry
//
//   The collision grid is a fixed 10 x 10 x 10. The menu's grid size maximum is the same limit seen from the other
//   side, which is why it cannot be raised on its own.
//---------------------------------------------------------------------------------------------------------------------

constexpr int GRID_DIMENSION  = 10;
constexpr int GRID_CELL_COUNT = GRID_DIMENSION * GRID_DIMENSION * GRID_DIMENSION;

//---------------------------------------------------------------------------------------------------------------------
// Grid strides, in cells
//
//   Watch the order here. x carries the LARGEST stride and z the smallest, which is the reverse of the obvious
//   guess. Getting it backwards is invisible from inside the game, because every read agrees with every write, so
//   it is worth stating plainly rather than leaving to be worked out.
//---------------------------------------------------------------------------------------------------------------------

constexpr int GRID_STRIDE_Z = 1;
constexpr int GRID_STRIDE_Y = GRID_DIMENSION;
constexpr int GRID_STRIDE_X = GRID_DIMENSION * GRID_DIMENSION;

//---------------------------------------------------------------------------------------------------------------------
// Pod population
//
//   101 records: a hundred opponents and the player.
//---------------------------------------------------------------------------------------------------------------------

constexpr int POD_MAX      = 101;
constexpr int OPPONENT_MAX = POD_MAX - 1;

//---------------------------------------------------------------------------------------------------------------------
// Spawning
//
//   Pods are placed on the six faces of the arena cube in rotation, each heading inward along the face normal.
//   With more than six pods the rotation wraps and faces get reused.
//---------------------------------------------------------------------------------------------------------------------

constexpr int SPAWN_FACE_COUNT = 6;

//---------------------------------------------------------------------------------------------------------------------
// Trail
//
//   Trails grow as they go, so there is no ceiling to run into.
//
//   The sentinel is what an unwritten trail slot holds. It sits outside the arena on every axis, so it is easy to
//   spot and impossible to mistake for a real corner. The renderer tests against it before drawing out to the live
//   tip, which matters on the frame a turn commits and the new tip has not been written yet.
//---------------------------------------------------------------------------------------------------------------------

constexpr Vector3 TRAIL_SENTINEL_POINT { -1.0f, -1.0f, -1.0f };

//---------------------------------------------------------------------------------------------------------------------
// Speed tuning
//
//   Two of these are negative, and that is the whole character of the speed model. Coasting BLEEDS speed rather
//   than building it, and a turn COSTS speed rather than granting it, at double the rate for opponents. Erratic
//   steering punishes itself, which quietly handicaps the opponents, since they dodge constantly.
//---------------------------------------------------------------------------------------------------------------------

constexpr float SPEED_MINIMUM      =  0.4f;
constexpr float SPEED_MAXIMUM      =  1.2f;
constexpr float SPEED_ACCELERATION =  0.2f;
constexpr float SPEED_DECELERATION = -0.05f;
constexpr float SPEED_TURN_BONUS   = -0.2f;

//---------------------------------------------------------------------------------------------------------------------
// Arena scale
//---------------------------------------------------------------------------------------------------------------------

constexpr float CELL_SCALE = 0.2f;

//---------------------------------------------------------------------------------------------------------------------
// Scoring
//---------------------------------------------------------------------------------------------------------------------

constexpr int SCORE_PER_STEP = 5;
constexpr int SCORE_PER_KILL = 1000;

//---------------------------------------------------------------------------------------------------------------------
// Opponent behaviour
//
//   The wander roll is rand () % 500, and only rolls 0 to 3 attempt a turn. That works out at roughly 0.8% per
//   opponent per frame, biased toward pitching up because the cascade starts at roll + 1.
//---------------------------------------------------------------------------------------------------------------------

constexpr int WANDER_ROLL_RANGE = 500;

//---------------------------------------------------------------------------------------------------------------------
// First person trail tip pullback
//
//   Pull the player's live trail tip back along the forward axis in the first person view, so the wall does not
//   clip through the near plane of the camera.
//---------------------------------------------------------------------------------------------------------------------

constexpr float FIRST_PERSON_TRAIL_PULLBACK = 0.1f;

//---------------------------------------------------------------------------------------------------------------------
// Menu defaults and ranges
//
//   An opponent count of zero is a legal setting: one pod, yours, alone in the arena. It cannot be won, because
//   the victory test needs more than one pod alive, so the ride only ever ends in a crash.
//
//   The menu does not offer that game. PLAYABLE_OPPONENT_COUNT_MINIMUM in FrontEnd.hpp is the floor the menu
//   clamps to, and the two are kept as separate constants on purpose so the playable floor cannot be mistaken for
//   the limit the simulation itself imposes.
//---------------------------------------------------------------------------------------------------------------------

constexpr int MENU_OPPONENT_COUNT_DEFAULT = 10;
constexpr int MENU_OPPONENT_COUNT_MINIMUM = 0;
constexpr int MENU_OPPONENT_COUNT_MAXIMUM = 100;

constexpr int MENU_GRID_SIZE_DEFAULT      = 10;
constexpr int MENU_GRID_SIZE_MINIMUM      = 8;
constexpr int MENU_GRID_SIZE_MAXIMUM      = 10;

//---------------------------------------------------------------------------------------------------------------------
// Pod colour palette
//
//   Four pure primaries, which suits flat shaded untextured geometry. The spawn face picks the index, so a pod's
//   colour tells you which wall it came in off.
//---------------------------------------------------------------------------------------------------------------------

constexpr Vector3 POD_PALETTE [ 4 ] =
{
	{ 1.0f, 0.0f, 0.0f },                        // 0 : red     - spawn faces 1 and 5
	{ 0.0f, 0.0f, 1.0f },                        // 1 : blue    - spawn faces 0 and 4
	{ 0.0f, 1.0f, 0.0f },                        // 2 : green   - spawn face 2
	{ 1.0f, 1.0f, 0.0f }                         // 3 : yellow  - spawn face 3
};

}
