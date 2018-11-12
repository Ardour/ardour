
/*
  Copyright (C) 1999-2002 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <cmath>
#include <unistd.h>

#include "ardour/timestamps.h"

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/stacktrace.h"

#include "ardour/session.h"
#include "ardour/tempo.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* BBT TIME*/

void
Session::bbt_time (samplepos_t when, Timecode::BBT_Time& bbt)
{
	bbt = _tempo_map->bbt_at_sample (when);
}

/* Timecode TIME */

double
Session::timecode_frames_per_second() const
{
	return Timecode::timecode_to_frames_per_second (config.get_timecode_format());
}

bool
Session::timecode_drop_frames() const
{
	return Timecode::timecode_has_drop_frames(config.get_timecode_format());
}

void
Session::sync_time_vars ()
{
	_current_sample_rate = (samplecnt_t) round (_nominal_sample_rate * (1.0 + (config.get_video_pullup()/100.0)));
	_samples_per_timecode_frame = (double) _current_sample_rate / (double) timecode_frames_per_second();
	if (timecode_drop_frames()) {
	  _frames_per_hour = (int32_t)(107892 * _samples_per_timecode_frame);
	} else {
	  _frames_per_hour = (int32_t)(3600 * rint(timecode_frames_per_second()) * _samples_per_timecode_frame);
	}
	_timecode_frames_per_hour = rint(timecode_frames_per_second() * 3600.0);

	last_timecode_valid = false;
	// timecode type bits are the middle two in the upper nibble
	switch ((int) ceil (timecode_frames_per_second())) {
	case 24:
		mtc_timecode_bits = 0;
		break;

	case 25:
		mtc_timecode_bits = 0x20;
		break;

	case 30:
	default:
		if (timecode_drop_frames()) {
			mtc_timecode_bits = 0x40;
		} else {
			mtc_timecode_bits =  0x60;
		}
		break;
	};
	ltc_tx_parse_offset();
}

void
Session::timecode_to_sample( Timecode::Time& timecode, samplepos_t& sample, bool use_offset, bool use_subframes ) const
{
	timecode.rate = timecode_frames_per_second();

	Timecode::timecode_to_sample(
		timecode, sample, use_offset, use_subframes,
		_current_sample_rate,
		config.get_subframes_per_frame(),
		config.get_timecode_offset_negative(), config.get_timecode_offset()
		);

}

void
Session::sample_to_timecode (samplepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes ) const
{
	Timecode::sample_to_timecode (
		sample, timecode, use_offset, use_subframes,

		timecode_frames_per_second(),
		timecode_drop_frames(),
		double(_current_sample_rate),

		config.get_subframes_per_frame(),
		config.get_timecode_offset_negative(), config.get_timecode_offset()
		);
}

void
Session::timecode_time (samplepos_t when, Timecode::Time& timecode)
{
	if (last_timecode_valid && when == last_timecode_when) {
		timecode = last_timecode;
		return;
	}

	this->sample_to_timecode( when, timecode, true /* use_offset */, false /* use_subframes */ );

	last_timecode_when = when;
	last_timecode = timecode;
	last_timecode_valid = true;
}

void
Session::timecode_time_subframes (samplepos_t when, Timecode::Time& timecode)
{
	if (last_timecode_valid && when == last_timecode_when) {
		timecode = last_timecode;
		return;
	}

	this->sample_to_timecode( when, timecode, true /* use_offset */, true /* use_subframes */ );

	last_timecode_when = when;
	last_timecode = timecode;
	last_timecode_valid = true;
}

void
Session::timecode_duration (samplecnt_t when, Timecode::Time& timecode) const
{
	this->sample_to_timecode( when, timecode, false /* use_offset */, true /* use_subframes */ );
}

void
Session::timecode_duration_string (char* buf, size_t len, samplepos_t when) const
{
	Timecode::Time timecode;

	timecode_duration (when, timecode);
	snprintf (buf, len, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
}

void
Session::timecode_time (Timecode::Time &t)

{
	timecode_time (_transport_sample, t);
}

int
Session::backend_sync_callback (TransportState state, samplepos_t pos)
{
	bool slave = synced_to_engine();
	// cerr << "Session::backend_sync_callback() _transport_sample: " << _transport_sample << " pos: " << pos << " audible_sample: " << audible_sample() << endl;

	if (slave) {
		// cerr << "Session::backend_sync_callback() emitting Located()" << endl;
		Located (); /* EMIT SIGNAL */
	}

	switch (state) {
	case TransportStopped:
		if (slave && _transport_sample != pos && post_transport_work() == 0) {
			request_locate (pos, false);
			// cerr << "SYNC: stopped, locate to " << pos << " from " << _transport_sample << endl;
			return false;
		} else {
			// cerr << "SYNC: stopped, nothing to do" << endl;
			return true;
		}

	case TransportStarting:
		// cerr << "SYNC: starting @ " << pos << " a@ " << _transport_sample << " our work = " <<  post_transport_work() << " pos matches ? " << (_transport_sample == pos) << endl;
		if (slave) {
			return _transport_sample == pos && post_transport_work() == 0;
		} else {
			return true;
		}
		break;

	case TransportRolling:
		// cerr << "SYNC: rolling slave = " << slave << endl;
		if (slave) {
			start_transport ();
		}
		break;

	default:
		error << string_compose (_("Unknown transport state %1 in sync callback"), state)
		      << endmsg;
	}

	return true;
}


ARDOUR::samplecnt_t
Session::convert_to_samples (AnyTime const & position)
{
	double secs;

	switch (position.type) {
	case AnyTime::BBT:
		return _tempo_map->sample_at_bbt (position.bbt);
		break;

	case AnyTime::Timecode:
		/* XXX need to handle negative values */
		secs = position.timecode.hours * 60 * 60;
		secs += position.timecode.minutes * 60;
		secs += position.timecode.seconds;
		secs += position.timecode.frames / timecode_frames_per_second();
		if (config.get_timecode_offset_negative()) {
			return (samplecnt_t) floor (secs * sample_rate()) - config.get_timecode_offset();
		} else {
			return (samplecnt_t) floor (secs * sample_rate()) + config.get_timecode_offset();
		}
		break;

	case AnyTime::Seconds:
		return (samplecnt_t) floor (position.seconds * sample_rate());
		break;

	case AnyTime::Samples:
		return position.samples;
		break;
	}

	return position.samples;
}

ARDOUR::samplecnt_t
Session::any_duration_to_samples (samplepos_t position, AnyTime const & duration)
{
	double secs;

	switch (duration.type) {
	case AnyTime::BBT:
		return (samplecnt_t) ( _tempo_map->samplepos_plus_bbt (position, duration.bbt) - position);
		break;

	case AnyTime::Timecode:
		/* XXX need to handle negative values */
		secs = duration.timecode.hours * 60 * 60;
		secs += duration.timecode.minutes * 60;
		secs += duration.timecode.seconds;
		secs += duration.timecode.frames / timecode_frames_per_second();
		if (config.get_timecode_offset_negative()) {
			return (samplecnt_t) floor (secs * sample_rate()) - config.get_timecode_offset();
		} else {
			return (samplecnt_t) floor (secs * sample_rate()) + config.get_timecode_offset();
		}
		break;

	case AnyTime::Seconds:
                return (samplecnt_t) floor (duration.seconds * sample_rate());
		break;

	case AnyTime::Samples:
		return duration.samples;
		break;
	}

	return duration.samples;
}
