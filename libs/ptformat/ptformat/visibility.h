/*
 * Copyright (C) 2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libptformat_visibility_h__
#define __libptformat_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBPTFORMAT_DLL_IMPORT __declspec(dllimport)
  #define LIBPTFORMAT_DLL_EXPORT __declspec(dllexport)
  #define LIBPTFORMAT_DLL_LOCAL
#else
  #define LIBPTFORMAT_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBPTFORMAT_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBPTFORMAT_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBPTFORMAT_STATIC // libptformat is not a DLL
  #define LIBPTFORMAT_API
  #define LIBPTFORMAT_LOCAL
#else
  #ifdef LIBPTFORMAT_DLL_EXPORTS // defined if we are building the libptformat DLL (instead of using it)
     #define LIBPTFORMAT_API LIBPTFORMAT_DLL_EXPORT
  #else
     #define LIBPTFORMAT_API LIBPTFORMAT_DLL_IMPORT
  #endif
  #define    LIBPTFORMAT_LOCAL LIBPTFORMAT_DLL_LOCAL
#endif

#endif /* __libptformat_visibility_h__ */
