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

#include "ardour/visibility.h"
#include <limits.h>

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
