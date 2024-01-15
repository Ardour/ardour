/*
 * Copyright (C) 2023 Holger Dehnhardt <holger@dehnhardt.org>
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

#include "ardour/meter.h"
#include "ardour/monitor_control.h"
#include "ardour/phase_control.h"
#include "ardour/presentation_info.h"
#include "ardour/session.h"
#include "ardour/well_known_enum.h"

#include "c1_control.h"
#include "console1.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;

/* Operations */

void
Console1::bank (bool up)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::page\n");
	bool changed = false;
	uint32_t list_size = strip_inventory.size ();

	if (up) {
		if ((current_bank + 1) * bank_size < list_size) {
			changed = true;
			++current_bank;
			current_strippable_index = 0;
		}
	} else {
		if (current_bank > 0) {
			changed = true;
			--current_bank;
			current_strippable_index = bank_size - 1;
		}
	}
	if (changed) {
		uint32_t new_index = current_bank * bank_size + current_strippable_index;
		if (new_index > (list_size - 1))
			new_index = list_size - 1;
		select_rid_by_index (new_index);
		BankChange ();
	}
}

void
Console1::gain (const uint32_t value)
{
	if (!_current_stripable) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

void
Console1::mute (const uint32_t)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::mute ...\n");
	if (!_current_stripable) {
		return;
	}

	if (_current_stripable == session->monitor_out ()) {
		std::shared_ptr<MonitorProcessor> mp = _current_stripable->monitor_control ();
		mp->set_cut_all (!mp->cut_all ());
		return;
	}

	_current_stripable->mute_control ()->set_value (!_current_stripable->mute_control ()->muted (),
	                                                PBD::Controllable::UseGroup);
}

void
Console1::pan (const uint32_t value)
{
	if (!_current_stripable) {
		return;
	}
	if (current_pan_control) {
		std::shared_ptr<AutomationControl> control = current_pan_control;
		double pan = midi_to_control (control, value);
		session->set_control (control, pan, PBD::Controllable::UseGroup);
	}
}

void
Console1::phase (const uint32_t value)
{
	DEBUG_TRACE (DEBUG::Console1, "phase() \n");
	if (!_current_stripable || !_current_stripable->phase_control ()) {
		return;
	}
	bool inverted = _current_stripable->phase_control ()->inverted (0);
	for (uint64_t i = 0; i < _current_stripable->phase_control ()->size (); i++) {
		_current_stripable->phase_control ()->set_phase_invert (i, !inverted);
	}
}

void
Console1::rude_solo (const uint32_t value)
{

	DEBUG_TRACE (DEBUG::Console1, "rude_solo() \n");
	if (!value) {
		session->cancel_all_solo ();
	} else {
		try {
			get_button (ControllerID::DISPLAY_ON)->set_led_state (false);
		} catch (ControlNotFoundException const&) {
			DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
		}
	}
}

void
Console1::select (const uint32_t i)
{
	uint32_t strip_index = current_bank * bank_size + i;
	DEBUG_TRACE (DEBUG::Console1, string_compose ("select( %1 / %2 ) : idx %3\n", current_bank, i, strip_index));
	select_rid_by_index (strip_index);
}

void
Console1::shift (const uint32_t val)
{
	DEBUG_TRACE (DEBUG::Console1, "shift()\n");
	shift_state = !shift_state;
	ShiftChange (val);
}

void
Console1::plugin_state (const uint32_t)
{
	DEBUG_TRACE (DEBUG::Console1, "plugin_state()\n");
	in_plugin_state = !in_plugin_state;
	PluginStateChange (in_plugin_state);
}

void
Console1::solo (const uint32_t)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::solo())\n");
	if (!_current_stripable) {
		return;
	}

	session->set_control (_current_stripable->solo_control (),
	                      !_current_stripable->solo_control ()->self_soloed (),
	                      PBD::Controllable::UseGroup);
}

void
Console1::trim (const uint32_t value)
{
	if (!_current_stripable) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->trim_control ();
	double trim = midi_to_control (control, value);
	session->set_control (control, trim, PBD::Controllable::UseGroup);
}

void
Console1::window (const uint32_t value)
{
	DEBUG_TRACE (DEBUG::Console1, "window()\n");
	switch (value) {
		case 0:
			access_action ("Common/show-editor");
			break;
		case 63:
			access_action ("Common/show-mixer");
			break;
		case 127:
			access_action ("Common/show-trigger");
			break;
		default:
			break;
	}
}

void
Console1::zoom (const uint32_t value)
{
	DEBUG_TRACE (DEBUG::Console1, "zoom()\n");
	access_action ("Editor/zoom-to-selection");
}

// Filter Section
void
Console1::filter (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (HPF_Enable))
		return;
	session->set_control (
	  _current_stripable->mapped_control (HPF_Enable), value > 0, PBD::Controllable::UseGroup);
}

void
Console1::low_cut (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (HPF_Freq)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (HPF_Freq);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::high_cut (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (LPF_Freq)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (LPF_Freq);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

// Gate Section
void
Console1::gate (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Enable))
		return;
	session->set_control (_current_stripable->mapped_control (Gate_Enable), value > 0, PBD::Controllable::UseGroup);
}

void
Console1::gate_scf (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_KeyFilterEnable))
		return;
	session->set_control (
	  _current_stripable->mapped_control (Gate_KeyFilterEnable), value > 0, PBD::Controllable::UseGroup);
}

void
Console1::gate_listen (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_KeyListen))
		return;
	session->set_control (_current_stripable->mapped_control (Gate_KeyListen), value > 0, PBD::Controllable::UseGroup);
}

void
Console1::gate_thresh (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Threshold)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Threshold);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::gate_depth (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Depth)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Depth);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::gate_release (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Release)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Release);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::gate_attack (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Attack)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Attack);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::gate_hyst (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Hysteresis)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Hysteresis);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::gate_hold (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_Hold)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Hold);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::gate_filter_freq (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Gate_KeyFilterFreq)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_KeyFilterFreq);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

// EQ-Section

void
Console1::eq (const uint32_t value)
{
	DEBUG_TRACE (DEBUG::Console1, "EQ ...\n");
	if (!_current_stripable) {
		return;
	}
	if (_current_stripable->mapped_control (EQ_Enable))
		session->set_control (_current_stripable->mapped_control (EQ_Enable), value > 0, PBD::Controllable::UseGroup);
	else
		map_eq ();
}

void
Console1::eq_low_shape (const uint32_t value)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("EQ eq_low_shape( %1 )\n", value));
	if (!_current_stripable) {
		return;
	}
	if (_current_stripable->mapped_control (EQ_BandShape, 0))
		session->set_control (_current_stripable->mapped_control (EQ_BandShape, 0), value > 0, PBD::Controllable::UseGroup);
	else
		map_eq_low_shape ();
}

void
Console1::eq_high_shape (const uint32_t value)
{
	DEBUG_TRACE (DEBUG::Console1, "EQ eq_high_shape...\n");
	if (!_current_stripable) {
		return;
	}
	if (_current_stripable->mapped_control (EQ_BandShape, 3))
		session->set_control (_current_stripable->mapped_control (EQ_BandShape, 3), value > 0, PBD::Controllable::UseGroup);
	else
		map_eq_high_shape ();
}

void
Console1::eq_freq (const uint32_t band, uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (EQ_BandFreq, band)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (EQ_BandFreq, band);
	double freq = midi_to_control (control, value);
	session->set_control (control, freq, PBD::Controllable::UseGroup);
}

void
Console1::eq_gain (const uint32_t band, uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (EQ_BandGain, band)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (EQ_BandGain, band);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

// The Mixbus-Sends are in the EQ section
// Without Shift:
// LowMid Shape is Send 11
// HighMid Shape is Send 12
//
// With Shift
// LowMid Shape is Send 9
// HighMid Shape is Send 10
// And the rest is
// Send 01  02  03  04
// Send 05  06  07  08

void
Console1::mb_send_level (const uint32_t n, const uint32_t value)
{
	uint32_t n_offset = n;
#ifdef MIXBUS
	if (_current_stripable->presentation_info ().flags () & PresentationInfo::Flag::Mixbus) {
		n_offset = n - 8;
	}
#endif
	if (!_current_stripable || !_current_stripable->send_level_controllable (n_offset)) {
		return;
	}

	std::shared_ptr<AutomationControl> control = _current_stripable->send_level_controllable (n_offset);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
	if (value == 0) {
		std::shared_ptr<AutomationControl> enable_control = _current_stripable->send_enable_controllable (n_offset);
		if (enable_control) {
			session->set_control (enable_control, 0, PBD::Controllable::UseGroup);
		}
	}
}

// Drive Button
void
Console1::drive (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (TapeDrive_Drive)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (TapeDrive_Drive);
	if (_current_stripable->presentation_info ().flags () & PresentationInfo::AudioTrack) {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("drive audio track %1\n", value));
		session->set_control (control, value > 62 ? 1 : 0, PBD::Controllable::UseGroup);
	} else {
		double gain = midi_to_control (control, value);
		session->set_control (control, gain, PBD::Controllable::UseGroup);
	}
}

// Comp Section
void
Console1::comp (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Enable))
		return;
	session->set_control (_current_stripable->mapped_control (Comp_Enable), value > 0, PBD::Controllable::UseGroup);
}

void
Console1::comp_mode (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Mode))
		return;
	int new_val = value == 63 ? 1 : value == 127 ? 2 : 0;
	session->set_control (_current_stripable->mapped_control (Comp_Mode), new_val, PBD::Controllable::UseGroup);
}

void
Console1::comp_thresh (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Threshold)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Threshold);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

void
Console1::comp_attack (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Attack)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Attack);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

void
Console1::comp_release (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Release)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Release);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

void
Console1::comp_ratio (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Ratio)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Ratio);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

void
Console1::comp_makeup (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_Makeup)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Makeup);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

void
Console1::comp_emph (const uint32_t value)
{
	if (!_current_stripable || !_current_stripable->mapped_control (Comp_KeyFilterFreq)) {
		return;
	}
	std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_KeyFilterFreq);
	double gain = midi_to_control (control, value);
	session->set_control (control, gain, PBD::Controllable::UseGroup);
}

/* **********************************************************
************************** Mappings *************************
*************************************************************/
void
Console1::map_bank ()
{
	uint32_t list_size = strip_inventory.size ();
	try {
		get_button (PAGE_UP)->set_led_state (list_size > (current_bank + 1) * bank_size);
		get_button (PAGE_DOWN)->set_led_state (current_bank > 0);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_gain ()
{
	ControllerID controllerID = ControllerID::VOLUME;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
		map_encoder (controllerID, control);
	}
}

void
Console1::map_monitoring ()
{
	if (_current_stripable && _current_stripable->monitoring_control ()) {
		std::shared_ptr<MonitorControl> control = _current_stripable->monitoring_control ();
		monitor_state = control->monitoring_state ();
	} else {
		monitor_state = ARDOUR::MonitorState::MonitoringSilence;
	}
}

void
Console1::map_mute ()
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::map_mute ...\n");
	if (_current_stripable) {
		if (_current_stripable->mute_control ()->muted ()) {
			try {
				get_button (swap_solo_mute ? SOLO : MUTE)->set_led_state (true);
			} catch (ControlNotFoundException const&) {
				DEBUG_TRACE (DEBUG::Console1, "solo/mute button not found\n");
			}
		} else if (_current_stripable->mute_control ()->muted_by_others_soloing () ||
				_current_stripable->mute_control ()->muted_by_masters ()) {

			DEBUG_TRACE (DEBUG::Console1, "Console1::map_mute start blinking\n");
			start_blinking (swap_solo_mute ? SOLO : MUTE);
		} else {
			DEBUG_TRACE (DEBUG::Console1, "Console1::map_mute stop blinking\n");
			stop_blinking (swap_solo_mute ? SOLO : MUTE);
		}
	} else {
		DEBUG_TRACE (DEBUG::Console1, "Console1::map_mute stop blinking 2\n");
		stop_blinking (swap_solo_mute ? SOLO : MUTE);
	}
}

void
Console1::map_pan ()
{
	ControllerID controllerID = ControllerID::PAN;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = current_pan_control;
		map_encoder (controllerID, control);
	}
}

void
Console1::map_phase ()
{
	DEBUG_TRACE (DEBUG::Console1, "map_phase \n");
	ControllerButton* controllerButton = get_button (PHASE_INV);
	if (_current_stripable && _current_stripable->phase_control ()) {
		uint32_t channels = _current_stripable->phase_control ()->size ();
		uint32_t inverted = 0;
		for (uint32_t i = 0; i < channels; ++i) {
			if (_current_stripable->phase_control ()->inverted (i))
				++inverted;
		}
		if (inverted == 0) {
			stop_blinking (PHASE_INV);
			controllerButton->set_led_state (false);
		} else if (inverted == channels) {
			stop_blinking (PHASE_INV);
			controllerButton->set_led_state (true);
		} else
			start_blinking (PHASE_INV);
	} else {
		controllerButton->set_led_state (false);
	}
}

void
Console1::map_recenable ()
{
	DEBUG_TRACE (DEBUG::Console1, "map_recenable()\n");
	if (!_current_stripable)
		strip_recenabled = false;
	else if (_current_stripable->rec_enable_control ()) {
		strip_recenabled = _current_stripable->rec_enable_control ()->get_value ();
	}
}

void
Console1::map_select ()
{
	DEBUG_TRACE (DEBUG::Console1, "map_select())\n");
	for (uint32_t i = 0; i < bank_size; ++i) {
		get_button (ControllerID (FOCUS1 + i))->set_led_state (i == current_strippable_index);
	}
}

void
Console1::map_shift (bool shift)
{
	DEBUG_TRACE (DEBUG::Console1, "map_shift()\n");
	try {
		ControllerButton* controllerButton = get_button (PRESET);
		controllerButton->set_led_state (shift);
		map_stripable_state ();
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_plugin_state (bool plugin_state)
{
	DEBUG_TRACE (DEBUG::Console1, "map_plugin_state()\n");
	try {
		ControllerButton* controllerButton = get_button (TRACK_GROUP);
		controllerButton->set_led_state (in_plugin_state);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
	if (!plugin_state) {
		for (uint32_t i = 0; i < bank_size; ++i) {
			stop_blinking (ControllerID (FOCUS1 + i));
		}
		map_stripable_state ();
	} else {
		// I don't plan shift functionality with plugins...
		shift (0);
		// map all plugin related operations
	}
}

void
Console1::map_solo ()
{
	DEBUG_TRACE (DEBUG::Console1, "map_solo()\n");
	try {
		ControllerButton* controllerButton = get_button (swap_solo_mute ? MUTE : SOLO);
		if (_current_stripable) {
			controllerButton->set_led_state (_current_stripable->solo_control ()->soloed ());
		} else {
			controllerButton->set_led_state (false);
		}
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_trim ()
{
	ControllerID controllerID = ControllerID::GAIN;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->trim_control ();
		map_encoder (controllerID, control);
	}
}

// Filter Section
void
Console1::map_filter ()
{
	if (!_current_stripable) {
		return;
	}
	try {
		get_button (ControllerID::FILTER_TO_COMPRESSORS)
		  ->set_led_state (_current_stripable->mapped_control (HPF_Enable)
		                     ? _current_stripable->mapped_control (HPF_Enable)->get_value ()
		                     : false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_low_cut ()
{
	ControllerID controllerID = ControllerID::LOW_CUT;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (HPF_Freq);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_high_cut ()
{
	ControllerID controllerID = ControllerID::HIGH_CUT;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (LPF_Freq);
		map_encoder (controllerID, control);
	}
}

// Gate Section
void
Console1::map_gate ()
{
	if (!_current_stripable)
		return;
	try {
		get_button (ControllerID::SHAPE)
		  ->set_led_state (_current_stripable->mapped_control (Gate_Enable)
		                     ? _current_stripable->mapped_control (Gate_Enable)->get_value ()
		                     : false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_gate_scf ()
{
	if (!_current_stripable || shift_state)
		return;
	try {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("map_gate_scf() - shift: %1\n", shift_state));
		get_button (ControllerID::HARD_GATE)
		  ->set_led_state (_current_stripable->mapped_control (Gate_KeyFilterEnable)
		                     ? _current_stripable->mapped_control (Gate_KeyFilterEnable)->get_value ()
		                     : false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_gate_listen ()
{
	if (!_current_stripable || !shift_state)
		return;
	try {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("map_gate_listen() - shift: %1\n", shift_state));
		get_button (ControllerID::HARD_GATE)
		  ->set_led_state (_current_stripable->mapped_control (Gate_KeyListen)
		                     ? _current_stripable->mapped_control (Gate_KeyListen)->get_value ()
		                     : false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_gate_thresh ()
{
	ControllerID controllerID = ControllerID::SHAPE_GATE;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Threshold);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_gate_release ()
{
	if (shift_state) {
		return;
	}
	ControllerID controllerID = ControllerID::SHAPE_RELEASE;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Release);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_gate_attack ()
{
	if (shift_state) {
		return;
	}
	ControllerID controllerID = ControllerID::SHAPE_SUSTAIN;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Attack);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_gate_depth ()
{
	if (shift_state) {
		return;
	}
	ControllerID controllerID = ControllerID::SHAPE_PUNCH;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Depth);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_gate_hyst ()
{
	if (!shift_state) {
		return;
	}
	ControllerID controllerID = ControllerID::SHAPE_RELEASE;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Hysteresis);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_gate_hold ()
{
	if (!shift_state) {
		return;
	}
	ControllerID controllerID = ControllerID::SHAPE_SUSTAIN;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_Hold);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_gate_filter_freq ()
{
	if (!shift_state) {
		return;
	}
	ControllerID controllerID = ControllerID::SHAPE_PUNCH;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Gate_KeyFilterFreq);
		map_encoder (controllerID, control);
	}
}

// EQ Section
void
Console1::map_eq ()
{
	if (!_current_stripable)
		return;
	try {
		get_button (EQ)->set_led_state (_current_stripable->mapped_control (EQ_Enable)
		                                  ? _current_stripable->mapped_control (EQ_Enable)->get_value ()
		                                  : false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_eq_freq (const uint32_t band)
{
	if (shift_state) {
		return;
	}
	ControllerID controllerID = eq_freq_controller_for_band (band);
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (EQ_BandFreq, band);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_eq_gain (const uint32_t band)
{
	if (shift_state) {
		return;
	}
	ControllerID controllerID = eq_gain_controller_for_band (band);
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (EQ_BandGain, band);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_eq_low_shape ()
{
	if (!_current_stripable)
		return;
	try {
		uint32_t led_value = _current_stripable->mapped_control (EQ_BandShape, 0)
		                       ? _current_stripable->mapped_control (EQ_BandShape, 0)->get_value () == 0 ? 0 : 63
		                       : 0;
		get_button (ControllerID::LOW_SHAPE)->set_led_state (led_value);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_eq_high_shape ()
{
	if (!_current_stripable)
		return;
	try {
		uint32_t led_value = _current_stripable->mapped_control (EQ_BandShape, 3)
		                       ? _current_stripable->mapped_control (EQ_BandShape, 3)->get_value () == 0 ? 0 : 63
		                       : 0;
		get_button (ControllerID::HIGH_SHAPE)->set_led_state (led_value);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

// Drive
void
Console1::map_drive ()
{
	ControllerID controllerID = ControllerID::CHARACTER;

	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (TapeDrive_Drive);
		if (control && _current_stripable->presentation_info ().flags () & PresentationInfo::AudioTrack) {
			double val = control->get_value ();
			DEBUG_TRACE (DEBUG::Console1, string_compose ("map_drive audio track %1\n", val));
			try {
				get_encoder (controllerID)->set_value (val == 1 ? 127 : 0);
			} catch (ControlNotFoundException const&) {
				DEBUG_TRACE (DEBUG::Console1, "Encoder not found\n");
			}
		} else {
			map_encoder (controllerID, control);
		}
	}
}

// Sends
void
Console1::map_mb_send_level (const uint32_t n)
{
	uint32_t n_offset = n;
#ifdef MIXBUS
	if (_current_stripable->presentation_info ().flags () & PresentationInfo::Flag::Mixbus) {
		n_offset = n + 8;
	}
#endif
	// Theese two sends are available in non-shift state
	if (n_offset > 9 && shift_state) {
		return;
	} else if (n_offset < 10 && !shift_state) // while the rest needs the shift state
	{
		return;
	}
	ControllerID controllerID = get_send_controllerid (n_offset);
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->send_level_controllable (n);
		map_encoder (controllerID, control);
	}
}

// Comp Section
void
Console1::map_comp ()
{
	if (!_current_stripable)
		return;
	try {
		get_button (ControllerID::COMP)
		  ->set_led_state (_current_stripable->mapped_control (Comp_Enable)
		                     ? _current_stripable->mapped_control (Comp_Enable)->get_value ()
		                     : false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_comp_mode ()
{
	if (!_current_stripable)
		return;
	try {
		double value = _current_stripable->mapped_control (Comp_Mode)
		                 ? _current_stripable->mapped_control (Comp_Mode)->get_value ()
		                 : false;
		DEBUG_TRACE (DEBUG::Console1, string_compose ("****value from comp-type %1\n", value));
		get_mbutton (ControllerID::ORDER)->set_led_state (value);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button not found\n");
	}
}

void
Console1::map_comp_thresh ()
{
	ControllerID controllerID = ControllerID::COMP_THRESH;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Threshold);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_comp_attack ()
{
	ControllerID controllerID = ControllerID::COMP_ATTACK;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Attack);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_comp_release ()
{
	ControllerID controllerID = ControllerID::COMP_RELEASE;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Release);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_comp_ratio ()
{
	ControllerID controllerID = ControllerID::COMP_RATIO;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Ratio);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_comp_makeup ()
{
	ControllerID controllerID = ControllerID::COMP_PAR;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_Makeup);
		map_encoder (controllerID, control);
	}
}

void
Console1::map_comp_emph ()
{
	ControllerID controllerID = ControllerID::DRIVE;
	if (map_encoder (controllerID)) {
		std::shared_ptr<AutomationControl> control = _current_stripable->mapped_control (Comp_KeyFilterFreq);
		map_encoder (controllerID, control);
	}
}

bool
Console1::map_encoder (ControllerID controllerID)
{
	if (!_current_stripable) {
		try {
			get_encoder (controllerID)->set_value (0);
		} catch (ControlNotFoundException const&) {
			DEBUG_TRACE (DEBUG::Console1, "Encoder not found\n");
			return false;
		}
		return false;
	}
	return true;
}

void
Console1::map_encoder (ControllerID controllerID, std::shared_ptr<ARDOUR::AutomationControl> control)
{

	if (!_current_stripable) {
		try {
			get_encoder (controllerID)->set_value (0);
		} catch (ControlNotFoundException const&) {
			DEBUG_TRACE (DEBUG::Console1, "Encoder not found\n");
		}
		return;
	}

	double val;
	double gain;

	if (!control) {
		val = 0.0;
	} else {
		val = control->get_value ();
		gain = control_to_midi (control, val);
	}
	try {
		get_encoder (controllerID)->set_value (gain);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Encoder not found\n");
	}
}
