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
//   OpenGL 3.3 core profile renderer.
//
//   A core profile has no fixed function pipeline at all - no matrix stack, no immediate mode, no GL_QUADS and no
//   fog - so every one of those is written out here by hand.
//
//   What gets drawn:
//
//     - Grid nodes.  A wireframe cube (edge = cell scale = 0.2) at EVERY integer cell of the 10x10x10 lattice.
//                    Occupied cells take their occupant's colour brightened by +0.5; empty cells are mid grey.
//                    There are no arena walls as such - the arena IS this lattice, receding into black fog.
//     - Light pods.  A four sided pyramid (apex at local (6,0,0), scale 0.035) per living pod, two grey faces
//                    and two tinted with the pod's colour. Flat and unlit; the shading is painted on.
//     - Light trails.  A closed square section tube (half width 0.05) per trail segment, each end over extended
//                    by the half width so the right angle corners seam without a gap. The player's own newest
//                    two segments are hidden in the cockpit view, which stops the trail flashing across the
//                    camera during a turn. The same hiding sits behind the L toggle in the third person view,
//                    and L is armed at every game start and on every switch to F2, so it shows by default.
//     - Fog.         Black GL_EXP fog, density 0.35, over a black clear colour. That one effect is the whole
//                    Tron "darkness closing in" atmosphere. GL_EXP uses density alone, so the fog start and end
//                    do nothing; they are set anyway and cost nothing.
//     - Cameras.     F1 cockpit, F2 third person, F3 spot plane, each with its own eye, look target and field of
//                    view, driven by a smoothed copy of the player's orientation basis.
//     - HUD.         A 2D overlay drawn after the world in its own pass: the turn queue arrows (a blinking D-pad
//                    showing the player's queued 90 degree turn) and the proximity sensor (four segmented bars
//                    sampling the collision grid around the player, toggled with N). Both go in the z = -1 plane
//                    with an identity view, fog off and depth test off.
//     - Front end.   The menu (title backdrop, five items, selection outline) and the WINNER / GAME OVER banner
//                    screens, under a 2D projection (85 degree field of view, far plane 200, fog and depth test
//                    off).
//
//   The renderer only ever READS simulation state. It holds no game state and cannot reach back into the
//   simulation, which is what keeps the two independent. The proximity toggle is a presentation flag and nothing
//   more. The front end screens take plain values - selection, counts, score - from the application layer, for
//   the same reason: this draws the front end, it does not own it or advance it.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include "GlApi.hpp"
#include "Matrix4.hpp"
#include "Simulation.hpp"

namespace tron3d
{

//---------------------------------------------------------------------------------------------------------------------
// Fog densities
//
//   0.35 is what each of the three gameplay views installs. 0.25 is what GL startup sets, and what leaving the
//   free-look view writes back - note that is NOT 0.35, and nothing puts 0.35 back until a view key is next
//   pressed, so the world stays a little clearer for a while after the debug view. Leave it that way.
//
//   These two are shared with the application layer, which owns the F12 transition. The free-look view's own much
//   thinner density stays private to the renderer.
//---------------------------------------------------------------------------------------------------------------------

constexpr float FOG_DENSITY_GAMEPLAY        = 0.35f;
constexpr float FOG_DENSITY_AFTER_FREE_LOOK = 0.25f;

//---------------------------------------------------------------------------------------------------------------------
// CameraMode
//
//   Three views on F1, F2 and F3, each with its own eye offset, look target and field of view.
//---------------------------------------------------------------------------------------------------------------------

enum class CameraMode
{
	Cockpit,                                    // F1: eye at the pod, looking along its forward axis. The default
	ThirdPerson,                                // F2: 0.8 behind and 0.4 above the pod, wide field of view
	SpotPlane                                   // F3: pulled back by the arena size and lifted, looking at the pod
};

//---------------------------------------------------------------------------------------------------------------------
// Vertex
//
//   Position and a flat RGB colour, and that is the whole format. No lighting, so no normals; nothing is
//   textured, so no texture coordinates; geometry is opaque, so no alpha.
//---------------------------------------------------------------------------------------------------------------------

struct Vertex
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
};

//*********************************************************************************************************************
// Renderer
//*********************************************************************************************************************

class Renderer
{
	//=================================================================================================================
	// Data Members
	//=================================================================================================================

	private:

		unsigned int shaderProgram        = 0;

		int uniformViewProjection         = -1;
		int uniformFogColour              = -1;
		int uniformFogDensity             = -1;
		int uniformAlpha                  = -1;

		// One streamed buffer per primitive type. World geometry is rebuilt in world space every frame and drawn
		// from these. Grid nodes are lines; pods and trails are triangles. A core profile has no GL_QUADS, so each
		// trail quad is split into two triangles, which looks identical for flat opaque geometry.

		unsigned int nodeVertexArray      = 0;
		unsigned int nodeVertexBuffer     = 0;
		std::size_t  nodeBufferCapacity   = 0;

		unsigned int podVertexArray       = 0;
		unsigned int podVertexBuffer      = 0;
		std::size_t  podBufferCapacity    = 0;

		unsigned int trailVertexArray     = 0;
		unsigned int trailVertexBuffer    = 0;
		std::size_t  trailBufferCapacity  = 0;

		// The HUD is a 2D overlay drawn in its own pass after the world, and it needs two more streams. Line
		// vertices carry the arrow and bar outlines, expanded to GL_LINES pairs so many outlines can share one
		// draw call. Triangle vertices carry the filled arrows and bar segments.

		unsigned int hudLineVertexArray    = 0;
		unsigned int hudLineVertexBuffer   = 0;
		std::size_t  hudLineBufferCapacity = 0;

		unsigned int hudTriangleVertexArray    = 0;
		unsigned int hudTriangleVertexBuffer   = 0;
		std::size_t  hudTriangleBufferCapacity = 0;

		std::vector<Vertex> nodeScratch;
		std::vector<Vertex> podScratch;
		std::vector<Vertex> trailScratch;
		std::vector<Vertex> hudLineScratch;
		std::vector<Vertex> hudTriangleScratch;

		// The turn arrow blink phase. Advanced every frame at rate 4.0 and wrapped at 1.0. The arrow is filled
		// while the phase is at or below 0.5 and an outline above it. Timing state for the picture only.

		float hudBlinkPhase = 0.0f;

		// The end screen banner phase, advanced at 90 degrees per second while a WINNER or GAME OVER screen is
		// up. Timing state for the picture, like the blink above.

		float bannerPhase = 0.0f;

		// The proximity sensor toggle, bound to N. Armed at every game start along with the other display
		// toggles, so the sensor is on each new game until it is switched off. It only decides whether the bars
		// get drawn and changes nothing the simulation can see.

		bool proximitySensorEnabled = true;

		// The third person own-trail toggle, bound to L. Armed at every game start AND on every switch to F2, so
		// the player's full trail always shows on entering the view and L hides it for that stay. See the per
		// view trail visibility note in buildTrails. Presentation only, like the sensor toggle.

		bool thirdPersonFullTrailEnabled = true;

		// The 4:3 letterboxed viewport: its size drives every projection, and its origin offsets it inside a client
		// area of a different shape. See Renderer::resize for why the aspect is pinned rather than followed.

		int viewportWidth   = 1;
		int viewportHeight  = 1;
		int viewportOriginX = 0;
		int viewportOriginY = 0;

		// The fog density the gameplay views draw with. The three views install 0.35, but leaving free-look
		// writes 0.25 and nothing puts 0.35 back until a view key is next pressed, so the world stays briefly
		// clearer after the debug view.

		float gameplayFogDensity = FOG_DENSITY_GAMEPLAY;

		// The camera does not track the player's exact orientation. It tracks a smoothed copy, so the view eases
		// into each 90 degree turn instead of snapping round.
		//
		// The smoothed basis is moved toward the true basis at a per view rate every frame, ramped per component
		// and NOT renormalised. Only the view construction normalises. Do not be tempted to renormalise here: the
		// vector is meant to be short mid swing, and that is what softens the turn.

		Vector3 smoothedForward     {};
		Vector3 smoothedUp          {};
		bool    smoothingInitialised = false;

	//=================================================================================================================
	// Methods
	//=================================================================================================================

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Method: initialise
		//
		// Description:
		//
		//   Loads the GL entry points, compiles the shader and allocates the streaming buffers. A core profile
		//   context must already be current.
		//
		// Arguments:
		//
		//   - error : Receives a description if initialisation fails.
		//
		// Returns:
		//
		//   - True on success.
		//
		//-------------------------------------------------------------------------------------------------------------

		bool initialise ( std::string& error );

		//-------------------------------------------------------------------------------------------------------------
		// Method: describeContext
		//
		// Description:
		//
		//   Vendor, renderer and version strings of the live context, for diagnostics.
		//
		// Returns:
		//
		//   - A human-readable description.
		//
		//-------------------------------------------------------------------------------------------------------------

		std::string describeContext () const;

		//-------------------------------------------------------------------------------------------------------------
		// Method: shutdown
		//-------------------------------------------------------------------------------------------------------------

		void shutdown ();

		//-------------------------------------------------------------------------------------------------------------
		// Method: resize
		//-------------------------------------------------------------------------------------------------------------

		void resize ( int width, int height );

		//-------------------------------------------------------------------------------------------------------------
		// Method: renderFrame
		//
		// Description:
		//
		//   Draws one frame of the given simulation state.
		//
		// Arguments:
		//
		//   - simulation        : The state to draw. Not modified.
		//   - cameraMode        : Which of the three views to use.
		//   - frameDeltaSeconds : Real seconds since the previous frame, for the camera-basis smoothing only.
		//                         Nothing the simulation sees, and nothing that affects what is drawn beyond the
		//                         camera orientation.
		//
		//-------------------------------------------------------------------------------------------------------------

		void renderFrame ( const Simulation& simulation, CameraMode cameraMode, float frameDeltaSeconds );

		//-------------------------------------------------------------------------------------------------------------
		// Method: renderMenu
		//
		// Description:
		//
		//   Draw one frame of the menu: the title backdrop, the five items with the selected one highlighted, the
		//   values of the two configurable items, and the green selection outline. Drawn under the 2D projection
		//   with fog and depth test off.
		//
		// Arguments:
		//
		//   - selectedItem  : The selected menu item, 0 to 4.
		//   - opponentCount : The Number of Opponents value to display.
		//   - gridSize      : The Grid Size value to display.
		//
		//-------------------------------------------------------------------------------------------------------------

		void renderMenu ( int selectedItem, int opponentCount, int gridSize );

		//-------------------------------------------------------------------------------------------------------------
		// Method: renderEndScreen
		//
		// Description:
		//
		//   Draw one frame of an end screen: the rotating motion trail banner, which is 200 colour ramped copies
		//   of the text plus one solid white leading copy. WINNER is blue and carries a green SCORE readout;
		//   GAME OVER is red and has none.
		//
		// Arguments:
		//
		//   - winnerScreen      : True for the WINNER screen, false for GAME OVER.
		//   - score             : The player's score, shown on the WINNER screen only.
		//   - frameDeltaSeconds : Real seconds since the previous frame, advancing the banner rotation.
		//
		//-------------------------------------------------------------------------------------------------------------

		void renderEndScreen ( bool winnerScreen, int score, float frameDeltaSeconds );

		//-------------------------------------------------------------------------------------------------------------
		// Method: renderFreeLook
		//
		// Description:
		//
		//   Draw one frame of the F12 debug free-look view: the world seen from an orbit camera around the arena
		//   centre, under thinned fog, with the opponents readout and the "TEST MODE" label over it.
		//
		//   The player's pod is missing because the front end has taken it out of play, not because anything here
		//   hides it. No direction indicator, no proximity sensor, no SPEED and no SCORE - this view draws the
		//   world and the text block and nothing else.
		//
		// Arguments:
		//
		//   - simulation      : The state to draw. Read-only.
		//   - cameraMode      : The gameplay view whose projection is inherited - whichever one was current when
		//                       F12 was pressed.
		//   - pitchDegrees    : Orbit rotation about the x axis.
		//   - yawDegrees      : Orbit rotation about the y axis.
		//   - distance        : Orbit distance from the arena centre.
		//
		//-------------------------------------------------------------------------------------------------------------

		void renderFreeLook ( const Simulation& simulation, CameraMode cameraMode,
		                      float pitchDegrees, float yawDegrees, float distance );

		//-------------------------------------------------------------------------------------------------------------
		// Method: setGameplayFogDensity
		//
		// Description:
		//
		//   Set the fog density the gameplay views draw with: 0.35 from each of the three view projections, or
		//   0.25 on leaving free-look. See the note on gameplayFogDensity.
		//
		//-------------------------------------------------------------------------------------------------------------

		void setGameplayFogDensity ( float density ) { gameplayFogDensity = density; }

		//-------------------------------------------------------------------------------------------------------------
		// Method: toggleProximitySensor / setProximitySensorEnabled / isProximitySensorEnabled
		//
		// Description:
		//
		//   The proximity sensor is toggled with N. These expose the flag to the application's input layer, which
		//   owns the key binding.
		//
		//-------------------------------------------------------------------------------------------------------------

		void toggleProximitySensor       ()             { proximitySensorEnabled = !proximitySensorEnabled; }
		void setProximitySensorEnabled   ( bool value ) { proximitySensorEnabled = value; }
		bool isProximitySensorEnabled    () const       { return proximitySensorEnabled; }

		//-------------------------------------------------------------------------------------------------------------
		// Method: toggleThirdPersonTrail / setThirdPersonTrailEnabled / isThirdPersonTrailEnabled
		//
		// Description:
		//
		//   The player's newest trail segments are drawn in the third person view only while this flag is on.
		//   It is armed at every game start and on every switch to F2, with L toggling it in between. These
		//   expose the flag to the application's input layer, which owns the key binding and the re-arming.
		//
		//-------------------------------------------------------------------------------------------------------------

		void toggleThirdPersonTrail      ()             { thirdPersonFullTrailEnabled = !thirdPersonFullTrailEnabled; }
		void setThirdPersonTrailEnabled  ( bool value ) { thirdPersonFullTrailEnabled = value; }
		bool isThirdPersonTrailEnabled   () const       { return thirdPersonFullTrailEnabled; }

	private:

		bool    buildShader          ( std::string& error );

		void    updateCameraBasis    ( const LightPod& player, CameraMode cameraMode, float frameDeltaSeconds );
		Matrix4 buildViewMatrix      ( const LightPod& player, CameraMode cameraMode, float arenaExtent ) const;
		Matrix4 buildProjectionMatrix ( CameraMode cameraMode ) const;

		void    buildGridNodes       ( const Simulation& simulation );
		void    buildPods            ( const Simulation& simulation, bool drawPlayerPod );
		void    buildTrails          ( const Simulation& simulation, CameraMode cameraMode );

		void    buildHud             ( const Simulation& simulation, float hudScale );
		void    buildTurnArrows      ( const LightPod& player, float hudScale );
		void    buildProximitySensor ( const Simulation& simulation, float hudScale );
		void    buildReadouts        ( const Simulation& simulation, bool large );
		void    buildFreeLookText    ( const Simulation& simulation );
		void    buildText            ( float originX, float originY, float originZ, float scaleX, float scaleY,
		                               const Vector3& colour, const std::string& text );

		Matrix4 buildFrontEndProjection () const;

		void    buildMenuScreen      ( int selectedItem, int opponentCount, int gridSize );
		void    buildBannerText      ( const std::string& text, float angleYDegrees, float angleZDegrees,
		                               float pullBack, float centreX, const Vector3& colour );

		void    drawStream           ( unsigned int vertexArray, unsigned int vertexBuffer, std::size_t& capacity,
		                               const std::vector<Vertex>& vertices, unsigned int primitive );

		// The world pass, shared by the gameplay views and the free-look view so the two cannot drift apart. Draws
		// the opaque geometry first and the translucent grid-node lattice over it. See NODE_ALPHA in Renderer.cpp.

		void    drawWorldStreams     ();
};

}
