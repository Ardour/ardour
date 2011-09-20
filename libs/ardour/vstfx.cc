#include <stdio.h>
#include <stdarg.h>

#include <ardour/vstfx.h>

/***********************************************************/
/* VSTFX - A set of modules for managing linux VST plugins */
/* vstfx.cc, vstfxwin.cc and vstfxinfofile.cc              */
/***********************************************************/


/*Simple error handler stuff for VSTFX*/

void vstfx_error (const char *fmt, ...)
{
	va_list ap;
	char buffer[512];

	va_start (ap, fmt);
	vsnprintf (buffer, sizeof(buffer), fmt, ap);
	vstfx_error_callback (buffer);
	va_end (ap);
}

/*default error handler callback*/

void default_vstfx_error_callback (const char *desc)
{
	fprintf(stderr, "%s\n", desc);
}

void (*vstfx_error_callback)(const char *desc) = &default_vstfx_error_callback;
