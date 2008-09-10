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

#include <pbd/stacktrace.h>
#include <iostream>

void
PBD::trace_twb ()
{
}

/* Obtain a backtrace and print it to stdout. */

#ifdef HAVE_EXECINFO

#include <execinfo.h>
#include <cstdlib>

void
PBD::stacktrace (std::ostream& out, int levels)
{
	void *array[200];
	size_t size;
	char **strings;
	size_t i;
     
	size = backtrace (array, 200);
	strings = backtrace_symbols (array, size);
     
	if (strings) {

		printf ("Obtained %zd stack frames.\n", size);
		
		for (i = 0; i < size && (levels == 0 || i < size_t(levels)); i++) {
			out << strings[i] << std::endl;
		}
		
		free (strings);
	}
}

#else

void
PBD::stacktrace (std::ostream& out, int levels)
{
	out << "stack tracing is not enabled on this platform" << std::endl;
}

void
c_stacktrace ()
{
	PBD::stacktrace (std::cout);
}

#endif /* HAVE_EXECINFO */
