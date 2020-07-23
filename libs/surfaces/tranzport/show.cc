/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   */

#include <iostream>
#include <algorithm>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>

#include "pbd/pthread_utils.h"

#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/location.h"
#include "ardour/dB.h"

#include "tranzport_control_protocol.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.cc"

float
log_meter (float db)
{
	float def = 0.0f; /* Meter deflection %age */

	if (db < -70.0f) return 0.0f;
	if (db > 6.0f) return 1.0f;

	if (db < -60.0f) {
		def = (db + 70.0f) * 0.25f;
	} else if (db < -50.0f) {
		def = (db + 60.0f) * 0.5f + 2.5f;
	} else if (db < -40.0f) {
		def = (db + 50.0f) * 0.75f + 7.5f;
	} else if (db < -30.0f) {
		def = (db + 40.0f) * 1.5f + 15.0f;
	} else if (db < -20.0f) {
		def = (db + 30.0f) * 2.0f + 30.0f;
	} else if (db < 6.0f) {
		def = (db + 20.0f) * 2.5f + 50.0f;
	}

	/* 115 is the deflection %age that would be
	   when db=6.0. this is an arbitrary
	   endpoint for our scaling.
	*/

	return def/115.0f;
}

#define TRANZ_U  0x1 /* upper */
#define TRANZ_BL 0x2 /* lower left */
#define TRANZ_Q2 0x3 /* 2 quadrant block */
#define TRANZ_ULB 0x4 /* Upper + lower left */
#define TRANZ_L 0x5  /* lower  */
#define TRANZ_UBL 0x6 /* upper left + bottom all */
#define TRANZ_Q4 0x7 /* 4 quadrant block */
#define TRANZ_UL 0x08 /* upper left */

// Shift Space - switches your "view"
// Currently defined views are:
// BigMeter
//
// Shift Record - SAVE SNAPSHOT
// Somewhere I was rewriting this
// Other meters
// Inverted - show meters "inside out" For example 4 meters covering 2 cells each, and the
//
// each 4 character cell could be an 8 bar meter = 10 meters!
// Dual Meter mode - master and current track
// We have 16 rows of pixels so we COULD do a vertical meter
// BEAT BLOCKS - For each beat, flash a 8 block (could use the center for vertical meters)
// Could have something generic that could handle up to /20 time
// Odd times could flash the whole top bar for the first beat


// Vertical Meter _ .colon - + ucolon A P R I H FULLBLACK
// MV@$%&*()-

// 3 char block  rotating beat `\'/
// 1 char rotating beat {/\}
// 4 char in block rotating beat {/\}
//                               {\/)

void TranzportControlProtocol::show_mini_meter()
{
	// FIXME - show the current marker in passing
	const int meter_buf_size = 41;
	static uint32_t last_meter_fill_l = 0;
	static uint32_t last_meter_fill_r = 0;
	uint32_t meter_size;

	float speed = fabsf(get_transport_speed());
	char buf[meter_buf_size];

	if (speed == 1.0)  {
		meter_size = 32;
	}

	if (speed == 0.0) {
		meter_size = 20;  // not actually reached
	}

	if (speed > 0.0 && (speed < 1.0)) {
		meter_size = 20; // may shrink more one day
	}

	if (speed > 1.0 && (speed < 2.0)) {
		meter_size = 20;
	}

	if (speed >= 2.0) {
		meter_size = 24;
	}


	// you only seem to get a route_table[0] == 0 on moving forward - bug in next_track?

	if (route_table[0] == 0) {
		// Principle of least surprise
		print (1, 0, "NoAUDIO  ");
		return;
	}

	float level_l = route_get_peak_input_power (0, 0);
	float fraction_l = log_meter (level_l);

	// how to figure out if we are mono?

	float level_r = route_get_peak_input_power (0, 1);
	float fraction_r = log_meter (level_r);

	uint32_t fill_left  = (uint32_t) floor (fraction_l * ((int) meter_size));
	uint32_t fill_right  = (uint32_t) floor (fraction_r * ((int) meter_size));

	if (fill_left == last_meter_fill_l && fill_right == last_meter_fill_r && !lcd_isdamaged(1,0,meter_size/2)) {
		/* nothing to do */
		return;
	}

	last_meter_fill_l = fill_left;	last_meter_fill_r = fill_right;

	// give some feedback when overdriving - override yellow and red lights

	if (fraction_l > 0.96 || fraction_r > 0.96) {
		light_on (LightLoop);
	}

	if (fraction_l == 1.0 || fraction_r == 1.0) {
		light_on (LightTrackrec);
	}

	const uint8_t char_map[16] = { ' ', TRANZ_UL,
				       TRANZ_U, TRANZ_U,
				       TRANZ_BL, TRANZ_Q2,
				       TRANZ_Q2, TRANZ_ULB,
				       TRANZ_L, TRANZ_UBL,
				       ' ',' ',
				       TRANZ_L, TRANZ_UBL,
				       TRANZ_Q4,TRANZ_Q4
	};
	unsigned int val,j,i;

	for(j = 1, i = 0; i < meter_size/2; i++, j+=2) {
		val = (fill_left >= j) | ((fill_left >= j+1) << 1) |
			((fill_right >=j) << 2) | ((fill_right >= j+1) << 3);
		buf[i] = char_map[val];
	}

	/* print() requires this */

	buf[meter_size/2] = '\0';

	print (1, 0, buf);

	/* Add a peak bar, someday do falloff */

	//		char peak[2]; peak[0] = ' '; peak[1] = '\0';
	//		if(fraction_l == 1.0 || fraction_r == 1.0) peak[0] = 'P';
	//		print (1,8,peak); // Put a peak meter - P in if we peaked.

}

void
TranzportControlProtocol::show_meter ()
{
	// you only seem to get a route_table[0] on moving forward - bug elsewhere
	if (route_table[0] == 0) {
		// Principle of least surprise
		print (0, 0, "No audio to meter!!!");
		print (1, 0, "Select another track");
		return;
	}

	float level = route_get_peak_input_power (0, 0);
	float fraction = log_meter (level);

	/* Someday add a peak bar*/

	/* we draw using a choice of a sort of double colon-like character ("::") or a single, left-aligned ":".
	   the screen is 20 chars wide, so we can display 40 different levels. compute the level,
	   then figure out how many "::" to fill. if the answer is odd, make the last one a ":"
	*/

	uint32_t fill  = (uint32_t) floor (fraction * 40);
	char buf[21];
	uint32_t i;

	if (fill == last_meter_fill) {
		/* nothing to do */
		return;
	}

	last_meter_fill = fill;

	bool add_single_level = (fill % 2 != 0);
	fill /= 2;

	if (fraction > 0.96) {
		light_on (LightLoop);
	}


	if (fraction == 1.0) {
		light_on (LightTrackrec);
	}


	/* add all full steps */

	for (i = 0; i < fill; ++i) {
		buf[i] = 0x07; /* tranzport special code for 4 quadrant LCD block */
	}

	/* add a possible half-step */

	if (i < 20 && add_single_level) {
		buf[i] = 0x03; /* tranzport special code for 2 left quadrant LCD block */
		++i;
	}

	/* fill rest with space */

	for (; i < 20; ++i) {
		buf[i] = ' ';
	}

	/* print() requires this */

	buf[20] = '\0';

	print (0, 0, buf);
	print (1, 0, buf);
}

void
TranzportControlProtocol::show_bbt (samplepos_t where)
{
	if (where != last_where) {
		char buf[16];
		Temporal::BBT_Time bbt;

		// When recording or playing back < 1.0 speed do 1 or 2
		// FIXME - clean up state machine & break up logic
		// this has to co-operate with the mini-meter and
		// this is NOT the right way.

		session->tempo_map().bbt_time (where, bbt);
		last_bars = bbt.bars;
		last_beats = bbt.beats;
		last_ticks = bbt.ticks;
		last_where = where;

		float speed = fabsf(get_transport_speed());

		if (speed == 1.0)  {
			sprintf (buf, "%03" PRIu32 "%1" PRIu32, bbt.bars,bbt.beats); // switch to hex one day
			print (1, 16, buf);
		}

		if (speed == 0.0) {
			sprintf (buf, "%03" PRIu32 "|%1" PRIu32 "|%04" PRIu32, bbt.bars,bbt.beats,bbt.ticks);
			print (1, 10, buf);
		}

		if (speed > 0.0 && (speed < 1.0)) {
			sprintf (buf, "%03" PRIu32 "|%1" PRIu32 "|%04" PRIu32, bbt.bars,bbt.beats,bbt.ticks);
			print (1, 10, buf);
		}

		if (speed > 1.0 && (speed < 2.0)) {
			sprintf (buf, "%03" PRIu32 "|%1" PRIu32 "|%04" PRIu32, bbt.bars,bbt.beats,bbt.ticks);
			print (1, 10, buf);
		}

		if (speed >= 2.0) {
			sprintf (buf, "%03" PRIu32 "|%1" PRIu32 "|%02" PRIu32, bbt.bars,bbt.beats,bbt.ticks);
			print (1, 12, buf);
		}

		TempoMap::Metric m (session->tempo_map().metric_at (where));

		// the lights stop working well at above 100 bpm so don't bother
		if(m.tempo().beats_per_minute() < 101.0 && (speed > 0.0)) {

			// something else can reset these, so we need to

			lights_pending[LightRecord] = false;
			lights_pending[LightAnysolo] = false;
			switch(last_beats) {
			case 1: if(last_ticks < 250 || last_ticks >= 0) lights_pending[LightRecord] = true; break;
			default: if(last_ticks < 250) lights_pending[LightAnysolo] = true;
			}
		}
	}
}

void
TranzportControlProtocol::show_transport_time ()
{
	show_bbt (session->transport_sample ());
}

void
TranzportControlProtocol::show_timecode (samplepos_t where)
{
	if ((where != last_where) || lcd_isdamaged(1,9,10)) {

		char buf[5];
		Timecode::Time timecode;

		session->timecode_time (where, timecode);

		if (timecode.negative) {
			sprintf (buf, "-%02" PRIu32 ":", timecode.hours);
		} else {
			sprintf (buf, " %02" PRIu32 ":", timecode.hours);
		}
		print (1, 8, buf);

		sprintf (buf, "%02" PRIu32 ":", timecode.minutes);
		print (1, 12, buf);

		sprintf (buf, "%02" PRIu32 ":", timecode.seconds);
		print (1, 15, buf);

		sprintf (buf, "%02" PRIu32, timecode.frames);
		print_noretry (1, 18, buf);

		last_where = where;
	}
}

void
TranzportControlProtocol::show_track_gain ()
{
// FIXME last_track gain has to become meter/track specific
	if (route_table[0]) {
		gain_t g = route_get_gain (0);
		if ((g != last_track_gain) || lcd_isdamaged(0,12,8)) {
			char buf[16];
			snprintf (buf, sizeof (buf), "%6.1fdB", coefficient_to_dB (route_get_effective_gain (0)));
			print (0, 12, buf);
			last_track_gain = g;
		}
	} else {
		print (0, 9, "        ");
	}
}
