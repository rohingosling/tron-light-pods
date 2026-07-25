//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   Application entry point: window, context, renderer, the front end controller, and the frame loop.
//
//   Launches into the menu and follows the flow from there - menu, game, WINNER or GAME OVER, back to the menu.
//   The state machine itself lives in the front end controller; see FrontEnd.hpp.
//
//   THE FRAME LOOP RUNS ON A FIXED TIMESTEP, and that is not an arbitrary choice.
//
//   Measuring frame time off the tick counter does not work. It has a resolution of about 15.6 ms, so on a fast
//   machine most frames complete inside a single tick and measure a delta of zero. Guard that with a
//   "if ( delta > 0 )" and reuse the last value when it fails, and the game applies a fifteen millisecond step
//   thousands of times a second and is completely unplayable. That is the whole mechanism behind an old game
//   running at ludicrous speed on a new machine, and it is a fault in the timing rather than in the game.
//
//   So the simulation is driven on a fixed timestep accumulator instead, at 0.016 s, while rendering as fast as
//   the display allows.
//
// Usage:
//
//   tron3d [--opponents N] [--grid N] [--seed N] [--window WIDTHxHEIGHT]
//
//   The options seed the menu's values, so they show up in the menu rather than bypassing it. The window may be
//   any shape; the picture is letterboxed to the authored 4:3 inside it.
//
// Controls:
//
//   Numpad 8 / 4 / 6 / 2, or the arrow keys   Steer in play; navigate and adjust at the menu; orbit in free-look
//                                             At the menu, hold Left or Right and the value keeps counting
//   Numpad 5                                  Straight on, and resynchronise the basis
//   Numpad + / -                              Move the free-look camera in and out
//   F1 / F2 / F3                              Cockpit / third-person / spot-plane view
//   F12                                       Toggle the debug free-look view (the AI keeps playing without you)
//   N                                         Toggle the proximity sensor
//   L                                         Toggle the player's own near trail in the third-person view
//   Shift / Ctrl                              Accelerate / brake
//   Enter                                     Activate the selected menu item
//   Escape                                    Back out to the menu; twice from the menu quits
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "FrontEnd.hpp"
#include "Renderer.hpp"
#include "Simulation.hpp"
#include "Window.hpp"

//---------------------------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------------------------

namespace
{
	// The step the game is tuned around. Change it and the whole feel changes with it, so it is not a knob.

	constexpr float  FIXED_TIME_STEP        = 0.016f;

	// If the process is suspended or the window dragged, the wall clock can jump by seconds. Without a ceiling
	// the accumulator would then run hundreds of simulation steps in one frame, which looks like a freeze and
	// can cascade. Dropping the excess is the standard remedy.

	constexpr double MAXIMUM_FRAME_DELTA    = 0.25;

	// A 4:3 client area, because every on-screen layout is placed against a 640 x 480 frame. 960 x 720 is the
	// same shape at a size that suits a modern display. The renderer letterboxes to 4:3 whatever the window is
	// resized to, so this only picks the starting size; see Renderer::resize.

	constexpr int    DEFAULT_WINDOW_WIDTH   = 960;
	constexpr int    DEFAULT_WINDOW_HEIGHT  = 720;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: analyseFrame
//
// Description:
//
//   Reads the rendered back buffer and measures how much of it is not black and how bright the brightest pixel is.
//   This upgrades the self test from "no OpenGL error was raised" to "the renderer actually drew something": a
//   black screen from a camera facing the wrong way, or geometry that never reached the framebuffer, raises no GL
//   error at all, and only reading the pixels back can catch it. Must be called after the frame is drawn and
//   before the buffers are swapped, so the back buffer still holds the frame.
//
//   RGBA is read rather than RGB so the tightly-packed rows need no pack-alignment change, which would need
//   another GL entry point for no benefit here.
//
// Arguments:
//
//   - width, height    : The framebuffer size.
//   - nonBlackFraction : Receives the fraction of pixels with any channel above the black threshold.
//   - peakBrightness   : Receives the brightest single channel seen, in [0, 1].
//
//---------------------------------------------------------------------------------------------------------------------

static void analyseFrame
(
	int         width,
	int         height,
	double&     nonBlackFraction,
	float&      peakBrightness,
	const char* screenshotPath
)
{
	nonBlackFraction = 0.0;
	peakBrightness   = 0.0f;

	if ( ( width <= 0 ) || ( height <= 0 ) )
	{
		return;
	}

	const std::size_t pixelCount = static_cast<std::size_t> ( width ) * static_cast<std::size_t> ( height );

	std::vector<unsigned char> pixels ( pixelCount * 4, 0 );

	tron3d::gl::glReadPixels ( 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data () );

	constexpr unsigned char BLACK_THRESHOLD = 8;    // out of 255, to ignore rounding fuzz around true black

	std::size_t   nonBlackCount = 0;
	unsigned char peakChannel   = 0;

	for ( std::size_t i = 0; i < pixelCount; i++ )
	{
		const unsigned char red   = pixels [ i * 4 + 0 ];
		const unsigned char green = pixels [ i * 4 + 1 ];
		const unsigned char blue  = pixels [ i * 4 + 2 ];

		if ( ( red > BLACK_THRESHOLD ) || ( green > BLACK_THRESHOLD ) || ( blue > BLACK_THRESHOLD ) )
		{
			nonBlackCount++;
		}

		peakChannel = std::max ( { peakChannel, red, green, blue } );
	}

	nonBlackFraction = static_cast<double> ( nonBlackCount ) / static_cast<double> ( pixelCount );
	peakBrightness   = static_cast<float> ( peakChannel ) / 255.0f;

	// Optional visual dump, so the frame can be inspected rather than only measured. A plain 24-bit BMP, which
	// needs no library: glReadPixels already gives bottom-up rows, which is the order BMP stores, so no flip.

	if ( screenshotPath == nullptr )
	{
		return;
	}

	const int          rowStride  = ( ( width * 3 ) + 3 ) & ~3;             // rows padded to a 4-byte boundary
	const unsigned int imageBytes = static_cast<unsigned int> ( rowStride ) * static_cast<unsigned int> ( height );
	const unsigned int fileBytes  = 54u + imageBytes;

	unsigned char header [ 54 ] = { 0 };

	header [  0 ] = 'B';  header [  1 ] = 'M';
	header [  2 ] = static_cast<unsigned char> ( fileBytes        );
	header [  3 ] = static_cast<unsigned char> ( fileBytes  >>  8 );
	header [  4 ] = static_cast<unsigned char> ( fileBytes  >> 16 );
	header [  5 ] = static_cast<unsigned char> ( fileBytes  >> 24 );
	header [ 10 ] = 54;                                                     // pixel data offset
	header [ 14 ] = 40;                                                     // info header size
	header [ 18 ] = static_cast<unsigned char> ( width          );
	header [ 19 ] = static_cast<unsigned char> ( width   >>  8   );
	header [ 20 ] = static_cast<unsigned char> ( width   >> 16   );
	header [ 21 ] = static_cast<unsigned char> ( width   >> 24   );
	header [ 22 ] = static_cast<unsigned char> ( height         );
	header [ 23 ] = static_cast<unsigned char> ( height  >>  8   );
	header [ 24 ] = static_cast<unsigned char> ( height  >> 16   );
	header [ 25 ] = static_cast<unsigned char> ( height  >> 24   );
	header [ 26 ] = 1;                                                      // colour planes
	header [ 28 ] = 24;                                                     // bits per pixel

	std::vector<unsigned char> rows ( imageBytes, 0 );

	for ( int y = 0; y < height; y++ )
	{
		for ( int x = 0; x < width; x++ )
		{
			const std::size_t source      = ( static_cast<std::size_t> ( y ) * width + x ) * 4;
			const std::size_t destination = static_cast<std::size_t> ( y ) * rowStride + x * 3;

			rows [ destination + 0 ] = pixels [ source + 2 ];              // BMP is BGR
			rows [ destination + 1 ] = pixels [ source + 1 ];
			rows [ destination + 2 ] = pixels [ source + 0 ];
		}
	}

	if ( std::FILE* file = std::fopen ( screenshotPath, "wb" ) )
	{
		std::fwrite ( header, 1, sizeof ( header ), file );
		std::fwrite ( rows.data (), 1, rows.size (), file );
		std::fclose ( file );
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Entry point
//---------------------------------------------------------------------------------------------------------------------

int main ( int argumentCount, char** argumentValue )
{
	tron3d::GameSettings settings;

	// Self-test mode renders a fixed number of real frames through the real pipeline and then exits, reporting
	// any OpenGL error raised along the way. It exists so that "does the renderer actually work" is a question
	// the build can answer on its own, rather than one that needs a person to watch a window. By default the
	// self test starts a game immediately; --pin holds a front-end screen up instead, so the menu, the end screens
	// and the free-look view can be captured and measured the same way.

	int         selfTestFrames = 0;
	const char* screenshotPath = nullptr;
	int         forcedView     = -1;            // -1 lets the self test pod the views; 0/1/2 pins one
	const char* pinnedScreen   = nullptr;       // "menu", "gameover", "winner" or "freelook"

	int         windowWidth    = DEFAULT_WINDOW_WIDTH;
	int         windowHeight   = DEFAULT_WINDOW_HEIGHT;

	for ( int i = 1; i < argumentCount; i++ )
	{
		if ( ( std::strcmp ( argumentValue [ i ], "--opponents" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			settings.opponentCount = std::atoi ( argumentValue [ ++i ] );
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--grid" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			settings.gridSize = std::atoi ( argumentValue [ ++i ] );
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--seed" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			settings.randomSeed = static_cast<unsigned int> ( std::atoi ( argumentValue [ ++i ] ) );
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--selftest" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			selfTestFrames = std::atoi ( argumentValue [ ++i ] );
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--shot" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			screenshotPath = argumentValue [ ++i ];
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--view" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			forcedView = std::atoi ( argumentValue [ ++i ] );
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--pin" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			pinnedScreen = argumentValue [ ++i ];
		}
		else if ( ( std::strcmp ( argumentValue [ i ], "--window" ) == 0 ) && ( i + 1 < argumentCount ) )
		{
			// WIDTHxHEIGHT. Any shape is accepted; the renderer letterboxes the 4:3 picture inside it.

			int parsedWidth  = 0;
			int parsedHeight = 0;

			if ( std::sscanf ( argumentValue [ ++i ], "%dx%d", &parsedWidth, &parsedHeight ) == 2 )
			{
				if ( ( parsedWidth > 0 ) && ( parsedHeight > 0 ) )
				{
					windowWidth  = parsedWidth;
					windowHeight = parsedHeight;
				}
			}
		}
	}

	// Clamp to the menu's own ranges, except at the bottom of the opponent count where the playable floor of one
	// opponent applies. See PLAYABLE_OPPONENT_COUNT_MINIMUM in FrontEnd.hpp.

	if ( settings.opponentCount < tron3d::PLAYABLE_OPPONENT_COUNT_MINIMUM )
	{
		settings.opponentCount = tron3d::PLAYABLE_OPPONENT_COUNT_MINIMUM;
	}

	if ( settings.opponentCount > tron3d::MENU_OPPONENT_COUNT_MAXIMUM )
	{
		settings.opponentCount = tron3d::MENU_OPPONENT_COUNT_MAXIMUM;
	}

	if ( settings.gridSize < tron3d::MENU_GRID_SIZE_MINIMUM ) { settings.gridSize = tron3d::MENU_GRID_SIZE_MINIMUM; }
	if ( settings.gridSize > tron3d::MENU_GRID_SIZE_MAXIMUM ) { settings.gridSize = tron3d::MENU_GRID_SIZE_MAXIMUM; }

	// Keep spawn positions off the arena boundary, so every pod starts with turning room on both sides of its
	// heading. Off by default in the Simulation class itself; the application opts in. See GameSettings.

	settings.spawnAwayFromEdges = true;

	// Window and context.

	tron3d::Window window;

	std::string error;

	if ( !window.create ( "3D Tron", windowWidth, windowHeight, error ) )
	{
		std::fprintf ( stderr, "error: %s\n", error.c_str () );

		return 1;
	}

	tron3d::Renderer renderer;

	if ( !renderer.initialise ( error ) )
	{
		std::fprintf ( stderr, "error: %s\n", error.c_str () );

		window.destroy ();

		return 1;
	}

	std::printf ( "3D Tron\n\n" );
	std::printf ( "  context   %s\n", renderer.describeContext ().c_str () );
	std::printf ( "  opponents %d, grid %d, step %.3f s\n", settings.opponentCount, settings.gridSize,
	              FIXED_TIME_STEP );
	std::printf ( "  arrows or numpad navigate the menu and steer, Enter activates, Esc backs out, "
	              "F1/F2/F3 views, N proximity sensor, L own trail in F2, shift/ctrl throttle\n\n" );

	// The front end owns the simulation and launches at the menu. The camera mode lives out here because it is
	// presentation state the render call needs every frame; the controller updates it on the view keys and on
	// game start.

	tron3d::FrontEnd   frontEnd ( settings );
	tron3d::CameraMode cameraMode = tron3d::CameraMode::Cockpit;

	// A pinned self-test view, if one was requested on the command line.

	if ( forcedView == 1 ) { cameraMode = tron3d::CameraMode::ThirdPerson; }
	if ( forcedView == 2 ) { cameraMode = tron3d::CameraMode::SpotPlane;   }

	// The self test skips the menu and starts a game directly - unless the menu itself is the screen being
	// tested. Pinning an end screen starts a game first so the score readout has a live player behind it.

	if ( ( selfTestFrames > 0 ) && ( pinnedScreen == nullptr ) )
	{
		frontEnd.startGame ( renderer, cameraMode );

		if ( forcedView == 1 ) { cameraMode = tron3d::CameraMode::ThirdPerson; }
		if ( forcedView == 2 ) { cameraMode = tron3d::CameraMode::SpotPlane;   }

		frontEnd.simulation ().setFirstPersonView ( cameraMode == tron3d::CameraMode::Cockpit );
	}
	else if ( pinnedScreen != nullptr )
	{
		if ( std::strcmp ( pinnedScreen, "gameover" ) == 0 )
		{
			frontEnd.startGame ( renderer, cameraMode );
			frontEnd.simulation ().setState ( tron3d::AppState::GameOver );
		}
		else if ( std::strcmp ( pinnedScreen, "winner" ) == 0 )
		{
			frontEnd.startGame ( renderer, cameraMode );
			frontEnd.simulation ().setState ( tron3d::AppState::Winner );
		}
		else if ( std::strcmp ( pinnedScreen, "freelook" ) == 0 )
		{
			// Start a game and step into the debug view exactly as F12 would, so the capture shows the real thing
			// rather than a state assembled by hand.

			frontEnd.startGame    ( renderer, cameraMode );
			frontEnd.handleAction ( tron3d::GameAction::ToggleFreeLook, renderer, cameraMode );
		}

		// "menu" needs nothing: the front end is already there.
	}

	// Timing.

	LARGE_INTEGER frequency;
	LARGE_INTEGER previousCount;

	QueryPerformanceFrequency ( &frequency );
	QueryPerformanceCounter   ( &previousCount );

	double accumulator    = 0.0;
	int    renderedFrames = 0;

	// Filled on the final self-test frame by reading the framebuffer back; reported below.

	double selfTestNonBlackFraction = 0.0;
	float  selfTestPeakBrightness   = 0.0f;

	while ( window.pumpMessages () )
	{
		// Real elapsed time.

		LARGE_INTEGER currentCount;

		QueryPerformanceCounter ( &currentCount );

		double frameDelta = static_cast<double> ( currentCount.QuadPart - previousCount.QuadPart ) /
		                    static_cast<double> ( frequency.QuadPart );

		previousCount = currentCount;

		if ( frameDelta > MAXIMUM_FRAME_DELTA )
		{
			frameDelta = MAXIMUM_FRAME_DELTA;
		}

		accumulator += frameDelta;

		// Input, routed through the front-end state machine.

		bool quitRequested = false;

		for ( const tron3d::GameAction action : window.drainActions () )
		{
			if ( frontEnd.handleAction ( action, renderer, cameraMode ) )
			{
				quitRequested = true;
			}
		}

		if ( quitRequested )
		{
			break;
		}

		frontEnd.simulation ().setPlayerAccelerating ( window.isAccelerating () );
		frontEnd.simulation ().setPlayerBraking      ( window.isBraking      () );

		tron3d::FreeLookInput freeLookInput = window.freeLookInput ();

		// Under the self test, orbit the free-look camera off its opening pose. Both rotations are identity at the
		// entry angles, so without this the capture would exercise only the translate half of the camera chain.

		if ( ( selfTestFrames > 0 ) && ( frontEnd.simulation ().state () == tron3d::AppState::FreeLook ) )
		{
			freeLookInput.yawRight  = true;
			freeLookInput.pitchDown = true;
		}

		frontEnd.updateFreeLookCamera ( freeLookInput, static_cast<float> ( frameDelta ) );

		// The menu's value repeat: hold Left or Right and the selected item keeps counting. Driven from held
		// state at the operating system's own repeat delay and rate rather than from a flood of key messages;
		// see the note in Window.hpp.

		frontEnd.updateMenuValueRepeat ( window.menuAdjustDirection (), static_cast<float> ( frameDelta ),
		                                 window.keyRepeatDelay (), window.keyRepeatInterval () );

		// Age the end screen, so it knows when it has been up long enough to accept a key.

		frontEnd.updateEndScreen ( static_cast<float> ( frameDelta ) );

		// Fixed timestep. The simulation runs in the playing state AND in free-look - the opponents carrying on
		// while the player watches from outside is the entire point of the debug view.
		//
		// It does NOT run on the menu or the end screens. Running past a game over would let the victory check
		// overwrite it and hand the player a win they never had.

		const bool simulationRunning = frontEnd.simulation ().isPlaying () ||
		                               ( frontEnd.simulation ().state () == tron3d::AppState::FreeLook );

		while ( accumulator >= static_cast<double> ( FIXED_TIME_STEP ) )
		{
			if ( simulationRunning )
			{
				frontEnd.simulation ().update ( FIXED_TIME_STEP );
			}

			accumulator -= static_cast<double> ( FIXED_TIME_STEP );
		}

		// Render whichever screen the state machine is on.

		if ( window.consumeResize () )
		{
			renderer.resize ( window.width (), window.height () );
		}

		switch ( frontEnd.simulation ().state () )
		{
			case tron3d::AppState::Menu:

				renderer.renderMenu ( frontEnd.menu ().selection, frontEnd.menu ().opponentCount,
				                      frontEnd.menu ().gridSize );
				break;

			case tron3d::AppState::Playing:

				renderer.renderFrame ( frontEnd.simulation (), cameraMode, static_cast<float> ( frameDelta ) );
				break;

			case tron3d::AppState::FreeLook:

				renderer.renderFreeLook ( frontEnd.simulation (), cameraMode,
				                          frontEnd.freeLook ().pitchDegrees,
				                          frontEnd.freeLook ().yawDegrees,
				                          frontEnd.freeLook ().distance );
				break;

			case tron3d::AppState::GameOver:

				renderer.renderEndScreen ( false, frontEnd.simulation ().player ().score,
				                           static_cast<float> ( frameDelta ) );
				break;

			case tron3d::AppState::Winner:

				renderer.renderEndScreen ( true, frontEnd.simulation ().player ().score,
				                           static_cast<float> ( frameDelta ) );
				break;
		}

		// On the final self-test frame, read the drawn back buffer back before it is swapped away, to confirm the
		// renderer actually put visible geometry on screen.

		if ( ( selfTestFrames > 0 ) && ( renderedFrames + 1 >= selfTestFrames ) )
		{
			analyseFrame ( window.width (), window.height (), selfTestNonBlackFraction, selfTestPeakBrightness,
			               screenshotPath );
		}

		window.present ();

		renderedFrames++;

		if ( ( selfTestFrames > 0 ) && ( renderedFrames >= selfTestFrames ) )
		{
			const unsigned int glError = tron3d::gl::glGetError ();

			// The frame should not be black: the renderer must have drawn visible geometry. The threshold is very
			// low because most of a normal frame genuinely is black - the arena recedes into black fog - so only
			// a truly empty frame (a camera facing away, geometry that never reached the buffer) falls below it.

			const bool drewGeometry = selfTestNonBlackFraction > 0.0005;
			const bool hasPlayer    = !frontEnd.simulation ().allPods ().empty ();

			std::printf ( "\nself test\n" );
			std::printf ( "  frames rendered   %d\n", renderedFrames );
			std::printf ( "  simulation state  %d\n", static_cast<int> ( frontEnd.simulation ().state () ) );
			std::printf ( "  player score      %d\n", hasPlayer ? frontEnd.simulation ().player ().score : 0 );
			std::printf ( "  player trail      %d corners\n",
			              hasPlayer ? frontEnd.simulation ().player ().cornerCount () : 0 );
			std::printf ( "  alive pods      %d\n", frontEnd.simulation ().alivePodCount () );
			std::printf ( "  non-black pixels  %.2f%% %s\n", selfTestNonBlackFraction * 100.0,
			              drewGeometry ? "(drew geometry)" : "(BLACK FRAME)" );
			std::printf ( "  peak brightness   %.2f\n", selfTestPeakBrightness );
			std::printf ( "  OpenGL error      0x%04X %s\n\n", glError, ( glError == 0 ) ? "(none)" : "(FAILURE)" );

			renderer.shutdown ();
			window.destroy ();

			return ( ( glError == 0 ) && drewGeometry ) ? 0 : 1;
		}

		// Rotate the viewpoint every second of the self test, so all three camera paths are covered - unless a
		// single view was pinned on the command line or a front-end screen is up, in which case leave it alone.

		if ( ( selfTestFrames > 0 ) && ( forcedView < 0 ) && frontEnd.simulation ().isPlaying () )
		{
			const int phase = ( renderedFrames / 60 ) % 3;

			cameraMode = ( phase == 0 ) ? tron3d::CameraMode::Cockpit
			           : ( phase == 1 ) ? tron3d::CameraMode::ThirdPerson
			                            : tron3d::CameraMode::SpotPlane;

			frontEnd.simulation ().setFirstPersonView ( cameraMode == tron3d::CameraMode::Cockpit );
		}
	}

	renderer.shutdown ();
	window.destroy ();

	return 0;
}
