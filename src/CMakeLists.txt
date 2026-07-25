#-----------------------------------------------------------------------------------------------------------------------
# Project: Tron Light Pods
# Version: 1.9
# Date:    2000
# Author:  Rohin Gosling
#
# Description:
#
#   Build for Tron Light Pods.
#
#   Targets:
#
#     tron3d_core             The simulation. No platform or graphics dependency at all, which is what makes it
#                             testable on its own.
#     tron3d                  The game: Win32 window, OpenGL 3.3 core renderer, fixed timestep. Installed as
#                             tron-light-pods-1-9.exe.
#
#   FLOATING POINT. Do not add -ffast-math, -funsafe-math-optimizations or -Ofast to any target. The simulation is
#   deterministic from its seed, and one seed is supposed to identify one game - the same spawns, the same opponent
#   decisions, the same outcome. Reassociating the arithmetic quietly breaks that. The check below enforces it
#   rather than trusting anyone to remember.
#
# Usage, from the repository root:
#
#   cmake -S src -B build -G Ninja
#   cmake --build build
#   build/tron-light-pods-1-9
#
#-----------------------------------------------------------------------------------------------------------------------

cmake_minimum_required ( VERSION 3.20 )

project ( tron3d VERSION 1.9 LANGUAGES CXX )

#-----------------------------------------------------------------------------------------------------------------------
# Language standard
#-----------------------------------------------------------------------------------------------------------------------

set ( CMAKE_CXX_STANDARD          20  )
set ( CMAKE_CXX_STANDARD_REQUIRED ON  )
set ( CMAKE_CXX_EXTENSIONS        OFF )

if ( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
	set ( CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE )
endif ()

#-----------------------------------------------------------------------------------------------------------------------
# Output names
#
#   The target names are internal; this is the file that actually ships. Kept here as one constant so the name is
#   set in a single place rather than repeated down the file.
#-----------------------------------------------------------------------------------------------------------------------

set ( GAME_EXECUTABLE_NAME "tron-light-pods-1-9" )

#-----------------------------------------------------------------------------------------------------------------------
# Warnings and floating point safety
#-----------------------------------------------------------------------------------------------------------------------

add_compile_options ( -Wall -Wextra )

# Belt and braces. If someone drops a fast-math flag into the compiler flags the build stops here, rather than
# producing a binary that quietly gives different answers for the same seed.

foreach ( FLAG_VARIABLE CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE )
	if ( "${${FLAG_VARIABLE}}" MATCHES "fast-math|Ofast|funsafe-math" )
		message ( FATAL_ERROR
		          "${FLAG_VARIABLE} enables unsafe floating point optimisation. "
		          "The simulation requires exact, reproducible float behaviour. Remove it." )
	endif ()
endforeach ()

#-----------------------------------------------------------------------------------------------------------------------
# tron3d_core - the simulation
#-----------------------------------------------------------------------------------------------------------------------

add_library ( tron3d_core STATIC
	core/Simulation.cpp
)

target_include_directories ( tron3d_core PUBLIC core )

#-----------------------------------------------------------------------------------------------------------------------
# tron3d - the game
#
#   Links only against the system libraries: opengl32 for the context and the 1.1 entry points, gdi32 for pixel
#   format selection and buffer swapping, user32 for the window. Everything from OpenGL 1.2 onward is resolved at
#   runtime by render/GlApi.cpp, so there is no loader library to vendor and nothing to fetch.
#-----------------------------------------------------------------------------------------------------------------------

add_executable ( tron3d
	app/FrontEnd.cpp
	app/Main.cpp
	platform/Window.cpp
	render/GlApi.cpp
	render/Renderer.cpp
)

target_include_directories ( tron3d PRIVATE core platform render )

target_link_libraries ( tron3d PRIVATE tron3d_core opengl32 gdi32 user32 )

set_target_properties ( tron3d PROPERTIES OUTPUT_NAME "${GAME_EXECUTABLE_NAME}" )

#-----------------------------------------------------------------------------------------------------------------------
# Tests
#
#   tests/ carries its own CMakeLists and is not part of a source distribution, so it is added only when it is
#   actually present. A tree without it configures and builds the game in exactly the same way.
#-----------------------------------------------------------------------------------------------------------------------

if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt" )

	enable_testing ()

	add_subdirectory ( tests )

endif ()
