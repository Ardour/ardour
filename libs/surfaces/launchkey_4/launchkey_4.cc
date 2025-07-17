/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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
#include <bitset>
#include <cmath>
#include <limits>
#include <regex>

#include <stdlib.h>
#include <pthread.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"

#include "temporal/time.h"
#include "temporal/bbt_time.h"

#include "ardour/amp.h"
#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/internal_send.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/triggerbox.h"
#include "ardour/types_convert.h"
#include "ardour/utils.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "gtkmm2ext/colors.h"

#include "gui.h"
#include "launchkey_4.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourSurface::LAUNCHPAD_NAMESPACE;
using namespace Gtkmm2ext;

#include "pbd/abstract_ui.inc.cc" // instantiate template

/* USB IDs */

#define NOVATION             0x1235

#define LAUNCHKEY4_MINI_25   0x0141
#define LAUNCHKEY4_MINI_37   0x0142
#define LAUNCHKEY4_25        0x0143
#define LAUNCHKEY4_37        0x0144
#define LAUNCHKEY4_49        0x0145
#define LAUNCHKEY4_61        0x0146

static int first_fader = 0x9;
static const int PAD_COLUMNS = 8;
static const int PAD_ROWS = 2;
static const int NFADERS = 9;
static int last_detected = 0x0;

bool
LaunchKey4::available ()
{
	/* no preconditions other than the device being present */
	return true;
}

bool
LaunchKey4::match_usb (uint16_t vendor, uint16_t device)
{
	if (vendor != NOVATION) {
		return false;
	}

	switch (device) {
	case LAUNCHKEY4_MINI_25:
	case LAUNCHKEY4_MINI_37:
	case LAUNCHKEY4_25:
	case LAUNCHKEY4_37:
	case LAUNCHKEY4_49:
	case LAUNCHKEY4_61:
		last_detected = device;
		return true;
	}


	return false;
}

bool
LaunchKey4::probe (std::string& i, std::string& o)
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;

	AudioEngine::instance()->get_ports ("", DataType::MIDI, PortFlags (IsOutput|IsTerminal), midi_inputs);
	AudioEngine::instance()->get_ports("", DataType::MIDI, PortFlags(IsInput | IsTerminal), midi_outputs);

	if (midi_inputs.empty() || midi_outputs.empty()) {
		return false;
	}

	std::regex rx (X_("Launchkey (Mini MK4|MK4).*MI"), std::regex::extended);

	auto has_lppro = [&rx](string const &s) {
		std::string pn = AudioEngine::instance()->get_hardware_port_name_by_name(s);
		return std::regex_search (pn, rx);
	};

	auto pi = std::find_if (midi_inputs.begin(), midi_inputs.end(), has_lppro);
	auto po = std::find_if (midi_outputs.begin (), midi_outputs.end (), has_lppro);

	if (pi == midi_inputs.end () || po == midi_outputs.end ()) {
		return false;
	}

	i = *pi;
	o = *po;
	return true;
}

LaunchKey4::LaunchKey4 (ARDOUR::Session& s)
#ifdef LAUNCHPAD_MINI
	: MIDISurface (s, X_("Novation Launchkey Mini"), X_("Launchkey Mini"), true)
#else
	: MIDISurface (s, X_("Novation Launchkey 4"), X_("Launchkey MK4"), true)
#endif
	, _daw_out_port (nullptr)
	, _gui (nullptr)
	, scroll_x_offset (0)
	, scroll_y_offset (0)
	, device_pid (0x0)
	, mode_channel (0xf)
	, pad_function (MuteSolo)
	, shift_pressed (false)
	, layer_pressed (false)
	, bank_start (0)
	, button_mode (ButtonsRecEnable) // reset via toggle later
	, encoder_mode (EncoderMixer)
	, num_plugin_controls (0)
{
	run_event_loop ();
	port_setup ();

	std::string  pn_in, pn_out;
	if (probe (pn_in, pn_out)) {
		_async_in->connect (pn_in);
		_async_out->connect (pn_out);
	}

	build_color_map ();
	build_pad_map ();

	Trigger::TriggerPropertyChange.connect (trigger_connections, invalidator (*this), std::bind (&LaunchKey4::trigger_property_change, this, _1, _2), this);
	ControlProtocol::PluginSelected.connect (session_connections, invalidator (*this), std::bind (&LaunchKey4::plugin_selected, this, _1), this);

	session->RecordStateChanged.connect (session_connections, invalidator(*this), std::bind (&LaunchKey4::record_state_changed, this), this);
	session->TransportStateChange.connect (session_connections, invalidator(*this), std::bind (&LaunchKey4::transport_state_changed, this), this);
	session->RouteAdded.connect (session_connections, invalidator(*this), std::bind (&LaunchKey4::stripables_added, this), this);
	session->SoloChanged.connect (session_connections, invalidator(*this), std::bind (&LaunchKey4::solo_changed, this), this);
}

LaunchKey4::~LaunchKey4 ()
{
	DEBUG_TRACE (DEBUG::Launchkey, "launchkey control surface object being destroyed\n");

	trigger_connections.drop_connections ();
	route_connections.drop_connections ();
	session_connections.drop_connections ();

	for (size_t n = 0; n < sizeof (pads) / sizeof (pads[0]); ++n) {
		pads[n].timeout_connection.disconnect ();
	}

	stop_event_loop ();
	tear_down_gui ();

	MIDISurface::drop ();

}

void
LaunchKey4::transport_state_changed ()
{
	MIDI::byte msg[9];

	msg[0] = 0xb0 | mode_channel;
	msg[1] = 0x73;

	msg[3] = 0xb0 | mode_channel;
	msg[4] = Play;

	msg[6] = 0xb0 | mode_channel;
	msg[7] = Stop;

	if (session->transport_rolling()) {
		msg[2] = 0x7f;
		msg[5] = 0x0;
	} else {
		msg[2] = 0x0;
		msg[5] = 0x7f;
	}

	if (session->get_play_loop()) {
		msg[8] = 0x7f;
	} else {
		msg[8] = 0x0;
	}

	daw_write (msg, 9);

	map_rec_enable ();
}

void
LaunchKey4::record_state_changed ()
{
	map_rec_enable();
}

void
LaunchKey4::map_rec_enable ()
{
	if (button_mode != ButtonsRecEnable) {
		return;
	}

	MIDI::byte msg[3];
	int channel = session->actively_recording() ? 0x0 : 0x2;
	const int rec_color_index = 0x5; /* bright red */
	const int norec_color_index = 0x0;

	/* The global rec-enable button */

	msg[0] = 0xb0 | channel;
	msg[1] = 0x75;
	msg[2] = session->get_record_enabled() ? rec_color_index : norec_color_index;

	daw_write (msg, 3);

	/* Now all the tracks */

	for (int i = 0; i < NFADERS-1; ++i) {
		show_rec_enable (i);
	}
}

void
LaunchKey4::show_rec_enable (int n)
{
	LightingMode mode = session->actively_recording() ? Solid : Pulse;
	const int rec_color_index = 0x5; /* bright red */
	const int norec_color_index = 0x0;

	if (stripable[n]) {
		std::shared_ptr<AutomationControl> ac = stripable[n]->rec_enable_control();
		if (ac) {
			light_button (Button1 + n, mode, ac->get_value() ? rec_color_index : norec_color_index);
		} else {
			light_button (Button1 + n, Solid, 0x0);
		}
	} else {
		light_button (Button1 + n, Solid, 0x0);
	}
}

int
LaunchKey4::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Launchkey, string_compose("Launchpad X::set_active init with yn: %1\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		if (device_acquire ()) {
			return -1;
		}

	} else {
		/* Control Protocol Manager never calls us with false, but
		 * insteads destroys us.
		 */
	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::Launchkey, string_compose("Launchpad X::set_active done with yn: '%1'\n", yn));

	return 0;
}

void
LaunchKey4::run_event_loop ()
{
	DEBUG_TRACE (DEBUG::Launchkey, "start event loop\n");
	BaseUI::run ();
}

void
LaunchKey4::stop_event_loop ()
{
	DEBUG_TRACE (DEBUG::Launchkey, "stop event loop\n");
	BaseUI::quit ();
}

int
LaunchKey4::begin_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchkey, "begin using device\n");

	/* get device model */

	_data_required = true;
	MidiByteArray device_inquiry (6, 0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7);
	write (device_inquiry);

	return 0;
}

void
LaunchKey4::finish_begin_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchkey, "finish begin using device\n");

	_data_required = false;

	if (MIDISurface::begin_using_device ()) {
		return;
	}

	connect_daw_ports ();

	/* enter DAW mode */

	set_daw_mode (true);
	set_pad_function (MuteSolo);

	/* catch current selection, if any so that we can wire up the pads if appropriate */
	stripable_selection_changed ();
	switch_bank (0);
	toggle_button_mode ();
	use_encoders (true);
	set_encoder_bank (0);

	/* Set configuration for fader displays, which is never altered */

	MIDI::byte display_config[10];

	display_config[0] = 0xf0;
	display_config[1] = 0x0;
	display_config[2] = 0x20;
	display_config[3] = 0x29;
	display_config[4] = (device_pid>>8) & 0x7f;
	display_config[5] = device_pid & 0x7f;
	display_config[6] = 0x4;

	display_config[8] = 0x61;
	display_config[9] = 0xf7;

	for (int fader = 0; fader < 9; ++fader) {
		/* 2 line display for all faders */
		display_config[7] = 0x5 + fader;
		daw_write (display_config, 10);
	}
	std::cerr << "Configuring displays now\n";
	configure_display (StationaryDisplay, 0x1);
	set_display_target (StationaryDisplay, 0, "ardour", true);
	set_display_target (StationaryDisplay, 1, string(), true);

	configure_display (DAWPadFunctionDisplay, 0x1);

	/* In this DAW, mixer mode controls pan */
	set_display_target (MixerPotMode, 1, "Level", false);
}

void
LaunchKey4::set_daw_mode (bool yn)
{
	MidiByteArray msg;

	msg.push_back (0x9f);
	msg.push_back (0xc);
	msg.push_back (yn ? 0x7f : 0x0);
	daw_write (msg);

	if (yn) {
		mode_channel = 0x0;
	} else {
		mode_channel = 0xf;
	}

	if (yn) {
		all_pads_out ();
	}
}

void
LaunchKey4::all_pads (int color_index)
{
	MIDI::byte msg[3];

	msg[0] = 0x90;
	msg[2] = color_index;

	/* top row */
	for (int i = 0; i < 8; ++i) {
		msg[1] = 0x60 + i;
		daw_write (msg, 3);
	}
	for (int i = 0; i < 8; ++i) {
		msg[1] = 0x70 + i;
		daw_write (msg, 3);
	}
}

void
LaunchKey4::all_pads_out ()
{
	all_pads (0x0);
}

int
LaunchKey4::stop_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchkey, "stop using device\n");

	if (!_in_use) {
		DEBUG_TRACE (DEBUG::Launchkey, "nothing to do, device not in use\n");
		return 0;
	}

	set_daw_mode (false);

	return MIDISurface::stop_using_device ();
}

XMLNode&
LaunchKey4::get_state() const
{
	XMLNode& node (MIDISurface::get_state());

	XMLNode* child = new XMLNode (X_("DAWInput"));
	child->add_child_nocopy (_daw_in->get_state());
	node.add_child_nocopy (*child);
	child = new XMLNode (X_("DAWOutput"));
	child->add_child_nocopy (_daw_out->get_state());
	node.add_child_nocopy (*child);

	return node;
}

int
LaunchKey4::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::Launchkey, string_compose ("LaunchKey4::set_state: active %1\n", active()));

	int retval = 0;

	if (MIDISurface::set_state (node, version)) {
		return -1;
	}

	return retval;
}

std::string
LaunchKey4::input_port_name () const
{
	switch (last_detected) {
	case LAUNCHKEY4_MINI_25:
	case LAUNCHKEY4_MINI_37:
		return X_(":Launchpad Mini MK3.*MIDI (In|2)");
	default:
		break;
	}
	return X_(":Launchpad X MK3.*MIDI (In|2)");
}

std::string
LaunchKey4::output_port_name () const
{
	switch (last_detected) {
	case LAUNCHKEY4_MINI_25:
	case LAUNCHKEY4_MINI_37:
		return X_(":Launchpad Mini MK3.*MIDI (Out|2)");
	default:
		break;
	}

	return X_(":Launchpad X MK3.*MIDI (Out|2)");
}

void
LaunchKey4::relax (Pad & pad)
{
}

void
LaunchKey4::relax (Pad & pad, int)
{
}

void
LaunchKey4::build_pad_map ()
{
	for (int n = 0; n < 8; ++n) {
		int pid = 0x60 + n;
		pads[n] = Pad (pid, n, 0);
	}
	for (int n = 0; n < 8; ++n) {
		int pid = 0x70 + n;
		pads[8+n] = Pad (pid, n, 1);
	}
}

void
LaunchKey4::use_encoders (bool onoff)
{
	MIDI::byte msg[3];
	msg[0] = 0xb6;
	msg[1] = 0x45;
	msg[2] = (onoff ? 0x7f : 0x0);
	daw_write (msg, 3);

	if (!onoff) {
		return;
	}

	MIDI::byte display_config[10];

	display_config[0] = 0xf0;
	display_config[1] = 0x0;
	display_config[2] = 0x20;
	display_config[3] = 0x29;
	display_config[4] = (device_pid>>8) & 0x7f;
	display_config[5] = device_pid & 0x7f;
	display_config[6] = 0x4;

	display_config[8] = 0x62;
	display_config[9] = 0xf7;

	for (int encoder = 0; encoder < 8; ++encoder) {
		/* 2 line display for all encoders */
		display_config[7] = 0x15 + encoder;
		daw_write (display_config, 10);
	}
}

void
LaunchKey4::handle_midi_sysex (MIDI::Parser& parser, MIDI::byte* raw_bytes, size_t sz)
{
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::Launchkey)) {
		std::stringstream str;
		str << "Sysex received, size " << sz << std::endl;
		str << hex;
		for (size_t n = 0; n < sz; ++n) {
			str << "0x" << (int) raw_bytes[n] << ' ';
		}
		str << std::endl;
		std::cerr << str.str();
	}
#endif

	if (sz != 17) {
		return;
	}

	MIDI::byte dp_lsb;
	MIDI::byte dp_msb;

	if (raw_bytes[1] == 0x7e &&
	    raw_bytes[2] == 0x0 &&
	    raw_bytes[3] == 0x6 &&
	    raw_bytes[4] == 0x2 &&
	    raw_bytes[5] == 0x0 &&
	    raw_bytes[6] == 0x20 &&
	    raw_bytes[7] == 0x29) {
		dp_lsb = raw_bytes[8];
		dp_msb = raw_bytes[9];

		const int family = (dp_msb<<8)|dp_lsb;
		switch (family) {
		case LAUNCHKEY4_MINI_25:
		case LAUNCHKEY4_MINI_37:
			device_pid = 0x0213;
			break;
		case LAUNCHKEY4_25:
		case LAUNCHKEY4_37:
		case LAUNCHKEY4_49:
		case LAUNCHKEY4_61:
			device_pid = 0x0214;
			break;
		default:
			return;
		}

		finish_begin_using_device ();
		return;
	}
}

void
LaunchKey4::handle_midi_controller_message_chnF (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	if (ev->controller_number < 0x05 || ev->controller_number > 0xd) {
		return;
	}

	int fader_number = ev->controller_number - 0x5;
	fader_move (fader_number, ev->value);
}

void
LaunchKey4::handle_midi_controller_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	/* Remember: fader controller events are delivered via if (ev->controller_::handle_midi_controller_message_chnF() */
	if (&parser != _daw_in_port->parser()) {
		if (ev->controller_number == 0x69 && ev->value == 0x7f) {
			DEBUG_TRACE (DEBUG::Launchkey, string_compose ("function button press on non-DAW port, CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));
			function_press ();
			return;
		}
		/* we don't process CC messages from the regular port */
		DEBUG_TRACE (DEBUG::Launchkey, string_compose ("skip non-DAW CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));
		return;
	}

#ifndef NDEBUG
	std::stringstream ss;
	ss << "CC 0x" << std::hex << (int) ev->controller_number << std::dec << " value (" << (int) ev->value << ")\n";
	DEBUG_TRACE (DEBUG::Launchkey, ss.str());
#endif

	/* Shift being pressed can change everything */

	if (ev->controller_number == 0x48) {
		if (ev->value) {
			shift_pressed = true;
		} else {
			shift_pressed = false;
		}
		return;
	}

	/* Scene launch */
	if (ev->controller_number == 0x68) {
		if (ev->value) {
			scene_press ();
		}
		return;
	}

	/* Button 9 (below fader 9 */

	if (ev->controller_number == Button9) {
		/* toggle on press only */

		if (ev->value) {
			toggle_button_mode ();
		}
		return;
	}

	/* Encoder Mode button */

	if (ev->controller_number == 0x41) {
		switch (ev->value) {
		case 2:
			set_encoder_mode (EncoderPlugins);
			break;
		case 1:
			set_encoder_mode (EncoderMixer);
			break;
		case 4:
			set_encoder_mode (EncoderSendA);
			break;
		case 5:
			set_encoder_mode (EncoderTransport);
			break;
		default:
			break;
		}
		return;
	}

	/* Encoder Bank Buttons */

	if (ev->controller_number == 0x33) {
		/* up'; use press only  */
		if (ev->value && encoder_bank >  0) {
			set_encoder_bank (encoder_bank - 1);
		}
		return;
	}

	if (ev->controller_number == 0x34) {
		/* down; use press only */
		if (ev->value && encoder_bank < 2) {
			set_encoder_bank (encoder_bank + 1);
		}
		return;
	}

	switch (ev->controller_number) {
	case 0x6a:
		if (ev->value) button_up ();
		return;
	case 0x6b:
		if (ev->value) button_down ();
		return;
	case 0x67:
		if (ev->value) button_left ();
		return;
	case 0x66:
		if (ev->value) button_right ();
		return;
	}

	/* Buttons below faders */

	if (ev->controller_number >= Button1 && ev->controller_number <= Button8) {

		if (ev->value == 0x7f) {
			button_press (ev->controller_number - Button1);
		} else {
			button_release (ev->controller_number - Button1);
		}

		return;

	} else  if (ev->controller_number >= Knob1 && ev->controller_number <= Knob8) {

		encoder (ev->controller_number - Knob1, ev->value - 64);
		return;

	} else  if (ev->controller_number >= 0x55 && ev->controller_number <= 0x5c) {

		encoder (ev->controller_number - Knob1, ev->value - 64);
		return;
	}

	if (ev->value == 0x7f) {
		switch (ev->controller_number) {
		case Function:
			function_press ();
			break;
		case Undo:
			undo_press ();
			break;
		case Play:
			if (device_pid == 0x213) {
				/* Mini version only play button, so toggle */
				if (session->transport_rolling()) {
					transport_stop();
				} else {
					transport_play();
				}
			} else {
				transport_play ();
			}
			break;
		case Stop:
			transport_stop ();
			break;
		case RecEnable:
			set_record_enable (!get_record_enabled());
			break;
		case Loop:
			loop_toggle ();
			break;
		default:
			break;
		}
	}
}

void
LaunchKey4::handle_midi_note_on_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	if (ev->velocity == 0) {
		handle_midi_note_off_message (parser, ev);
		return;
	}

	if (&parser != _daw_in_port->parser()) {
		/* we don't process note messages from the regular port */
		DEBUG_TRACE (DEBUG::Launchkey, string_compose ("skip non-DAW Note On %1/0x%3%4%5 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity, std::hex, (int) ev->note_number, std::dec));
		return;
	}

	int pad_number;

	switch (ev->note_number) {
	case 0x60:
		pad_number = 0;
		break;
	case 0x61:
		pad_number = 1;
		break;
	case 0x62:
		pad_number = 2;
		break;
	case 0x63:
		pad_number = 3;
		break;
	case 0x64:
		pad_number = 4;
		break;
	case 0x65:
		pad_number = 5;
		break;
	case 0x66:
		pad_number = 6;
		break;
	case 0x67:
		pad_number = 7;
		break;

	case 0x70:
		pad_number = 8;
		break;
	case 0x71:
		pad_number = 9;
		break;
	case 0x72:
		pad_number = 10;
		break;
	case 0x73:
		pad_number = 11;
		break;
	case 0x74:
		pad_number = 12;
		break;
	case 0x75:
		pad_number = 13;
		break;
	case 0x76:
		pad_number = 14;
		break;
	case 0x77:
		pad_number = 15;
		break;
	default:
		return;
	}

	DEBUG_TRACE (DEBUG::Launchkey, string_compose ("Note On %1/0x%3%4%5 (velocity %2) => pad %6\n", (int) ev->note_number, (int) ev->velocity, std::hex, (int) ev->note_number, std::dec, pad_number));

	Pad& pad = pads[pad_number];

	switch (pad_function) {
	case MuteSolo:
		pad_mute_solo (pad);
		break;
	case Triggers:
		pad_trigger (pad, ev->velocity);
		break;
	default:
		break;
	}
}

void
LaunchKey4::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	int pad_number;

	switch (ev->note_number) {
	case 0x60:
		pad_number = 0;
		break;
	case 0x61:
		pad_number = 1;
		break;
	case 0x62:
		pad_number = 2;
		break;
	case 0x63:
		pad_number = 3;
		break;
	case 0x64:
		pad_number = 4;
		break;
	case 0x65:
		pad_number = 5;
		break;
	case 0x66:
		pad_number = 6;
		break;
	case 0x67:
		pad_number = 7;
		break;

	case 0x70:
		pad_number = 8;
		break;
	case 0x71:
		pad_number = 9;
		break;
	case 0x72:
		pad_number = 10;
		break;
	case 0x73:
		pad_number = 11;
		break;
	case 0x74:
		pad_number = 12;
		break;
	case 0x75:
		pad_number = 13;
		break;
	case 0x76:
		pad_number = 14;
		break;
	case 0x77:
		pad_number = 15;
		break;
	default:
		return;
	}

	DEBUG_TRACE (DEBUG::Launchkey, string_compose ("Note Off %1/0x%3%4%5 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity, std::hex, (int) ev->note_number, std::dec));
	pad_release (pads[pad_number]);
}

void
LaunchKey4::pad_trigger (Pad& pad, int velocity)
{
	if (shift_pressed) {
		trigger_stop_col (pad.x, true); /* immediate */
	} else {
		TriggerPtr trigger = session->trigger_at (pad.x, pad.y + scroll_y_offset);
		switch (trigger->state()) {
		case Trigger::Stopped:
			trigger->bang (velocity / 127.0f);
			break;
		default:
			break;
		}
		start_press_timeout (pad);
	}
}

void
LaunchKey4::pad_release (Pad& pad)
{
	pad.timeout_connection.disconnect ();
}

void
LaunchKey4::start_press_timeout (Pad& pad)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (250); // milliseconds
	pad.timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &LaunchKey4::long_press_timeout), pad.x));
	timeout->attach (main_loop()->get_context());
}

bool
LaunchKey4::long_press_timeout (int col)
{
	std::cerr << "timeout!\n";
	trigger_stop_col (col, false); /* non-immediate */
	return false; /* don't get called again */
}

void
LaunchKey4::trigger_property_change (PropertyChange pc, Trigger* t)
{
	if (pad_function != Triggers) {
		return;
	}

	int x = t->box().order();
	int y = t->index();

	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("prop change %1 for trigger at %2, %3\n", pc, x, y));

	if (y < scroll_y_offset || y > scroll_y_offset + 1) {
		/* not visible at present */
		return;
	}

	if (x < scroll_x_offset || x > scroll_x_offset + 7) {
		/* not visible at present */
		return;
	}

	y -= scroll_y_offset;
	x -= scroll_x_offset;

	/* name property change is sent when slots are loaded or unloaded */

	PropertyChange our_interests;
	our_interests.add (Properties::running);
	our_interests.add (Properties::name);;

	if (pc.contains (our_interests)) {

		Pad& pad (pads[(y*8) + x]);
		std::shared_ptr<Route> r = session->get_remote_nth_route (scroll_x_offset + x);

		trigger_pad_light (pad, r, t);
	}
}

void
LaunchKey4::trigger_pad_light (Pad& pad, std::shared_ptr<Route> r, Trigger* t)
{
	if (!r || !t || !t->playable()) {
		unlight_pad (pad.id);
		return;
	}

	MIDI::byte msg[3];

	msg[0] = 0x90;
	msg[1] = pad.id;

	switch (t->state()) {
	case Trigger::Stopped:
		msg[2] = find_closest_palette_color (r->presentation_info().color());
		break;

	case Trigger::WaitingToStart:
		msg[0] |= 0x2; /* channel 2=> pulsing */
		msg[2] = 0x17; // find_closest_palette_color (r->presentation_info().color()));
		break;

	case Trigger::Running:
		/* choose contrasting color from the base one */
		msg[2] = find_closest_palette_color (HSV(r->presentation_info().color()).opposite());
		break;

	case Trigger::WaitingForRetrigger:
	case Trigger::WaitingToStop:
	case Trigger::WaitingToSwitch:
	case Trigger::Stopping:
		msg[0] |= 0x2; /* pulse */
		msg[2] = find_closest_palette_color (HSV(r->presentation_info().color()).opposite());
	}

	daw_write (msg, 3);
}

void
LaunchKey4::map_triggers ()
{
	for (int x = 0; x < PAD_COLUMNS; ++x) {
		map_triggerbox (x);
	}
}

void
LaunchKey4::map_triggerbox (int x)
{
	std::shared_ptr<Route> r = session->get_remote_nth_route (x + scroll_x_offset);

	for (int y = 0; y < PAD_ROWS; ++y) {
		Pad& pad (pads[(y*8) + x]);
		TriggerPtr t = session->trigger_at (x + scroll_x_offset, y + scroll_y_offset);
		trigger_pad_light (pad, r, t.get());
	}
}

void
LaunchKey4::pad_mute_solo (Pad& pad)
{
	if (!stripable[pad.x]) {
		return;
	}

	if (pad.y == 0) {
		session->set_control (stripable[pad.x]->mute_control(), !stripable[pad.x]->mute_control()->get_value(), PBD::Controllable::UseGroup);
	} else {
		session->set_control (stripable[pad.x]->solo_control(), !stripable[pad.x]->solo_control()->get_value(), PBD::Controllable::UseGroup);
	}
}

void
LaunchKey4::port_registration_handler ()
{
	MIDISurface::port_registration_handler ();
	connect_daw_ports ();
}

void
LaunchKey4::connect_daw_ports ()
{
	if (!_daw_in || !_daw_out) {
		/* ports not registered yet */
		return;
	}

	if (_daw_in->connected() && _daw_out->connected()) {
		/* don't waste cycles here */
		return;
	}

	std::vector<std::string> midi_inputs;
	std::vector<std::string> midi_outputs;

	/* get all MIDI Ports */

	AudioEngine::instance()->get_ports ("", DataType::MIDI, PortFlags (IsOutput|IsTerminal), midi_inputs);
        AudioEngine::instance()->get_ports("", DataType::MIDI, PortFlags(IsInput | IsTerminal), midi_outputs);

        if (midi_inputs.empty() || midi_outputs.empty()) {
		return;
	}

        /* Try to find the DAW port, whose pretty name varies on Linux
         * depending on the version of ALSA, but is fairly consistent across
         * newer ALSA and other platforms.
         */

        std::string regex_str;

        if (device_pid == 0x213) {
	        regex_str =  X_("Launchkey Mini MK4.*(DAW|MIDI 2|DA$)");
        } else {
	        regex_str =  X_("Launchkey MK4.*(DAW|MIDI 2|DA$)");
        }

        std::regex rx (regex_str, std::regex::extended);

        auto is_dawport = [&rx](string const &s) {
	        std::string pn = AudioEngine::instance()->get_hardware_port_name_by_name(s);
	        return std::regex_search (pn, rx);
        };

        auto pi = std::find_if (midi_inputs.begin(), midi_inputs.end(), is_dawport);
        auto po = std::find_if (midi_outputs.begin (), midi_outputs.end (), is_dawport);

        if (pi == midi_inputs.end() || po == midi_inputs.end()) {
	        std::cerr << "daw port not found\n";
	        return;
        }

        if (!_daw_in->connected()) {
	        AudioEngine::instance()->connect (_daw_in->name(), *pi);
        }

        if (!_daw_out->connected()) {
	        AudioEngine::instance()->connect (_daw_out->name(), *po);
        }


	connect_to_port_parser (*_daw_in_port);

	MIDI::Parser* p = _daw_in_port->parser();
	/* fader messages are controllers but always on channel 0xf */
	p->channel_controller[15].connect_same_thread (*this, std::bind (&LaunchKey4::handle_midi_controller_message_chnF, this, _1, _2));

	/* Connect DAW input port to event loop */

	AsyncMIDIPort* asp;

	asp = dynamic_cast<AsyncMIDIPort*> (_daw_in_port);
	asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &MIDISurface::midi_input_handler), _daw_in_port));
	asp->xthread().attach (main_loop()->get_context());
}

int
LaunchKey4::ports_acquire ()
{
	int ret = MIDISurface::ports_acquire ();

	if (!ret) {
		_daw_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, string_compose (X_("%1 daw in"), port_name_prefix), true);
		if (_daw_in) {
			_daw_in_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_daw_in).get();
			_daw_out = AudioEngine::instance()->register_output_port (DataType::MIDI, string_compose (X_("%1 daw out"), port_name_prefix), true);
		}
		if (_daw_out) {
			_daw_out_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_daw_out).get();
			return 0;
		}

		ret = -1;
	}

	return ret;
}

void
LaunchKey4::ports_release ()
{
	/* wait for button data to be flushed */
	MIDI::Port* daw_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_daw_out).get();
	AsyncMIDIPort* asp;
	asp = dynamic_cast<AsyncMIDIPort*> (daw_port);
	asp->drain (10000, 500000);

	{
		Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
		AudioEngine::instance()->unregister_port (_daw_in);
		AudioEngine::instance()->unregister_port (_daw_out);
	}

	_daw_in.reset ((ARDOUR::Port*) 0);
	_daw_out.reset ((ARDOUR::Port*) 0);

	MIDISurface::ports_release ();
}

void
LaunchKey4::daw_write (const MidiByteArray& data)
{
	DEBUG_TRACE (DEBUG::Launchkey, string_compose ("daw write %1 %2\n", data.size(), data));
	_daw_out_port->write (&data[0], data.size(), 0);
}

void
LaunchKey4::daw_write (MIDI::byte const * data, size_t size)
{

#ifndef NDEBUG
	std::stringstream str;

	if (DEBUG_ENABLED(DEBUG::Launchkey)) {
		str << hex;
		for (size_t n = 0; n < size; ++n) {
			str << (int) data[n] << ' ';
		}
	}
#endif

	DEBUG_TRACE (DEBUG::Launchkey, string_compose ("daw write %1 [%2]\n", size, str.str()));
	_daw_out_port->write (data, size, 0);
}

void
LaunchKey4::stripable_selection_changed ()
{
	map_selection ();

	if (session->selection().first_selected_stripable()) {
		set_display_target (GlobalTemporaryDisplay, 0, session->selection().first_selected_stripable()->name(), true);
	}
}

void
LaunchKey4::show_scene_ids ()
{
	set_display_target (DAWPadFunctionDisplay, 0, string_compose ("Scenes %1 + %2", scroll_y_offset + 1, scroll_y_offset + 2), true);
}

void
LaunchKey4::button_up ()
{
	if (pad_function != Triggers) {
		return;
	}

	if (scroll_y_offset >= 1) {
		scroll_y_offset -= 1;
		show_scene_ids ();
	}
}

void
LaunchKey4::button_down()
{
	if (pad_function != Triggers) {
		return;
	}

	scroll_y_offset += 1;
	show_scene_ids ();
}

void
LaunchKey4::build_color_map ()
{
	/* RGB values taken from using color picker on PDF of LP manual, page
	 * 10, but without zero (off)
	 */

	static uint32_t novation_color_chart_left_side[] = {
		0xb3b3b3ff,
		0xddddddff,
		0xffffffff,
		0xffb3b3ff,
		0xff6161ff,
		0xdd6161ff,
		0xb36161ff,
		0xfff3d5ff,
		0xffb361ff,
		0xdd8c61ff,
		0xb37661ff,
		0xffeea1ff,
		0xffff61ff,
		0xdddd61ff,
		0xb3b361ff,
		0xddffa1ff,
		0xc2ff61ff,
		0xa1dd61ff,
		0x81b361ff,
		0xc2ffb3ff,
		0x61ff61ff,
		0x61dd61ff,
		0x61b361ff,
		0xc2ffc2ff,
		0x61ff8cff,
		0x61dd76ff,
		0x61b36bff,
		0xc2ffccff,
		0x61ffccff,
		0x61dda1ff,
		0x61b381ff,
		0xc2fff3ff,
		0x61ffe9ff,
		0x61ddc2ff,
		0x61b396ff,
		0xc2f3ffff,
		0x61eeffff,
		0x61c7ddff,
		0x61a1b3ff,
		0xc2ddffff,
		0x61c7ffff,
		0x61a1ddff,
		0x6181b3ff,
		0xa18cffff,
		0x6161ffff,
		0x6161ddff,
		0x6161b3ff,
		0xccb3ffff,
		0xa161ffff,
		0x8161ddff,
		0x7661b3ff,
		0xffb3ffff,
		0xff61ffff,
		0xdd61ddff,
		0xb361b3ff,
		0xffb3d5ff,
		0xff61c2ff,
		0xdd61a1ff,
		0xb3618cff,
		0xff7661ff,
		0xe9b361ff,
		0xddc261ff,
		0xa1a161ff,
	};

	static uint32_t novation_color_chart_right_side[] = {
		0x61b361ff,
		0x61b38cff,
		0x618cd5ff,
		0x6161ffff,
		0x61b3b3ff,
		0x8c61f3ff,
		0xccb3c2ff,
		0x8c7681ff,
		/**/
		0xff6161ff,
		0xf3ffa1ff,
		0xeefc61ff,
		0xccff61ff,
		0x76dd61ff,
		0x61ffccff,
		0x61e9ffff,
		0x61a1ffff,
		/**/
		0x8c61ffff,
		0xcc61fcff,
		0xcc61fcff,
		0xa17661ff,
		0xffa161ff,
		0xddf961ff,
		0xd5ff8cff,
		0x61ff61ff,
		/**/
		0xb3ffa1ff,
		0xccfcd5ff,
		0xb3fff6ff,
		0xcce4ffff,
		0xa1c2f6ff,
		0xd5c2f9ff,
		0xf98cffff,
		0xff61ccff,
		/**/
		0xff61ccff,
		0xf3ee61ff,
		0xe4ff61ff,
		0xddcc61ff,
		0xb3a161ff,
		0x61ba76ff,
		0x76c28cff,
		0x8181a1ff,
		/**/
		0x818cccff,
		0xccaa81ff,
		0xdd6161ff,
		0xf9b3a1ff,
		0xf9ba76ff,
		0xfff38cff,
		0xe9f9a1ff,
		0xd5ee76ff,
		/**/
		0x8181a1ff,
		0xf9f9d5ff,
		0xddfce4ff,
		0xe9e9ffff,
		0xe4d5ffff,
		0xb3b3b3ff,
		0xd5d5d5ff,
		0xf9ffffff,
		/**/
		0xe96161ff,
		0xe96161ff,
		0x81f661ff,
		0x61b361ff,
		0xf3ee61ff,
		0xb3a161ff,
		0xeec261ff,
		0xc27661ff
	};

	for (size_t n = 0; n < sizeof (novation_color_chart_left_side) / sizeof (novation_color_chart_left_side[0]); ++n) {
		uint32_t color = novation_color_chart_left_side[n];
		/* Add 1 to account for missing zero at zero in the table */
		std::pair<int,uint32_t> p (1 + n, color);
		color_map.insert (p);
	}

	for (size_t n = 0; n < sizeof (novation_color_chart_right_side) / sizeof (novation_color_chart_right_side[0]); ++n) {
		uint32_t color = novation_color_chart_right_side[n];
		/* Add 40 to account for start offset number shown in page 10 of the LP manual */
		std::pair<int,uint32_t> p (40 + n, color);
		color_map.insert (p);
	}
}

int
LaunchKey4::find_closest_palette_color (uint32_t color)
{
	auto distance = std::numeric_limits<double>::max();
	int index = -1;

	NearestMap::iterator n = nearest_map.find (color);
	if (n != nearest_map.end()) {
		return n->second;
	}

	HSV hsv_c (color);

	for (auto const & c : color_map) {

		HSV hsv_p (c.second);

		double chr = M_PI * (hsv_c.h / 180.0);
		double phr = M_PI * (hsv_p.h /180.0);
		double t1 = (sin (chr) * hsv_c.s * hsv_c.v) - (sin (phr) * hsv_p.s* hsv_p.v);
		double t2 = (cos (chr) * hsv_c.s * hsv_c.v) - (cos (phr) * hsv_p.s * hsv_p.v);
		double t3 = hsv_c.v - hsv_p.v;
		double d = (t1 * t1) + (t2 * t2) + (0.5 * (t3 * t3));


		if (d < distance) {
			index = c.first;
			distance = d;
		}
	}

	nearest_map.insert (std::pair<uint32_t,int> (color, index));

	return index;
}

void
LaunchKey4::route_property_change (PropertyChange const & pc, int col)
{
	if (pc.contains (Properties::color)) {
		map_triggerbox (col);
	}


	if (pc.contains (Properties::selected)) {
	}
}

void
LaunchKey4::fader_move (int which, int val)
{
	std::shared_ptr<AutomationControl> ac;

	if (which == 8) {
		std::shared_ptr<Route> monitor = session->monitor_out();

		if (monitor) {
			ac = monitor->gain_control();
		} else {
			std::shared_ptr<Route> master = session->master_out();
			if (!master) {
				return;
			}
			ac = master->gain_control();
		}

	} else {
		if (!stripable[which]) {
			return;
		}

		ac = stripable[which]->gain_control();
	}

	if (ac) {
		gain_t gain = ARDOUR::slider_position_to_gain_with_max (val/127.0, ARDOUR::Config->get_max_gain());
		session->set_control (ac, gain, PBD::Controllable::NoGroup);

		char buf[16];
		snprintf (buf, sizeof (buf), "%.1f dB", accurate_coefficient_to_dB (gain));
		set_display_target (DisplayTarget (0x5 + which), 1, buf, true);
	}
}

void
LaunchKey4::automation_control_change (int n, std::weak_ptr<AutomationControl> wac)
{
	std::shared_ptr<AutomationControl> ac = wac.lock();
	if (!ac) {
		return;
	}

	MIDI::byte msg[3];
	msg[0] = 0xb4;
	msg[1] = first_fader + n;

	switch (current_fader_bank) {
	case VolumeFaders:
	case SendAFaders:
	case SendBFaders:
		msg[2] = (MIDI::byte) (ARDOUR::gain_to_slider_position_with_max (ac->get_value(), ARDOUR::Config->get_max_gain()) * 127.0);
		break;
	case PanFaders:
		msg[2] = (MIDI::byte) (ac->get_value() * 127.0);
		break;
	default:
		break;
	}

	daw_write (msg, 3);
}

void
LaunchKey4::encoder (int which, int step)
{
	switch (encoder_mode) {
	case EncoderPlugins:
		encoder_plugin (which, step);
		break;
	case EncoderMixer:
		encoder_mixer (which, step);
		break;
	case EncoderSendA:
		encoder_senda (which, step);
		break;
	case EncoderTransport:
		encoder_transport (which, step);
		break;
	}
}

void
LaunchKey4::plugin_selected (std::weak_ptr<PluginInsert> wpi)
{
	std::shared_ptr<PluginInsert> pi (wpi.lock());
	if (!pi) {
		return;
	}

	current_plugin = pi->plugin();
	uint32_t n = 0;

	while (n < 24) {

		Evoral::ParameterDescriptor pd;
		Evoral::Parameter param (PluginAutomation, 0, n);

		std::shared_ptr<AutomationControl> ac = pi->automation_control (param, false);
		if (ac) {
			controls[n] = ac;
		} else {
			break;
		}

		++n;
	}

	num_plugin_controls = n;

	while (n < 24) {
		controls[n].reset ();
		++n;
	}

	if (encoder_mode == EncoderPlugins) {
		label_encoders ();
		/* light up/down arrows appropriately */
		set_encoder_bank (encoder_bank);
	}
}

void
LaunchKey4::show_encoder_value (int n, std::shared_ptr<Plugin> plugin, int control, std::shared_ptr<AutomationControl> ac, bool display)
{
	bool ok;
	std::string str;
	uint32_t p = plugin->nth_parameter (control, ok);

	if (!ok || !plugin->print_parameter (p, str)) {
		char buf[32];
		double val = ac->get_value ();
		snprintf (buf, sizeof (buf), "%.2f", val);
		set_display_target (DisplayTarget (0x15 + n), 2, buf, display);
		return;
	}

	set_display_target (DisplayTarget (0x15 + n), 2, str, true);
}

void
LaunchKey4::setup_screen_for_encoder_plugins ()
{
	uint32_t n = 0;

	std::shared_ptr<ARDOUR::Plugin> plugin = current_plugin.lock();

	if (plugin) {
		while (n < 8) {
			uint32_t ctrl = (encoder_bank * 8) + n;

			std::shared_ptr<AutomationControl> ac = controls[ctrl].lock();
			bool ok;

			if (!ac) {
				break;
			}
			int p = plugin->nth_parameter (n, ok);
			if (!ok) {
				break;
			}

			std::string label = plugin->parameter_label (p);

			set_display_target (DisplayTarget (0x15+n), 0, plugin->name(), (n == 0));
			set_display_target (DisplayTarget (0x15+n), 1, label,(n == 0));
			show_encoder_value (n, plugin, ctrl, ac, (n == 0));
			++n;
		}
	}

	while (n < 8) {
		set_display_target (DisplayTarget (0x15+n), 0, plugin->name(), (n == 0));
		set_display_target (DisplayTarget (0x15+n), 1, "--", (n == 0));
		set_display_target (DisplayTarget (0x15+n), 2, string(), (n == 0));
		++n;
	}
}

void
LaunchKey4::encoder_plugin (int which, int step)
{
	std::shared_ptr<Plugin> plugin (current_plugin.lock());
	if (!plugin)  {
		return;
	}

	int control = which + (encoder_bank * 8);
	std::shared_ptr<AutomationControl> ac (controls[control].lock());

	if (!ac) {
		return;
	}

	double val = ac->internal_to_interface (ac->get_value());
	val += step/127.0;
	ac->set_value (ac->interface_to_internal (val), PBD::Controllable::NoGroup);

	show_encoder_value (which, plugin, control, ac, true);
}

void
LaunchKey4::encoder_mixer (int which, int step)
{
	switch (encoder_bank) {
	case 0:
		encoder_level (which, step);
		break;
	case 1:
		encoder_pan (which, step);
		break;
	default:
		break;
	}
}

void
LaunchKey4::encoder_pan (int which, int step)
{
	if (!stripable[which]) {
		return;
	}

	std::shared_ptr<AutomationControl> ac (stripable[which]->pan_azimuth_control());

	if (!ac) {
		return;
	}

	double val = ac->internal_to_interface (ac->get_value());
	session->set_control (ac, ac->interface_to_internal (val - (step/127.0)), Controllable::NoGroup);

	char buf[64];
	snprintf (buf, sizeof (buf), _("L:%3d R:%3d"), (int) rint (100.0 * (1.0 - val)), (int) rint (100.0 * val));
	set_display_target (DisplayTarget (0x15 + which), 2, buf, true);
}


void
LaunchKey4::encoder_level (int which, int step)
{
	if (!stripable[which]) {
		return;
	}

	std::shared_ptr<GainControl> gc (stripable[which]->gain_control());

	if (!gc) {
		return;
	}

	gain_t gain;

	if (shift_pressed) {
		gain = gc->get_value();
	} else {
		double pos = ARDOUR::gain_to_slider_position_with_max (gc->get_value(), ARDOUR::Config->get_max_gain());
		pos += (step/127.0);
		gain = ARDOUR::slider_position_to_gain_with_max (pos, ARDOUR::Config->get_max_gain());
		session->set_control (gc, gain, Controllable::NoGroup);
	}

	char buf[16];
	snprintf (buf, sizeof (buf), "%.1f dB", accurate_coefficient_to_dB (gain));
	set_display_target (DisplayTarget (0x15 + which), 2, buf, true);
}

void
LaunchKey4::encoder_senda (int which, int step)
{
	std::shared_ptr<Stripable> s = session->selection().first_selected_stripable();
	if (!s) {
		return;
	}

	std::shared_ptr<Route> target_bus = std::dynamic_pointer_cast<Route> (s);
	if (!target_bus) {
		return;
	}

	if (!stripable[which]) {
		return;
	}

	std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (stripable[which]);
	if (!route) {
		return;
	}

	std::shared_ptr<InternalSend> send = std::dynamic_pointer_cast<InternalSend> (route->internal_send_for (target_bus));
	if (!send) {
		return;
	}

	std::shared_ptr<GainControl> gc = send->gain_control();
	if (!gc) {
		return;
	}
	gain_t gain;

	if (shift_pressed) {
		/* Just display current value */
		gain = gc->get_value();
	} else {
		double pos = ARDOUR::gain_to_slider_position_with_max (gc->get_value(), ARDOUR::Config->get_max_gain());
		pos += (step/127.0);
		gain = ARDOUR::slider_position_to_gain_with_max (pos, ARDOUR::Config->get_max_gain());
		session->set_control (gc, gain, Controllable::NoGroup);
	}

	char buf[16];
	snprintf (buf, sizeof (buf), "%.1f dB", accurate_coefficient_to_dB (gain));
	set_display_target (DisplayTarget (0x15 + which), 1, string_compose ("> %1", send->target_route()->name()), true);
	set_display_target (DisplayTarget (0x15 + which), 2, buf, true);
}

void
LaunchKey4::encoder_transport (int which, int step)
{
	switch (which) {
	case 0:
		transport_shuttle (step);
		break;
	case 1:
		zoom (step);
		break;
	case 2:
		loop_start_move (step);
		break;
	case 3:
		loop_end_move (step);
		break;
	case 4:
		jump_to_marker (step);
		break;
	case 5:
		break;
	case 6:
		break;
	case 7:
		break;
	}
}

void
LaunchKey4::transport_shuttle (int step)
{
	using namespace Temporal;

	/* 1 step == 1/10th current page */
	timepos_t pos (session->transport_sample());

	if (pos == 0  && step < 0) {
		return;
	}

	Beats b = pos.beats();

	if (step > 0) {
		b = b.round_up_to_beat ();
		b += Beats (1, 0) * step;
	} else {
		b = b.round_down_to_beat ();
		b += Beats (1, 0) * step; // step is negative, so add
		if (b < Beats()) {
			b = Beats();
		}
	}

	BBT_Time bbt = TempoMap::use()->bbt_at (b);
	std::stringstream str;
	str << bbt;

	set_display_target (DisplayTarget (0x15), 2, str.str(), true);

	session->request_locate (timepos_t (b).samples());
}

void
LaunchKey4::zoom (int step)
{
	if (step > 0) {
		while (step--) {
			temporal_zoom_in ();
		}
	} else {
		while (step++ < 0) {
			temporal_zoom_out ();
		}
	}
	set_display_target (DisplayTarget (0x15 + 1), 2, string(), true);
}

void
LaunchKey4::loop_start_move (int step)
{
	using namespace Temporal;

	Location* l = session->locations()->auto_loop_location ();
	BBT_Offset dur;

	if (!l) {
		/* XXX NEEDS WRAPPING IN REVERSIBLE COMMAND */
		timepos_t ph (session->transport_sample());
		timepos_t beat_later ((ph.beats() + Beats (1,0)).round_to_beat());

		Location* loc = new Location (*session, timepos_t (ph.beats()), beat_later, _("Loop"),  Location::IsAutoLoop);
		session->locations()->add (loc, true);
		session->set_auto_loop_location (loc);

		dur = BBT_Offset (0, 1, 0);

	} else {
		timepos_t start = l->start();
		start = start.beats() + Beats (step, 0);
		if (start.is_zero() || start.is_negative()) {
			return;
		}
		l->set_start (start);

		TempoMap::SharedPtr map (TempoMap::use());
		BBT_Time bbt_start = map->bbt_at (start);
		BBT_Time bbt_end = map->bbt_at (l->end());

		dur = bbt_delta (bbt_end, bbt_start);
	}

	std::stringstream str;
	str << dur;
	set_display_target (DisplayTarget (0x15 + 2), 2, str.str(), true);
}

void
LaunchKey4::loop_end_move (int step)
{
	using namespace Temporal;

	Location* l = session->locations()->auto_loop_location ();
	BBT_Offset dur;

	if (!l) {
		/* XXX NEEDS WRAPPING IN REVERSIBLE COMMAND */
		timepos_t ph (session->transport_sample());
		timepos_t beat_later ((ph.beats() + Beats (1,0)).round_to_beat());

		Location* loc = new Location (*session, timepos_t (ph.beats()), beat_later, _("Loop"),  Location::IsAutoLoop);
		session->locations()->add (loc, true);
		session->set_auto_loop_location (loc);
		dur = BBT_Offset (0, 1, 0);
	} else {
		timepos_t end = l->end();
		end = end.beats() + Beats (step, 0);
		if (end.is_zero() || end.is_negative()) {
			return;
		}
		l->set_end (end);

		TempoMap::SharedPtr map (TempoMap::use());
		BBT_Time bbt_start = map->bbt_at (l->start());
		BBT_Time bbt_end = map->bbt_at (end);

		dur = bbt_delta (bbt_end, bbt_start);
	}

	std::stringstream str;
	str << dur;
	set_display_target (DisplayTarget (0x15 + 3), 2, str.str(), true);
}

void
LaunchKey4::jump_to_marker (int step)
{
	timepos_t pos;
	Location::Flags noflags = Location::Flags (0);
	Location* loc;

	if (step > 0) {
		pos = session->locations()->first_mark_after_flagged (timepos_t (session->audible_sample()+1), true, noflags, noflags, noflags, &loc);

		if (pos == timepos_t::max (Temporal::AudioTime)) {
			return;
		}

	} else {
		pos = session->locations()->first_mark_before_flagged (timepos_t (session->audible_sample()), true, noflags, noflags, noflags, &loc);

		//handle the case where we are rolling, and we're less than one-half second past the mark, we want to go to the prior mark...
		if (session->transport_rolling()) {
			if ((session->audible_sample() - pos.samples()) < session->sample_rate()/2) {
				timepos_t prior = session->locations()->first_mark_before (pos);
				pos = prior;
			}
		}

		if (pos == timepos_t::max (Temporal::AudioTime)) {
			return;
		}
	}

	session->request_locate (pos.samples());

	set_display_target (DisplayTarget (0x15+4), 2, loc->name(), true);
}

void
LaunchKey4::set_pad_function (PadFunction f)
{
	MIDI::byte msg[3];
	std::string str;

	/* make the LK forget about any currently lit pads, because we overload
	   mode 0x2 and it gets confusing when it tries to restore lighting.
	*/
	all_pads (0x5);
	all_pads_out ();

	msg[0] = 0xb6;
	msg[1] = 0x40; /* set pad layout */

	switch (f) {
	case MuteSolo:
		str = "Mute/Solo";
		break;
	case Triggers:
		str = "Cues & Scenes";
		break;
	}

	pad_function = f;

	if (pad_function == Triggers) {
		map_triggers ();
	} else if (pad_function == MuteSolo) {
		map_mute_solo ();
	}

	/* Turn up/down arrows on/off depending on pad mode, also scene mode */

	msg[0] = 0xb0;
	msg[2] = (pad_function == Triggers ? 0x3 : 0x0);

	msg[1] = 0x6a; /* upper */
	daw_write (msg, 3);
	msg[1] = 0x6b; /* lower */
	daw_write (msg, 3);
	msg[1] = 0x68; /* scene */
	daw_write (msg, 3);

	configure_display (DAWPadFunctionDisplay, 0x1);
	set_display_target (DAWPadFunctionDisplay, 0, str, true);
}

void
LaunchKey4::select_display_target (DisplayTarget dt)
{
	MidiByteArray msg;

	msg.push_back (0xf0);
	msg.push_back (0x0);
	msg.push_back (0x20);
	msg.push_back (0x29);
	msg.push_back ((device_pid >> 8) & 0x7f);
	msg.push_back (device_pid & 0x7f);
	msg.push_back (0x4);
	msg.push_back (dt);
	msg.push_back (0x7f);
	msg.push_back (0xf7);

	daw_write (msg);
}

void
LaunchKey4::set_plugin_encoder_name (int encoder, int field, std::string const & str)
{
	set_display_target (PluginPotMode, field, str, true);
}

void
LaunchKey4::set_display_target (DisplayTarget dt, int field, std::string const & str, bool display)
{
	MidiByteArray msg;

	msg.push_back (0xf0);
	msg.push_back (0x0);
	msg.push_back (0x20);
	msg.push_back (0x29);
	msg.push_back ((device_pid >> 8) & 0x7f);
	msg.push_back (device_pid & 0x7f);
	msg.push_back (0x6);
	msg.push_back (dt);
	msg.push_back (display ? ((1<<6) | (field & 0x7f)) : (field & 0x7f));

	for (auto c : str) {
		msg.push_back (c & 0x7f);
	}

	msg.push_back (0xf7);

	daw_write (msg);
	write (msg);
}

void
LaunchKey4::configure_display (DisplayTarget target, int config)
{
	MidiByteArray msg (9, 0xf0, 0x00, 0x29, 0xff, 0xff, 0x04, 0xff, 0xff, 0xf7);

	msg[3] = (device_pid >> 8) & 0x7f;
	msg[4] = device_pid & 0x7f;

	msg[6] = target;
	msg[7] = config & 0x7f;

	daw_write (msg);
}

void
LaunchKey4::function_press ()
{
	switch (pad_function) {
	case MuteSolo:
		set_pad_function (Triggers);
		break;
	case Triggers:
		set_pad_function (MuteSolo);
		break;
	}
}

void
LaunchKey4::undo_press ()
{
	if (shift_pressed) {
		redo ();
	} else {
		undo ();
	}
}

void
LaunchKey4::button_press (int n)
{
	std::shared_ptr<AutomationControl> ac;

	if (!stripable[n]) {
		return;
	}

	switch (button_mode) {
	case ButtonsSelect:
		session->selection().select_stripable_and_maybe_group (stripable[n], SelectionSet);
		break;
	case ButtonsRecEnable:
		ac = stripable[n]->rec_enable_control();
		if (ac) {
			ac->set_value (!ac->get_value(), Controllable::NoGroup);
		}
		break;
	}
}

void
LaunchKey4::button_release (int n)
{
}

void
LaunchKey4::solo_changed ()
{
	map_mute_solo ();
}

void
LaunchKey4::mute_changed (uint32_t n)
{
	show_mute (n);
}

void
LaunchKey4::rec_enable_changed (uint32_t n)
{
	show_rec_enable (n);
}

void
LaunchKey4::switch_bank (uint32_t base)
{
	stripable_connections.drop_connections ();

	/* work backwards so we can tell if we should actually switch banks */

	std::shared_ptr<Stripable> s[8];

	for (int n = 0; n < 8; ++n) {
		s[n] = session->get_remote_nth_stripable (base+n, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	}

	if (!s[0]) {
		/* not even the first stripable exists, do nothing */
		return;
	}

	for (int n = 0; n < 8; ++n) {
		stripable[n] = s[n];
	}

	/* at least one stripable in this bank */

	bank_start = base;

	for (int n = 0; n < 8; ++n) {

		if (stripable[n]) {

			/* stripable goes away? refill the bank, starting at the same point */

			stripable[n]->DropReferences.connect (stripable_connections, invalidator (*this), std::bind (&LaunchKey4::switch_bank, this, bank_start), this);
			stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, invalidator (*this), std::bind (&LaunchKey4::stripable_property_change, this, _1, n), this);
			stripable[n]->mute_control()->Changed.connect (stripable_connections, invalidator (*this), std::bind (&LaunchKey4::mute_changed, this, n), this);
			std::shared_ptr<AutomationControl> ac = stripable[n]->rec_enable_control();
			if (ac) {
				ac->Changed.connect (stripable_connections, invalidator (*this), std::bind (&LaunchKey4::rec_enable_changed, this, n), this);
			}
		}

		/* Set fader "title" fields to show current bank */

		for (int n = 0; n < 8; ++n) {
			if (stripable[n]) {
				set_display_target (DisplayTarget (0x5 + n), 0, stripable[n]->name(), true);
			} else {
				set_display_target (DisplayTarget (0x5 + n), 0, string(), true);
			}
		}

		if (session->monitor_out()) {
			set_display_target (DisplayTarget (0x5 + 8), 0, session->monitor_out()->name(), true);
		} else if (session->master_out()) {
			set_display_target (DisplayTarget (0x5 + 8), 0, session->master_out()->name(), true);
		}
	}

	if (button_mode == ButtonsSelect) {
		map_selection ();
	} else {
		map_rec_enable ();
	}

	switch (pad_function) {
	case Triggers:
		map_triggers ();
		break;
	case MuteSolo:
		map_mute_solo ();
		break;
	default:
		break;
	}

	if (encoder_mode != EncoderTransport) {
		set_encoder_titles_to_route_names ();
	}
}

void
LaunchKey4::stripable_property_change (PropertyChange const& what_changed, uint32_t which)
{
	if (what_changed.contains (Properties::color)) {
		show_selection (which);
	}

	if (what_changed.contains (Properties::hidden)) {
		switch_bank (bank_start);
	}

	if (what_changed.contains (Properties::selected)) {

		if (!stripable[which]) {
			return;
		}
	}

}

void
LaunchKey4::stripables_added ()
{
	/* reload current bank */
	switch_bank (bank_start);
}

void
LaunchKey4::button_right ()
{
	if (pad_function == Triggers) {
		switch_bank (bank_start + 1);
		scroll_x_offset = bank_start;
	} else {
		switch_bank (bank_start + 8);
	}
	std::cerr << "rright to " << bank_start << std::endl;

	if (stripable[0]) {
		set_display_target (GlobalTemporaryDisplay, 0, stripable[0]->name(), true);
	}
}

void
LaunchKey4::button_left ()
{
	if (pad_function == Triggers) {
		if (bank_start > 0) {
			switch_bank (bank_start - 1);
			scroll_x_offset = bank_start;
		}
	} else {
		if (bank_start > 7) {
			switch_bank (bank_start - 8);
		}
	}

	std::cerr << "left to " << bank_start << std::endl;

	if (stripable[0]) {
		set_display_target (GlobalTemporaryDisplay, 0, stripable[0]->name(), true);
	}
}

void
LaunchKey4::toggle_button_mode ()
{
	switch (button_mode) {
	case ButtonsSelect:
		button_mode = ButtonsRecEnable;
		map_rec_enable ();
		break;
	case ButtonsRecEnable:
		button_mode = ButtonsSelect;
		map_selection ();
		break;
	}

	MIDI::byte msg[3];
	msg[0] = 0xb0;
	msg[1] = Button9;

	if (button_mode == ButtonsSelect) {
		msg[2] = 0x3; /* brght white */
	} else {
		msg[2] = 0x5; /* red */
	}

	daw_write (msg, 3);
}

void
LaunchKey4::map_selection ()
{
	for (int n = 0; n < 8; ++n) {
		show_selection (n);
	}
}

void
LaunchKey4::show_selection (int n)
{
	const int first_button = 0x25;
	const int selection_color = 0xd; /* bright yellow */

	if (!stripable[n]) {
		light_button (first_button + n, Off, 0);
	} else if (stripable[n]->is_selected()) {
		light_button (first_button + n, Solid, selection_color);
	} else {
		light_button (first_button + n, Solid, find_closest_palette_color (stripable[n]->presentation_info().color ()));
	}
}

void
LaunchKey4::map_mute_solo ()
{
	for (int n = 0; n < 8; ++n)  {
		show_mute (n);
		show_solo (n);
	}
}

void
LaunchKey4::show_mute (int n)
{
	if (!stripable[n]) {
		return;
	}

	std::shared_ptr<MuteControl> mc (stripable[n]->mute_control());
	if (!mc) {
		return;
	}
	MIDI::byte msg[3];
	msg[0] = 0x90;
	msg[1] = 0x60 + n;
	if (mc->muted_by_self()) {
		// std::cerr << stripable[n]->name() << " muted by self\n";
		msg[2] = 0xd; /* bright yellow */
	} else if (mc->muted_by_others_soloing() || mc->muted_by_masters()) {
		// std::cerr << stripable[n]->name() << " muted by others\n";
		msg[2] = 0x49; /* soft yellow */
	} else {
		// std::cerr << stripable[n]->name() << " not muted\n";
		msg[2] = 0x0;;
	}

	daw_write (msg, 3);
}

void
LaunchKey4::show_solo (int n)
{
	if (!stripable[n]) {
		return;
	}

	std::shared_ptr<SoloControl> sc (stripable[n]->solo_control());
	if (!sc) {
		return;
	}
	MIDI::byte msg[3];
	msg[0] = 0x90;
	msg[1] = 0x70 + n;
	if (sc->soloed_by_self_or_masters()) {
		msg[2] = 0x15; /* bright green */
	} else if (sc->soloed_by_others()) {
		msg[2] = 0x4b; /* soft green */
	} else {
		msg[2] = 0x0;
	}

	daw_write (msg, 3);
}

void
LaunchKey4::light_button (int which, LightingMode mode, int color_index)
{
	MIDI::byte msg[3];

	msg[1] = which;

	switch (mode) {
	case Off:
		msg[0] = 0xb0;
		msg[2] = 0x0;
		break;

	case Solid:
		msg[0] = 0xb0;
		msg[2] = color_index & 0x7f;
		break;

	case Flash:
		msg[0] = 0xb1;
		msg[2] = color_index & 0x7f;
		break;

	case Pulse:
		msg[0] = 0xb2;
		msg[2] = color_index & 0x7f;
		break;
	}

	daw_write (msg, 3);
}


void
LaunchKey4::light_pad (int pid, LightingMode mode, int color_index)
{
	MIDI::byte msg[3];

	msg[1] = pid;

	switch (mode) {
	case Off:
		msg[0] = 0x90;
		msg[2] = 0x0;
		break;

	case Solid:
		msg[0] = 0x90;
		msg[2] = color_index & 0x7f;
		break;

	case Flash:
		msg[0] = 0x91;
		msg[2] = color_index & 0x7f;
		break;

	case Pulse:
		msg[0] = 0x92;
		msg[2] = color_index & 0x7f;
		break;
	}

	daw_write (msg, 3);
}

void
LaunchKey4::unlight_pad (int pad_id)
{
	light_pad (pad_id, Solid, 0x0);
}

void
LaunchKey4::set_encoder_bank (int n)
{
	bool light_up_arrow = false;
	bool light_down_arrow = false;

	encoder_bank = n;

	/* Ordering:

	   9
	   1
	   2
	*/

	if (encoder_mode == EncoderPlugins) {

		switch (encoder_bank) {
		case 0:
			if (num_plugin_controls > 8) {
				light_down_arrow = true;
			}
			break;
		case 1:
			if (num_plugin_controls > 8) {
				light_up_arrow = true;
			}
			if (num_plugin_controls > 16) {
				light_down_arrow = true;
			}
			break;
		case 2:
			if (num_plugin_controls > 16) {
				light_up_arrow = true;
			}
			break;
		}

	} else if (encoder_mode == EncoderMixer) {

		switch (encoder_bank) {
		case 0:
			light_down_arrow = true;
			break;
		case 1:
			light_down_arrow = true;
			light_up_arrow = true;
			break;
		case 2:
			light_up_arrow = true;
			break;
		default:
			return;
		}
	}

	MIDI::byte msg[6];
	/* Color doesn't really matter, these LEDs are single-color. Just turn
	   it on or off.
	*/
	const int color_index = 0x3;

	msg[0] = 0xb0;
	msg[1] = 0x33; /* top */
	msg[3] = 0xb0;
	msg[4] = 0x34; /* bottom */

	if (light_up_arrow) {
		msg[2] = color_index;
	} else {
		msg[2] = 0x0;
	}

	if (light_down_arrow) {
		msg[5] = color_index;
	} else {
		msg[5] = 0x0;
	}

	/* Stupid device doesn't seem to like both messages "at once" */
	daw_write (msg, 3);
	daw_write (&msg[3], 3);

	label_encoders ();
}

void
LaunchKey4::label_encoders ()
{
	std::shared_ptr<Plugin> plugin (current_plugin.lock());

	switch (encoder_mode) {
	case EncoderMixer:
	case EncoderSendA:
		set_encoder_titles_to_route_names ();
		switch (encoder_bank) {
		case 0:
			for (int n = 0; n < 8; ++n) {
				set_display_target (DisplayTarget (0x15 + n), 1, "Level", false);
			}
			set_display_target (GlobalTemporaryDisplay, 0, "Levels", true);
			break;
		case 1:
			for (int n = 0; n < 8; ++n) {
				set_display_target (DisplayTarget (0x15 + n), 1, "Pan", false);
			}
			set_display_target (GlobalTemporaryDisplay, 0, "Panning", true);
			break;
		default:
			break;
		}
		break;
	case EncoderPlugins:
		setup_screen_for_encoder_plugins ();
		break;
	case EncoderTransport:
		set_display_target (DisplayTarget (0x15), 1, "Shuttle", true);
		set_display_target (DisplayTarget (0x16), 1, "Zoom", true);
		set_display_target (DisplayTarget (0x17), 1, "Loop Start", true);
		set_display_target (DisplayTarget (0x18), 1, "Loop End", true);
		set_display_target (DisplayTarget (0x19), 1, "Jump to Marker", true);
		set_display_target (DisplayTarget (0x1a), 1, string(), true);
		set_display_target (DisplayTarget (0x1b), 1, string(), true);
		set_display_target (DisplayTarget (0x1c), 1, string(), true);
		for (int n = 0; n < 8; ++n) {
			set_display_target (DisplayTarget (0x15 + n), 0, "Transport", true);
		}
		set_display_target (GlobalTemporaryDisplay, 0, "Transport", true);
		break;
	}
}

void
LaunchKey4::set_encoder_mode (EncoderMode m)
{
	encoder_mode = m;
	set_encoder_bank (0);

	/* device firmware reset to continuous controller mode, so switch back
	 * if (ev->controller_to encoders
	 */

	use_encoders (true);
	label_encoders ();
}

void
LaunchKey4::set_encoder_titles_to_route_names ()
{
	/* Set encoder "title" fields to show current bank */
	bool first = true;

	for (int n = 0; n < 8; ++n) {
		if (stripable[n]) {
			set_display_target (DisplayTarget (0x15 + n), 0, stripable[n]->name(), first);
			first = false;
		} else {
			set_display_target (DisplayTarget (0x15 + n), 0, string(), true);
		}
	}
}

void
LaunchKey4::in_msecs (int msecs, std::function<void()> func)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (msecs); // milliseconds
	timeout->connect (sigc::bind_return (func, false));
	timeout->attach (main_loop()->get_context());
}

void
LaunchKey4::scene_press ()
{
	if (shift_pressed) {
		trigger_stop_all (true); /* immediate stop */
	} else {
		trigger_cue_row (scroll_y_offset);
	}
}
