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

#include <tranzport_common.h>
#include <tranzport_control_protocol.h>

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "pbd/i18n.h"

#include <pbd/abstract_ui.cc>
// HA, I don't need this anymore
#include <slider_gain.h>

//   FIXME, flash recording light when recording and transport is moving
int TranzportControlProtocol::lights_show_recording()
{
	return     lights_show_normal();
}

void TranzportControlProtocol::show_bling() {
	lights_show_bling();
	screen_show_bling();
}

void TranzportControlProtocol::notify(const char *msg) {
	last_notify=100;
	if(strlen(msg) < 21) {
		strcpy(last_notify_msg,msg);
	} else {
		strncpy(last_notify_msg,msg,16);
		last_notify_msg[16] = '\n';
	}
}

void TranzportControlProtocol::show_notify() {
// FIXME: Get width of the notify area somehow
	if(last_notify==0) {
		print(1,0,"                ");
		last_notify=-1;
	}
	if(last_notify > 0) {
		print(1,0,last_notify_msg);
		--last_notify;
	}
}

// Need more bling!

int TranzportControlProtocol::lights_show_bling()
{
	switch (bling_mode) {
	case BlingOff: break;
	case BlingKit: break; // rotate rec/mute/solo/any solo back and forth
	case BlingRotating: break; // switch between lights
	case BlingPairs: break; // Show pairs of lights
	case BlingRows: break; // light each row in sequence
	case BlingFlashAll: break; // Flash everything randomly
	case BlingEnter: lights_on(); // Show intro
	case BlingExit:
		lights_off();
		break;
	}
	return 0;
}

int TranzportControlProtocol::screen_show_bling()
{
	switch (bling_mode) {
	case BlingOff: break;
	case BlingKit: break; // rotate rec/mute/solo/any solo back and forth
	case BlingRotating: break; // switch between lights
	case BlingPairs: break; // Show pairs of lights
	case BlingRows: break; // light each row in sequence
	case BlingFlashAll: break; // Flash everything randomly
	case BlingEnter: // Show intro
		print(0,0,"!!Welcome to Ardour!");
		print(1,0,"Peace through Music!");
		break;
	case BlingExit:
		break;
	}
	return 0;
}

int TranzportControlProtocol::lights_show_normal()
{
	/* Track only */

	if (route_table[0]) {
		boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack> (route_table[0]);
		lights_pending[LightTrackrec]  = at && at->record_enabled();
		lights_pending[LightTrackmute] = route_get_muted(0);
		lights_pending[LightTracksolo] = route_get_soloed(0);
	} else {
		lights_pending[LightTrackrec]  = false;
		lights_pending[LightTracksolo] = false;
		lights_pending[LightTrackmute] = false;
	}

	/* Global settings */

	lights_pending[LightLoop]        = session->get_play_loop();
	lights_pending[LightPunch]       = session->config.get_punch_in() || session->config.get_punch_out();
	lights_pending[LightRecord]      = session->get_record_enabled();
	lights_pending[LightAnysolo]     = session->soloing();

	return 0;
}

int TranzportControlProtocol::lights_show_tempo()
{
	// someday soon fiddle with the lights more sanely based on the tempo
	return     lights_show_normal();
}

int
TranzportControlProtocol::update_state ()
{
	/* do the text and light updates */

	switch (display_mode) {
	case DisplayBigMeter:
		lights_show_tempo();
		show_meter ();
		break;

	case DisplayNormal:
		lights_show_normal();
		normal_update();
		break;

	case DisplayConfig:
		break;

	case DisplayRecording:
		lights_show_recording();
		normal_update();
		break;

	case DisplayRecordingMeter:
		lights_show_recording();
		show_meter();
		break;

	case DisplayBling:
		show_bling();
		break;

	case DisplayBlingMeter:
		lights_show_bling();
		show_meter();
		break;
	}
	show_notify();

	return 0;

}

void
TranzportControlProtocol::prev_marker ()
{
	Location *location = session->locations()->first_location_before (session->transport_sample());

	if (location) {
		session->request_locate (location->start());
		notify(location->name().c_str());
	} else {
		session->goto_start ();
		notify("START");
	}

}

void
TranzportControlProtocol::next_marker ()
{
	Location *location = session->locations()->first_location_after (session->transport_sample());

	if (location) {
		session->request_locate (location->start());
		notify(location->name().c_str());
	} else {
		session->request_locate (session->current_end_sample());
		notify("END ");
	}
}


void
TranzportControlProtocol::show_current_track ()
{
	char pad[COLUMNS];
	char *v;
	int len;
	if (route_table[0] == 0) {
		print (0, 0, "---------------");
		last_track_gain = FLT_MAX;
	} else {
		strcpy(pad,"               ");
		v =  (char *)route_get_name (0).substr (0, 14).c_str();
		if((len = strlen(v)) > 0) {
			strncpy(pad,(char *)v,len);
		}
		print (0, 0, pad);
	}
}


#if 0
void
TranzportControlProtocol::step_gain (float increment)
{
// FIXME: buttonstop is used elsewhere
	if (buttonmask & ButtonStop) {
		gain_fraction += 0.001*increment;
	} else {
		gain_fraction += 0.01*increment;
	}

	if (fabsf(gain_fraction) > 2.0) {
		gain_fraction = 2.0*sign(gain_fraction);
	}

	route_set_gain (0, slider_position_to_gain (gain_fraction));
}
#endif

void
TranzportControlProtocol::step_gain_up ()
{
	if (buttonmask & ButtonStop) {
		gain_fraction += 0.001;
	} else {
		gain_fraction += 0.01;
	}

	if (gain_fraction > 2.0) {
		gain_fraction = 2.0;
	}

	route_set_gain (0, slider_position_to_gain (gain_fraction));
}

void
TranzportControlProtocol::step_gain_down ()
{
	if (buttonmask & ButtonStop) {
		gain_fraction -= 0.001;
	} else {
		gain_fraction -= 0.01;
	}

	if (gain_fraction < 0.0) {
		gain_fraction = 0.0;
	}

	route_set_gain (0, slider_position_to_gain (gain_fraction));
}


void
TranzportControlProtocol::next_track ()
{
	ControlProtocol::next_track (current_track_id);
	gain_fraction = gain_to_slider_position (route_get_effective_gain (0));
//	notify("NextTrak"); // not needed til we have more modes
}

void
TranzportControlProtocol::prev_track ()
{
	ControlProtocol::prev_track (current_track_id);
	gain_fraction = gain_to_slider_position (route_get_effective_gain (0));
//	notify("PrevTrak");
}

// This should kind of switch to using notify

// Was going to keep state around saying to retry or not
// haven't got to it yet, still not sure it's a good idea

void
TranzportControlProtocol::print (int row, int col, const char *text) {
	print_noretry(row,col,text);
}

// -1 on failure
// 0 on no damage
// count of bit set on damage?

void
TranzportControlProtocol::print_noretry (int row, int col, const char *text)
{
	uint32_t length = strlen (text);
	if (row*COLUMNS+col+length > (ROWS*COLUMNS)) {
		return;
	}
	// FIXME - be able to print the whole screen at a go.
	uint32_t t,r,c;
	std::bitset<ROWS*COLUMNS> mask(screen_invalid);
	for(r = row, c = col, t = 0 ; t < length; c++,t++) {
		screen_pending[r][c] = text[t];
		mask[r*COLUMNS+c] = (screen_current[r][c] != screen_pending[r][c]);
	}
	screen_invalid = mask;
}

void TranzportControlProtocol::invalidate()
{
	lcd_damage(); lights_invalidate(); screen_invalidate(); // one of these days lcds can be fine but screens not
}
