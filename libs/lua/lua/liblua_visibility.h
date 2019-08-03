/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __liblua_visibility_h__
#define __liblua_visibility_h__

#if defined(COMPILER_MSVC)
#  define LIBLUA_DLL_IMPORT __declspec(dllimport)
#  define LIBLUA_DLL_EXPORT __declspec(dllexport)
#  define LIBLUA_DLL_LOCAL
#else
#  define LIBLUA_DLL_IMPORT __attribute__ ((visibility ("default")))
#  define LIBLUA_DLL_EXPORT __attribute__ ((visibility ("default")))
#  define LIBLUA_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif


#ifdef COMPILER_MSVC
// MSVC: build liblua as DLL
#  define LIBLUA_BUILD_AS_DLL
#else
// others currently use a static lib (incl. with libardour)
#  define LIBLUA_STATIC
#endif


#ifdef LIBLUA_STATIC
#  define LIBLUA_API
#else
// define when building the DLL (instead of using it)
#  ifdef LIBLUA_DLL_EXPORTS
#    define LIBLUA_API LIBLUA_DLL_EXPORT
#  else
#    define LIBLUA_API LIBLUA_DLL_IMPORT
#  endif
#endif

#endif /* __liblua_visibility_h__ */
