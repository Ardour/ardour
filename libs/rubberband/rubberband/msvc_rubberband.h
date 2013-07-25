/*
    Copyright (C) 2009 John Emmas

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
#ifndef __msvc_rubberband_h__
#define __msvc_rubberband_h__

#ifdef  RUBBERBAND_IS_IN_WIN_STATIC_LIB  // #define if your project uses librubberband (under Windows) as a static library
#define RUBBERBAND_IS_IN_WINDLL 0
#endif

#if !defined(RUBBERBAND_IS_IN_WINDLL)
	#if defined(_MSC_VER) || defined(__MINGW__) || defined(_MINGW32__)
	// If you need '__declspec' compatibility, add extra compilers to the above as necessary
		#define RUBBERBAND_IS_IN_WINDLL 1
	#else
		#define RUBBERBAND_IS_IN_WINDLL 0
	#endif
#endif

#if RUBBERBAND_IS_IN_WINDLL && !defined(RUBBERBAND_API)
	#if defined(BUILDING_RUBBERBAND)
		#define RUBBERBAND_API __declspec(dllexport)
		#define RUBBERBAND_APICALLTYPE __stdcall
	#elif defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW__) || defined(_MINGW32__)
		#define RUBBERBAND_API __declspec(dllimport)
		#define RUBBERBAND_APICALLTYPE __stdcall
	#else
		#error "Attempting to define __declspec with an incompatible compiler !"
	#endif
#elif !defined(RUBBERBAND_API)
	// Other compilers / platforms could be accommodated here
	#define RUBBERBAND_API
	#define RUBBERBAND_APICALLTYPE
	#define GETOPT_API
	#define GETOPT_APICALLTYPE
#endif

#ifndef GETOPT_API
	#if defined(BUILDING_GETOPT)
		#define GETOPT_API __declspec(dllexport)
		#define GETOPT_APICALLTYPE __cdecl
	#elif defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW__) || defined(_MINGW32__)
		#define GETOPT_API __declspec(dllimport)
		#define GETOPT_APICALLTYPE __cdecl
	#else
		#error "Attempting to define __declspec with an incompatible compiler !"
	#endif
#endif  // GETOPT_API

#ifdef _MSC_VER
#include <rpc.h>

#ifndef __THROW
#define __THROW throw()
#endif

namespace RubberBand {

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

// These are used to replicate 'dirent.h' functionality
// RUBBERBAND_API int RUBBERBAND_APICALLTYPE placeholder();

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

}  // namespace Rubberband

#endif  // _MSC_VER
#endif  // __msvc_rubberband_h__
