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

#ifndef __libaudiographer_visibility_h__
#define __libaudiographer_visibility_h__

/* _WIN32 is defined by most compilers targetting Windows, but within the
 * audiographer source tree, we also define COMPILER_MSVC or COMPILER_MINGW depending
 * on how a Windows build is built.
 */

#if defined _WIN32 || defined __CYGWIN__ || defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
  #define LIBAUDIOGRAPHER_DLL_IMPORT __declspec(dllimport)
  #define LIBAUDIOGRAPHER_DLL_EXPORT __declspec(dllexport)
  #define LIBAUDIOGRAPHER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define LIBAUDIOGRAPHER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define LIBAUDIOGRAPHER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define LIBAUDIOGRAPHER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define LIBAUDIOGRAPHER_DLL_IMPORT
    #define LIBAUDIOGRAPHER_DLL_EXPORT
    #define LIBAUDIOGRAPHER_DLL_LOCAL
  #endif
#endif

#ifdef LIBAUDIOGRAPHER_DLL // libaudiographer is a DLL
#ifdef LIBAUDIOGRAPHER_DLL_EXPORTS // defined if we are building the libaudiographer DLL (instead of using it)
    #define LIBAUDIOGRAPHER_API LIBAUDIOGRAPHER_DLL_EXPORT
#else
    #define LIBAUDIOGRAPHER_API LIBAUDIOGRAPHER_DLL_IMPORT
#endif 
#define     LIBAUDIOGRAPHER_LOCAL LIBAUDIOGRAPHER_DLL_LOCAL
#else /* static lib, not DLL */
#define LIBAUDIOGRAPHER_API
#define LIBAUDIOGRAPHER_LOCAL
#endif

#endif /* __libaudiographer_visibility_h__ */
