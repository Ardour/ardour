/*
 * Copyright (C) 2000-2007 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/demangle.h"

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif

std::string
PBD::demangle_symbol (const std::string& mangled_symbol)
{
#if defined(__GLIBCXX__)

	try {
		int status;
		char* realname = abi::__cxa_demangle (mangled_symbol.c_str(), 0, 0, &status);
		std::string demangled_symbol (realname);
		free (realname);
		return demangled_symbol;
	} catch (...) {
		/* may happen if realname == NULL */
	}
#endif

	/* Note: on win32, you can use UnDecorateSymbolName.
	   See http://msdn.microsoft.com/en-us/library/ms681400%28VS.85%29.aspx
	   See also: http://msdn.microsoft.com/en-us/library/ms680344%28VS.85%29.aspx
	*/

	return mangled_symbol;
}

std::string
PBD::demangle (std::string const& str)
{
	std::string::size_type const b = str.find_first_of ("(");

	if (b == std::string::npos) {
		return demangle_symbol (str);
	}

	std::string::size_type const p = str.find_last_of ("+");
	if (p == std::string::npos) {
		return demangle_symbol (str);
	}

	if ((p - b) <= 1) {
		return demangle_symbol (str);
	}

	std::string const symbol = str.substr (b + 1, p - b - 1);

	return demangle_symbol (symbol);
}
