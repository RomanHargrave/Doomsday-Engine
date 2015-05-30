/** @file libgui.h  Common definitions for libgui.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBGUI_MAIN_H
#define LIBGUI_MAIN_H

#include <de/c_wrapper.h>

/*
 * The LIBGUI_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libgui.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __LIBGUI__
// This is defined when compiling the library.
#    define LIBGUI_PUBLIC __declspec(dllexport)
#  else
#    define LIBGUI_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define LIBGUI_PUBLIC
#endif

#if defined(WIN32) || defined(MACOSX)
#  define LIBGUI_ACCURATE_TEXT_BOUNDS
#endif

// Assertion specific to GL errors.
#if 1 //|| defined(DENG_X11) || defined(WIN32)
#  define LIBGUI_ASSERT_GL(cond) // just logged, no abort
#else
#  define LIBGUI_ASSERT_GL(cond) DENG2_ASSERT(cond)
#endif

#define LIBGUI_GL_ERROR_STR(error) \
    (error == GL_NO_ERROR?          "GL_NO_ERROR" : \
     error == GL_INVALID_ENUM?      "GL_INVALID_ENUM" : \
     error == GL_INVALID_VALUE?     "GL_INVALID_VALUE" : \
     error == GL_INVALID_OPERATION? "GL_INVALID_OPERATION" : \
     error == GL_STACK_OVERFLOW?    "GL_STACK_OVERFLOW" : \
     error == GL_STACK_UNDERFLOW?   "GL_STACK_UNDERFLOW" : \
     error == GL_OUT_OF_MEMORY?     "GL_OUT_OF_MEMORY" : \
     error == GL_INVALID_FRAMEBUFFER_OPERATION? "GL_INVALID_FRAMEBUFFER_OPERATION" : \
                                    "?")

#ifndef NDEBUG
#  define LIBGUI_ASSERT_GL_OK() {GLuint _er = GL_NO_ERROR; do { \
    _er = glGetError(); if(_er != GL_NO_ERROR) { \
    LogBuffer_Flush(); qWarning(__FILE__":%i: OpenGL error: 0x%x (%s)", __LINE__, _er, \
    LIBGUI_GL_ERROR_STR(_er)); LIBGUI_ASSERT_GL(!"OpenGL operation failed"); \
    }} while(_er != GL_NO_ERROR);}
#else
#  define LIBGUI_ASSERT_GL_OK()
#endif

#ifdef __cplusplus
#  define LIBGUI_EXTERN_C extern "C"
#else
#  define LIBGUI_EXTERN_C extern
#endif

namespace de {

} // namespace de

#endif // LIBGUI_MAIN_H
