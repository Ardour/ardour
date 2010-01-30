#include <cstring>
#include <locale.h>
#include <stdlib.h>

#include "pbd/localeguard.h"

PBD::LocaleGuard::LocaleGuard (const char* str)
{
	old = strdup (setlocale (LC_NUMERIC, NULL));
	if (strcmp (old, str)) {
		setlocale (LC_NUMERIC, str);
	} 
}

PBD::LocaleGuard::~LocaleGuard ()
{
	setlocale (LC_NUMERIC, old);
	free ((char*)old);
}


