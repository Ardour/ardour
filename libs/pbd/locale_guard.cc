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

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/locale_guard.h"

using namespace PBD;

/* Neither C nor C++ pick up a user's preferred locale choice without the
 * application actively taking steps to make this happen.
 *
 * For C: setlocale (LC_ALL, "");
 * For C++ (assuming that the C version was called):
 *      std::locale::global (std::locale (setlocale (LC_ALL, 0)));
 *
 * The application needs to make these calls, probably in main().
 *
 * Setting the C++ locale will change the C locale, but not the other way 'round.
 * and some plugin may change either behind our back.
 */

LocaleGuard::LocaleGuard ()
	: old_c_locale (0)
{
	/* A LocaleGuard object ensures that the
	 * LC_NUMERIC/std::locale::numeric aspect of the C and C++ locales are
	 * set to "C" during its lifetime, so that printf/iostreams use a
	 * portable format for numeric output (i.e. 1234.5 is always 1234.5 and
	 * not sometimes 1234,5, as it would be in fr or de locales)
	 */

	char const * const current_c_locale = setlocale (LC_NUMERIC, 0);

	if (strcmp ("C", current_c_locale) != 0) {

		old_c_locale = strdup (current_c_locale);

		/* set the C++ global/default locale to whatever we are using
		 * now, but with "C" numeric handling.
		 *
		 * this also sets the C locale, so no call to setlocale() is required.
		 */

		std::locale::global (std::locale (old_cpp_locale, "C", std::locale::numeric));
		pre_cpp_locale = std::locale();

		DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: change locale from '%1' => %2\n", old_cpp_locale.name(), pre_cpp_locale.name()));
	}
}

LocaleGuard::~LocaleGuard ()
{
	char const * current_c_locale = setlocale (LC_NUMERIC, 0);
	std::locale current_cpp_locale;

	if (current_cpp_locale != pre_cpp_locale) {

		PBD::error << string_compose ("LocaleGuard: someone (a plugin) changed the C++ locale from\n\t%1\nto\n\t%2\n, expect non-portable session files. Decimal OK ? %2",
		                              old_cpp_locale.name(), current_cpp_locale.name(),
		                              (std::use_facet<std::numpunct<char> >(std::locale()).decimal_point() == '.'))
		           << endmsg;
		DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: someone (a plugin) changed the C++ locale (Decimal OK ? '%1'); expect non-portable session files.\n",
		                                            (std::use_facet<std::numpunct<char> >(std::locale()).decimal_point() == '.')));

		/* this resets C & C++ locales */
		std::locale::global (old_cpp_locale);
		DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: restore C & C++ locale: '%1'\n", std::locale().name()));

	} else if (old_c_locale && (strcmp (current_c_locale, old_c_locale) != 0)) {

		/* reset only the C locale */
		setlocale (LC_NUMERIC, old_c_locale);
		DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: restore C locale from %1 to\n'%2'\n(C++ is '%3')\n", current_c_locale, old_c_locale, std::locale().name()));
	}

	free (const_cast<char*> (old_c_locale));
}
