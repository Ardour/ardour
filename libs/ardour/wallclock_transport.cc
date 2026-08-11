/*
 * Copyright (C) 2026 Robin Gareus <robin@gareus.org>
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

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/transport_master.h"

using namespace ARDOUR;
using namespace PBD;

WallClock_TransportMaster::WallClock_TransportMaster (std::string const& name)
	: TimecodeTransportMaster (name, WallClock)
	, _set (false)
{
}

WallClock_TransportMaster::~WallClock_TransportMaster ()
{
}

void
WallClock_TransportMaster::init ()
{
	reset (true);
}

void
WallClock_TransportMaster::check_backend ()
{
	reset (true);
}

void
WallClock_TransportMaster::reset (bool)
{
	_set = false;
}

void
WallClock_TransportMaster::set_session (Session* s)
{
  TransportMaster::set_session (s);
	if (s) {
		_connected = true;
	} else {
		_connected = false;
	}
}

void
WallClock_TransportMaster::pre_process (pframes_t nframes, samplepos_t now, std::optional<samplepos_t>)
{
	if (!_set) {
		_set = true;

		int64_t   gnow;
		time_t    xnow;
		struct tm tm_now;

		gnow = g_get_real_time ();
		xnow = gnow / 1000000;

		localtime_r (&xnow, &tm_now);

		samplecnt_t sample_rate = _session->nominal_sample_rate ();

		_time = tm_now.tm_hour * (60 * 60 * sample_rate);
		_time += tm_now.tm_min * (60 * sample_rate);
		_time += tm_now.tm_sec * sample_rate;
		_time += (gnow % 1000000) /  1000000.0 * sample_rate;
	} else {
		_time += nframes;
	}

	current.update (_time, now, 1.0);
}

samplecnt_t
WallClock_TransportMaster::update_interval () const
{
	return AudioEngine::instance()->samples_per_cycle();
}

samplecnt_t
WallClock_TransportMaster::resolution () const
{
	return AudioEngine::instance()->samples_per_cycle();
}

Timecode::TimecodeFormat
WallClock_TransportMaster::apparent_timecode_format () const
{
	if (_session) {
		return _session->config.get_timecode_format();
	} else {
		return Timecode::timecode_30;
	}
}

std::string
WallClock_TransportMaster::position_string () const
{
	SafeTime last;
	current.safe_read (last);
	if (last.timestamp == 0) {
		return " --:--:--:--";
	}
	Timecode::Time tc;
	_session->sample_to_timecode (last.position, tc, true, false);
	return Timecode::timecode_format_time(tc);
}
