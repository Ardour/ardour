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
#include "pbd/debug.h"
#include "pbd/demangle.h"
#include "pbd/compose.h"
#include "pbd/pthread_utils.h"

#include <cstdio>
#include <iostream>
#include <string>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <dbghelp.h>
#endif

void
PBD::trace_twb ()
{
}

/* Obtain a backtrace and print it to stdout. */

#ifdef HAVE_EXECINFO

#include <execinfo.h>

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

#if defined DEBUG && !defined CaptureStackBackTrace
#define CaptureStackBackTrace RtlCaptureStackBackTrace

extern "C" {
    __declspec(dllimport) USHORT WINAPI CaptureStackBackTrace (
                                 ULONG  SamplesToSkip,
                                 ULONG  SamplesToCapture,
                                 PVOID  *BackTrace,
                                 PULONG BackTraceHash
	                      );
}
#endif

void
PBD::stacktrace( std::ostream& out, int)
{
#ifdef DEBUG
	const size_t levels = 62; // does not support more then 62 levels of stacktrace
	unsigned int   i;
	void         * stack[ levels ];
	unsigned short samples;
	SYMBOL_INFO  * symbol;
	HANDLE         process;

	process = GetCurrentProcess();
	out << "+++++Backtrace process: " <<  DEBUG_THREAD_SELF << std::endl;

	SymInitialize( process, NULL, TRUE );

	samples               = CaptureStackBackTrace( 0, levels, stack, NULL );

	out << "+++++Backtrace samples: " <<  samples << std::endl;

	symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
	symbol->MaxNameLen   = 255;
	symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

	for( i = 0; i < samples; i++ )
	{
		SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );
		out << string_compose( "%1: %2 - %3\n", samples - i - 1, symbol->Name, symbol->Address );
	}

	out.flush();

	free( symbol );
#endif
}

#else

void
PBD::stacktrace (std::ostream& out, int /*levels*/)
{
	out << "stack tracing is not enabled on this platform" << std::endl;
}

#endif

void
c_stacktrace ()
{
	PBD::stacktrace (std::cout);
}
