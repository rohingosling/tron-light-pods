//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   Definitions and runtime resolution for the OpenGL function pointers declared in GlApi.hpp.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "GlApi.hpp"

namespace tron3d::gl
{

//---------------------------------------------------------------------------------------------------------------------
// Pointer definitions
//---------------------------------------------------------------------------------------------------------------------

#define TRON3D_GL_DEFINE_POINTER(TYPE, NAME) TYPE NAME = nullptr;

TRON3D_GL_FUNCTION_LIST ( TRON3D_GL_DEFINE_POINTER )

#undef TRON3D_GL_DEFINE_POINTER

//---------------------------------------------------------------------------------------------------------------------
// Function: resolve
//
// Description:
//
//   Resolves one entry point.
//
//   Two lookups are needed, and the order matters. wglGetProcAddress returns the driver's implementation for
//   anything from OpenGL 1.2 onward, but on the Microsoft ICD it returns null for the 1.0 and 1.1 functions,
//   because those are exported from opengl32.dll directly and are expected to be linked rather than fetched.
//   Falling back to GetProcAddress on the module covers them.
//
//   The null check is also less simple than it looks. Some drivers historically returned 1, 2, 3 or -1 rather
//   than 0 to mean "not supported", and a bare null test lets those through as valid-looking pointers that crash
//   on first call. All five values are treated as failure.
//
// Arguments:
//
//   - name : The entry point name.
//
// Returns:
//
//   - The resolved address, or null.
//
//---------------------------------------------------------------------------------------------------------------------

static PROC resolve ( const char* name )
{
	PROC address = wglGetProcAddress ( name );

	const bool isSentinel = ( address == nullptr                                 ) ||
	                        ( address == reinterpret_cast<PROC> (  1 )           ) ||
	                        ( address == reinterpret_cast<PROC> (  2 )           ) ||
	                        ( address == reinterpret_cast<PROC> (  3 )           ) ||
	                        ( address == reinterpret_cast<PROC> ( -1 )           );

	if ( isSentinel )
	{
		static HMODULE openGlModule = LoadLibraryA ( "opengl32.dll" );

		address = ( openGlModule != nullptr ) ? GetProcAddress ( openGlModule, name ) : nullptr;
	}

	return address;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: loadFunctions
//---------------------------------------------------------------------------------------------------------------------

bool loadFunctions ( const char** missingName )
{
	bool        allResolved = true;
	const char* firstMissing = nullptr;

	// The cast goes through void* deliberately. Casting a PROC, which is declared as a no-argument function
	// pointer, straight to a typed entry point is a cast between incompatible function types and warns; routing
	// it through an object pointer is the conventional way to say "yes, this is intentional".

	#define TRON3D_GL_RESOLVE_POINTER(TYPE, NAME)                                   \
		NAME = reinterpret_cast<TYPE> ( reinterpret_cast<void*> ( resolve ( #NAME ) ) ); \
		if ( ( NAME == nullptr ) && ( firstMissing == nullptr ) )                   \
		{                                                                           \
			firstMissing = #NAME;                                                   \
			allResolved  = false;                                                   \
		}

	TRON3D_GL_FUNCTION_LIST ( TRON3D_GL_RESOLVE_POINTER )

	#undef TRON3D_GL_RESOLVE_POINTER

	if ( missingName != nullptr )
	{
		*missingName = firstMissing;
	}

	return allResolved;
}

}
