#include <cstring>
#include <locale.h>
#include <stdlib.h>

#include "pbd/localeguard.h"

// JE - added temporarily, to reduce the delay effects when calling
// setlocale() recursively in a Windows GUI thread (we should think
// about moving the caller(s) into a dedicated worker thread).
std::string PBD::LocaleGuard::current;

PBD::LocaleGuard::LocaleGuard (const char* str)
 : old(0)
{
	if (current != str) {
		old = strdup (setlocale (LC_NUMERIC, NULL));
		if (strcmp (old, str)) {
			if (setlocale (LC_NUMERIC, str))
				current = str; 
		}
	}
}

PBD::LocaleGuard::~LocaleGuard ()
{
	if (old) {
		if (setlocale (LC_NUMERIC, old))
			current = old;

		free ((char*)old);
	}
}


