
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

float
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
	double my_frames_per_timecode_frame = _frames_per_timecode_frame;
	if (timecode.rate > 0) {
		my_frames_per_timecode_frame = (double) _current_frame_rate / (double) timecode.rate;
	}

	if (timecode.drop) {
		// The drop frame format was created to better approximate the 30000/1001 = 29.97002997002997....
		// framerate of NTSC color TV. The used frame rate of drop frame is 29.97, which drifts by about
		// 0.108 frame per hour, or about 1.3 frames per 12 hours. This is not perfect, but a lot better
		// than using 30 non drop, which will drift with about 1.8 frame per minute.
		// Using 29.97, drop frame real time can be accurate only every 10th minute (10 minutes of 29.97 fps
		// is exactly 17982 frames). One minute is 1798.2 frames, but we count 30 frames per second
		// (30 * 60 = 1800). This means that at the first minute boundary (at the end of 0:0:59:29) we
		// are 1.8 frames too late relative to real time. By dropping 2 frames (jumping to 0:1:0:2) we are
		// approx. 0.2 frames too early. This adds up with 0.2 too early for each minute until we are 1.8
		// frames too early at 0:9:0:2 (9 * 0.2 = 1.8). The 10th minute brings us 1.8 frames later again
		// (at end of 0:9:59:29), which sums up to 0 (we are back to zero at 0:10:0:0 :-).
		//
		// In table form:
		//
		// Timecode value    frames offset   subframes offset   seconds (rounded)  44100 sample (rounded)
		//  0:00:00:00        0.0             0                     0.000                0 (accurate)
		//  0:00:59:29        1.8           144                    60.027          2647177
		//  0:01:00:02       -0.2           -16                    60.060          2648648
		//  0:01:59:29        1.6           128                   120.020          5292883
		//  0:02:00:02       -0.4           -32                   120.053          5294354
		//  0:02:59:29        1.4           112                   180.013          7938588
		//  0:03:00:02       -0.6           -48                   180.047          7940060
		//  0:03:59:29        1.2            96                   240.007         10584294
		//  0:04:00:02       -0.8           -64                   240.040         10585766
		//  0:04:59:29        1.0            80                   300.000         13230000
		//  0:05:00:02       -1.0           -80                   300.033         13231471
		//  0:05:59:29        0.8            64                   359.993         15875706
		//  0:06:00:02       -1.2           -96                   360.027         15877177
		//  0:06:59:29        0.6            48                   419.987         18521411
		//  0:07:00:02       -1.4          -112                   420.020         18522883
		//  0:07:59:29        0.4            32                   478.980         21167117
		//  0:08:00:02       -1.6          -128                   480.013         21168589
		//  0:08:59:29        0.2            16                   539.973         23812823
		//  0:09:00:02       -1.8          -144                   540.007         23814294
		//  0:09:59:29        0.0+            0+                  599.967         26458529
		//  0:10:00:00        0.0             0                   600.000         26460000 (accurate)
		//
		//  Per Sigmond <per@sigmond.no>

		// Samples inside time dividable by 10 minutes (real time accurate)
		framecnt_t base_samples = (framecnt_t) (((timecode.hours * 107892) + ((timecode.minutes / 10) * 17982)) * my_frames_per_timecode_frame);

		// Samples inside time exceeding the nearest 10 minutes (always offset, see above)
		int32_t exceeding_df_minutes = timecode.minutes % 10;
		int32_t exceeding_df_seconds = (exceeding_df_minutes * 60) + timecode.seconds;
		int32_t exceeding_df_frames = (30 * exceeding_df_seconds) + timecode.frames - (2 * exceeding_df_minutes);
		framecnt_t exceeding_samples = (framecnt_t) rint(exceeding_df_frames * my_frames_per_timecode_frame);
		sample = base_samples + exceeding_samples;
	} else {
		/*
		   Non drop is easy.. just note the use of
		   rint(timecode.rate) * _frames_per_timecode_frame
		   (frames per Timecode second), which is larger than
		   frame_rate() in the non-integer Timecode rate case.
		*/

		sample = (framecnt_t)rint((((timecode.hours * 60 * 60) + (timecode.minutes * 60) + timecode.seconds) * (rint(timecode.rate) * my_frames_per_timecode_frame)) + (timecode.frames * my_frames_per_timecode_frame));
	}

	if (use_subframes) {
		sample += (int32_t) (((double)timecode.subframes * my_frames_per_timecode_frame) / config.get_subframes_per_frame());
	}

	if (use_offset) {
		if (config.get_timecode_offset_negative()) {
			if (sample >= config.get_timecode_offset()) {
				sample -= config.get_timecode_offset();
			} else {
				/* Prevent song-time from becoming negative */
				sample = 0;
			}
		} else {
			if (timecode.negative) {
				if (sample <= config.get_timecode_offset()) {
					sample = config.get_timecode_offset() - sample;
				} else {
					sample = 0;
				}
			} else {
				sample += config.get_timecode_offset();
			}
		}
	}

}


void
Session::sample_to_timecode (framepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes ) const
{
	framepos_t offset_sample;

	if (!use_offset) {
		offset_sample = sample;
		timecode.negative = false;
	} else {
		if (config.get_timecode_offset_negative()) {
			offset_sample = sample + config.get_timecode_offset ();
			timecode.negative = false;
		} else {
			if (sample < config.get_timecode_offset()) {
				offset_sample = (config.get_timecode_offset() - sample);
				timecode.negative = true;
			} else {
				offset_sample =  sample - config.get_timecode_offset();
				timecode.negative = false;
			}
		}
	}

	double timecode_frames_left_exact;
	double timecode_frames_fraction;
	uint32_t timecode_frames_left;

	// Extract whole hours. Do this to prevent rounding errors with
	// high sample numbers in the calculations that follow.
	timecode.hours = offset_sample / _frames_per_hour;
	offset_sample = offset_sample % _frames_per_hour;

	// Calculate exact number of (exceeding) timecode frames and fractional frames
	timecode_frames_left_exact = (double) offset_sample / _frames_per_timecode_frame;
	timecode_frames_fraction = timecode_frames_left_exact - floor( timecode_frames_left_exact );
	timecode.subframes = (int32_t) rint(timecode_frames_fraction * config.get_subframes_per_frame());

	// XXX Not sure if this is necessary anymore...
	if (timecode.subframes == config.get_subframes_per_frame()) {
		// This can happen with 24 fps (and 29.97 fps ?)
		timecode_frames_left_exact = ceil( timecode_frames_left_exact );
		timecode.subframes = 0;
	}

	// Extract hour-exceeding frames for minute, second and frame calculations
	timecode_frames_left = (uint32_t) floor (timecode_frames_left_exact);

	if (timecode_drop_frames()) {
		// See int32_t explanation in timecode_to_sample()...

		// Number of 10 minute chunks
		timecode.minutes = (timecode_frames_left / 17982) * 10; // exactly 17982 frames in 10 minutes
		// frames exceeding the nearest 10 minute barrier
		int32_t exceeding_df_frames = timecode_frames_left % 17982;

		// Find minutes exceeding the nearest 10 minute barrier
		if (exceeding_df_frames >= 1800) { // nothing to do if we are inside the first minute (0-1799)
			exceeding_df_frames -= 1800; // take away first minute (different number of frames than the others)
			int32_t extra_minutes_minus_1 = exceeding_df_frames / 1798; // how many minutes after the first one
			exceeding_df_frames -= extra_minutes_minus_1 * 1798; // take away the (extra) minutes just found
			timecode.minutes += extra_minutes_minus_1 + 1; // update with exceeding minutes
		}

		// Adjust frame numbering for dropped frames (frame 0 and 1 skipped at start of every minute except every 10th)
		if (timecode.minutes % 10) {
			// Every minute except every 10th
			if (exceeding_df_frames < 28) {
				// First second, frames 0 and 1 are skipped
				timecode.seconds = 0;
				timecode.frames = exceeding_df_frames + 2;
			} else {
				// All other seconds, all 30 frames are counted
				exceeding_df_frames -= 28;
				timecode.seconds = (exceeding_df_frames / 30) + 1;
				timecode.frames = exceeding_df_frames % 30;
			}
		} else {
			// Every 10th minute, all 30 frames counted in all seconds
			timecode.seconds = exceeding_df_frames / 30;
			timecode.frames = exceeding_df_frames % 30;
		}
	} else {
		// Non drop is easy
		timecode.minutes = timecode_frames_left / ((int32_t) rint (timecode_frames_per_second ()) * 60);
		timecode_frames_left = timecode_frames_left % ((int32_t) rint (timecode_frames_per_second ()) * 60);
		timecode.seconds = timecode_frames_left / (int32_t) rint(timecode_frames_per_second ());
		timecode.frames = timecode_frames_left % (int32_t) rint(timecode_frames_per_second ());
	}

	if (!use_subframes) {
		timecode.subframes = 0;
	}
	/* set frame rate and drop frame */
	timecode.rate = timecode_frames_per_second ();
	timecode.drop = timecode_drop_frames();
}

void
Session::timecode_time (framepos_t when, Timecode::Time& timecode)
{
	if (last_timecode_valid && when == last_timecode_when) {
		timecode = last_timecode;
		return;
	}

	sample_to_timecode( when, timecode, true /* use_offset */, false /* use_subframes */ );

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

	sample_to_timecode( when, timecode, true /* use_offset */, true /* use_subframes */ );

	last_timecode_when = when;
	last_timecode = timecode;
	last_timecode_valid = true;
}

void
Session::timecode_duration (framecnt_t when, Timecode::Time& timecode) const
{
	sample_to_timecode( when, timecode, false /* use_offset */, true /* use_subframes */ );
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
