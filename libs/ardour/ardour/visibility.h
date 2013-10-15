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

#ifndef __libardour_visibility_h__
#define __libardour_visibility_h__

#ifdef  LIBARDOUR_IS_IN_WIN_STATIC_LIB  // #define if your project uses libardour (under Windows) as a static library
#define LIBARDOUR_IS_IN_WINDLL 0
#endif

#if !defined(LIBARDOUR_IS_IN_WINDLL)
	#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
	// If you need '__declspec' compatibility, add extra compilers to the above as necessary
		#define LIBARDOUR_IS_IN_WINDLL 1
	#else
		#define LIBARDOUR_IS_IN_WINDLL 0
	#endif
#endif

#if LIBARDOUR_IS_IN_WINDLL && !defined(LIBARDOUR_API)
	#if defined(BUILDING_LIBARDOUR)
		#define LIBARDOUR_API __declspec(dllexport)
		#define LIBARDOUR_APICALLTYPE __cdecl
	#elif defined(COMPILER_MSVC) || defined(COMPILER_MINGW) // Probably needs Cygwin too, at some point
		#define LIBARDOUR_API __declspec(dllimport)
		#define LIBARDOUR_APICALLTYPE __cdecl
	#else
		#error "Attempting to define __declspec with an incompatible compiler !"
	#endif
#elif !defined(LIBARDOUR_API)
	// Other compilers / platforms could be accommodated here (as an example, see LIBARDOUR_HELPER_DLL, below)
	#define LIBARDOUR_API
	#define LIBARDOUR_APICALLTYPE
#endif


/* _WIN32 is defined by most compilers targetting Windows, but within the
 * ardour source tree, we also define COMPILER_MSVC or COMPILER_MINGW depending
 * on how a Windows build is built.
 */

#if defined _WIN32 || defined __CYGWIN__ || defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
  #define LIBARDOUR_HELPER_DLL_IMPORT __declspec(dllimport)
  #define LIBARDOUR_HELPER_DLL_EXPORT __declspec(dllexport)
  #define LIBARDOUR_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define LIBARDOUR_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define LIBARDOUR_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define LIBARDOUR_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define LIBARDOUR_HELPER_DLL_IMPORT
    #define LIBARDOUR_HELPER_DLL_EXPORT
    #define LIBARDOUR_HELPER_DLL_LOCAL
  #endif
#endif

#endif /* __libardour_visibility_h__ */
