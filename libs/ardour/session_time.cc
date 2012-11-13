
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

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* BBT TIME*/

void
Session::bbt_time (framepos_t when, Timecode::BBT_Time& bbt)
{
	_tempo_map->bbt_time (when, bbt);
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
	_current_frame_rate = (framecnt_t) round (_base_frame_rate * (1.0 + (config.get_video_pullup()/100.0)));
	_frames_per_timecode_frame = (double) _current_frame_rate / (double) timecode_frames_per_second();
	if (timecode_drop_frames()) {
	  _frames_per_hour = (int32_t)(107892 * _frames_per_timecode_frame);
	} else {
	  _frames_per_hour = (int32_t)(3600 * rint(timecode_frames_per_second()) * _frames_per_timecode_frame);
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
}

void
Session::timecode_to_sample( Timecode::Time& timecode, framepos_t& sample, bool use_offset, bool use_subframes ) const
{
	timecode.rate = timecode_frames_per_second();

	Timecode::timecode_to_sample(
		timecode, sample, use_offset, use_subframes,
		_current_frame_rate,
		config.get_subframes_per_frame(),
		config.get_timecode_offset_negative(), config.get_timecode_offset()
		);

}

void
Session::sample_to_timecode (framepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes ) const
{
	Timecode::sample_to_timecode (
		sample, timecode, use_offset, use_subframes,

		timecode_frames_per_second(),
		timecode_drop_frames(),
		double(_current_frame_rate),

		config.get_subframes_per_frame(),
		config.get_timecode_offset_negative(), config.get_timecode_offset()
		);
}

void
Session::timecode_time (framepos_t when, Timecode::Time& timecode)
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
Session::timecode_time_subframes (framepos_t when, Timecode::Time& timecode)
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
Session::timecode_duration (framecnt_t when, Timecode::Time& timecode) const
{
	this->sample_to_timecode( when, timecode, false /* use_offset */, true /* use_subframes */ );
}

void
Session::timecode_duration_string (char* buf, framepos_t when) const
{
	Timecode::Time timecode;

	timecode_duration (when, timecode);
	snprintf (buf, sizeof (buf), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
}

void
Session::timecode_time (Timecode::Time &t)

{
	timecode_time (_transport_frame, t);
}

int
Session::jack_sync_callback (jack_transport_state_t state,
			     jack_position_t* pos)
{
	bool slave = synced_to_jack();

	switch (state) {
	case JackTransportStopped:
		if (slave && _transport_frame != pos->frame && post_transport_work() == 0) {
			request_locate (pos->frame, false);
			// cerr << "SYNC: stopped, locate to " << pos->frame << " from " << _transport_frame << endl;
			return false;
		} else {
			return true;
		}

	case JackTransportStarting:
		// cerr << "SYNC: starting @ " << pos->frame << " a@ " << _transport_frame << " our work = " <<  post_transport_work() << " pos matches ? " << (_transport_frame == pos->frame) << endl;
		if (slave) {
			return _transport_frame == pos->frame && post_transport_work() == 0;
		} else {
			return true;
		}
		break;

	case JackTransportRolling:
		// cerr << "SYNC: rolling slave = " << slave << endl;
		if (slave) {
			start_transport ();
		}
		break;

	default:
		error << string_compose (_("Unknown JACK transport state %1 in sync callback"), state)
		      << endmsg;
	}

	return true;
}

void
Session::jack_timebase_callback (jack_transport_state_t /*state*/,
				 pframes_t /*nframes*/,
				 jack_position_t* pos,
				 int /*new_position*/)
{
	Timecode::BBT_Time bbt;

	if (pos->frame != _transport_frame) {
		cerr << "ARDOUR says " << _transport_frame << " JACK says " << pos->frame << endl;
	}

	/* BBT info */

	if (_tempo_map) {

		TempoMetric metric (_tempo_map->metric_at (_transport_frame));

		try {
			_tempo_map->bbt_time_rt (_transport_frame, bbt);

			pos->bar = bbt.bars;
			pos->beat = bbt.beats;
			pos->tick = bbt.ticks;
			
			// XXX still need to set bar_start_tick
			
			pos->beats_per_bar = metric.meter().divisions_per_bar();
			pos->beat_type = metric.meter().note_divisor();
			pos->ticks_per_beat = Timecode::BBT_Time::ticks_per_beat;
			pos->beats_per_minute = metric.tempo().beats_per_minute();
			
			pos->valid = jack_position_bits_t (pos->valid | JackPositionBBT);

		} catch (...) {
			/* no message */
		}
	}

#ifdef HAVE_JACK_VIDEO_SUPPORT
	//poke audio video ratio so Ardour can track Video Sync
	pos->audio_frames_per_video_frame = frame_rate() / timecode_frames_per_second();
	pos->valid = jack_position_bits_t (pos->valid | JackAudioVideoRatio);
#endif

#if 0
	/* Timecode info */

	pos->timecode_offset = config.get_timecode_offset();
	t.timecode_frame_rate = timecode_frames_per_second();
	pos->valid = jack_position_bits_t (pos->valid | JackPositionTimecode;

	if (_transport_speed) {

		if (play_loop) {

			Location* location = _locations.auto_loop_location();

			if (location) {

				t.transport_state = JackTransportLooping;
				t.loop_start = location->start();
				t.loop_end = location->end();
				t.valid = jack_transport_bits_t (t.valid | JackTransportLoop);

			} else {

				t.loop_start = 0;
				t.loop_end = 0;
				t.transport_state = JackTransportRolling;

			}

		} else {

			t.loop_start = 0;
			t.loop_end = 0;
			t.transport_state = JackTransportRolling;

		}

	}
#endif
}

ARDOUR::framecnt_t
Session::convert_to_frames (AnyTime const & position)
{
	double secs;

	switch (position.type) {
	case AnyTime::BBT:
		return _tempo_map->frame_time (position.bbt);
		break;

	case AnyTime::Timecode:
		/* XXX need to handle negative values */
		secs = position.timecode.hours * 60 * 60;
		secs += position.timecode.minutes * 60;
		secs += position.timecode.seconds;
		secs += position.timecode.frames / timecode_frames_per_second();
		if (config.get_timecode_offset_negative()) {
			return (framecnt_t) floor (secs * frame_rate()) - config.get_timecode_offset();
		} else {
			return (framecnt_t) floor (secs * frame_rate()) + config.get_timecode_offset();
		}
		break;

	case AnyTime::Seconds:
		return (framecnt_t) floor (position.seconds * frame_rate());
		break;

	case AnyTime::Frames:
		return position.frames;
		break;
	}

	return position.frames;
}

ARDOUR::framecnt_t
Session::any_duration_to_frames (framepos_t position, AnyTime const & duration)
{
	double secs;

	switch (duration.type) {
	case AnyTime::BBT:
		return (framecnt_t) ( _tempo_map->framepos_plus_bbt (position, duration.bbt) - position);
		break;

	case AnyTime::Timecode:
		/* XXX need to handle negative values */
		secs = duration.timecode.hours * 60 * 60;
		secs += duration.timecode.minutes * 60;
		secs += duration.timecode.seconds;
		secs += duration.timecode.frames / timecode_frames_per_second();
		if (config.get_timecode_offset_negative()) {
			return (framecnt_t) floor (secs * frame_rate()) - config.get_timecode_offset();
		} else {
			return (framecnt_t) floor (secs * frame_rate()) + config.get_timecode_offset();
		}
		break;

	case AnyTime::Seconds:
                return (framecnt_t) floor (duration.seconds * frame_rate());
		break;

	case AnyTime::Frames:
		return duration.frames;
		break;
	}

	return duration.frames;
}
