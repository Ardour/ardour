/*
    Copyright (C) 2013 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __libmidi_libmidi_visibility_h__
#define __libmidi_libmidi_visibility_h__

/* _WIN32 is defined by most compilers targetting Windows, but within the
 * midi source tree, we also define COMPILER_MSVC or COMPILER_MINGW depending
 * on how a Windows build is built.
 */

#if defined _WIN32 || defined __CYGWIN__ || defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
  #define LIBMIDIPP_DLL_IMPORT __declspec(dllimport)
  #define LIBMIDIPP_DLL_EXPORT __declspec(dllexport)
  #define LIBMIDIPP_DLL_LOCAL
#else
  #define LIBMIDIPP_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBMIDIPP_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBMIDIPP_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBMIDIPP_STATIC // libmidi is not a DLL
  #define LIBMIDIPP_API
  #define LIBMIDIPP_LOCAL
#else
  #ifdef LIBMIDIPP_DLL_EXPORTS // defined if we are building the libmidi DLL (instead of using it)
     #define LIBMIDIPP_API LIBMIDIPP_DLL_EXPORT
  #else
     #define LIBMIDIPP_API LIBMIDIPP_DLL_IMPORT
  #endif 
  #define    LIBMIDIPP_LOCAL LIBMIDIPP_DLL_LOCAL
#endif

#endif /* __libmidi_libmidi_visibility_h__ */
