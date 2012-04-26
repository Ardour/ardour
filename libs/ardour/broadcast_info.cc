/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include "ardour/broadcast_info.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#include <glibmm.h>

#include "ardour/svn_revision.h"
#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/session_metadata.h"

#include "pbd/convert.h"

using namespace PBD;

namespace ARDOUR
{

static void
snprintf_bounded_null_filled (char* target, size_t target_size, char const * fmt, ...)
{
	char buf[target_size+1];
	va_list ap;

	va_start (ap, fmt);
	vsnprintf (buf, target_size+1, fmt, ap);
	va_end (ap);

	memset (target, 0, target_size);
	memcpy (target, buf, target_size);

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
BroadcastInfo::set_originator_ref_from_session (Session const & session)
{
	_has_info = true;

	/* random code is 9 digits */

	int random_code = random() % 999999999;

	/* Serial number is 12 chars */

	std::ostringstream serial_number;
	serial_number << "ARDOUR" << "r" <<  std::setfill('0') << std::right << std::setw(5) << svn_revision;

	snprintf_bounded_null_filled (info->originator_reference, sizeof (info->originator_reference), "%2s%3s%12s%02d%02d%02d%9d",
		  SessionMetadata::Metadata()->country().c_str(),
		  SessionMetadata::Metadata()->organization().c_str(),
		  serial_number.str().c_str(),
		  _time.tm_hour,
		  _time.tm_min,
		  _time.tm_sec,
		  random_code);

}

} // namespace ARDOUR

