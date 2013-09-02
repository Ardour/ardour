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

#ifndef __libardour_visibility_h__
#define __libardour_visibility_h__

#if defined _WIN32 || defined __CYGWIN__
  #define LIBARDOUR_HELPER_DLL_IMPORT __declspec(dllimport)
  #define LIBARDOUR_HELPER_DLL_EXPORT __declspec(dllexport)
  #define LIBARDOUR_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define LIBARDOUR_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define LIBARDOUR_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define LIBARDOUR_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define LIBARDOUR_HELPER_DLL_IMPORT
    #define LIBARDOUR_HELPER_DLL_EXPORT
    #define LIBARDOUR_HELPER_DLL_LOCAL
  #endif
#endif

#endif /* __libardour_visibility_h__ */
