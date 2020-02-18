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

#ifndef __libpbd_libpbd_visibility_h__
#define __libpbd_libpbd_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBPBD_DLL_IMPORT __declspec(dllimport)
  #define LIBPBD_DLL_EXPORT __declspec(dllexport)
  #define LIBPBD_DLL_LOCAL
  #define LIBPBD_TEMPLATE_DLL_IMPORT
  #define LIBPBD_TEMPLATE_DLL_EXPORT
  #define LIBPBD_TEMPLATE_MEMBER_DLL_IMPORT __declspec(dllimport)
  #define LIBPBD_TEMPLATE_MEMBER_DLL_EXPORT __declspec(dllexport)
#else
  #define LIBPBD_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBPBD_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBPBD_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #define LIBPBD_TEMPLATE_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBPBD_TEMPLATE_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBPBD_TEMPLATE_MEMBER_DLL_IMPORT
  #define LIBPBD_TEMPLATE_MEMBER_DLL_EXPORT
#endif

#ifdef LIBPBD_STATIC // libpbd is a DLL
  #define LIBPBD_API
  #define LIBPBD_LOCAL
  #define LIBPBD_TEMPLATE_API
  #define LIBPBD_TEMPLATE_MEMBER_API
#else
  #ifdef LIBPBD_DLL_EXPORTS // defined if we are building the libpbd DLL (instead of using it)
    #define LIBPBD_API LIBPBD_DLL_EXPORT
    #define LIBPBD_TEMPLATE_API LIBPBD_TEMPLATE_DLL_EXPORT
    #define LIBPBD_TEMPLATE_MEMBER_API LIBPBD_TEMPLATE_MEMBER_DLL_EXPORT
  #else
    #define LIBPBD_API LIBPBD_DLL_IMPORT
    #define LIBPBD_TEMPLATE_API LIBPBD_TEMPLATE_DLL_IMPORT
    #define LIBPBD_TEMPLATE_MEMBER_API LIBPBD_TEMPLATE_MEMBER_DLL_IMPORT
  #endif
  #define LIBPBD_LOCAL LIBPBD_DLL_LOCAL
#endif

#endif /* __libpbd_libpbd_visibility_h__ */
