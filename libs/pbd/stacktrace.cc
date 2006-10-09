#include <pbd/stacktrace.h>
#include <iostream>

/* Obtain a backtrace and print it to stdout. */

#ifdef HAVE_EXECINFO

#include <execinfo.h>
#include <stdlib.h>

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
		
		for (i = 0; i < size && (levels == 0 || i < levels); i++) {
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

#endif /* HAVE_EXECINFO */
