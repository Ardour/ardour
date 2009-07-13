
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

#include "ardour/ardour.h"
#include "ardour/configuration.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* BBT TIME*/

void
Session::bbt_time (nframes_t when, BBT_Time& bbt)
{
	_tempo_map->bbt_time (when, bbt);
}

/* SMPTE TIME */
float
Session::smpte_frames_per_second() const
{
	switch (config.get_smpte_format()) {
		case smpte_23976: 
			return 23.976;

			break;
		case smpte_24: 
			return 24;

			break;
		case smpte_24976: 
			return 24.976;

			break;
		case smpte_25: 
			return 25;

			break;
		case smpte_2997: 
			return 29.97;

			break;
		case smpte_2997drop: 
			return 29.97;

			break;
		case smpte_30: 
			return 30;

			break;
		case smpte_30drop: 
			return 30;

			break;
		case smpte_5994: 
			return 59.94;

			break;
		case smpte_60: 
			return 60;

			break;
	        default:
		  cerr << "Editor received unexpected smpte type" << endl;
	}
	return 30.0;
}
bool
Session::smpte_drop_frames() const
{
	switch (config.get_smpte_format()) {
		case smpte_23976: 
			return false;

			break;
		case smpte_24: 
			return false;

			break;
		case smpte_24976: 
			return false;

			break;
		case smpte_25: 
			return false;

			break;
		case smpte_2997: 
			return false;

			break;
		case smpte_2997drop: 
			return true;

			break;
		case smpte_30: 
			return false;

			break;
		case smpte_30drop: 
			return true;

			break;
		case smpte_5994: 
			return false;

			break;
		case smpte_60: 
			return false;

			break;
	        default:
			cerr << "Editor received unexpected smpte type" << endl;
	}
	return false;
}
void
Session::sync_time_vars ()
{
	_current_frame_rate = (nframes_t) round (_base_frame_rate * (1.0 + (config.get_video_pullup()/100.0)));
	_frames_per_smpte_frame = (double) _current_frame_rate / (double) smpte_frames_per_second();
	if (smpte_drop_frames()) {
	  _frames_per_hour = (long)(107892 * _frames_per_smpte_frame);
	} else {
	  _frames_per_hour = (long)(3600 * rint(smpte_frames_per_second()) * _frames_per_smpte_frame);
	}
	_smpte_frames_per_hour = (nframes_t)rint(smpte_frames_per_second() * 3600.0);

	last_smpte_valid = false;
	// smpte type bits are the middle two in the upper nibble
	switch ((int) ceil (smpte_frames_per_second())) {
	case 24:
		mtc_smpte_bits = 0;
		break;

	case 25:
		mtc_smpte_bits = 0x20;
		break;

	case 30:
	default:
		if (smpte_drop_frames()) {
			mtc_smpte_bits = 0x40;
		} else {
			mtc_smpte_bits =  0x60;
		}
		break;
	};
}

void
Session::set_smpte_offset (nframes_t off)
{
	_smpte_offset = off;
	last_smpte_valid = false;

	SMPTEOffsetChanged (); /* EMIT SIGNAL */
}

void
Session::set_smpte_offset_negative (bool neg)
{
	_smpte_offset_negative = neg;
	last_smpte_valid = false;

	SMPTEOffsetChanged (); /* EMIT SIGNAL */
}

void
Session::smpte_to_sample( SMPTE::Time& smpte, nframes_t& sample, bool use_offset, bool use_subframes ) const
{

	if (smpte.drop) {
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
		// SMPTE value    frames offset   subframes offset   seconds (rounded)  44100 sample (rounded)
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
		nframes_t base_samples = (nframes_t) (((smpte.hours * 107892) + ((smpte.minutes / 10) * 17982)) * _frames_per_smpte_frame);

		// Samples inside time exceeding the nearest 10 minutes (always offset, see above)
		long exceeding_df_minutes = smpte.minutes % 10;
		long exceeding_df_seconds = (exceeding_df_minutes * 60) + smpte.seconds;
		long exceeding_df_frames = (30 * exceeding_df_seconds) + smpte.frames - (2 * exceeding_df_minutes);
		nframes_t exceeding_samples = (nframes_t) rint(exceeding_df_frames * _frames_per_smpte_frame);
		sample = base_samples + exceeding_samples;
	} else {
		/* 
		   Non drop is easy.. just note the use of 
		   rint(smpte.rate) * _frames_per_smpte_frame
		   (frames per SMPTE second), which is larger than  
		   frame_rate() in the non-integer SMPTE rate case.
		*/

		sample = (nframes_t)rint((((smpte.hours * 60 * 60) + (smpte.minutes * 60) + smpte.seconds) * (rint(smpte.rate) * _frames_per_smpte_frame)) + (smpte.frames * _frames_per_smpte_frame));
	}
  
	if (use_subframes) {
		sample += (long) (((double)smpte.subframes * _frames_per_smpte_frame) / config.get_subframes_per_frame());
	}
  
	if (use_offset) {
		if (smpte_offset_negative()) {
			if (sample >= smpte_offset()) {
				sample -= smpte_offset();
			} else {
				/* Prevent song-time from becoming negative */
				sample = 0;
			}
		} else {
			if (smpte.negative) {
				if (sample <= smpte_offset()) {
					sample = smpte_offset() - sample;
				} else {
					sample = 0;
				}
			} else {
				sample += smpte_offset();
			}
		}
	}

}


void
Session::sample_to_smpte( nframes_t sample, SMPTE::Time& smpte, bool use_offset, bool use_subframes ) const
{
	nframes_t offset_sample;

	if (!use_offset) {
		offset_sample = sample;
		smpte.negative = false;
	} else {
		if (_smpte_offset_negative) {
			offset_sample =  sample + _smpte_offset;
			smpte.negative = false;
		} else {
			if (sample < _smpte_offset) {
				offset_sample = (_smpte_offset - sample);
				smpte.negative = true;
			} else {
				offset_sample =  sample - _smpte_offset;
				smpte.negative = false;
			}
		}
	}
  
	double smpte_frames_left_exact;
	double smpte_frames_fraction;
	unsigned long smpte_frames_left;
  
	// Extract whole hours. Do this to prevent rounding errors with
	// high sample numbers in the calculations that follow.
	smpte.hours = offset_sample / _frames_per_hour;
	offset_sample = offset_sample % _frames_per_hour;

	// Calculate exact number of (exceeding) smpte frames and fractional frames
	smpte_frames_left_exact = (double) offset_sample / _frames_per_smpte_frame;
	smpte_frames_fraction = smpte_frames_left_exact - floor( smpte_frames_left_exact );
	smpte.subframes = (long) rint(smpte_frames_fraction * config.get_subframes_per_frame());
  
	// XXX Not sure if this is necessary anymore...
	if (smpte.subframes == config.get_subframes_per_frame()) {
		// This can happen with 24 fps (and 29.97 fps ?)
		smpte_frames_left_exact = ceil( smpte_frames_left_exact );
		smpte.subframes = 0;
	}

	// Extract hour-exceeding frames for minute, second and frame calculations
	smpte_frames_left = ((long) floor( smpte_frames_left_exact ));

	if (smpte_drop_frames()) {
		// See long explanation in smpte_to_sample()...

		// Number of 10 minute chunks
		smpte.minutes = (smpte_frames_left / 17982) * 10; // exactly 17982 frames in 10 minutes
		// frames exceeding the nearest 10 minute barrier
		long exceeding_df_frames = smpte_frames_left % 17982;

		// Find minutes exceeding the nearest 10 minute barrier
		if (exceeding_df_frames >= 1800) { // nothing to do if we are inside the first minute (0-1799)
			exceeding_df_frames -= 1800; // take away first minute (different number of frames than the others)
			long extra_minutes_minus_1 = exceeding_df_frames / 1798; // how many minutes after the first one
			exceeding_df_frames -= extra_minutes_minus_1 * 1798; // take away the (extra) minutes just found
			smpte.minutes += extra_minutes_minus_1 + 1; // update with exceeding minutes
		}
    
		// Adjust frame numbering for dropped frames (frame 0 and 1 skipped at start of every minute except every 10th)
		if (smpte.minutes % 10) {
			// Every minute except every 10th
			if (exceeding_df_frames < 28) {
				// First second, frames 0 and 1 are skipped
				smpte.seconds = 0;
				smpte.frames = exceeding_df_frames + 2;
			} else {
				// All other seconds, all 30 frames are counted
				exceeding_df_frames -= 28;
				smpte.seconds = (exceeding_df_frames / 30) + 1;
				smpte.frames = exceeding_df_frames % 30;
			}
		} else {
			// Every 10th minute, all 30 frames counted in all seconds
			smpte.seconds = exceeding_df_frames / 30;
			smpte.frames = exceeding_df_frames % 30;
		}
	} else {
		// Non drop is easy
		smpte.minutes = smpte_frames_left / ((long) rint (smpte_frames_per_second ()) * 60);
		smpte_frames_left = smpte_frames_left % ((long) rint (smpte_frames_per_second ()) * 60);
		smpte.seconds = smpte_frames_left / (long) rint(smpte_frames_per_second ());
		smpte.frames = smpte_frames_left % (long) rint(smpte_frames_per_second ());
	}

	if (!use_subframes) {
		smpte.subframes = 0;
	}
	/* set frame rate and drop frame */
	smpte.rate = smpte_frames_per_second ();
	smpte.drop = smpte_drop_frames();
}

void
Session::smpte_time (nframes_t when, SMPTE::Time& smpte)
{
	if (last_smpte_valid && when == last_smpte_when) {
		smpte = last_smpte;
		return;
	}

	sample_to_smpte( when, smpte, true /* use_offset */, false /* use_subframes */ );

	last_smpte_when = when;
	last_smpte = smpte;
	last_smpte_valid = true;
}

void
Session::smpte_time_subframes (nframes_t when, SMPTE::Time& smpte)
{
	if (last_smpte_valid && when == last_smpte_when) {
		smpte = last_smpte;
		return;
	}
  
	sample_to_smpte( when, smpte, true /* use_offset */, true /* use_subframes */ );

	last_smpte_when = when;
	last_smpte = smpte;
	last_smpte_valid = true;
}

void
Session::smpte_duration (nframes_t when, SMPTE::Time& smpte) const
{
	sample_to_smpte( when, smpte, false /* use_offset */, true /* use_subframes */ );
}

void
Session::smpte_duration_string (char* buf, nframes_t when) const
{
	SMPTE::Time smpte;

	smpte_duration (when, smpte);
	snprintf (buf, sizeof (buf), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
}

void
Session::smpte_time (SMPTE::Time &t)

{
	smpte_time (_transport_frame, t);
}

int
Session::jack_sync_callback (jack_transport_state_t state,
			     jack_position_t* pos)
{
	bool slave = synced_to_jack();

	switch (state) {
	case JackTransportStopped:
		if (slave && _transport_frame != pos->frame && post_transport_work == 0) {
		 	request_locate (pos->frame, false);
			// cerr << "SYNC: stopped, locate to " << pos->frame << " from " << _transport_frame << endl;
			return false;
		} else {
			return true;
		}
		
	case JackTransportStarting:
		// cerr << "SYNC: starting @ " << pos->frame << " a@ " << _transport_frame << " our work = " <<  post_transport_work << " pos matches ? " << (_transport_frame == pos->frame) << endl;
		if (slave) {
			return _transport_frame == pos->frame && post_transport_work == 0;
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
Session::jack_timebase_callback (jack_transport_state_t state,
				 nframes_t nframes,
				 jack_position_t* pos,
				 int new_position)
{
	BBT_Time bbt;

	/* frame info */

	pos->frame = _transport_frame;
	pos->valid = JackPositionTimecode;

	/* BBT info */
	
	if (_tempo_map) {

		TempoMap::Metric metric (_tempo_map->metric_at (_transport_frame));
		_tempo_map->bbt_time_with_metric (_transport_frame, bbt, metric);
		
		pos->bar = bbt.bars;
		pos->beat = bbt.beats;
		pos->tick = bbt.ticks;

		// XXX still need to set bar_start_tick

		pos->beats_per_bar = metric.meter().beats_per_bar();
		pos->beat_type = metric.meter().note_divisor();
		pos->ticks_per_beat = Meter::ticks_per_beat;
		pos->beats_per_minute = metric.tempo().beats_per_minute();

		pos->valid = jack_position_bits_t (pos->valid | JackPositionBBT);
	}

#ifdef HAVE_JACK_VIDEO_SUPPORT
	//poke audio video ratio so Ardour can track Video Sync
	pos->audio_frames_per_video_frame = frame_rate() / smpte_frames_per_second();
	pos->valid = jack_position_bits_t (pos->valid | JackAudioVideoRatio);
#endif

#if 0
	/* SMPTE info */

	t.smpte_offset = _smpte_offset;
	t.smpte_frame_rate = smpte_frames_per_second();

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

ARDOUR::nframes_t
Session::convert_to_frames_at (nframes_t position, AnyTime const & any)
{
	double secs;
	
	switch (any.type) {
	case AnyTime::BBT:
		return _tempo_map->frame_time ( any.bbt);
		break;

	case AnyTime::SMPTE:
		/* XXX need to handle negative values */
		secs = any.smpte.hours * 60 * 60;
		secs += any.smpte.minutes * 60;
		secs += any.smpte.seconds;
		secs += any.smpte.frames / smpte_frames_per_second();
		if (_smpte_offset_negative) 
		{
			return (nframes_t) floor (secs * frame_rate()) - _smpte_offset;
		}
		else
		{
			return (nframes_t) floor (secs * frame_rate()) + _smpte_offset;
		}
		break;

	case AnyTime::Seconds:
		return (nframes_t) floor (any.seconds * frame_rate());
		break;

	case AnyTime::Frames:
		return any.frames;
		break;
	}

	return any.frames;
}
