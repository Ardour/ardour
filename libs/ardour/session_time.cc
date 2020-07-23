/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2013 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2016 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <cmath>
#include <unistd.h>

#include "ardour/timestamps.h"

#include "pbd/error.h"
#include "pbd/enumwriter.h"

#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/transport_fsm.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#define TFSM_EVENT(evtype) { _transport_fsm->enqueue (new TransportFSM::Event (evtype)); }

/* BBT TIME*/

void
Session::bbt_time (samplepos_t when, Temporal::BBT_Time& bbt)
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
	bool slaved = synced_to_engine();
	int ready = true;

	// cerr << "SYNC state = " << enum_2_string (state) << endl;

	switch (state) {
	case TransportStopped:
		if (slaved && (_transport_sample != pos) && !locate_pending()) {
			/* we need to locate. This will be picked up in
			 * Session::follow_transport_master and the locate will
			 * be initiated there.
			*/
			// cerr << "SYNC: stopped, need locate to " << pos << " from " << _transport_sample << endl;
			ready = false;
		} else {
			// cerr << "SYNC: stopped, nothing to do" << endl;
		}
		break;

	case TransportStarting:
		if (slaved) {
			/* JACK is stopped (though starting). Our position
			 * should be a buffer-size-rounded
			 * worst_latency_preroll() ahead of JACK.
			 */
			const samplepos_t matching = pos + worst_latency_preroll_buffer_size_ceil ();

			ready = (_transport_sample == matching) && !locate_pending() && !declick_in_progress() && (remaining_latency_preroll() == 0);
			DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK Transport: ts %1 = %2 lp = %3 dip = %4 rlp = %5 RES: %6\n", _transport_sample, pos, locate_pending(), declick_in_progress(), remaining_latency_preroll(), ready));
		} else {
			/* we're not participating, so just say we are in sync
			   to stop interfering with other components of the engine
			   transport (JACK) system.
			*/
		}
		break;

	case TransportRolling:
		break;

	default:
		error << string_compose (_("Unknown transport state %1 in sync callback"), state) << endmsg;
	}

	// cerr << "SYNC, ready ? " << ready << endl;
	return ready;
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
