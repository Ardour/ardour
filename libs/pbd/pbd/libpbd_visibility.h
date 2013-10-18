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

#ifndef __libpbd_libpbd_visibility_h__
#define __libpbd_libpbd_visibility_h__

/* _WIN32 is defined by most compilers targetting Windows, but within the
 * ardour source tree, we also define COMPILER_MSVC or COMPILER_MINGW depending
 * on how a Windows build is built.
 */

#if defined _WIN32 || defined __CYGWIN__ || defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
  #define LIBPBD_DLL_IMPORT __declspec(dllimport)
  #define LIBPBD_DLL_EXPORT __declspec(dllexport)
  #define LIBPBD_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define LIBPBD_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define LIBPBD_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define LIBPBD_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define LIBPBD_DLL_IMPORT
    #define LIBPBD_DLL_EXPORT
    #define LIBPBD_DLL_LOCAL
  #endif
#endif

#ifdef LIBPBD_DLL // libpbd is a DLL
#ifdef LIBPBD_DLL_EXPORTS // defined if we are building the libpbd DLL (instead of using it)
    #define LIBPBD_API LIBPBD_DLL_EXPORT
#else
    #define LIBPBD_API LIBPBD_DLL_IMPORT
#endif 
#define     LIBPBD_LOCAL LIBPBD_DLL_LOCAL
#else /* static lib, not DLL */
#define LIBPBD_API
#define LIBPBD_LOCAL
#endif

#endif /* __libpbd_libpbd_visibility_h__ */
