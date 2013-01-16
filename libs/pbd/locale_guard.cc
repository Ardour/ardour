/*
    Copyright (C) 2012 Paul Davis 

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

