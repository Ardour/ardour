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

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/location.h"
#include "ardour/rc_configuration.h"

#include "mackie_control_protocol.h"

/* handlers for all buttons, broken into a separate file to avoid clutter in
 * mackie_control_protocol.cc 
 */

using namespace Mackie;
using namespace ARDOUR;

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
	return on;
}
LedState
MackieControlProtocol::control_release (Button &)
{
	_modifier_state &= ~MODIFIER_CONTROL;
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

/////////////////////////////////////
// Transport Buttons
/////////////////////////////////////

LedState 
MackieControlProtocol::frm_left_press (Button &)
{
	// can use first_mark_before/after as well
	unsigned long elapsed = _frm_left_last.restart();

	Location * loc = session->locations()->first_location_before (
		session->transport_frame()
	);

	// allow a quick double to go past a previous mark
	if (session->transport_rolling() && elapsed < 500 && loc != 0) {
		Location * loc_two_back = session->locations()->first_location_before (loc->start());
		if (loc_two_back != 0)
		{
			loc = loc_two_back;
		}
	}

	// move to the location, if it's valid
	if (loc != 0) {
		session->request_locate (loc->start(), session->transport_rolling());
	}

	return on;
}

LedState 
MackieControlProtocol::frm_left_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::frm_right_press (Button &)
{
	// can use first_mark_before/after as well
	Location * loc = session->locations()->first_location_after (session->transport_frame());
	
	if (loc != 0) {
		session->request_locate (loc->start(), session->transport_rolling());
	}
		
	return on;
}

LedState 
MackieControlProtocol::frm_right_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::stop_press (Button &)
{
	session->request_stop();
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
	session->request_transport_speed (1.0);
	return on;
}

LedState 
MackieControlProtocol::play_release (Button &)
{
	return session->transport_rolling();
}

LedState 
MackieControlProtocol::record_press (Button &)
{
	if (session->get_record_enabled()) {
		session->disable_record (false);
	} else {
		session->maybe_enable_record();
	}
	return on;
}

LedState 
MackieControlProtocol::record_release (Button &)
{
	if (session->get_record_enabled()) {
		if (session->transport_rolling()) {
			return on;
		} else {
			return flashing;
		}
	} else {
		return off;
	}
}

LedState 
MackieControlProtocol::rewind_press (Button &)
{
	_jog_wheel.push (JogWheel::speed);
	_jog_wheel.transport_direction (-1);
	session->request_transport_speed (-_jog_wheel.transport_speed());
	return on;
}

LedState 
MackieControlProtocol::rewind_release (Button &)
{
	_jog_wheel.pop();
	_jog_wheel.transport_direction (0);
	if (_transport_previously_rolling) {
		session->request_transport_speed (1.0);
	} else {
		session->request_stop();
	}
	return off;
}

LedState 
MackieControlProtocol::ffwd_press (Button &)
{
	_jog_wheel.push (JogWheel::speed);
	_jog_wheel.transport_direction (1);
	session->request_transport_speed (_jog_wheel.transport_speed());
	return on;
}

LedState 
MackieControlProtocol::ffwd_release (Button &)
{
	_jog_wheel.pop();
	_jog_wheel.transport_direction (0);
	if (_transport_previously_rolling) {
		session->request_transport_speed (1.0);
	} else {
		session->request_stop();
	}
	return off;
}

LedState 
MackieControlProtocol::loop_press (Button &)
{
	session->request_play_loop (!session->get_play_loop());
	return on;
}

LedState 
MackieControlProtocol::loop_release (Button &)
{
	return session->get_play_loop();
}

LedState 
MackieControlProtocol::punch_in_press (Button &)
{
	bool const state = !session->config.get_punch_in();
	session->config.set_punch_in (state);
	return state;
}

LedState 
MackieControlProtocol::punch_in_release (Button &)
{
	return session->config.get_punch_in();
}

LedState 
MackieControlProtocol::punch_out_press (Button &)
{
	bool const state = !session->config.get_punch_out();
	session->config.set_punch_out (state);
	return state;
}

LedState 
MackieControlProtocol::punch_out_release (Button &)
{
	return session->config.get_punch_out();
}

LedState 
MackieControlProtocol::home_press (Button &)
{
	session->goto_start();
	return on;
}

LedState 
MackieControlProtocol::home_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::end_press (Button &)
{
	session->goto_end();
	return on;
}

LedState 
MackieControlProtocol::end_release (Button &)
{
	return off;
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

/* UNIMPLEMENTED */

LedState
MackieControlProtocol::io_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::io_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::sends_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::sends_release (Button &) 
{ 
	return off; 
}
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
	return off; 
}
LedState
MackieControlProtocol::eq_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::dyn_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::dyn_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::flip_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::flip_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::edit_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::edit_release (Button &) 
{ 
	return off; 
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
	return off; 
}
LedState
MackieControlProtocol::F8_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F9_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F9_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F10_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F10_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F11_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F11_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F12_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F12_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F13_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F13_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F14_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F14_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F15_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F15_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F16_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::F16_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::on_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::on_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::rec_ready_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::rec_ready_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::snapshot_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::snapshot_release (Button &) 
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
MackieControlProtocol::mixer_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::mixer_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::user_a_press (Button &) 
{ 
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
	return off; 
}
LedState
MackieControlProtocol::user_b_release (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::fader_touch_press (Button &) 
{ 
	return off; 
}
LedState
MackieControlProtocol::fader_touch_release (Button &) 
{ 
	return off; 
}
