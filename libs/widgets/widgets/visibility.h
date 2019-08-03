/*
 * Copyright (C) 2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libwidgets_visibility_h__
#define __libwidgets_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBWIDGETS_DLL_IMPORT __declspec(dllimport)
  #define LIBWIDGETS_DLL_EXPORT __declspec(dllexport)
  #define LIBWIDGETS_DLL_LOCAL
#else
  #define LIBWIDGETS_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBWIDGETS_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBWIDGETS_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBWIDGETS_STATIC // libwidgets is not a DLL
#define LIBWIDGETS_API
#define LIBWIDGETS_LOCAL
#else
  #ifdef LIBWIDGETS_DLL_EXPORTS // defined if we are building the libwidgets DLL (instead of using it)
    #define LIBWIDGETS_API LIBWIDGETS_DLL_EXPORT
  #else
    #define LIBWIDGETS_API LIBWIDGETS_DLL_IMPORT
  #endif
  #define LIBWIDGETS_LOCAL LIBWIDGETS_DLL_LOCAL
#endif

#endif /* __libwidgets_visibility_h__ */
