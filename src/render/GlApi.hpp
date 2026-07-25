//---------------------------------------------------------------------------------------------------------------------
// Project: Tron Light Pods
// Version: 1.9
// Date:    2000
// Author:  Rohin Gosling
//
// Description:
//
//   Minimal OpenGL 3.3 core profile function loader.
//
//   Windows ships an opengl32.dll that exports the OpenGL 1.1 entry points and nothing later, so everything from
//   1.2 onward has to be fetched at runtime through wglGetProcAddress against a context that is already current.
//   That is the whole job of this file.
//
//   It is hand-written rather than generated. A generator would emit some thousands of lines covering every
//   extension in the registry, of which this project uses about forty functions; the list below is the actual
//   dependency surface of the renderer, written out where it can be read. It also means the project keeps its
//   property of having no external dependencies at all beyond a compiler and the system libraries.
//
//   glcorearb.h is included WITHOUT GL_GLEXT_PROTOTYPES defined, which gives the enums and the function pointer
//   typedefs but no prototypes, leaving the names below free to be our own pointers.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include <windows.h>

// windows.h still defines the 16-bit segmented-memory keywords `near` and `far` as empty macros. They have
// meant nothing since Win16, but they silently mangle any identifier of either name in every translation unit
// that includes this header, and the resulting errors point at the innocent line rather than the cause. Retire
// them here, once, for everything downstream.

#ifdef near
	#undef near
#endif

#ifdef far
	#undef far
#endif

#include <GL/glcorearb.h>

namespace tron3d::gl
{

//---------------------------------------------------------------------------------------------------------------------
// The function list
//
//   One list, used three times: to declare the pointers, to define them, and to load them. Adding a GL call to
//   the renderer means adding exactly one line here.
//---------------------------------------------------------------------------------------------------------------------

#define TRON3D_GL_FUNCTION_LIST(ENTRY)                                              \
	/* OpenGL 1.0 and 1.1 - exported directly by opengl32.dll */                    \
	ENTRY ( PFNGLGETSTRINGPROC,               glGetString               )           \
	ENTRY ( PFNGLGETINTEGERVPROC,             glGetIntegerv             )           \
	ENTRY ( PFNGLGETERRORPROC,                glGetError                )           \
	ENTRY ( PFNGLVIEWPORTPROC,                glViewport                )           \
	ENTRY ( PFNGLCLEARPROC,                   glClear                   )           \
	ENTRY ( PFNGLCLEARCOLORPROC,              glClearColor              )           \
	ENTRY ( PFNGLREADPIXELSPROC,              glReadPixels              )           \
	ENTRY ( PFNGLENABLEPROC,                  glEnable                  )           \
	ENTRY ( PFNGLDISABLEPROC,                 glDisable                 )           \
	ENTRY ( PFNGLBLENDFUNCPROC,               glBlendFunc               )           \
	ENTRY ( PFNGLDEPTHFUNCPROC,               glDepthFunc               )           \
	ENTRY ( PFNGLDEPTHMASKPROC,               glDepthMask               )           \
	ENTRY ( PFNGLDRAWARRAYSPROC,              glDrawArrays              )           \
	ENTRY ( PFNGLDRAWELEMENTSPROC,            glDrawElements            )           \
	ENTRY ( PFNGLLINEWIDTHPROC,               glLineWidth               )           \
	                                                                                \
	/* OpenGL 1.5 - buffer objects */                                               \
	ENTRY ( PFNGLGENBUFFERSPROC,              glGenBuffers              )           \
	ENTRY ( PFNGLBINDBUFFERPROC,              glBindBuffer              )           \
	ENTRY ( PFNGLBUFFERDATAPROC,              glBufferData              )           \
	ENTRY ( PFNGLBUFFERSUBDATAPROC,           glBufferSubData           )           \
	ENTRY ( PFNGLDELETEBUFFERSPROC,           glDeleteBuffers           )           \
	                                                                                \
	/* OpenGL 2.0 - shaders and programs */                                         \
	ENTRY ( PFNGLCREATESHADERPROC,            glCreateShader            )           \
	ENTRY ( PFNGLSHADERSOURCEPROC,            glShaderSource            )           \
	ENTRY ( PFNGLCOMPILESHADERPROC,           glCompileShader           )           \
	ENTRY ( PFNGLGETSHADERIVPROC,             glGetShaderiv             )           \
	ENTRY ( PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog        )           \
	ENTRY ( PFNGLDELETESHADERPROC,            glDeleteShader            )           \
	ENTRY ( PFNGLCREATEPROGRAMPROC,           glCreateProgram           )           \
	ENTRY ( PFNGLATTACHSHADERPROC,            glAttachShader            )           \
	ENTRY ( PFNGLLINKPROGRAMPROC,             glLinkProgram             )           \
	ENTRY ( PFNGLGETPROGRAMIVPROC,            glGetProgramiv            )           \
	ENTRY ( PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog       )           \
	ENTRY ( PFNGLUSEPROGRAMPROC,              glUseProgram              )           \
	ENTRY ( PFNGLDELETEPROGRAMPROC,           glDeleteProgram           )           \
	ENTRY ( PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation      )           \
	ENTRY ( PFNGLUNIFORMMATRIX4FVPROC,        glUniformMatrix4fv        )           \
	ENTRY ( PFNGLUNIFORM4FPROC,               glUniform4f               )           \
	ENTRY ( PFNGLUNIFORM3FPROC,               glUniform3f               )           \
	ENTRY ( PFNGLUNIFORM2FPROC,               glUniform2f               )           \
	ENTRY ( PFNGLUNIFORM1FPROC,               glUniform1f               )           \
	ENTRY ( PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer     )           \
	ENTRY ( PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray )           \
	                                                                                \
	/* OpenGL 3.0 - vertex array objects */                                         \
	ENTRY ( PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays         )           \
	ENTRY ( PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray         )           \
	ENTRY ( PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays      )

//---------------------------------------------------------------------------------------------------------------------
// Pointer declarations
//---------------------------------------------------------------------------------------------------------------------

#define TRON3D_GL_DECLARE_POINTER(TYPE, NAME) extern TYPE NAME;

TRON3D_GL_FUNCTION_LIST ( TRON3D_GL_DECLARE_POINTER )

#undef TRON3D_GL_DECLARE_POINTER

//---------------------------------------------------------------------------------------------------------------------
// Function: loadFunctions
//
// Description:
//
//   Resolves every entry in the list above. A context must already be current on the calling thread.
//
// Arguments:
//
//   - missingName : Receives the name of the first function that could not be resolved, if any.
//
// Returns:
//
//   - True if every function resolved.
//
//---------------------------------------------------------------------------------------------------------------------

bool loadFunctions ( const char** missingName );

}
