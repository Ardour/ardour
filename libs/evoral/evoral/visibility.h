/*
    Copyright (C) 2013 Paul Davis

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

#ifndef __libevoral_visibility_h__
#define __libevoral_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBEVORAL_DLL_IMPORT __declspec(dllimport)
  #define LIBEVORAL_DLL_EXPORT __declspec(dllexport)
  #define LIBEVORAL_DLL_LOCAL
  #define LIBEVORAL_TEMPLATE_DLL_IMPORT
  #define LIBEVORAL_TEMPLATE_DLL_EXPORT
#else
  #define LIBEVORAL_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBEVORAL_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBEVORAL_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #define LIBEVORAL_TEMPLATE_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBEVORAL_TEMPLATE_DLL_EXPORT __attribute__ ((visibility ("default")))
#endif

#ifdef LIBEVORAL_STATIC // libevoral is not a DLL
  #define LIBEVORAL_API
  #define LIBEVORAL_LOCAL
#else
  #ifdef LIBEVORAL_DLL_EXPORTS // defined if we are building the libevoral DLL (instead of using it)
    #define LIBEVORAL_API LIBEVORAL_DLL_EXPORT
    #define LIBEVORAL_TEMPLATE_API LIBEVORAL_TEMPLATE_DLL_EXPORT
  #else
    #define LIBEVORAL_API LIBEVORAL_DLL_IMPORT
    #define LIBEVORAL_TEMPLATE_API LIBEVORAL_TEMPLATE_DLL_IMPORT
  #endif 
  #define     LIBEVORAL_LOCAL LIBEVORAL_DLL_LOCAL
#endif

#endif /* __libevoral_visibility_h__ */
