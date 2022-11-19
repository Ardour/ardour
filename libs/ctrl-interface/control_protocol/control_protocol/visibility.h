/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libcontrolcp_visibility_h__
#define __libcontrolcp_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBCONTROLCP_DLL_IMPORT __declspec(dllimport)
  #define LIBCONTROLCP_DLL_EXPORT __declspec(dllexport)
  #define LIBCONTROLCP_DLL_LOCAL
#else
  #define LIBCONTROLCP_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBCONTROLCP_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBCONTROLCP_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBCONTROLCP_DLL_EXPORTS // defined if we are building the libcontrolcp DLL (instead of using it)
    #define LIBCONTROLCP_API LIBCONTROLCP_DLL_EXPORT
#else
    #define LIBCONTROLCP_API LIBCONTROLCP_DLL_IMPORT
#endif
#define LIBCONTROLCP_LOCAL LIBCONTROLCP_DLL_LOCAL

/* These should be used by surfaces/control interfaces. They use (probably)
 * libcontrolcp but they are not part of it. The idea here is to avoid
 * having to define per-surface macros for each and every surface. Instead,
 * every surface defines ARDOURSURFACE_DLL_EXPORTS during building and
 * uses ARDOURSURFACE_API in its declarations.
 */

#ifdef ARDOURSURFACE_DLL_EXPORTS // defined if we are building the libcontrolcp DLL (instead of using it)
    #define ARDOURSURFACE_API LIBCONTROLCP_DLL_EXPORT
#else
    #define ARDOURSURFACE_API LIBCONTROLCP_DLL_IMPORT
#endif
#define ARDOURSURFACE_LOCAL LIBCONTROLCP_DLL_LOCAL


#endif /* __libcontrolcp_visibility_h__ */
