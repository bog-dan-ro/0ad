/* Copyright (c) 2011 Wildfire Games
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * bring in OpenGL header+library, with compatibility fixes
 */

#ifndef INCLUDED_OPENGL
#define INCLUDED_OPENGL

#include "lib/config2.h" // CONFIG2_GLES

#if OS_WIN
// wgl.h is a private header and should only be included from here.
// if this isn't defined, it'll complain.
#define WGL_HEADER_NEEDED
#include "lib/sysdep/os/win/wgl.h"
#endif

#if CONFIG2_GLES
# include <GLES2/gl2.h>
#elif OS_MACOSX || OS_MAC
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

// if gl.h provides real prototypes for 1.2 / 1.3 functions,
// exclude the corresponding function pointers in glext_funcs.h
#ifdef GL_VERSION_1_2
#define REAL_GL_1_2
#endif
#ifdef GL_VERSION_1_3
#define REAL_GL_1_3
#endif

// this must come after GL/gl.h include, so we can't combine the
// including GL/glext.h.
#ifndef OS_ANDROID
# undef GL_GLEXT_PROTOTYPES
#else
# define GL_GLEXT_PROTOTYPES
#endif

#if CONFIG2_GLES
# include <GLES2/gl2ext.h>
#elif OS_MACOSX || OS_MAC
# include <OpenGL/glext.h>
#else
# include <GL/glext.h>
# if OS_WIN
#  include <GL/wglext.h>
# endif
#endif

#ifndef GL_BGR
# define GL_BGR 0x80E0
#endif

#ifndef GL_RED
# define GL_RED 0x1903
#endif

#ifndef GL_RG
# define GL_RG 0x8227
#endif

#endif	// #ifndef INCLUDED_OPENGL
