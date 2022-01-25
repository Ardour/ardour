/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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
PBD::stacktrace (std::ostream& out, int levels, int start)
{
	void *array[200];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 200);

	if (size && size >= start) {
		if (start == 0) {
			out << "-- Stacktrace Thread: " << pthread_name () << std::endl;
		}
		strings = backtrace_symbols (array, size);

		if (strings) {

			for (i = start; i < size && (levels == 0 || i < size_t(levels)); i++) {
				out << "  " << demangle (strings[i]) << std::endl;
			}

			free (strings);
		}
	} else {
		out << "No stacktrace available!" << std::endl;
	}
}

#elif defined PLATFORM_WINDOWS

#if !defined CaptureStackBackTrace
#define CaptureStackBackTrace RtlCaptureStackBackTrace

extern "C" {
	__declspec(dllimport) USHORT WINAPI CaptureStackBackTrace (
	                             ULONG  FramesToSkip,
	                             ULONG  FramesToCapture,
	                             PVOID  *BackTrace,
	                             PULONG BackTraceHash);
}
#endif

void
PBD::stacktrace (std::ostream& out, int levels, int start)
{
	void*          stack[62]; // does not support more then 62 levels of stacktrace
	unsigned short frames;
	SYMBOL_INFO*   symbol;
	HANDLE         process;

	process = GetCurrentProcess();
	out << string_compose ("Backtrace thread: %1", DEBUG_THREAD_SELF) << std::endl;

	SymInitialize (process, NULL, TRUE);

	frames = CaptureStackBackTrace (0, 62, stack, NULL);

	out << "Backtrace frames: " << (int) frames << std::endl;

	symbol               = (SYMBOL_INFO*)calloc (sizeof (SYMBOL_INFO) + 256 * sizeof (char), 1);
	symbol->MaxNameLen   = 255;
	symbol->SizeOfStruct = sizeof (SYMBOL_INFO);

	for (int i = start; i < frames && (levels == 0 || i < levels); ++i) {
		SymFromAddr (process, (DWORD64)(stack[i]), 0, symbol);
		out << string_compose (" %1: %2 - %3\n", frames - i - 1, symbol->Name, symbol->Address);
	}

	out.flush ();

	free (symbol);
}

#else

void
PBD::stacktrace (std::ostream& out, int, int)
{
	out << "stack tracing is not enabled on this platform" << std::endl;
}

#endif
