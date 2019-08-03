/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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
		setlocale (LC_NUMERIC, "C");
		pre_cpp_locale = std::locale();
		DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: change C locale from '%1' => 'C' (C++ locale is %2)\n", old_c_locale, pre_cpp_locale.name()));
	}
}

LocaleGuard::~LocaleGuard ()
{
	char const * current_c_locale = setlocale (LC_NUMERIC, 0);
	std::locale current_cpp_locale;

	if (current_cpp_locale != pre_cpp_locale) {

		PBD::warning << string_compose ("LocaleGuard: someone (a plugin) changed the C++ locale from\n\t%1\nto\n\t%2\n, expect non-portable session files. Decimal OK ? %3",
		                              old_cpp_locale.name(), current_cpp_locale.name(),
		                              (std::use_facet<std::numpunct<char> >(std::locale()).decimal_point() == '.'))
		           << endmsg;

		try {
			/* this resets C & C++ locales */
			std::locale::global (old_cpp_locale);
			DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: restore C & C++ locale: '%1'\n", std::locale().name()));
		} catch (...) {
			/* see comments in the constructor regarding the
			 * exception.
			 *
			 * This should restore restore numeric handling back to
			 * the default (which may reflect user
			 * preferences). This probably can't fail, because
			 * old_c_locale was already in use during the
			 * constructor for this object.
			 *
			 * Still ... Apple ... locale support ... just sayin' ....
			 */
			setlocale (LC_NUMERIC, old_c_locale);
			DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: C++ locale API failed, restore C locale from %1 to\n'%2'\n(C++ is '%3')\n", current_c_locale, old_c_locale, std::locale().name()));
		}

	}
	if (old_c_locale && (strcmp (current_c_locale, old_c_locale) != 0)) {
		/* reset only the C locale */
		setlocale (LC_NUMERIC, old_c_locale);
		DEBUG_TRACE (DEBUG::Locale, string_compose ("LG: restore C locale from %1 to\n'%2'\n(C++ is '%3')\n", current_c_locale, old_c_locale, std::locale().name()));
	}

	free (const_cast<char*> (old_c_locale));
}
