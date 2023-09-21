/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2019 Ben Loftis <ben@harrisonconsoles.com>
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

#include <algorithm>

#include "pbd/memento_command.h"

#include "ardour/debug.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/location.h"
#include "ardour/rc_configuration.h"

#include "mackie_control_protocol.h"
#include "subview.h"
#include "surface.h"
#include "fader.h"

#include "pbd/i18n.h"

/* handlers for all buttons, broken into a separate file to avoid clutter in
 * mackie_control_protocol.cc
 */

using std::string;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;
using namespace ArdourSurface::MACKIE_NAMESPACE;

LedState
MackieControlProtocol::shift_press (Button &)
{
	_modifier_state |= MODIFIER_SHIFT;
	return on;
}
LedState
MackieControlProtocol::shift_release (Button &)
{
	_modifier_state &= ~MODIFIER_SHIFT;
	return off;
}
LedState
MackieControlProtocol::option_press (Button &)
{
	_modifier_state |= MODIFIER_OPTION;
	return on;
}
LedState
MackieControlProtocol::option_release (Button &)
{
	_modifier_state &= ~MODIFIER_OPTION;
	return off;
}
LedState
MackieControlProtocol::control_press (Button &)
{
	_modifier_state |= MODIFIER_CONTROL;
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("CONTROL Press: modifier state now set to %1\n", _modifier_state));
	return on;
}
LedState
MackieControlProtocol::control_release (Button &)
{
	_modifier_state &= ~MODIFIER_CONTROL;
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("CONTROL Release: modifier state now set to %1\n", _modifier_state));
	return off;
}
LedState
MackieControlProtocol::cmd_alt_press (Button &)
{
	_modifier_state |= MODIFIER_CMDALT;
	return on;
}
LedState
MackieControlProtocol::cmd_alt_release (Button &)
{
	_modifier_state &= ~MODIFIER_CMDALT;
	return off;
}

LedState
MackieControlProtocol::left_press (Button &)
{
	if (_subview->subview_mode() != MACKIE_NAMESPACE::Subview::None) {
		return none;
	}

	Sorted sorted = get_sorted_stripables();
	uint32_t strip_cnt = n_strips ();

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("bank left with current initial = %1 nstrips = %2 tracks/busses = %3\n",
							   _current_initial_bank, strip_cnt, sorted.size()));

	if (_current_initial_bank > 0) {
		uint32_t initial = (_current_initial_bank - 1) / strip_cnt * strip_cnt;
		while (initial >= sorted.size())
		{
			initial -= strip_cnt;
		}
		(void) switch_banks (initial);
	} else {
		(void) switch_banks (0);
	}


	return on;
}

LedState
MackieControlProtocol::left_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::right_press (Button &)
{
	if (_subview->subview_mode() != MACKIE_NAMESPACE::Subview::None) {
		return none;
	}

	Sorted sorted = get_sorted_stripables();
	uint32_t strip_cnt = n_strips();
	uint32_t route_cnt = sorted.size();
	uint32_t max_bank = route_cnt / strip_cnt * strip_cnt;


	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("bank right with current initial = %1 nstrips = %2 tracks/busses = %3\n",
							   _current_initial_bank, strip_cnt, route_cnt));

	if (_current_initial_bank < max_bank) {
		uint32_t new_initial = (_current_initial_bank / strip_cnt * strip_cnt) + strip_cnt;
		(void) switch_banks (new_initial);
	}

	return on;
}

LedState
MackieControlProtocol::right_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::cursor_left_press (Button& )
{
	bool press_handled_by_subview = _subview->handle_cursor_left_press();
	if (press_handled_by_subview)
	{
		return off;
	}

	if (zoom_mode()) {

		if (main_modifier_state() & MODIFIER_OPTION) {
			/* reset selected tracks to default vertical zoom */
		} else {
			ZoomOut (); /* EMIT SIGNAL */
		}
	} else {
		float page_fraction;
		if (main_modifier_state() == MODIFIER_CONTROL) {
			page_fraction = 1.0;
		} else if (main_modifier_state() == MODIFIER_OPTION) {
			page_fraction = 0.1;
		} else if (main_modifier_state() == MODIFIER_SHIFT) {
			page_fraction = 2.0;
		} else {
			page_fraction = 0.25;
		}

		ScrollTimeline (-page_fraction);
	}

	return off;
}

LedState
MackieControlProtocol::cursor_left_release (Button&)
{
	return off;
}

LedState
MackieControlProtocol::cursor_right_press (Button& )
{
	bool press_handled_by_subview = _subview->handle_cursor_right_press();
	if (press_handled_by_subview)
	{
		return off;
	}

	if (zoom_mode()) {

		if (main_modifier_state() & MODIFIER_OPTION) {
			/* reset selected tracks to default vertical zoom */
		} else {
			ZoomIn (); /* EMIT SIGNAL */
		}
	} else {
		float page_fraction;
		if (main_modifier_state() == MODIFIER_CONTROL) {
			page_fraction = 1.0;
		} else if (main_modifier_state() == MODIFIER_OPTION) {
			page_fraction = 0.1;
		} else if (main_modifier_state() == MODIFIER_SHIFT) {
			page_fraction = 2.0;
		} else {
			page_fraction = 0.25;
		}

		ScrollTimeline (page_fraction);
	}

	return off;
}

LedState
MackieControlProtocol::cursor_right_release (Button&)
{
	return off;
}

LedState
MackieControlProtocol::cursor_up_press (Button&)
{
	if (zoom_mode()) {

		if (main_modifier_state() & MODIFIER_CONTROL) {
			VerticalZoomInSelected (); /* EMIT SIGNAL */
		} else {
			VerticalZoomInAll (); /* EMIT SIGNAL */
		}
	} else {
		access_action ("Editor/select-prev-route");
	}
	return off;
}

LedState
MackieControlProtocol::cursor_up_release (Button&)
{
	return off;
}

LedState
MackieControlProtocol::cursor_down_press (Button&)
{
	if (zoom_mode()) {
		if (main_modifier_state() & MODIFIER_OPTION) {
			VerticalZoomOutSelected (); /* EMIT SIGNAL */
		} else {
			VerticalZoomOutAll (); /* EMIT SIGNAL */
		}
	} else {
		access_action ("Editor/select-next-route");
	}
	return off;
}

LedState
MackieControlProtocol::cursor_down_release (Button&)
{
	return off;
}

LedState
MackieControlProtocol::channel_left_press (Button &)
{
	if (_device_info.single_fader_follows_selection()) {
		access_action ("Editor/select-prev-route");
		return on;
	}

	if (_subview->subview_mode() != MACKIE_NAMESPACE::Subview::None) {
		return none;
	}
	Sorted sorted = get_sorted_stripables();
	if (sorted.size() > n_strips()) {
		prev_track();
		return on;
	} else {
		return flashing;
	}
}

LedState
MackieControlProtocol::channel_left_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::channel_right_press (Button &)
{
	if (_device_info.single_fader_follows_selection()) {
		access_action ("Editor/select-next-route");
		return on;
	}

	if (_subview->subview_mode() != MACKIE_NAMESPACE::Subview::None) {
		return none;
	}
	Sorted sorted = get_sorted_stripables();
	if (sorted.size() > n_strips()) {
		next_track();
		return on;
	} else {
		return flashing;
	}
}

LedState
MackieControlProtocol::channel_right_release (Button &)
{
	return off;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::zoom_press (MACKIE_NAMESPACE::Button &)
{
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::zoom_release (MACKIE_NAMESPACE::Button &)
{
	if (_modifier_state & MODIFIER_ZOOM) {
		_modifier_state &= ~MODIFIER_ZOOM;
	} else {
		_modifier_state |= MODIFIER_ZOOM;
	}

	return (zoom_mode() ? on : off);
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::scrub_press (MACKIE_NAMESPACE::Button &)
{
	if (_master_surface) {
		_master_surface->next_jog_mode ();
	}

	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::scrub_release (MACKIE_NAMESPACE::Button &)
{
	return none;
}

LedState
MackieControlProtocol::undo_press (Button&)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		redo();
	} else {
		undo ();
	}
	return none;
}

LedState
MackieControlProtocol::undo_release (Button&)
{
	return none;
}

LedState
MackieControlProtocol::redo_press (Button &)
{
	redo ();
	return on;
}

LedState
MackieControlProtocol::redo_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::drop_press (Button &)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		toggle_punch_in();
		return none;
	} else {
		access_action ("Common/start-range-from-playhead");
	}
	return none;
}

LedState
MackieControlProtocol::drop_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::save_press (Button &)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		quick_snapshot_switch();
	} else {
		save_state ();
	}

	return none;
}

LedState
MackieControlProtocol::save_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::open_press (Button &)
{
	access_action ("Main/Open");
	return on;
}

LedState
MackieControlProtocol::open_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::timecode_beats_press (Button &)
{
	switch (_timecode_type) {
	case ARDOUR::AnyTime::BBT:
		_timecode_type = ARDOUR::AnyTime::Timecode;
		break;
	case ARDOUR::AnyTime::Timecode:
		_timecode_type = ARDOUR::AnyTime::BBT;
		break;
	default:
		return off;
	}

	update_timecode_beats_led();

	return on;
}

LedState
MackieControlProtocol::timecode_beats_release (Button &)
{
	return off;
}

/////////////////////////////////////
// Functions
/////////////////////////////////////
LedState
MackieControlProtocol::marker_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Common/remove-location-from-playhead");
		return off;
	} else {
		_modifier_state |= MODIFIER_MARKER;
		marker_modifier_consumed_by_button = false;
		return on;
	}
}

LedState
MackieControlProtocol::marker_release (Button &)
{
	_modifier_state &= ~MODIFIER_MARKER;

	if (main_modifier_state() & MODIFIER_SHIFT) {
		return off;   //if shift was held, we already did the action
	}

	if (marker_modifier_consumed_by_button) {
		DEBUG_TRACE (DEBUG::MackieControl, "marked modifier consumed by button, ignored\n");
		/* marker was used a modifier for some other button(s), so do
		   nothing
		*/
		return off;
	}

	string markername;

	/* Don't add another mark if one exists within 1/100th of a second of
	 * the current position and we're not rolling.
	 */

	samplepos_t where = session->audible_sample();

	if (session->transport_stopped_or_stopping() && session->locations()->mark_at (timepos_t (where), timecnt_t (session->sample_rate() / 100.0))) {
		return off;
	}

	session->locations()->next_available_name (markername,"mark");
	add_marker (markername);

	return off;
}

LedState
MackieControlProtocol::prev_marker_press (Button &)
{
	prev_marker ();
	return on;
}

LedState
MackieControlProtocol::prev_marker_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::next_marker_press (Button &)
{
	next_marker ();
	return on;
}

LedState
MackieControlProtocol::next_marker_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::flip_window_press (Button &)
{
	access_action("Common/toggle-editor-and-mixer");
	return on;
}

LedState
MackieControlProtocol::flip_window_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::master_press (Button &)
{
	_master_surface->toggle_master_monitor ();
	return on;
}

LedState
MackieControlProtocol::master_release (Button &)
{
	return off;
}

/////////////////////////////////////
// Transport Buttons
/////////////////////////////////////

LedState
MackieControlProtocol::stop_press (Button &)
{
	transport_stop ();

	if (main_modifier_state() == MODIFIER_SHIFT) {
		session->midi_panic();
	}

	return on;
}

LedState
MackieControlProtocol::stop_release (Button &)
{
	return session->transport_stopped_or_stopping();
}

LedState
MackieControlProtocol::play_press (Button &)
{
	/* if we're already rolling at normal speed, and we're pressed
	   again, jump back to where we started last time
	*/

	transport_play (get_transport_speed() == 1.0);
	return none;
}

LedState
MackieControlProtocol::play_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::record_press (Button &)
{
	rec_enable_toggle ();
	return none;
}

LedState
MackieControlProtocol::record_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::rewind_press (Button &)
{
	if (modifier_state() & MODIFIER_MARKER) {
		prev_marker ();
	} else if (modifier_state() & MODIFIER_NUDGE) {
		access_action ("Common/nudge-playhead-backward");
	} else if (main_modifier_state() & MODIFIER_SHIFT) {
		goto_start ();
	} else {
		rewind ();
	}
	return none;
}

LedState
MackieControlProtocol::rewind_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::ffwd_press (Button &)
{
	if (modifier_state() & MODIFIER_MARKER) {
		next_marker ();
	} else if (modifier_state() & MODIFIER_NUDGE) {
		access_action ("Common/nudge-playhead-forward");
	} else if (main_modifier_state() & MODIFIER_SHIFT) {
		goto_end();
	} else {
		ffwd ();
	}
	return none;
}

LedState
MackieControlProtocol::ffwd_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::loop_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Editor/set-loop-from-edit-range");
		return off;
	} else {
		bool was_on = session->get_play_loop();
		loop_toggle ();
		return was_on ? off : on;
	}
}

LedState
MackieControlProtocol::loop_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::enter_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Transport/ToggleFollowEdits");
	} else {
		access_action ("Common/select-all-tracks");
	}
	return none;
}

LedState
MackieControlProtocol::enter_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::bank_release (Button& b, uint32_t basic_bank_num)
{
	if (_subview->subview_mode() != MACKIE_NAMESPACE::Subview::None) {
		return none;
	}

	uint32_t bank_num = basic_bank_num;

	if (b.long_press_count() > 0) {
		bank_num = 8 + basic_bank_num;
	}

	(void) switch_banks (n_strips() * bank_num);

	return on;
}

/*  F-KEYS are only used for actions that are bound from the control panel; no need to address them here
LedState
MackieControlProtocol::F1_press (Button &b)
{
	return on;
}
LedState
MackieControlProtocol::F1_release (Button &b)
{
	return off;
}
LedState
MackieControlProtocol::F2_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F2_release (Button &b)
{
	return off;
}
LedState
MackieControlProtocol::F3_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F3_release (Button &b)
{
	return off;
}
LedState
MackieControlProtocol::F4_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F4_release (Button &b)
{
	return off;
}
LedState
MackieControlProtocol::F5_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F5_release (Button &)
{
	return off;
}
LedState
MackieControlProtocol::F6_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F6_release (Button &)
{
	return off;
}
LedState
MackieControlProtocol::F7_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F7_release (Button &)
{
	return off;
}
LedState
MackieControlProtocol::F8_press (Button &)
{
	return on;
}
LedState
MackieControlProtocol::F8_release (Button &)
{
	return off;
}
*/


/* UNIMPLEMENTED */

LedState
MackieControlProtocol::pan_press (Button &)
{
	/* XXX eventually pan may have its own subview mode */
	set_subview_mode (MACKIE_NAMESPACE::Subview::None, std::shared_ptr<Stripable>());
	return none;
}
LedState
MackieControlProtocol::pan_release (Button &)
{
	return none;
}
LedState
MackieControlProtocol::plugin_press (Button &)
{
	set_subview_mode (Subview::Plugin, first_selected_stripable());
	return none;
}
LedState
MackieControlProtocol::plugin_release (Button &)
{
	// Do not do this yet, since it does nothing
	// set_view_mode (Plugins);
	return none; /* LED state set by set_view_mode */
}
LedState
MackieControlProtocol::eq_press (Button &)
{
	set_subview_mode (Subview::EQ, first_selected_stripable ());
	return none; /* led state handled by set_subview_mode() */

}
LedState
MackieControlProtocol::eq_release (Button &)
{
	return none;
}
LedState
MackieControlProtocol::dyn_press (Button &)
{
	set_subview_mode (Subview::Dynamics, first_selected_stripable ());
	return none; /* led state handled by set_subview_mode() */
}

LedState
MackieControlProtocol::dyn_release (Button &)
{
	return none;
}
LedState
MackieControlProtocol::flip_press (Button &)
{
	if (_subview->permit_flipping_faders_and_pots()) {
		if (_flip_mode != Normal) {
			set_flip_mode (Normal);
		} else {
			set_flip_mode (Mirror);
		}
		return ((_flip_mode != Normal) ? on : off);
	}

	return none;
}

LedState
MackieControlProtocol::flip_release (Button &)
{
	return none;
}
LedState
MackieControlProtocol::name_value_press (Button &)
{
	return off;
}
LedState
MackieControlProtocol::name_value_release (Button &)
{
	return off;
}
LedState
MackieControlProtocol::touch_press (Button &)
{
	return none;
}
LedState
MackieControlProtocol::touch_release (Button &)
{
	set_automation_state (ARDOUR::Touch);
	return none;
}
LedState
MackieControlProtocol::cancel_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Transport/ToggleExternalSync");
	} else {
		access_action ("Main/Escape");
	}
	return none;
}
LedState
MackieControlProtocol::cancel_release (Button &)
{
	return none;
}
LedState
MackieControlProtocol::user_a_press (Button &)
{
	transport_play (get_transport_speed() == 1.0);
	return off;
}
LedState
MackieControlProtocol::user_a_release (Button &)
{
	return off;
}
LedState
MackieControlProtocol::user_b_press (Button &)
{
	transport_stop();
	return off;
}
LedState
MackieControlProtocol::user_b_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::master_fader_touch_press (MACKIE_NAMESPACE::Button &)
{
	if (_master_surface && _master_surface->master_fader() != 0) {
		DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::master_fader_touch_press\n");

		Fader* master_fader = _master_surface->master_fader();

		master_fader->set_in_use (true);
		master_fader->start_touch (timepos_t (transport_sample()));
	}
	return none;
}
LedState
MackieControlProtocol::master_fader_touch_release (MACKIE_NAMESPACE::Button &)
{
	if (_master_surface && _master_surface->master_fader() != 0) {
		DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::master_fader_touch_release\n");

		Fader* master_fader = _master_surface->master_fader();

		master_fader->set_in_use (false);
		master_fader->stop_touch (timepos_t (transport_sample()));

	}
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::read_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::read_release (MACKIE_NAMESPACE::Button&)
{
	set_automation_state (ARDOUR::Play);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::write_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::write_release (MACKIE_NAMESPACE::Button&)
{
	set_automation_state (ARDOUR::Write);
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::clearsolo_press (MACKIE_NAMESPACE::Button&)
{
	// clears all solos and listens (pfl/afl)

	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Editor/set-session-from-edit-range");
		return none;
	}

	cancel_all_solo ();
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::clearsolo_release (MACKIE_NAMESPACE::Button&)
{
	//return session->soloing();
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::track_press (MACKIE_NAMESPACE::Button&)
{
	set_subview_mode (Subview::TrackView, first_selected_stripable());
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::track_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::send_press (MACKIE_NAMESPACE::Button&)
{
	set_subview_mode (Subview::Sends, first_selected_stripable());
	return none; /* led state handled by set_subview_mode() */
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::send_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::miditracks_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::miditracks_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (MidiTracks);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::inputs_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::inputs_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (Inputs);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::audiotracks_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::audiotracks_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (AudioTracks);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::audioinstruments_press (MACKIE_NAMESPACE::Button& b)
{
	return none;
}

MACKIE_NAMESPACE::LedState
MackieControlProtocol::audioinstruments_release (MACKIE_NAMESPACE::Button& b)
{
	set_view_mode (AudioInstr);
	return none;

}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::aux_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::aux_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (Auxes);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::busses_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::busses_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (Busses);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::outputs_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::outputs_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (Outputs);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::user_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::user_release (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (Selected);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::trim_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::trim_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::latch_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::latch_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::grp_press (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::grp_release (MACKIE_NAMESPACE::Button&)
{
	/* There is no "Off" button for automation,
	   so we use Group for this purpose.
	*/
	set_automation_state (Off);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::nudge_press (MACKIE_NAMESPACE::Button&)
{
	_modifier_state |= MODIFIER_NUDGE;
	nudge_modifier_consumed_by_button = false;
	return on;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::nudge_release (MACKIE_NAMESPACE::Button&)
{
	_modifier_state &= ~MODIFIER_NUDGE;

	/* XXX these action names are stupid, because the action can affect
	 * regions, markers or the playhead depending on selection state.
	 */

	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Region/nudge-backward");
	} else {
		access_action ("Region/nudge-forward");
	}

	return off;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::replace_press (MACKIE_NAMESPACE::Button&)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		toggle_punch_out();
		return none;
	} else {
		access_action ("Common/finish-range-from-playhead");
	}
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::replace_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::click_press (MACKIE_NAMESPACE::Button&)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Editor/set-punch-from-edit-range");
		return none;
	} else {
		bool state = !Config->get_clicking();
		Config->set_clicking (state);
		return none;
	}
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::click_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::view_press (MACKIE_NAMESPACE::Button&)
{
	set_view_mode (Mixer);
	return none;
}
MACKIE_NAMESPACE::LedState
MackieControlProtocol::view_release (MACKIE_NAMESPACE::Button&)
{
	return none;
}

/////////////////////////////////////
// QCon Pro G2 Buttons
/////////////////////////////////////

LedState
MackieControlProtocol::prog2_undo_press (Button &)
{
	if(main_modifier_state () & MODIFIER_SHIFT) {
		access_action ("Common/menu-show-preferences");
		return on;
	}
	undo ();
	return on;
}

LedState
MackieControlProtocol::prog2_undo_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::prog2_clear_solo_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {

		StripableList sl;
		session->get_stripables (sl);
		for (StripableList::const_iterator i = sl.begin(); i != sl.end(); ++i)
		{
			std::shared_ptr<MuteControl> mc = (*i)->mute_control();
			if (!mc->muted() && (!(*i)->is_master()) && (!(*i)->is_monitor()))
			{
				mc->set_value(1.0, Controllable::UseGroup);
			}
		}

		return none;   
	}
	cancel_all_solo ();
	return none;
}

LedState
MackieControlProtocol::prog2_clear_solo_release (Button &)
{
	return none;
}

LedState
MackieControlProtocol::prog2_save_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT)
	{
		access_action("Main/SaveAs");
		return on;
	}
	save_state ();
	return on;
}

LedState
MackieControlProtocol::prog2_save_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::prog2_vst_press (Button &)
{
	access_action("Mixer/select-all-processors");
	access_action("Mixer/toggle-processors");

	return on;
}

LedState
MackieControlProtocol::prog2_vst_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::prog2_left_press (Button &)
{
	access_action("Mixer/select-prev-stripable");
	return on;
}

LedState
MackieControlProtocol::prog2_left_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::prog2_right_press (Button &)
{
	access_action("Mixer/select-next-stripable");
	return on;
}

LedState
MackieControlProtocol::prog2_right_release (Button &)
{
	return off;
}

LedState
MackieControlProtocol::prog2_marker_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Common/remove-location-from-playhead");
		return on;
	}

	samplepos_t where = session->audible_sample();
	if (session->transport_stopped_or_stopping() && session->locations()->mark_at (timepos_t (where), timecnt_t (session->sample_rate() / 100.0))) {
		return on;
	}

	string markername;
	session->locations()->next_available_name (markername,"mark");
	add_marker (markername);

	return on;
}

LedState
MackieControlProtocol::prog2_marker_release (Button &)
{
	return off;
}
