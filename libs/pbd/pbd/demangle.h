/*
    Copyright (C) 2009 Paul Davis
    Author: Sakari Bergen

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

#ifndef __libpbd_demangle_h__
#define __libpbd_demangle_h__

#include <string>
#include <cstdlib>
#include <typeinfo>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

#include "pbd/libpbd_visibility.h"

namespace PBD
{
	template<typename T> LIBPBD_API 
	std::string demangled_name (T const & obj)
	{
#ifdef __GNUC__
		int status;
		char * res = abi::__cxa_demangle (typeid(obj).name(), 0, 0, &status);
		if (status == 0) {
			std::string s(res);
			free (res);
			return s;
		}
#endif

                /* Note: on win32, you can use UnDecorateSymbolName.
                   See http://msdn.microsoft.com/en-us/library/ms681400%28VS.85%29.aspx
                   See also: http://msdn.microsoft.com/en-us/library/ms680344%28VS.85%29.aspx
                */
                
		return typeid(obj).name();
	}
} // namespace

#endif // __libpbd_demangle_h__
