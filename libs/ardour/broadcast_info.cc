/*
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/broadcast_info.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>

#include <glibmm.h>

#include "ardour/revision.h"
#include "ardour/session.h"
#include "ardour/session_metadata.h"

using namespace PBD;

namespace ARDOUR
{

static void
snprintf_bounded_null_filled (char* target, size_t target_size, char const * fmt, ...)
{
	std::vector<char> buf(target_size+1);
	va_list ap;

	va_start (ap, fmt);
	vsnprintf (&buf[0], target_size+1, fmt, ap);
	va_end (ap);

	memset (target, 0, target_size);
	memcpy (target, &buf[0], target_size);

}

BroadcastInfo::BroadcastInfo ()
{

}

void
BroadcastInfo::set_from_session (Session const & session, int64_t time_ref)
{
	set_description (session.name());
	set_time_reference (time_ref);
	set_origination_time ();
	set_originator ();
	set_originator_ref_from_session (session);
}

void
BroadcastInfo::set_originator (std::string const & str)
{
	_has_info = true;

	if (!str.empty()) {
		AudioGrapher::BroadcastInfo::set_originator (str);
		return;
	}

	snprintf_bounded_null_filled (info->originator, sizeof (info->originator), Glib::get_real_name().c_str());
}

void
BroadcastInfo::set_originator_ref_from_session (Session const & /*session*/)
{
	_has_info = true;

	/* random code is 9 digits */

	int random_code = g_random_int() % 999999999;

	/* Serial number is 12 chars */

	std::ostringstream serial_number;
	serial_number << PROGRAM_NAME << revision;

	std::string country = SessionMetadata::Metadata()->country().substr (0, 2).c_str(); // ISO 3166-1 2 digits
	if (country.empty ()) {
		/* "ZZ" is reserved and may be user-assigned.
		 * EBU Tech 3279 chapter 4.2.4 recommends "ZZ" for unregistered organizations
		 */
		country = "ZZ";
	}

	/* https://tech.ebu.ch/docs/tech/tech3279.pdf */
	std::string organization = SessionMetadata::Metadata()->organization().substr (0, 3).c_str(); // EBU assigned Organisation code
	if (organization.empty ()) {
		organization = "---";
	}

	// TODO sanitize to allowed char set: tech3279.pdf chapter 1.6
	// allowed: A-Z 0-9 <space> .,-()/=
	// possible, but not recommended: !"%&*;<>

	/* https://tech.ebu.ch/docs/r/r099.pdf
	 * CC Country code: (2 characters) based on the ISO 3166-1 standard
	 * OOO Organisation code: (3 characters) based on the EBU facility codes, Tech 3279
	 * NNNNNNNNNNNN Serial number: (12 characters extracted from the recorder model and serial number) This should identify the machine’s type and serial number.
	 * HHMMSS OriginationTime (6 characters,) from the <OriginationTime> field of the BWF.
	 * RRRRRRRRR Random Number (9 characters 0-9) Generated locally by the recorder using some reasonably random algorithm.
	 */
	snprintf_bounded_null_filled (info->originator_reference, sizeof (info->originator_reference), "%2s%3s%12s%02d%02d%02d%09d",
			country.c_str (), organization.c_str(),
			serial_number.str().substr (0, 12).c_str(),
			_time.tm_hour,
			_time.tm_min,
			_time.tm_sec,
			random_code);

}

} // namespace ARDOUR

