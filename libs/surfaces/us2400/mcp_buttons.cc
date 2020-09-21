/*
 * Copyright (C) 2017-2019 Ben Loftis <ben@harrisonconsoles.com>
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

#include "us2400_control_protocol.h"
#include "surface.h"
#include "fader.h"

#include "pbd/i18n.h"

/* handlers for all buttons, broken into a separate file to avoid clutter in
 * us2400_control_protocol.cc
 */

using std::string;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;
using namespace US2400;

LedState
US2400Protocol::shift_press (Button &)
{
	_modifier_state |= MODIFIER_SHIFT;
	return on;
}
LedState
US2400Protocol::shift_release (Button &)
{
	_modifier_state &= ~MODIFIER_SHIFT;
	return off;
}
LedState
US2400Protocol::option_press (Button &)
{
	_modifier_state |= MODIFIER_OPTION;
	return on;
}
LedState
US2400Protocol::option_release (Button &)
{
	_modifier_state &= ~MODIFIER_OPTION;
	return off;
}
LedState
US2400Protocol::control_press (Button &)
{
	_modifier_state |= MODIFIER_CONTROL;
	DEBUG_TRACE (DEBUG::US2400, string_compose ("CONTROL Press: modifier state now set to %1\n", _modifier_state));
	return on;
}
LedState
US2400Protocol::control_release (Button &)
{
	_modifier_state &= ~MODIFIER_CONTROL;
	DEBUG_TRACE (DEBUG::US2400, string_compose ("CONTROL Release: modifier state now set to %1\n", _modifier_state));
	return off;
}
LedState
US2400Protocol::cmd_alt_press (Button &)
{
	_modifier_state |= MODIFIER_CMDALT;
	return on;
}
LedState
US2400Protocol::cmd_alt_release (Button &)
{
	_modifier_state &= ~MODIFIER_CMDALT;
	return off;
}

LedState
US2400Protocol::left_press (Button &)
{
	if (_subview_mode != None) {
		return none;
	}

	Sorted sorted = get_sorted_stripables();
	uint32_t strip_cnt = n_strips ();

	DEBUG_TRACE (DEBUG::US2400, string_compose ("bank left with current initial = %1 nstrips = %2 tracks/busses = %3\n",
							   _current_initial_bank, strip_cnt, sorted.size()));
	if (_current_initial_bank > 0) {
		(void) switch_banks ((_current_initial_bank - 1) / strip_cnt * strip_cnt);
	} else {
		(void) switch_banks (0);
	}


	return on;
}

LedState
US2400Protocol::left_release (Button &)
{
	return none;
}

LedState
US2400Protocol::right_press (Button &)
{
	if (_subview_mode != None) {
		return none;
	}

	Sorted sorted = get_sorted_stripables();
	uint32_t strip_cnt = n_strips();
	uint32_t route_cnt = sorted.size();
	uint32_t max_bank = route_cnt / strip_cnt * strip_cnt;


	DEBUG_TRACE (DEBUG::US2400, string_compose ("bank right with current initial = %1 nstrips = %2 tracks/busses = %3\n",
							   _current_initial_bank, strip_cnt, route_cnt));

	if (_current_initial_bank < max_bank) {
		uint32_t new_initial = (_current_initial_bank / strip_cnt * strip_cnt) + strip_cnt;
		(void) switch_banks (new_initial);
	}

	return none;
}

LedState
US2400Protocol::right_release (Button &)
{
	return none;
}

LedState
US2400Protocol::cursor_left_press (Button& )
{
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
US2400Protocol::cursor_left_release (Button&)
{
	return off;
}

LedState
US2400Protocol::cursor_right_press (Button& )
{
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
US2400Protocol::cursor_right_release (Button&)
{
	return off;
}

LedState
US2400Protocol::cursor_up_press (Button&)
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
US2400Protocol::cursor_up_release (Button&)
{
	return off;
}

LedState
US2400Protocol::cursor_down_press (Button&)
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
US2400Protocol::cursor_down_release (Button&)
{
	return off;
}

LedState
US2400Protocol::channel_left_press (Button &)
{
	if (_subview_mode != None) {
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
US2400Protocol::channel_left_release (Button &)
{
	return off;
}

LedState
US2400Protocol::channel_right_press (Button &)
{
	if (_subview_mode != None) {
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
US2400Protocol::channel_right_release (Button &)
{
	return off;
}

US2400::LedState
US2400Protocol::zoom_press (US2400::Button &)
{
	return none;
}

US2400::LedState
US2400Protocol::zoom_release (US2400::Button &)
{
	if (_modifier_state & MODIFIER_ZOOM) {
		_modifier_state &= ~MODIFIER_ZOOM;
	} else {
		_modifier_state |= MODIFIER_ZOOM;
	}

	return (zoom_mode() ? on : off);
}

US2400::LedState
US2400Protocol::scrub_press (US2400::Button &)
{
	if (!surfaces.empty()) {
		// surfaces.front()->next_jog_mode ();
		_master_surface->next_jog_mode ();
	}
	return none;
}

US2400::LedState
US2400Protocol::scrub_release (US2400::Button &)
{
	return none;
}

LedState
US2400Protocol::undo_press (Button&)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		redo();
	} else {
		undo ();
	}
	return none;
}

LedState
US2400Protocol::undo_release (Button&)
{
	return none;
}

LedState
US2400Protocol::drop_press (Button &)
{
	_modifier_state |= MODIFIER_DROP;
printf("drop press, modifier drop state = %d\n", _modifier_state);

	return none;
}

LedState
US2400Protocol::drop_release (Button &)
{
	_modifier_state &= ~MODIFIER_DROP;
printf("drop release, modifier drop state = %d\n", _modifier_state);

	return none;
}

LedState
US2400Protocol::save_press (Button &)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		quick_snapshot_switch();
	} else {
		save_state ();
	}

	return none;
}

LedState
US2400Protocol::save_release (Button &)
{
	return none;
}

LedState
US2400Protocol::timecode_beats_press (Button &)
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
US2400Protocol::timecode_beats_release (Button &)
{
	return off;
}

/////////////////////////////////////
// Functions
/////////////////////////////////////
LedState
US2400Protocol::marker_press (Button &)
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
US2400Protocol::marker_release (Button &)
{
	_modifier_state &= ~MODIFIER_MARKER;

	if (main_modifier_state() & MODIFIER_SHIFT) {
		return off;   //if shift was held, we already did the action
	}

	if (marker_modifier_consumed_by_button) {
		DEBUG_TRACE (DEBUG::US2400, "marked modifier consumed by button, ignored\n");
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

/////////////////////////////////////
// Transport Buttons
/////////////////////////////////////

LedState
US2400Protocol::stop_press (Button &)
{
	transport_stop ();

	if (main_modifier_state() == MODIFIER_SHIFT) {
		session->midi_panic();
	}

	return on;
}

LedState
US2400Protocol::stop_release (Button &)
{
	return session->transport_stopped_or_stopping();
}

LedState
US2400Protocol::play_press (Button &)
{
	/* if we're already rolling at normal speed, and we're pressed
	   again, jump back to where we started last time
	*/

	transport_play (get_transport_speed() == 1.0);
	return none;
}

LedState
US2400Protocol::play_release (Button &)
{
	return none;
}

LedState
US2400Protocol::record_press (Button &)
{
	rec_enable_toggle ();
	return none;
}

LedState
US2400Protocol::record_release (Button &)
{
	return none;
}

LedState
US2400Protocol::rewind_press (Button &)
{
	if (modifier_state() & MODIFIER_MARKER) {
		prev_marker ();
	} else if ( (_modifier_state & MODIFIER_DROP) == MODIFIER_DROP) {
		access_action ("Common/start-range-from-playhead");
	} else if (main_modifier_state() & MODIFIER_SHIFT) {
		goto_start ();
	} else {
		rewind ();
	}
	return none;
}

LedState
US2400Protocol::rewind_release (Button &)
{
	return none;
}

LedState
US2400Protocol::ffwd_press (Button &)
{
	if (modifier_state() & MODIFIER_MARKER) {
		next_marker ();
	} else if ( (_modifier_state & MODIFIER_DROP) == MODIFIER_DROP) {
		access_action ("Common/finish-range-from-playhead");
	} else if (main_modifier_state() & MODIFIER_SHIFT) {
		goto_end();
	} else {
		ffwd ();
	}
	return none;
}

LedState
US2400Protocol::ffwd_release (Button &)
{
	return none;
}

LedState
US2400Protocol::loop_press (Button &)
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
US2400Protocol::loop_release (Button &)
{
	return none;
}

LedState
US2400Protocol::enter_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Transport/ToggleFollowEdits");
	} else {
		access_action ("Common/select-all-tracks");
	}
	return none;
}

LedState
US2400Protocol::enter_release (Button &)
{
	return none;
}

LedState
US2400Protocol::bank_release (Button& b, uint32_t basic_bank_num)
{
	if (_subview_mode != None) {
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
US2400Protocol::F1_press (Button &b)
{
	return on;
}
LedState
US2400Protocol::F1_release (Button &b)
{
	return off;
}
LedState
US2400Protocol::F2_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F2_release (Button &b)
{
	return off;
}
LedState
US2400Protocol::F3_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F3_release (Button &b)
{
	return off;
}
LedState
US2400Protocol::F4_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F4_release (Button &b)
{
	return off;
}
LedState
US2400Protocol::F5_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F5_release (Button &)
{
	return off;
}
LedState
US2400Protocol::F6_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F6_release (Button &)
{
	return off;
}
LedState
US2400Protocol::F7_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F7_release (Button &)
{
	return off;
}
LedState
US2400Protocol::F8_press (Button &)
{
	return on;
}
LedState
US2400Protocol::F8_release (Button &)
{
	return off;
}
*/


/* UNIMPLEMENTED */

LedState
US2400Protocol::pan_press (Button &)
{
	//US-2400: deselect all strips when the user asks for "Pan".  This resets us to default showing the panner only
	access_action ("Mixer/select-none");

	return none;
}
LedState
US2400Protocol::pan_release (Button &)
{
	return none;
}
LedState
US2400Protocol::plugin_press (Button &)
{
	return off;
}
LedState
US2400Protocol::plugin_release (Button &)
{
	// Do not do this yet, since it does nothing
	// set_view_mode (Plugins);
	return none; /* LED state set by set_view_mode */
}
LedState
US2400Protocol::eq_press (Button &)
{
	return none; /* led state handled by set_subview_mode() */

}
LedState
US2400Protocol::eq_release (Button &)
{
	return none;
}
LedState
US2400Protocol::dyn_press (Button &)
{
	return none; /* led state handled by set_subview_mode() */
}

LedState
US2400Protocol::dyn_release (Button &)
{
	return none;
}

LedState
US2400Protocol::flip_press (Button &)
{
	if (_view_mode == Busses) {
		set_view_mode (Mixer);
		return off;
	} else {
		set_view_mode (Busses);
		return on;
	}
}

LedState
US2400Protocol::flip_release (Button &)
{
	return none;
}

LedState
US2400Protocol::mstr_press (Button &)
{
//	access_action("Mixer/select-none");
	set_stripable_selection( session->master_out() );
	return on;
}

LedState
US2400Protocol::mstr_release (Button &)
{
	return none;
}

LedState
US2400Protocol::name_value_press (Button &)
{
	return off;
}
LedState
US2400Protocol::name_value_release (Button &)
{
	return off;
}
LedState
US2400Protocol::touch_press (Button &)
{
	return none;
}
LedState
US2400Protocol::touch_release (Button &)
{
	set_automation_state (ARDOUR::Touch);
	return none;
}
LedState
US2400Protocol::cancel_press (Button &)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Transport/ToggleExternalSync");
	} else {
		access_action ("Main/Escape");
	}
	return none;
}
LedState
US2400Protocol::cancel_release (Button &)
{
	return none;
}
LedState
US2400Protocol::user_a_press (Button &)
{
	transport_play (get_transport_speed() == 1.0);
	return off;
}
LedState
US2400Protocol::user_a_release (Button &)
{
	return off;
}
LedState
US2400Protocol::user_b_press (Button &)
{
	transport_stop();
	return off;
}
LedState
US2400Protocol::user_b_release (Button &)
{
	return off;
}

LedState
US2400Protocol::master_fader_touch_press (US2400::Button &)
{
	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::master_fader_touch_press\n");

	Fader* master_fader = _master_surface->master_fader();

	boost::shared_ptr<AutomationControl> ac = master_fader->control ();

	master_fader->set_in_use (true);
	master_fader->start_touch (timepos_t (transport_sample()));

	return none;
}
LedState
US2400Protocol::master_fader_touch_release (US2400::Button &)
{
	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::master_fader_touch_release\n");

	Fader* master_fader = _master_surface->master_fader();

	master_fader->set_in_use (false);
	master_fader->stop_touch (timepos_t (transport_sample()));

	return none;
}

US2400::LedState
US2400Protocol::read_press (US2400::Button&)
{
	return none;
}

US2400::LedState
US2400Protocol::read_release (US2400::Button&)
{
	set_automation_state (ARDOUR::Play);
	return none;
}
US2400::LedState
US2400Protocol::write_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::write_release (US2400::Button&)
{
	set_automation_state (ARDOUR::Write);
	return none;
}

US2400::LedState
US2400Protocol::clearsolo_press (US2400::Button&)
{
	// clears all solos and listens (pfl/afl)
	if (main_modifier_state() & MODIFIER_OPTION) {
		cancel_all_solo ();
	}

	return none;
}

US2400::LedState
US2400Protocol::clearsolo_release (US2400::Button&)
{
	//return session->soloing();
	return none;
}

US2400::LedState
US2400Protocol::track_press (US2400::Button&)
{
	set_subview_mode (TrackView, first_selected_stripable());
	return none;
}
US2400::LedState
US2400Protocol::track_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::send_press (US2400::Button&)
{
//	_modifier_state |= MODIFIER_AUX;  //US2400 ... AUX button is some kind of modifier 
//	return on;

//	set_subview_mode (Sends, first_selected_stripable());

	//DO NOTHING

	return none; /* led state handled by set_subview_mode() */
}
US2400::LedState
US2400Protocol::send_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::miditracks_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::miditracks_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::inputs_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::inputs_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::audiotracks_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::audiotracks_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::audioinstruments_press (US2400::Button& b)
{
	return none;
}

US2400::LedState
US2400Protocol::audioinstruments_release (US2400::Button& b)
{
	return none;

}
US2400::LedState
US2400Protocol::aux_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::aux_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::busses_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::busses_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::outputs_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::outputs_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::user_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::user_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::trim_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::trim_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::latch_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::latch_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::grp_press (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::grp_release (US2400::Button&)
{
	/* There is no "Off" button for automation,
	   so we use Group for this purpose.
	*/
	set_automation_state (Off);
	return none;
}
US2400::LedState
US2400Protocol::nudge_press (US2400::Button&)
{
//	_modifier_state |= MODIFIER_NUDGE;  //no such button on US2400
	nudge_modifier_consumed_by_button = false;
	return on;
}
US2400::LedState
US2400Protocol::nudge_release (US2400::Button&)
{
//	_modifier_state &= ~MODIFIER_NUDGE;  //no such button on US2400

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
US2400::LedState
US2400Protocol::replace_press (US2400::Button&)
{
	if (main_modifier_state() == MODIFIER_SHIFT) {
		toggle_punch_out();
		return none;
	} else {
		access_action ("Common/finish-range-from-playhead");
	}
	return none;
}
US2400::LedState
US2400Protocol::replace_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::click_press (US2400::Button&)
{
	if (main_modifier_state() & MODIFIER_SHIFT) {
		access_action ("Editor/set-punch-from-edit-range");
		return off;
	} else {
		bool state = !Config->get_clicking();
		Config->set_clicking (state);
		return state;
	}
}
US2400::LedState
US2400Protocol::click_release (US2400::Button&)
{
	return none;
}
US2400::LedState
US2400Protocol::view_press (US2400::Button&)
{
	set_view_mode (Mixer);
	return none;
}
US2400::LedState
US2400Protocol::view_release (US2400::Button&)
{
	return none;
}
