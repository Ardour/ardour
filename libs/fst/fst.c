#include <stdio.h>
#include <stdarg.h>

#include "fst.h"


void 
default_fst_error_callback (const char *desc)
{
	fprintf(stderr, "%s\n", desc);
}

void (*fst_error_callback)(const char *desc) = &default_fst_error_callback;

void 
fst_error (const char *fmt, ...)
{
	va_list ap;
	char buffer[512];

	va_start (ap, fmt);
	vsnprintf (buffer, sizeof(buffer), fmt, ap);
	fst_error_callback (buffer);
	va_end (ap);
}


