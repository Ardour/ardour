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

#include <tranzport_control_protocol.h>

void
TranzportControlProtocol::normal_update ()
{
	show_current_track ();
	show_transport_time ();
	show_track_gain ();
	show_wheel_mode ();
}

void
TranzportControlProtocol::next_display_mode ()
{
	switch (display_mode) {

	case DisplayNormal:
		enter_big_meter_mode();
		break;

	case DisplayBigMeter:
		enter_normal_display_mode();
		break;

	case DisplayRecording:
		enter_normal_display_mode();
		break;

	case DisplayRecordingMeter:
		enter_big_meter_mode();
		break;

	case DisplayConfig:
	case DisplayBling:
	case DisplayBlingMeter:
		enter_normal_display_mode();
		break;
	}
}

// FIXME: There should be both enter and exits
// EXIT would erase the portions of the screen being written
// to.
/* not sure going macro crazy is a good idea
#define DECLARE_ENTER_MODE(mode,modename) void TranzportControlProtocol::enter_##mode##_mode() \{\screen_clear(); lights_off(); display_mode=Display##modename;\;
*/
void
TranzportControlProtocol::enter_recording_mode ()
{
	screen_clear ();
	lights_off ();
	display_mode = DisplayRecording;
}

void
TranzportControlProtocol::enter_bling_mode ()
{
	screen_clear ();
	lights_off ();
	display_mode = DisplayBling;
}

void
TranzportControlProtocol::enter_config_mode ()
{
	lights_off ();
	screen_clear ();
	display_mode = DisplayConfig;
}


void
TranzportControlProtocol::enter_big_meter_mode ()
{
	lights_off (); // it will clear the screen for you
	last_meter_fill = 0;
	display_mode = DisplayBigMeter;
}

void
TranzportControlProtocol::enter_normal_display_mode ()
{
	lights_off ();
	screen_clear ();
	display_mode = DisplayNormal;
}

