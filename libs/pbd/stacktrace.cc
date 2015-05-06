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
#include "pbd/compose.h"
#include "pbd/pthread_utils.h"

#include <cstdio>
#include <iostream>
#include <string>

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <DbgHelp.h>
#endif

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

#elif defined (PLATFORM_WINDOWS)

std::string 
PBD::demangle (std::string const & l) /* JE - !!!! 'PBD' namespace might possibly get removed (except it's still used in 'libs/canvas/item.cc') */
{
	return std::string();
}

void
PBD::stacktrace( std::ostream& out, int)
{
#ifdef DEBUG
	const size_t levels = 62; // does not support more then 62 levels of stacktrace
	unsigned int   i;
	void         * stack[ levels ];
	unsigned short frames;
	SYMBOL_INFO  * symbol;
	HANDLE         process;

	process = GetCurrentProcess();
	out << "+++++Backtrace process: " <<  pthread_self() << std::endl;

	SymInitialize( process, NULL, TRUE );

	frames               = CaptureStackBackTrace( 0, levels, stack, NULL );

	out << "+++++Backtrace frames: " <<  frames << std::endl;

	symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
	symbol->MaxNameLen   = 255;
	symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

	for( i = 0; i < frames; i++ )
	{
		SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );
		out << string_compose( "%1: %2 - %3\n", frames - i - 1, symbol->Name, symbol->Address );
	}

	out.flush();

	free( symbol );
#endif
}

void
c_stacktrace ()
{
	PBD::stacktrace (std::cout);
}

#else

std::string 
PBD::demangle (std::string const & l) /* JE - !!!! 'PBD' namespace might possibly get removed (except it's still used in 'libs/canvas/item.cc') */
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
