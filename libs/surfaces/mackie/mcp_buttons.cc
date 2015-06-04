/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#include <algorithm>

#include "pbd/memento_command.h"

#include "ardour/debug.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/location.h"
#include "ardour/rc_configuration.h"

#include "mackie_control_protocol.h"
#include "surface.h"
#include "fader.h"

#include "i18n.h"

/* handlers for all buttons, broken into a separate file to avoid clutter in
 * mackie_control_protocol.cc 
 */

using std::string;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;
using namespace Mackie;

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
	return on;
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
	return on;
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
	return on;
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
	return on;
}

LedState 
MackieControlProtocol::left_press (Button &)
{
	Sorted sorted = get_sorted_routes();
	uint32_t strip_cnt = n_strips (); 

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("bank left with current initial = %1 nstrips = %2 tracks/busses = %3\n",
							   _current_initial_bank, strip_cnt, sorted.size()));
	if (_current_initial_bank > 0) {
		switch_banks ((_current_initial_bank - 1) / strip_cnt * strip_cnt);
	} else {
		switch_banks (0);
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
	Sorted sorted = get_sorted_routes();
	uint32_t strip_cnt = n_strips();
	uint32_t route_cnt = sorted.size();
	uint32_t max_bank = route_cnt / strip_cnt * strip_cnt;


	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("bank right with current initial = %1 nstrips = %2 tracks/busses = %3\n",
							   _current_initial_bank, strip_cnt, route_cnt));

	if (_current_initial_bank < max_bank) {
		uint32_t new_initial = (_current_initial_bank / strip_cnt * strip_cnt) + strip_cnt;

		switch_banks (new_initial);
	} else {
		switch_banks (max_bank);
	}

	return on;
}

LedState 
MackieControlProtocol::right_release (Button &)
{
	if (_zoom_mode) {

	}

	return off;
}

LedState
MackieControlProtocol::cursor_left_press (Button& )
{
	if (_zoom_mode) {

		if (_modifier_state & MODIFIER_OPTION) {
			/* reset selected tracks to default vertical zoom */
		} else {
			ZoomOut (); /* EMIT SIGNAL */
		}
	} else {
		float page_fraction;
		if (_modifier_state == MODIFIER_CONTROL) {
			page_fraction = 1.0;
		} else if (_modifier_state == MODIFIER_OPTION) {
			page_fraction = 0.1;
		} else if (_modifier_state == MODIFIER_SHIFT) {
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
	if (_zoom_mode) {
		
		if (_modifier_state & MODIFIER_OPTION) {
			/* reset selected tracks to default vertical zoom */
		} else {
			ZoomIn (); /* EMIT SIGNAL */
		}
	} else {
		float page_fraction;
		if (_modifier_state == MODIFIER_CONTROL) {
			page_fraction = 1.0;
		} else if (_modifier_state == MODIFIER_OPTION) {
			page_fraction = 0.1;
		} else if (_modifier_state == MODIFIER_SHIFT) {
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
	if (_zoom_mode) {
		
		if (_modifier_state & MODIFIER_CONTROL) {
			VerticalZoomInSelected (); /* EMIT SIGNAL */
		} else {
			VerticalZoomInAll (); /* EMIT SIGNAL */
		}
	} else {
		StepTracksUp (); /* EMIT SIGNAL */
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
	if (_zoom_mode) {
		if (_modifier_state & MODIFIER_OPTION) {
			VerticalZoomOutSelected (); /* EMIT SIGNAL */
		} else {
			VerticalZoomOutAll (); /* EMIT SIGNAL */
		}
	} else {
		StepTracksDown (); /* EMIT SIGNAL */
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
	Sorted sorted = get_sorted_routes();
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
	Sorted sorted = get_sorted_routes();
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

Mackie::LedState 
MackieControlProtocol::zoom_press (Mackie::Button &)
{
	_zoom_mode = !_zoom_mode;
	return (_zoom_mode ? on : off);
}

Mackie::LedState 
MackieControlProtocol::zoom_release (Mackie::Button &)
{
	return (_zoom_mode ? on : off);
}

Mackie::LedState 
MackieControlProtocol::scrub_press (Mackie::Button &)
{
	if (!surfaces.empty()) {
		surfaces.front()->next_jog_mode ();
	}
	return none;
}

Mackie::LedState 
MackieControlProtocol::scrub_release (Mackie::Button &)
{
	return none;
}

LedState
MackieControlProtocol::undo_press (Button&)
{
	if (_modifier_state & MODIFIER_SHIFT) {
		Redo(); /* EMIT SIGNAL */
	} else {
		Undo(); /* EMIT SIGNAL */
	}
	return off;
}

LedState
MackieControlProtocol::undo_release (Button&)
{
	return off;
}

LedState 
MackieControlProtocol::drop_press (Button &)
{
	session->remove_last_capture();
	return on;
}

LedState 
MackieControlProtocol::drop_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::save_press (Button &)
{
	session->save_state ("");
	return on;
}

LedState 
MackieControlProtocol::save_release (Button &)
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
	string markername;

	session->locations()->next_available_name (markername,"mcu");
	add_marker (markername);

	return on;
}

LedState 
MackieControlProtocol::marker_release (Button &)
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
	return on;
}

LedState 
MackieControlProtocol::stop_release (Button &)
{
	return session->transport_stopped();
}

LedState 
MackieControlProtocol::play_press (Button &)
{
	/* if we're already rolling at normal speed, and we're pressed
	   again, jump back to where we started last time
	*/

	transport_play (session->transport_speed() == 1.0);
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
	if (_modifier_state == MODIFIER_CONTROL) {
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
	if (_modifier_state == MODIFIER_CONTROL) {
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
	if (_modifier_state & MODIFIER_CONTROL) {
		set_view_mode (Loop);
		return on;
	} else {
		session->request_play_loop (!session->get_play_loop());
		return none;
	}
}

LedState 
MackieControlProtocol::loop_release (Button &)
{
	return none;
}

LedState 
MackieControlProtocol::clicking_press (Button &)
{
	bool state = !Config->get_clicking();
	Config->set_clicking (state);
	return state;
}

LedState 
MackieControlProtocol::clicking_release (Button &)
{
	return Config->get_clicking();
}

LedState MackieControlProtocol::global_solo_press (Button &)
{
	bool state = !session->soloing();
	session->set_solo (session->get_routes(), state);
	return state;
}

LedState MackieControlProtocol::global_solo_release (Button &)
{
	return session->soloing();
}

LedState
MackieControlProtocol::enter_press (Button &) 
{ 
	Enter(); /* EMIT SIGNAL */
	return off;
}

LedState
MackieControlProtocol::enter_release (Button &) 
{ 
	return off;
}

LedState
MackieControlProtocol::F1_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F1_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F2_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F2_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F3_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F3_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F4_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F4_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F5_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F5_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F6_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F6_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F7_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F7_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F8_press (Button &) 
{ 
	CloseDialog (); /* EMIT SIGNAL */
	return off; 
}
LedState
MackieControlProtocol::F8_release (Button &) 
{ 
	return off; 
}

/* UNIMPLEMENTED */

LedState
MackieControlProtocol::pan_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::pan_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::plugin_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::plugin_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::eq_press (Button &) 
{ 
	set_view_mode (EQ);
	return on;
}
LedState
MackieControlProtocol::eq_release (Button &) 
{ 
	return none;
}
LedState
MackieControlProtocol::dyn_press (Button &) 
{ 
	set_view_mode (Dynamics);
	return on;
}
LedState
MackieControlProtocol::dyn_release (Button &) 
{ 
	return none;
}
LedState
MackieControlProtocol::flip_press (Button &) 
{ 
	if (_flip_mode != Normal) {
		set_flip_mode (Normal);
	} else {
		set_flip_mode (Mirror);
	}
	return ((_flip_mode != Normal) ? on : off);
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
	return off; 
}
LedState
MackieControlProtocol::touch_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::cancel_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::cancel_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::user_a_press (Button &) 
{ 
	transport_play (session->transport_speed() == 1.0);
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
MackieControlProtocol::master_fader_touch_press (Mackie::Button &)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::master_fader_touch_press\n");

	Fader* master_fader = surfaces.front()->master_fader();

	boost::shared_ptr<AutomationControl> ac = master_fader->control ();

	master_fader->set_in_use (true);
	master_fader->start_touch (transport_frame());

	return none;
}
LedState
MackieControlProtocol::master_fader_touch_release (Mackie::Button &)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::master_fader_touch_release\n");

	Fader* master_fader = surfaces.front()->master_fader();

	master_fader->set_in_use (false);
	master_fader->stop_touch (transport_frame(), true);

	return none;
}

Mackie::LedState 
MackieControlProtocol::read_press (Mackie::Button&) 
{
	_metering_active = !_metering_active;
	notify_metering_state_changed ();
	return _metering_active;
}
Mackie::LedState 
MackieControlProtocol::read_release (Mackie::Button&) 
{
	return _metering_active;
}
Mackie::LedState 
MackieControlProtocol::write_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::write_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::clearsolo_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::clearsolo_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::track_press (Mackie::Button&) 
{

	return none;
}
Mackie::LedState 
MackieControlProtocol::track_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::send_press (Mackie::Button&) 
{
// code moved here from "sends_press"
	set_view_mode (Sends);
	return on;
//	return none;
}
Mackie::LedState 
MackieControlProtocol::send_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::miditracks_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::miditracks_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::inputs_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::inputs_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::audiotracks_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::audiotracks_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::audioinstruments_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::audioinstruments_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::aux_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::aux_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::busses_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::busses_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::outputs_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::outputs_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::user_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::user_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::trim_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::trim_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::latch_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::latch_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::grp_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::grp_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::nudge_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::nudge_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::replace_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::replace_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::click_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::click_release (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::view_press (Mackie::Button&) 
{
	return none;
}
Mackie::LedState 
MackieControlProtocol::view_release (Mackie::Button&) 
{
	return none;
}
