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

#ifndef __libaudiographer_visibility_h__
#define __libaudiographer_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBAUDIOGRAPHER_DLL_IMPORT __declspec(dllimport)
  #define LIBAUDIOGRAPHER_DLL_EXPORT __declspec(dllexport)
  #define LIBAUDIOGRAPHER_DLL_LOCAL
#else
  #define LIBAUDIOGRAPHER_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBAUDIOGRAPHER_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBAUDIOGRAPHER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBAUDIOGRAPHER_STATIC // libaudiographer is not a DLL
  #define LIBAUDIOGRAPHER_API
  #define LIBAUDIOGRAPHER_LOCAL
#else
  #ifdef LIBAUDIOGRAPHER_DLL_EXPORTS // defined if we are building the libaudiographer DLL (instead of using it)
    #define LIBAUDIOGRAPHER_API LIBAUDIOGRAPHER_DLL_EXPORT
  #else
    #define LIBAUDIOGRAPHER_API LIBAUDIOGRAPHER_DLL_IMPORT
  #endif 
  #define LIBAUDIOGRAPHER_LOCAL LIBAUDIOGRAPHER_DLL_LOCAL
#endif

#endif /* __libaudiographer_visibility_h__ */
