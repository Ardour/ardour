
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

  $Id$
*/

#include <iostream>
#include <cmath>
#include <unistd.h>

#include <ardour/timestamps.h>

#include <pbd/error.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/tempo.h>

#include "i18n.h"

using namespace ARDOUR;
//using namespace sigc;

/* BBT TIME*/

void
Session::bbt_time (jack_nframes_t when, BBT_Time& bbt)
{
	_tempo_map->bbt_time (when, bbt);
}

/* SMPTE TIME */

int
Session::set_smpte_type (float fps, bool drop_frames)
{
	smpte_frames_per_second = fps;
	smpte_drop_frames = drop_frames;
	_frames_per_smpte_frame = (double) _current_frame_rate / (double) smpte_frames_per_second;
	_frames_per_hour = _current_frame_rate * 3600;
	_smpte_frames_per_hour = (unsigned long) (smpte_frames_per_second * 3600.0);


	last_smpte_valid = false;
	// smpte type bits are the middle two in the upper nibble
	switch ((int) ceil (fps)) {
	case 24:
		mtc_smpte_bits = 0;
		break;

	case 25:
		mtc_smpte_bits = 0x20;
		break;

	case 30:
	default:
		if (drop_frames) {
			mtc_smpte_bits = 0x40;
		} else {
			mtc_smpte_bits =  0x60;
		}
		break;
	};

	SMPTETypeChanged (); /* EMIT SIGNAL */

	set_dirty();

	return 0;
}

void
Session::set_smpte_offset (jack_nframes_t off)
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

#define SMPTE_IS_AROUND_ZERO( sm ) (!(sm).frames && !(sm).seconds && !(sm).minutes && !(sm).hours)
#define SMPTE_IS_ZERO( sm ) (!(sm).frames && !(sm).seconds && !(sm).minutes && !(sm).hours && !(sm.subframes))

// Increment by exactly one frame (keep subframes value)
// Return true if seconds wrap
smpte_wrap_t
Session::smpte_increment( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;

	if (smpte.negative) {
		if (SMPTE_IS_AROUND_ZERO(smpte) && smpte.subframes) {
			// We have a zero transition involving only subframes
			smpte.subframes = 80 - smpte.subframes;
			smpte.negative = false;
			return smpte_wrap_seconds;
		}
    
		smpte.negative = false;
		wrap = smpte_decrement( smpte );
		if (!SMPTE_IS_ZERO( smpte )) {
			smpte.negative = true;
		}
		return wrap;
	}
  
	switch (mtc_smpte_bits >> 5) {
	case MIDI::MTC_24_FPS:
		if (smpte.frames == 23) {
			smpte.frames = 0;
			wrap = smpte_wrap_seconds;
		}
		break;
	case MIDI::MTC_25_FPS:
		if (smpte.frames == 24) {
			smpte.frames = 0;
			wrap = smpte_wrap_seconds;
		}
		break;
	case MIDI::MTC_30_FPS_DROP:
		if (smpte.frames == 29) {
			if ( ((smpte.minutes + 1) % 10) && (smpte.seconds == 59) ) {
				smpte.frames = 2;
			}
			else {
				smpte.frames = 0;
			}
			wrap = smpte_wrap_seconds;
		}
		break;
	case MIDI::MTC_30_FPS:
		if (smpte.frames == 29) {
			smpte.frames = 0;
			wrap = smpte_wrap_seconds;
		}
		break;
	}
  
	if (wrap == smpte_wrap_seconds) {
		if (smpte.seconds == 59) {
			smpte.seconds = 0;
			wrap = smpte_wrap_minutes;
			if (smpte.minutes == 59) {
				smpte.minutes = 0;
				wrap = smpte_wrap_hours;
				smpte.hours++;
			} else {
				smpte.minutes++;
			}
		} else {
			smpte.seconds++;
		}
	} else {
		smpte.frames++;
	}
  
	return wrap;
}

// Decrement by exactly one frame (keep subframes value)
smpte_wrap_t
Session::smpte_decrement( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;
  
  
	if (smpte.negative || SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
		wrap = smpte_increment( smpte );
		smpte.negative = true;
		return wrap;
	} else if (SMPTE_IS_AROUND_ZERO(smpte) && smpte.subframes) {
		// We have a zero transition involving only subframes
		smpte.subframes = 80 - smpte.subframes;
		smpte.negative = true;
		return smpte_wrap_seconds;
	}
  
	switch (mtc_smpte_bits >> 5) {
	case MIDI::MTC_24_FPS:
		if (smpte.frames == 0) {
			smpte.frames = 23;
			wrap = smpte_wrap_seconds;
		}
		break;
	case MIDI::MTC_25_FPS:
		if (smpte.frames == 0) {
			smpte.frames = 24;
			wrap = smpte_wrap_seconds;
		}
		break;
	case MIDI::MTC_30_FPS_DROP:
		if ((smpte.minutes % 10) && (smpte.seconds == 0)) {
			if (smpte.frames <= 2) {
				smpte.frames = 29;
				wrap = smpte_wrap_seconds;
			}
		} else if (smpte.frames == 0) {
			smpte.frames = 29;
			wrap = smpte_wrap_seconds;
		}
		break;
	case MIDI::MTC_30_FPS:
		if (smpte.frames == 0) {
			smpte.frames = 29;
			wrap = smpte_wrap_seconds;
		}
		break;
	}
  
	if (wrap == smpte_wrap_seconds) {
		if (smpte.seconds == 0) {
			smpte.seconds = 59;
			wrap = smpte_wrap_minutes;
			if (smpte.minutes == 0) {
				smpte.minutes = 59;
				wrap = smpte_wrap_hours;
				smpte.hours--;
			}
			else {
				smpte.minutes--;
			}
		} else {
			smpte.seconds--;
		}
	} else {
		smpte.frames--;
	}
  
	if (SMPTE_IS_ZERO( smpte )) {
		smpte.negative = false;
	}
  
	return wrap;
}

// Go to lowest absolute subframe value in this frame (set to 0 :-)
void
Session::smpte_frames_floor( SMPTE_Time& smpte ) const
{
	smpte.subframes = 0;
	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}

// Increment by one subframe
smpte_wrap_t
Session::smpte_increment_subframes( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;
  
	if (smpte.negative) {
		smpte.negative = false;
		wrap = smpte_decrement_subframes( smpte );
		if (!SMPTE_IS_ZERO(smpte)) {
			smpte.negative = true;
		}
		return wrap;
	}
  
	smpte.subframes++;
	if (smpte.subframes >= 80) {
		smpte.subframes = 0;
		smpte_increment( smpte );
		return smpte_wrap_frames;
	}
	return smpte_wrap_none;
}


// Decrement by one subframe
smpte_wrap_t
Session::smpte_decrement_subframes( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;
  
	if (smpte.negative) {
		smpte.negative = false;
		wrap = smpte_increment_subframes( smpte );
		smpte.negative = true;
		return wrap;
	}
  
	if (smpte.subframes <= 0) {
		smpte.subframes = 0;
		if (SMPTE_IS_ZERO(smpte)) {
			smpte.negative = true;
			smpte.subframes = 1;
			return smpte_wrap_frames;
		} else {
			smpte_decrement( smpte );
			smpte.subframes = 79;
			return smpte_wrap_frames;
		}
	} else {
		smpte.subframes--;
		if (SMPTE_IS_ZERO(smpte)) {
			smpte.negative = false;
		}
		return smpte_wrap_none;
	}
}


// Go to next whole second (frames == 0 or frames == 2)
smpte_wrap_t
Session::smpte_increment_seconds( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;
  
	// Clear subframes
	smpte_frames_floor( smpte );
  
	if (smpte.negative) {
		// Wrap second if on second boundary
		wrap = smpte_increment(smpte);
		// Go to lowest absolute frame value
		smpte_seconds_floor( smpte );
		if (SMPTE_IS_ZERO(smpte)) {
			smpte.negative = false;
		}
	} else {
		// Go to highest possible frame in this second
		switch (mtc_smpte_bits >> 5) {
		case MIDI::MTC_24_FPS:
			smpte.frames = 23;
			break;
		case MIDI::MTC_25_FPS:
			smpte.frames = 24;
			break;
		case MIDI::MTC_30_FPS_DROP:
		case MIDI::MTC_30_FPS:
			smpte.frames = 29;
			break;
		}
    
		// Increment by one frame
		wrap = smpte_increment( smpte );
	}
  
	return wrap;
}

// Go to lowest (absolute) frame value in this second
// Doesn't care about positive/negative
void
Session::smpte_seconds_floor( SMPTE_Time& smpte ) const
{
	// Clear subframes
	smpte_frames_floor( smpte );
  
	// Go to lowest possible frame in this second
	switch (mtc_smpte_bits >> 5) {
	case MIDI::MTC_24_FPS:
	case MIDI::MTC_25_FPS:
	case MIDI::MTC_30_FPS:
		smpte.frames = 0;
		break;
	case MIDI::MTC_30_FPS_DROP:
		if ((smpte.minutes % 10) && (smpte.seconds == 0)) {
			smpte.frames = 2;
		} else {
			smpte.frames = 0;
		}
		break;
	}
  
	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}


// Go to next whole minute (seconds == 0, frames == 0 or frames == 2)
smpte_wrap_t
Session::smpte_increment_minutes( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;
  
	// Clear subframes
	smpte_frames_floor( smpte );
  
	if (smpte.negative) {
		// Wrap if on minute boundary
		wrap = smpte_increment_seconds( smpte );
		// Go to lowest possible value in this minute
		smpte_minutes_floor( smpte );
	} else {
		// Go to highest possible second
		smpte.seconds = 59;
		// Wrap minute by incrementing second
		wrap = smpte_increment_seconds( smpte );
	}
  
	return wrap;
}

// Go to lowest absolute value in this minute
void
Session::smpte_minutes_floor( SMPTE_Time& smpte ) const
{
	// Go to lowest possible second
	smpte.seconds = 0;
	// Go to lowest possible frame
	smpte_seconds_floor( smpte );

	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}

// Go to next whole hour (minute = 0, second = 0, frame = 0)
smpte_wrap_t
Session::smpte_increment_hours( SMPTE_Time& smpte ) const
{
	smpte_wrap_t wrap = smpte_wrap_none;
  
	// Clear subframes
	smpte_frames_floor(smpte);
  
	if (smpte.negative) {
		// Wrap if on hour boundary
		wrap = smpte_increment_minutes( smpte );
		// Go to lowest possible value in this hour
		smpte_hours_floor( smpte );
	} else {
		smpte.minutes = 59;
		wrap = smpte_increment_minutes( smpte );
	}
  
	return wrap;
}

// Go to lowest absolute value in this hour
void
Session::smpte_hours_floor( SMPTE_Time& smpte ) const
{
	smpte.minutes = 0;
	smpte.seconds = 0;
	smpte.frames = 0;
	smpte.subframes = 0;
  
	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}


void
Session::smpte_to_sample( SMPTE_Time& smpte, jack_nframes_t& sample, bool use_offset, bool use_subframes ) const
{
	if (smpte_drop_frames) {
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
		jack_nframes_t base_samples = ((smpte.hours * 60 * 60) + ((smpte.minutes / 10) * 10 * 60)) * frame_rate();
		// Samples inside time exceeding the nearest 10 minutes (always offset, see above)
		long exceeding_df_minutes = smpte.minutes % 10;
		long exceeding_df_seconds = (exceeding_df_minutes * 60) + smpte.seconds;
		long exceeding_df_frames = (30 * exceeding_df_seconds) + smpte.frames - (2 * exceeding_df_minutes);
		jack_nframes_t exceeding_samples = (jack_nframes_t) rint(exceeding_df_frames * _frames_per_smpte_frame);
		sample = base_samples + exceeding_samples;
	} else {
		// Non drop is easy:
		sample = (((smpte.hours * 60 * 60) + (smpte.minutes * 60) + smpte.seconds) * frame_rate()) + (jack_nframes_t)rint(smpte.frames * _frames_per_smpte_frame);
	}
  
	if (use_subframes) {
		sample += (long) (((double)smpte.subframes * _frames_per_smpte_frame) / 80.0);
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
Session::sample_to_smpte( jack_nframes_t sample, SMPTE_Time& smpte, bool use_offset, bool use_subframes ) const
{
	jack_nframes_t offset_sample;
  
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
	smpte.subframes = (long) rint(smpte_frames_fraction * 80.0);
  
	// XXX Not sure if this is necessary anymore...
	if (smpte.subframes == 80) {
		// This can happen with 24 fps (and 29.97 fps ?)
		smpte_frames_left_exact = ceil( smpte_frames_left_exact );
		smpte.subframes = 0;
	}

	// Extract hour-exceeding frames for minute, second and frame calculations
	smpte_frames_left = ((long) floor( smpte_frames_left_exact ));

	if (smpte_drop_frames) {
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
		smpte.minutes = smpte_frames_left / ((long) smpte_frames_per_second * 60);
		smpte_frames_left = smpte_frames_left % ((long) smpte_frames_per_second * 60);
		smpte.seconds = smpte_frames_left / (long) smpte_frames_per_second;
		smpte.frames = smpte_frames_left % (long) smpte_frames_per_second;
	}

	if (!use_subframes) {
		smpte.subframes = 0;
	}
}

void
Session::smpte_time (jack_nframes_t when, SMPTE_Time& smpte)
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
Session::smpte_time_subframes (jack_nframes_t when, SMPTE_Time& smpte)
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
Session::smpte_duration (jack_nframes_t when, SMPTE_Time& smpte) const
{
	sample_to_smpte( when, smpte, false /* use_offset */, true /* use_subframes */ );
}

void
Session::smpte_duration_string (char* buf, jack_nframes_t when) const
{
	SMPTE_Time smpte;

	smpte_duration (when, smpte);
	snprintf (buf, sizeof (buf), "%02ld:%02ld:%02ld:%02ld", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
}

void
Session::smpte_time (SMPTE_Time &t)

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
		error << compose (_("Unknown JACK transport state %1 in sync callback"), state)
		      << endmsg;
	} 

	return true;
}

void
Session::jack_timebase_callback (jack_transport_state_t state,
				 jack_nframes_t nframes,
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

#if 0
	/* SMPTE info */

	t.smpte_offset = _smpte_offset;
	t.smpte_frame_rate = smpte_frames_per_second;

	if (_transport_speed) {

		if (auto_loop) {

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

jack_nframes_t
Session::convert_to_frames_at (jack_nframes_t position, AnyTime& any)
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
		secs += any.smpte.frames / smpte_frames_per_second;
		if (_smpte_offset_negative) 
		{
			return (jack_nframes_t) floor (secs * frame_rate()) - _smpte_offset;
		}
		else
		{
			return (jack_nframes_t) floor (secs * frame_rate()) + _smpte_offset;
		}
		break;

	case AnyTime::Seconds:
		return (jack_nframes_t) floor (any.seconds * frame_rate());
		break;

	case AnyTime::Frames:
		return any.frames;
		break;
	}

	return any.frames;
}
