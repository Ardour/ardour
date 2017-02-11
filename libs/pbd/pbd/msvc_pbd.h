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
#ifndef _msvc_pbd_h_
#define _msvc_pbd_h_

#ifdef  PBD_IS_IN_WIN_STATIC_LIB  // #define if your project uses libpbd (under Windows) as a static library
#undef  LIBPBD_DLL
#define PBD_IS_IN_WINDLL 0
#endif

#include <pbd/libpbd_visibility.h>

#ifndef COMPILER_MSVC
#include <sys/time.h>
#else
#include <ardourext/misc.h>
#include <ardourext/sys/time.h>
#endif

#if !defined(PBD_IS_IN_WINDLL)
	#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
	// If you need '__declspec' compatibility, add extra compilers to the above as necessary
		#define PBD_IS_IN_WINDLL 1
	#else
		#define PBD_IS_IN_WINDLL 0
	#endif
#endif

#if PBD_IS_IN_WINDLL && !defined(PBD_APICALLTYPE)
	#if defined(BUILDING_PBD)
		#define PBD_APICALLTYPE __cdecl
	#elif defined(COMPILER_MSVC) || defined(COMPILER_MINGW) // Probably needs Cygwin too, at some point
		#define PBD_APICALLTYPE __cdecl
	#else
		#error "Attempting to define __declspec with an incompatible compiler !"
	#endif
#elif !defined(PBD_APICALLTYPE)
	// Other compilers / platforms could be accommodated here
	#define PBD_APICALLTYPE
#ifndef GETOPT_API
	#define GETOPT_API
	#define GETOPT_APICALLTYPE
#endif
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

#ifndef _MAX_PATH
#define _MAX_PATH  260
#endif
#ifndef  PATH_MAX
#define  PATH_MAX _MAX_PATH
#endif

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

#ifdef PLATFORM_WINDOWS

#ifndef PBDEXTN_API
	#if defined(BUILDING_PBDEXTN)
		#define PBDEXTN_API __declspec(dllexport)
		#define PBDEXTN_APICALLTYPE __cdecl
	#elif defined(COMPILER_MSVC) || defined(COMPILER_MINGW) // Probably needs Cygwin too, at some point
		#define PBDEXTN_API __declspec(dllimport)
		#define PBDEXTN_APICALLTYPE __cdecl
	#else
		#error "Attempting to define __declspec with an incompatible compiler !"
	#endif
#endif  // PBDEXTN_API

#ifndef CYGIMPORT_API
		#define CYGIMPORT_API __declspec(dllimport)
		#define CYGIMPORT_APICALLTYPE __cdecl
#endif  // CYGIMPORT_API

#ifndef __THROW
#define __THROW throw()
#endif

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT       ((void *) 0)
#define RTLD_NEXT          ((void *) -1L)
#define RTLD_LAZY          0x00001
#define RTLD_NOW           0x00002
#define RTLD_BINDING_MASK  0x00003
#define RTLD_NOLOAD        0x00004
#define RTLD_GLOBAL        0x00004
#define RTLD_DEEPBIND      0x00008
#endif

#ifndef OPEN_MAX
#define OPEN_MAX			32
#endif

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

PBDEXTN_API	   int		PBDEXTN_APICALLTYPE cyginit (unsigned int result);
LIBPBD_API     int 		PBD_APICALLTYPE     dlclose (void *handle) __THROW;
LIBPBD_API     void*	PBD_APICALLTYPE     dlopen  (const char *file_name, int mode) __THROW;
LIBPBD_API     void* 	PBD_APICALLTYPE     dlsym   (void *handle, const char *symbol_name) __THROW;
LIBPBD_API     char* 	PBD_APICALLTYPE     dlerror () __THROW;

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

#ifndef __CYGWIN__
/* For whatever reason, Ardour's 'libevoral' refuses to build as a DLL if we include both 'rpc.h' */
/* and 'WinSock2.h'. It doesn't seem to matter which order we #include them. Given that we can't  */
/* edit 'rpc.h' or 'WinSock2.h', just make sure we don't #include them when building libevoral.   */
#if !defined(BUILDING_EVORAL) && !defined(BUILDING_QMDSP) && !defined(BUILDING_VAMPPLUGINS)
#include <rpc.h>
typedef int (FAR PBDEXTN_APICALLTYPE *CYGINIT_API)(unsigned int);
#endif
#include <io.h>
#include <sys/types.h>

#ifndef FILENAME_MAX
#define FILENAME_MAX (260)
#endif

#ifndef _SSIZE_T_
#define _SSIZE_T_
typedef long _ssize_t;

#ifndef	_NO_OLDNAMES
typedef _ssize_t ssize_t;
#endif
#endif /* ! _SSIZE_T_ */

struct dirent
{
	long			d_ino;				  // Always zero
	unsigned short	d_reclen;			  // Always zero
	unsigned short	d_namlen;			  // Length of name in d_name
	char			d_name[FILENAME_MAX]; // File name
};

// This is an internal data structure. Do not use it
// except as an argument to one of the functions below.
typedef struct
{
	// Disk transfer area for this dir
	struct _finddata_t	dd_dta;

	// 'dirent' struct to return from dir (NOTE: this
	// is not thread safe).
	struct dirent		dd_dir;

	// '_findnext()' handle
	long				dd_handle;

	// Current status of search:
	//  0 = not started yet (next entry to read is first entry)
	// -1 = off the end
	//  Otherwise - positive (0 based) index of next entry
	int					dd_stat;

	// Full path for dir with search pattern (struct will be extended)
	char				dd_name[1];
} DIR;

typedef unsigned int nfds_t;

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

LIBPBD_API int				__cdecl         gettimeofday(struct timeval *__restrict tv, __timezone_ptr_t tz);
LIBPBD_API ssize_t			PBD_APICALLTYPE pread(int handle, void *buf, size_t nbytes, off_t offset);
LIBPBD_API ssize_t			PBD_APICALLTYPE pwrite(int handle, const void *buf, size_t nbytes, off_t offset);

#if defined(_MSC_VER) && (_MSC_VER < 1800)
LIBPBD_API double			PBD_APICALLTYPE expm1(double x);
LIBPBD_API double			PBD_APICALLTYPE log1p(double x);
LIBPBD_API double			PBD_APICALLTYPE round(double x);
LIBPBD_API float			PBD_APICALLTYPE roundf(float x);
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1900)
LIBPBD_API double			PBD_APICALLTYPE log2 (double x);
LIBPBD_API double			PBD_APICALLTYPE trunc(double x);
#endif

namespace PBD {

LIBPBD_API bool 			PBD_APICALLTYPE TestForMinimumSpecOS(char *revision="currently ignored");
LIBPBD_API char*			PBD_APICALLTYPE realpath    (const char *original_path, char resolved_path[_MAX_PATH+1]);
LIBPBD_API int				PBD_APICALLTYPE mkstemp     (char *template_name);
LIBPBD_API int				PBD_APICALLTYPE ntfs_link   (const char *existing_filepath, const char *link_filepath);
LIBPBD_API int				PBD_APICALLTYPE ntfs_unlink (const char *link_filepath);

// These are used to replicate 'dirent.h' functionality
LIBPBD_API DIR*				PBD_APICALLTYPE opendir  (const char *szPath);
LIBPBD_API struct dirent*	PBD_APICALLTYPE readdir  (DIR *pDir);
LIBPBD_API int				PBD_APICALLTYPE closedir (DIR *pDir);

}  // namespace PBD

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

#endif  // !__CYGWIN__
#endif  // PLATFORM_WINDOWS
#endif  // _msvc_pbd_h_
