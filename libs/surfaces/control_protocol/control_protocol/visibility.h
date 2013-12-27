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

#ifdef LIBCONTROLCP_STATIC // libcontrolcp is not a DLL
  #define LIBCONTROLCP_API
  #define LIBCONTROLCP_LOCAL
#else
  #ifdef LIBCONTROLCP_DLL_EXPORTS // defined if we are building the libcontrolcp DLL (instead of using it)
    #define LIBCONTROLCP_API LIBCONTROLCP_DLL_EXPORT
  #else
    #define LIBCONTROLCP_API LIBCONTROLCP_DLL_IMPORT
  #endif 
  #define LIBCONTROLCP_LOCAL LIBCONTROLCP_DLL_LOCAL
#endif

#endif /* __libcontrolcp_visibility_h__ */
