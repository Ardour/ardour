#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "pbd/locale_guard.h"

using namespace PBD;

LocaleGuard::LocaleGuard (const char* str)
{
	old = setlocale (LC_NUMERIC, NULL);

        if (old) {
                old = strdup (old);
                if (strcmp (old, str)) {
                        setlocale (LC_NUMERIC, str);
                }
        }
}

LocaleGuard::~LocaleGuard ()
{
	setlocale (LC_NUMERIC, old);

        if (old) {
                free (const_cast<char*>(old));
        }
}

