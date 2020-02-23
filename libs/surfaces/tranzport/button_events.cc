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



void
TranzportControlProtocol::button_event_battery_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_battery_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_backlight_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_backlight_release (bool shifted)
{
#if DEBUG_TRANZPORT
	printf("backlight released, redrawing (and possibly crashing) display\n");
#endif
	if (shifted) {
		lcd_damage();
		lcd_clear();
		last_where += 1; /* force time redisplay */
		last_track_gain = FLT_MAX;
	}
}

void
TranzportControlProtocol::button_event_trackleft_press (bool shifted)
{
	prev_track ();
	// not really the right layer for this
	if(display_mode == DisplayBigMeter) {
		if (route_table[0] != 0) {
			notify(route_get_name (0).substr (0, 15).c_str());
		}
	}
}

void
TranzportControlProtocol::button_event_trackleft_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackright_press (bool shifted)
{
	next_track ();
	// not really the right layer for this
	if(display_mode == DisplayBigMeter) {
		if (route_table[0] != 0) {
			notify(route_get_name (0).substr (0, 15).c_str());
		}
	}
}

void
TranzportControlProtocol::button_event_trackright_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackrec_press (bool shifted)
{
	if (shifted) {
		toggle_all_rec_enables ();
	} else {
		route_set_rec_enable (0, !route_get_rec_enable (0));
	}
}

void
TranzportControlProtocol::button_event_trackrec_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackmute_press (bool shifted)
{
	if (shifted) {
		// Mute ALL? Something useful when a phone call comes in. Mute master?
	} else {
		route_set_muted (0, !route_get_muted (0));
	}
}

void
TranzportControlProtocol::button_event_trackmute_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_tracksolo_press (bool shifted)
{
#if DEBUG_TRANZPORT
	printf("solo pressed\n");
#endif
	if (display_mode == DisplayBigMeter) {
		light_off (LightAnysolo);
		return;
	}

	if (shifted) {
		session->set_all_solo (!session->soloing());
	} else {
		route_set_soloed (0, !route_get_soloed (0));
	}
}

void
TranzportControlProtocol::button_event_tracksolo_release (bool shifted)
{
#if DEBUG_TRANZPORT
	printf("solo released\n");
#endif
}

void
TranzportControlProtocol::button_event_undo_press (bool shifted)
{
// undohistory->get_state(1);
//XMLNode&
//UndoHistory::get_state (uint32_t depth)

	if (shifted) {
		redo (); // someday flash the screen with what was redone
		notify("Redone!!");
	} else {
		undo (); // someday flash the screen with what was undone
		notify("Undone!!");
	}
}

void
TranzportControlProtocol::button_event_undo_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_in_press (bool shifted)
{
	if (shifted) {
		toggle_punch_in ();
	} else {
		ControlProtocol::ZoomIn (); /* EMIT SIGNAL */
	}
}

void
TranzportControlProtocol::button_event_in_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_out_press (bool shifted)
{
	if (shifted) {
		toggle_punch_out ();
	} else {
		ControlProtocol::ZoomOut (); /* EMIT SIGNAL */
	}
}

void
TranzportControlProtocol::button_event_out_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_punch_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_punch_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_loop_press (bool shifted)
{
	if (shifted) {
		next_wheel_shift_mode ();
	} else {
		loop_toggle ();
	}
}

void
TranzportControlProtocol::button_event_loop_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_prev_press (bool shifted)
{
	if (shifted) {
		ControlProtocol::ZoomToSession (); /* EMIT SIGNAL */
	} else {
		prev_marker ();
	}
}

void
TranzportControlProtocol::button_event_prev_release (bool shifted)
{
}

// Note - add_marker should adhere to the snap to setting
// maybe session->audible_sample does that

void
TranzportControlProtocol::button_event_add_press (bool shifted)
{
	add_marker ();
}

void
TranzportControlProtocol::button_event_add_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_next_press (bool shifted)
{
	if (shifted) {
		next_wheel_mode ();
	} else {
		next_marker ();
	}
}

void
TranzportControlProtocol::button_event_next_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_rewind_press (bool shifted)
{
	if (shifted) {
		goto_start ();
	} else {
		rewind ();
	}
}

void
TranzportControlProtocol::button_event_rewind_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_fastforward_press (bool shifted)
{
	if (shifted) {
		goto_end ();
	} else {
		ffwd ();
	}
}

void
TranzportControlProtocol::button_event_fastforward_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_stop_press (bool shifted)
{
	if (shifted) {
		next_display_mode ();
	} else {
		transport_stop ();
	}
}

void
TranzportControlProtocol::button_event_stop_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_play_press (bool shifted)
{
	if (shifted) {
		set_transport_speed (1.0f);
	} else {
		transport_play ();
	}
}

void
TranzportControlProtocol::button_event_play_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_record_press (bool shifted)
{
	if (shifted) {
		save_state ();
	} else {
		rec_enable_toggle ();
	}
}

void
TranzportControlProtocol::button_event_record_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_footswitch_press (bool shifted)
{
	if (shifted) {
		next_marker (); // think this through, we could also do punch in
	} else {
		prev_marker (); // think this through, we could also do punch in
	}
}

void
TranzportControlProtocol::button_event_footswitch_release (bool shifted)
{
	if(get_transport_speed() == 0.0)
	{
		transport_play ();
	}
}

// Possible new api example
// tries harder to do the right thing if we somehow missed a button down event
// which currently happens... a lot.

void button_event_mute (bool pressed, bool shifted)
{
	static int was_pressed = 0;
	if((!pressed && !was_pressed) || pressed) {
		was_pressed = 1;
	}

	was_pressed = 0;
}
