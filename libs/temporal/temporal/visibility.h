/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libtimecode_visibility_h__
#define __libtimecode_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBTEMPORAL_DLL_IMPORT __declspec(dllimport)
  #define LIBTEMPORAL_DLL_EXPORT __declspec(dllexport)
  #define LIBTEMPORAL_DLL_LOCAL
  #define LIBTEMPORAL_TEMPLATE_DLL_IMPORT __declspec(dllimport)
  #define LIBTEMPORAL_TEMPLATE_DLL_EXPORT __declspec(dllexport)
#else
  #define LIBTEMPORAL_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBTEMPORAL_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBTEMPORAL_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #define LIBTEMPORAL_TEMPLATE_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBTEMPORAL_TEMPLATE_DLL_EXPORT __attribute__ ((visibility ("default")))
#endif

#ifdef LIBTEMPORAL_DLL_EXPORTS // defined if we are building the libtimecode DLL (instead of using it)
    #define LIBTEMPORAL_API LIBTEMPORAL_DLL_EXPORT
    #define LIBTEMPORAL_TEMPLATE_API LIBTEMPORAL_TEMPLATE_DLL_EXPORT
#else
    #define LIBTEMPORAL_API LIBTEMPORAL_DLL_IMPORT
    #define LIBTEMPORAL_TEMPLATE_API LIBTEMPORAL_TEMPLATE_DLL_IMPORT
#endif
#define LIBTEMPORAL_LOCAL LIBTEMPORAL_DLL_LOCAL

#endif /* __libtimecode_visibility_h__ */
