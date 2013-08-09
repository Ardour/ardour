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
#ifndef __msvc_libardour_h__
#define __msvc_libardour_h__

#include <limits.h>

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
		#define LIBARDOUR_APICALLTYPE __stdcall
	#elif defined(COMPILER_MSVC) || defined(COMPILER_MINGW) // Probably needs Cygwin too, at some point
		#define LIBARDOUR_API __declspec(dllimport)
		#define LIBARDOUR_APICALLTYPE __stdcall
	#else
		#error "Attempting to define __declspec with an incompatible compiler !"
	#endif
#elif !defined(LIBARDOUR_API)
	// Other compilers / platforms could be accommodated here
	#define LIBARDOUR_API
	#define LIBARDOUR_APICALLTYPE
#endif

#ifndef _MAX_PATH
#define _MAX_PATH  260
#endif
#ifndef  PATH_MAX
#define  PATH_MAX _MAX_PATH
#endif

namespace ARDOUR {

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

// LIBARDOUR_API char*  LIBARDOUR_APICALLTYPE placeholder_for_non_msvc_specific_function(s);

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

}  // namespace ARDOUR

#ifdef COMPILER_MSVC
#include <rpc.h>
//#include <io.h>

#ifndef __THROW
#define __THROW throw()
#endif
#include <ardourext/sys/time.h>

namespace ARDOUR {

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

LIBARDOUR_API int    LIBARDOUR_APICALLTYPE symlink(const char *dest, const char *shortcut, const char *working_directory = 0);
LIBARDOUR_API int    LIBARDOUR_APICALLTYPE readlink(const char *__restrict shortcut, char *__restrict buf, size_t bufsize);

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

}  // namespace ARDOUR

#endif  // 	COMPILER_MSVC
#endif  // __mavc_libardour_h__
