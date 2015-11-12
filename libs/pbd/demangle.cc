/*
    Copyright (C) 2000-2007 Paul Davis

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

#include "pbd/demangle.h"

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif

std::string
PBD::symbol_demangle (const std::string& l)
{
#if defined(__GLIBCXX__)
	int status;

	try {

		char* realname = abi::__cxa_demangle (l.c_str(), 0, 0, &status);
		std::string d (realname);
		free (realname);
		return d;
	} catch (std::exception) {

	}
#endif

	return l;
}

std::string
PBD::demangle (std::string const & l)
{
	std::string::size_type const b = l.find_first_of ("(");

	if (b == std::string::npos) {
		return symbol_demangle (l);
	}

	std::string::size_type const p = l.find_last_of ("+");
	if (p == std::string::npos) {
		return symbol_demangle (l);
	}

	if ((p - b) <= 1) {
		return symbol_demangle (l);
	}

	std::string const fn = l.substr (b + 1, p - b - 1);

	return symbol_demangle (fn);
}
