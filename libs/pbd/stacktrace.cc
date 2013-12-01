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

#include "libpbd-config.h"

#include "pbd/stacktrace.h"
#include <cstdio>
#include <iostream>
#include <string>

void
PBD::trace_twb ()
{
}

/* Obtain a backtrace and print it to stdout. */

#ifdef HAVE_EXECINFO

#include <execinfo.h>
#include <cxxabi.h>

static std::string 
symbol_demangle (const std::string& l)
{
	int status;

	try {
		
		char* realname = abi::__cxa_demangle (l.c_str(), 0, 0, &status);
		std::string d (realname);
		free (realname);
		return d;
	} catch (std::exception) {
		
	}

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

void
PBD::stacktrace (std::ostream& out, int levels)
{
	void *array[200];
	size_t size;
	char **strings;
	size_t i;
     
	size = backtrace (array, 200);

	if (size) {
		strings = backtrace_symbols (array, size);
     
		if (strings) {
			
			for (i = 0; i < size && (levels == 0 || i < size_t(levels)); i++) {
				out << "  " << demangle (strings[i]) << std::endl;
			}
			
			free (strings);
		}
	} else {
		out << "no stacktrace available!" << std::endl;
	}
}

#else

std::string 
/* JE - !!!! 'PBD' namespace might possibly get removed (except it's still used in 'libs/canvas/item.cc') */PBD::demangle (std::string const & l)
{
	return std::string();
}

void
PBD::stacktrace (std::ostream& out, int /*levels*/)
{
	out << "stack tracing is not enabled on this platform" << std::endl;
}

void
c_stacktrace ()
{
	PBD::stacktrace (std::cout);
}

#endif /* HAVE_EXECINFO */
