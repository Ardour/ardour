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

#include <sstream>
#include <iomanip>

#include <glibmm.h>

#include "ardour/svn_revision.h"
#include "ardour/ardour.h"
#include "ardour/session.h"

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

BroadcastInfo::BroadcastInfo () :
  _has_info (false)
{
	info = new SF_BROADCAST_INFO;
	memset (info, 0, sizeof (*info));
	
	// Note: Set version to 1 when UMID is used, otherwise version should stay at 0
	info->version = 0;
	
	time_t rawtime;
	std::time (&rawtime);
	_time = *localtime (&rawtime);
}

BroadcastInfo::~BroadcastInfo ()
{
	delete info;
}

void
BroadcastInfo::set_from_session (Session const & session, int64_t time_ref)
{
	set_description (session.name());
	set_time_reference (time_ref);
	set_origination_time ();
	set_originator ();
	set_originator_ref ();
}

bool
BroadcastInfo::load_from_file (string const & filename)
{
	SNDFILE * file = 0;
	SF_INFO info;
	
	info.format = 0;
	
	if (!(file = sf_open (filename.c_str(), SFM_READ, &info))) {
		update_error();
		return false;
	}
	
	bool ret = load_from_file (file);
	
	sf_close (file);
	return ret;
}

bool
BroadcastInfo::load_from_file (SNDFILE* sf)
{
	if (sf_command (sf, SFC_GET_BROADCAST_INFO, info, sizeof (*info)) != SF_TRUE) {
		update_error();
		_has_info = false;
		return false;
	}
	
	_has_info = true;
	return true;
}

string
BroadcastInfo::get_description () const
{
	return info->description;
}

int64_t
BroadcastInfo::get_time_reference () const
{
	if (!_has_info) {
		return 0;
	}
	
	int64_t ret = (uint32_t) info->time_reference_high;
	ret <<= 32;
	ret |= (uint32_t) info->time_reference_low;
	return ret;
}

struct tm
BroadcastInfo::get_origination_time () const
{
	struct tm ret;
	
	string date = info->origination_date;
	ret.tm_year = atoi (date.substr (0, 4)) - 1900;
	ret.tm_mon = atoi (date.substr (5, 2));
	ret.tm_mday = atoi (date.substr (8, 2));
	
	string time = info->origination_time;
	ret.tm_hour = atoi (time.substr (0,2));
	ret.tm_min = atoi (time.substr (3,2));
	ret.tm_sec = atoi (time.substr (6,2));
	
	return ret;
}

string
BroadcastInfo::get_originator () const
{
	return info->originator;
}

string
BroadcastInfo::get_originator_ref () const
{
	return info->originator_reference;
}

bool
BroadcastInfo::write_to_file (string const & filename)
{
	SNDFILE * file = 0;
	SF_INFO info;
	
	info.format = 0;
	
	if (!(file = sf_open (filename.c_str(), SFM_RDWR, &info))) {
		update_error();
		return false;
	}
	
	bool ret = write_to_file (file);
	
	sf_close (file);
	return ret;
}

bool
BroadcastInfo::write_to_file (SNDFILE* sf)
{
	if (sf_command (sf, SFC_SET_BROADCAST_INFO, info, sizeof (*info)) != SF_TRUE) {
		update_error();
		return false;
	}
	
	return true;
}

void
BroadcastInfo::set_description (string const & desc)
{
	_has_info = true;
	
	snprintf_bounded_null_filled (info->description, sizeof (info->description), desc.c_str());
}

void
BroadcastInfo::set_time_reference (int64_t when)
{
	_has_info = true;
	
	info->time_reference_high = (when >> 32);
	info->time_reference_low = (when & 0xffffffff);
}

void
BroadcastInfo::set_origination_time (struct tm * now)
{
	_has_info = true;
	
	if (now) {
		_time = *now;
	}
	
	snprintf_bounded_null_filled (info->origination_date, sizeof (info->origination_date), "%4d-%02d-%02d",
		  _time.tm_year + 1900,
		  _time.tm_mon + 1,
		  _time.tm_mday);
	
	snprintf_bounded_null_filled (info->origination_time, sizeof (info->origination_time), "%02d:%02d:%02d",
		  _time.tm_hour,
		  _time.tm_min,
		  _time.tm_sec);
}

void
BroadcastInfo::set_originator (string const & str)
{
	_has_info = true;
	
	if (!str.empty()) {
		snprintf_bounded_null_filled (info->originator, sizeof (info->originator), str.c_str());
		return;
	}
	
	snprintf_bounded_null_filled (info->originator, sizeof (info->originator), Glib::get_real_name().c_str());
}

void
BroadcastInfo::set_originator_ref (string const & str)
{
	_has_info = true;
	
	if (!str.empty()) {
		snprintf_bounded_null_filled (info->originator_reference, sizeof (info->originator_reference), str.c_str());
		return;
	}
	
	/* random code is 9 digits */
	
	int random_code = random() % 999999999;
	
	/* Serial number is 12 chars */
	
	std::ostringstream serial_number;
	serial_number << "ARDOUR" << "r" <<  std::setfill('0') << std::right << std::setw(5) << svn_revision;
	
	snprintf_bounded_null_filled (info->originator_reference, sizeof (info->originator_reference), "%2s%3s%12s%02d%02d%02d%9d",
		  Config->get_bwf_country_code().c_str(),
		  Config->get_bwf_organization_code().c_str(),
		  serial_number.str().c_str(),
		  _time.tm_hour,
		  _time.tm_min,
		  _time.tm_sec,
		  random_code);
	
}

void
BroadcastInfo::update_error ()
{
	char errbuf[256];
	sf_error_str (0, errbuf, sizeof (errbuf) - 1);
	error = errbuf;
}

} // namespace ARDOUR

