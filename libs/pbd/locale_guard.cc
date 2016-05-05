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
#include <assert.h>
#include <locale.h>

#include "pbd/locale_guard.h"
#include "pbd/error.h"

using namespace PBD;

/* The initial C++ locale is "C" regardless of the user's preferred locale.
 * The C locale from setlocale() matches the user's preferred locale
 *
 * Setting the C++ locale will change the C locale, but not the other way 'round.
 * and some plugin may change either behind our back.
 */

LocaleGuard::LocaleGuard (const char*)
	: old_c (0)
{
	init ();
}

LocaleGuard::LocaleGuard ()
	: old_c (0)
{
	init ();
}

void
LocaleGuard::init ()
{
	char* actual = setlocale (LC_NUMERIC, NULL);
	if (strcmp ("C", actual)) {
		/* purpose of LocaleGuard is to make sure we're using "C" for
		   the numeric locale during its lifetime, so make it so.
		*/
		old_c = strdup (actual);
		/* this changes both C++ and C locale */
		std::locale::global (std::locale (std::locale::classic(), "C", std::locale::numeric));
	}
	if (old_cpp != std::locale::classic ()) {
		PBD::error << "LocaleGuard: initial C++ locale is not 'C'. Expect non-portable session files.\n";
	}
}

LocaleGuard::~LocaleGuard ()
{
	char* actual = setlocale (LC_NUMERIC, NULL);
	std::locale current;

	if (current != old_cpp) {
		/* the C++ locale should always be "C", that's the default
		 * at application start, and ardour never changes it to
		 * anything but "C".
		 *
		 * if it's not: some plugin meddled with it.
		 */
		if (old_cpp != std::locale::classic ()) {
			PBD::error << "LocaleGuard: someone (a plugin) changed the C++ locale, expect non-portable session files.\n";
		}
		std::locale::global (old_cpp);
	}
	if (old_c && strcmp (old_c, actual)) {
		setlocale (LC_NUMERIC, old_c);
	}
	free (old_c);
}
