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

#include "audiographer/broadcast_info.h"
#include "audiographer/sndfile/sndfile_base.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdarg>
#include <cstring>
#include <inttypes.h>
#include <cstdlib>

namespace AudioGrapher
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
	: _has_info (false)
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

bool
BroadcastInfo::load_from_file (std::string const & filename)
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

std::string
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

	std::string date = info->origination_date;
	ret.tm_year = atoi (date.substr (0, 4).c_str()) - 1900;
	ret.tm_mon = atoi (date.substr (5, 2).c_str());
	ret.tm_mday = atoi (date.substr (8, 2).c_str());

	std::string time = info->origination_time;
	ret.tm_hour = atoi (time.substr (0,2).c_str());
	ret.tm_min = atoi (time.substr (3,2).c_str());
	ret.tm_sec = atoi (time.substr (6,2).c_str());

	return ret;
}

std::string
BroadcastInfo::get_originator () const
{
	return info->originator;
}

std::string
BroadcastInfo::get_originator_ref () const
{
	return info->originator_reference;
}

bool
BroadcastInfo::write_to_file (std::string const & filename)
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
	std::cerr << "AG set BWF as " << sizeof(*info) << std::endl;
	if (sf_command (sf, SFC_SET_BROADCAST_INFO, info, sizeof (*info)) != SF_TRUE) {
		update_error();
		return false;
	}

	return true;
}

bool
BroadcastInfo::write_to_file (SndfileHandle* sf)
{
	if (sf->command (SFC_SET_BROADCAST_INFO, info, sizeof (*info)) != SF_TRUE) {
		update_error ();
		return false;
	}

	return true;
}

void
BroadcastInfo::set_description (std::string const & desc)
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
BroadcastInfo::set_originator (std::string const & str)
{
	_has_info = true;

	snprintf_bounded_null_filled (info->originator, sizeof (info->originator), str.c_str());
}

void
BroadcastInfo::set_originator_ref (std::string const & str)
{
	_has_info = true;

	snprintf_bounded_null_filled (info->originator_reference, sizeof (info->originator_reference), str.c_str());
}

void
BroadcastInfo::update_error ()
{
	char errbuf[256];
	sf_error_str (0, errbuf, sizeof (errbuf) - 1);
	error = errbuf;
}

} // namespace AudioGrapher

