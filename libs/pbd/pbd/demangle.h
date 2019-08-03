/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __libpbd_demangle_h__
#define __libpbd_demangle_h__

#include <string>
#include <cstdlib>
#include <typeinfo>

#include "pbd/libpbd_visibility.h"

namespace PBD
{

/**
 * @param symbol a mangled symbol/name
 * @return a demangled symbol/name
 */
LIBPBD_API std::string demangle_symbol(const std::string& symbol);

/**
 * @param str a string containing a mangled symbol/name
 * @return a string with the mangled symbol/name replaced with a demangled
 * name
 */
LIBPBD_API std::string demangle(const std::string& str);

template <typename T>
std::string demangled_name(T const& obj)
{
	return demangle_symbol(typeid(obj).name());
}

} // namespace

#endif // __libpbd_demangle_h__
