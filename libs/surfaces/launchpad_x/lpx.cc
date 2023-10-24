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
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
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
#include "lpx.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace Gtkmm2ext;

#include "pbd/abstract_ui.cc" // instantiate template

#define NOVATION     0x1235
#define LAUNCHPADX   0x0103
static const std::vector<MIDI::byte> sysex_header ({ 0xf0, 0x00, 0x20, 0x29, 0x2, 0xc });

const LaunchPadX::Layout LaunchPadX::AllLayouts[] = {
	SessionLayout, Fader, ChordLayout, CustomLayout, NoteLayout, Scale, SequencerSettings,
	SequencerSteps, SequencerVelocity, SequencerPatternSettings, SequencerProbability, SequencerMutation,
	SequencerMicroStep, SequencerProjects, SequencerPatterns, SequencerTempo, SequencerSwing, ProgrammerLayout, Settings, CustomSettings
};

bool
LaunchPadX::available ()
{
	/* no preconditions other than the device being present */
	return true;
}

bool
LaunchPadX::match_usb (uint16_t vendor, uint16_t device)
{
	return vendor == NOVATION && device == LAUNCHPADX;
}

bool
LaunchPadX::probe (std::string& i, std::string& o)
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;

	AudioEngine::instance()->get_ports ("", DataType::MIDI, PortFlags (IsOutput|IsTerminal), midi_inputs);
        AudioEngine::instance()->get_ports("", DataType::MIDI, PortFlags(IsInput | IsTerminal), midi_outputs);

	if (midi_inputs.empty() || midi_outputs.empty()) {
		return false;
	}

	std::regex rx (X_("Launchpad X.*MIDI"));

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

LaunchPadX::LaunchPadX (ARDOUR::Session& s)
	: MIDISurface (s, X_("Novation LaunchPad X"), X_("LaunchPad X"), true)
	, logo_color (4)
	, scroll_x_offset  (0)
	, scroll_y_offset  (0)
	, _daw_out_port (nullptr)
	, _gui (nullptr)
	, _current_layout (SessionLayout)
	, _session_pressed (false)
	, _session_mode (SessionMode)
	, current_fader_bank (VolumeFaders)
	, revert_layout_on_fader_release (false)
	, pre_fader_layout (SessionLayout)
{
	run_event_loop ();
	port_setup ();

	std::string  pn_in, pn_out;
	if (probe (pn_in, pn_out)) {
		_async_in->connect (pn_in);
		_async_out->connect (pn_out);
	}

	connect_daw_ports ();

	build_color_map ();
	build_pad_map ();

	Trigger::TriggerPropertyChange.connect (trigger_connections, invalidator (*this), boost::bind (&LaunchPadX::trigger_property_change, this, _1, _2), this);

	session->RecordStateChanged.connect (session_connections, invalidator(*this), boost::bind (&LaunchPadX::record_state_changed, this), this);
	session->TransportStateChange.connect (session_connections, invalidator(*this), boost::bind (&LaunchPadX::transport_state_changed, this), this);
	session->RouteAdded.connect (session_connections, invalidator(*this), boost::bind (&LaunchPadX::viewport_changed, this), this);
}

LaunchPadX::~LaunchPadX ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "push2 control surface object being destroyed\n");

	trigger_connections.drop_connections ();
	route_connections.drop_connections ();
	session_connections.drop_connections ();

	for (auto & p : pad_map) {
		p.second.timeout_connection.disconnect ();
	}

	stop_event_loop ();
	tear_down_gui ();

	MIDISurface::drop ();

}

void
LaunchPadX::transport_state_changed ()
{
	MIDI::byte msg[3];
	msg[0] = 0x90;

	if (session->transport_rolling()) {
		msg[1] = Play;
		msg[2] = 21;
		daw_write (msg, 3);
	} else {
		msg[1] = Play;
		msg[2] = 17;
		daw_write (msg, 3);
	}
}

void
LaunchPadX::record_state_changed ()
{
}

int
LaunchPadX::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose("Launchpad X::set_active init with yn: %1\n", yn));

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

	DEBUG_TRACE (DEBUG::Launchpad, string_compose("Launchpad X::set_active done with yn: '%1'\n", yn));

	return 0;
}

void
LaunchPadX::run_event_loop ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "start event loop\n");
	BaseUI::run ();
}

void
LaunchPadX::stop_event_loop ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "stop event loop\n");
	BaseUI::quit ();
}

int
LaunchPadX::begin_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "begin using device\n");

	connect_to_port_parser (*_daw_in_port);

	/* Connect DAW input port to event loop */

	AsyncMIDIPort* asp;

	asp = dynamic_cast<AsyncMIDIPort*> (_daw_in_port);
	asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &MIDISurface::midi_input_handler), _daw_in_port));
	asp->xthread().attach (main_loop()->get_context());

	light_logo ();

	set_device_mode (DAW);
	setup_faders (VolumeFaders);
	setup_faders (PanFaders);
	setup_faders (SendFaders);
	setup_faders (DeviceFaders);
	set_layout (SessionLayout);

	/* catch current selection, if any so that we can wire up the pads if appropriate */
	stripable_selection_changed ();
	viewport_changed ();

	return MIDISurface::begin_using_device ();
}

int
LaunchPadX::stop_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "stop using device\n");

	if (!_in_use) {
		DEBUG_TRACE (DEBUG::Launchpad, "nothing to do, device not in use\n");
		return 0;
	}

	all_pads_out ();
	set_device_mode (Standalone);

	return MIDISurface::stop_using_device ();
}

XMLNode&
LaunchPadX::get_state() const
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
LaunchPadX::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("LaunchPadX::set_state: active %1\n", active()));

	int retval = 0;

	if (MIDISurface::set_state (node, version)) {
		return -1;
	}

	return retval;
}

std::string
LaunchPadX::input_port_name () const
{
	return X_(":Launchpad X MK3.*MIDI (In|2)");
}

std::string
LaunchPadX::output_port_name () const
{
	return X_(":Launchpad X MK3.*MIDI (Out|2)");
}

void
LaunchPadX::relax (Pad & pad)
{
}

void
LaunchPadX::build_pad_map ()
{

#define BUTTON0(id)                             pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), &LaunchPadX::relax)))
#define BUTTON(id, press)                       pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), (press))))
#define BUTTON2(id, press, long_press)          pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), (press), (long_press))))
#define BUTTON3(id, press, long_press, release) pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), (press), (long_press), (release))))

	BUTTON  (Down, &LaunchPadX::down_press);
	BUTTON  (Up, &LaunchPadX::up_press);
	BUTTON  (Left, &LaunchPadX::left_press);
	BUTTON  (Right, &LaunchPadX::right_press);
	BUTTON3 (Session, &LaunchPadX::session_press, &LaunchPadX::session_long_press, &LaunchPadX::session_release);
	BUTTON0 (Custom);
	BUTTON  (CaptureMIDI, &LaunchPadX::capture_midi_press);

	BUTTON (Volume, &LaunchPadX::rh0_press);
	BUTTON (Pan, &LaunchPadX::rh1_press);
	BUTTON (SendA, &LaunchPadX::rh2_press);
	BUTTON (SendB, &LaunchPadX::rh3_press);
	BUTTON (StopClip, &LaunchPadX::rh4_press);
	BUTTON (Mute, &LaunchPadX::rh5_press);
	BUTTON(Solo, &LaunchPadX::rh6_press);
	BUTTON (RecordArm, &LaunchPadX::rh7_press);

	/* Now add the 8x8 central pad grid */

	for (int row = 0; row < 8; ++row) {
		for (int col = 0; col < 8; ++col) {
			int pid = (11 + (row * 10)) + col;
			std::pair<int,Pad> p (pid, Pad (pid, col, 7 - row, &LaunchPadX::pad_press, &LaunchPadX::pad_long_press, &LaunchPadX::relax));
			if (!pad_map.insert (p).second) abort();
		}
	}
}

void
LaunchPadX::all_pads_out ()
{
	MIDI::byte msg[3];
	msg[0] = 0x90;
	msg[2] = 0x0;

	for (auto const & p : pad_map) {
		msg[1] = p.second.id;
		daw_write (msg, 3);
	}

	/* Finally, the logo */
	msg[1] = 0x63;
	daw_write (msg, 3);
}


bool
LaunchPadX::light_logo ()
{
	MIDI::byte msg[3];

	msg[0] = 0x91; /* pulse with tempo/midi clock */
	msg[1] = 0x63;
	msg[2] = 4 + (random() % 0x3c);

	daw_write (msg, 3);

	return true;
}

LaunchPadX::Pad*
LaunchPadX::pad_by_id (int pid)
{
	PadMap::iterator p = pad_map.find (pid);
	if (p == pad_map.end()) {
		return nullptr;
	}
	return &p->second;
}

void
LaunchPadX::light_pad (int pad_id, int color, int mode)
{
	MIDI::byte msg[3];
	msg[0] = 0x90 | mode;
	msg[1] = pad_id;
	msg[2] = color;
	daw_write (msg, 3);
}

void
LaunchPadX::pad_off (int pad_id)
{
	MIDI::byte msg[3];
	msg[0] = 0x90;
	msg[1] = pad_id;
	msg[2] = 0;
	daw_write (msg, 3);
}

void
LaunchPadX::all_pads_off ()
{
	MidiByteArray msg (sysex_header);
	msg.reserve (msg.size() + (106 * 3) + 3);
	msg.push_back (0x3);
	for (size_t n = 1; n < 32; ++n) {
		msg.push_back (0x0);
		msg.push_back (n);
		msg.push_back (13);
	}
	msg.push_back (0xf7);
	daw_write (msg);
}

void
LaunchPadX::all_pads_on (int color)
{
	MidiByteArray msg (sysex_header);
	msg.push_back (0xe);
	msg.push_back (color & 0x7f);
	msg.push_back (0xf7);
	daw_write (msg);

#if 0
	for (PadMap::iterator p = pad_map.begin(); p != pad_map.end(); ++p) {
		Pad& pad (p->second);
		pad.set (random() % color_map.size(), Pad::Static);
		daw_write (pad.state_msg());
	}
#endif
}

void
LaunchPadX::set_layout (Layout l, int page)
{
	MidiByteArray msg (sysex_header);
	msg.push_back (0x0);
	msg.push_back (l);
	msg.push_back (page);
	msg.push_back (0x0);
	msg.push_back (0xf7);
	daw_write (msg);

	if (l == Fader) {
		pre_fader_layout = _current_layout;
		current_fader_bank = (FaderBank) page;
	}
}

void
LaunchPadX::set_device_mode (DeviceMode m)
{
	/* programming manual, pages 14 and 18 */
	MidiByteArray standalone_or_daw (sysex_header);
	MidiByteArray live_or_programmer (sysex_header);

	switch (m) {
	case Standalone:
		live_or_programmer.push_back (0xe);
		live_or_programmer.push_back (0x0);
		live_or_programmer.push_back (0xf7);
		/* Back to "live" state */
		write (live_or_programmer);
		g_usleep (100000);
		/* disable "daw" mode */
		standalone_or_daw.push_back (0x10);
		standalone_or_daw.push_back (0x0);
		standalone_or_daw.push_back (0xf7);
		daw_write (standalone_or_daw);
		break;

	case DAW:
		// live_or_programmer.push_back (0xe);
		// live_or_programmer.push_back (0x0);
		// live_or_programmer.push_back (0xf7);
		/* Back to "live" state */
		// daw_write (live_or_programmer);
		// g_usleep (100000);
		/* Enable DAW mode */
		standalone_or_daw.push_back (0x10);
		standalone_or_daw.push_back (0x1);
		standalone_or_daw.push_back (0xf7);
		daw_write (standalone_or_daw);
		break;

	case Programmer:
		live_or_programmer.push_back (0xe);
		live_or_programmer.push_back (0x1);
		live_or_programmer.push_back (0xf7);
		/* enter "programmer" state */
		daw_write (live_or_programmer);
		break;
	}
}

void
LaunchPadX::handle_midi_sysex (MIDI::Parser& parser, MIDI::byte* raw_bytes, size_t sz)
{
	MidiByteArray m (sz, raw_bytes);
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Sysex, %1 bytes parser %2 %s\n", sz, &parser, m));

	if (&parser != _daw_in_port->parser()) {
		DEBUG_TRACE (DEBUG::Launchpad, "sysex from non-DAW port, ignored\n");
		return;
	}

	if (sz < sysex_header.size() + 1) {
		return;
	}

	const size_t num_layouts = sizeof (AllLayouts) / sizeof (AllLayouts[0]);

	raw_bytes += sysex_header.size();

	switch (raw_bytes[0]) {
	case 0x0: /* layout info */
		if (sz < sysex_header.size() + 2) {
			return;
		}

		if (raw_bytes[1] < num_layouts) {
			_current_layout = AllLayouts[raw_bytes[1]];
			DEBUG_TRACE (DEBUG::Launchpad, string_compose ("new layout: %1\n", _current_layout));
			switch (_current_layout) {
			case SessionLayout:
				display_session_layout ();
				map_triggers ();
				break;
			case Fader:
				map_faders ();
				break;
			default:
				break;
			}
			stripable_selection_changed ();
		} else {
			std::cerr << "ignore illegal layout index " << (int) raw_bytes[1] << std::endl;
		}
		break;
	default:
		break;
	}
}

void
LaunchPadX::display_session_layout ()
{
	/* This only needs to be done once (in fact, the device even remembers
	 * it across power-cycling!
	 */

	MIDI::byte msg[3];
	msg[0] = 0x90;

	MIDI::byte color = (_session_mode == SessionMode ? 0x27 : 0x9);

	std::cerr << "redisplay sessionmode, sm " << _session_mode << std::endl;

	msg[1] = Session;
	msg[2] = color;
	daw_write (msg, 3);

	msg[1] = Volume;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = Pan;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = SendA;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = SendB;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = StopClip;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = Mute;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = Solo;
	msg[2] = color;
	daw_write (msg, 3);
	msg[1] = RecordArm;
	msg[2] = color;
	daw_write (msg, 3);

	msg[1] = CaptureMIDI;
	msg[2] = 5;
	daw_write (msg, 3);

	msg[1] = Up;
	msg[2] = 46;
	daw_write (msg, 3);
	msg[1] = Down;
	msg[2] = 46;
	daw_write (msg, 3);
	msg[1] = Left;
	msg[2] = 46;
	daw_write (msg, 3);
	msg[1] = Right;
	msg[2] = 46;
	daw_write (msg, 3);
}

void
LaunchPadX::handle_midi_controller_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));

	if (&parser != _daw_in_port->parser()) {
		/* we don't process CC messages from the regular port */
		return;
	}

	if (_current_layout == Fader) {
		/* Trap fader move messages and act on them */
		if (ev->controller_number >= 0x20 && ev->controller_number < 0x28) {
			fader_move (ev->controller_number, ev->value);
			return;
		}
	}

	PadMap::iterator p = pad_map.find (ev->controller_number);
	if (p == pad_map.end()) {
		return;
	}

	Pad& pad (p->second);

	set<int>::iterator c = consumed.find (pad.id);

	if (c == consumed.end()) {
		if (ev->value) {
			maybe_start_press_timeout (pad);
			(this->*pad.on_press) (pad);
		} else {
			pad.timeout_connection.disconnect ();
			(this->*pad.on_release) (pad);
		}
	} else {
		consumed.erase (c);
	}
}

void
LaunchPadX::handle_midi_note_on_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	if (ev->velocity == 0) {
		handle_midi_note_off_message (parser, ev);
		return;
	}

	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Note On %1/0x%3%4%5 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity, std::hex, (int) ev->note_number, std::dec));

	if (_current_layout != SessionLayout) {
		return;
	}

	PadMap::iterator p = pad_map.find (ev->note_number);
	if (p == pad_map.end()) {
		return;
	}

	Pad& pad (p->second);
	maybe_start_press_timeout (pad);
	(this->*pad.on_pad_press) (pad, ev->velocity);
}

void
LaunchPadX::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Note Off %1/0x%3%4%5 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity, std::hex, (int) ev->note_number, std::dec));

	if (_current_layout != SessionLayout) {
		return;
	}

	PadMap::iterator p = pad_map.find (ev->note_number);
	if (p == pad_map.end()) {
		return;
	}

	Pad& pad (p->second);

	set<int>::iterator c = consumed.find (pad.id);

	if (c == consumed.end()) {
		pad.timeout_connection.disconnect ();
		(this->*pad.on_release) (pad);
	} else {
		/* used for long press */
		consumed.erase (c);
	}

}

void
LaunchPadX::port_registration_handler ()
{
	MIDISurface::port_registration_handler ();
	connect_daw_ports ();
}

void
LaunchPadX::connect_daw_ports ()
{
	if (!_daw_in || !_daw_out) {
		/* ports not registered yet */
		std::cerr << "no daw port registered\n";
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

        std::regex rx (X_("Launchpad X.*(DAW|MIDI 1)"), std::regex::extended);

        auto is_dawport = [&rx](string const &s) {
	        std::string pn = AudioEngine::instance()->get_hardware_port_name_by_name(s);
	        return std::regex_search (pn, rx);
        };

        auto pi = std::find_if (midi_inputs.begin(), midi_inputs.end(), is_dawport);
        auto po = std::find_if (midi_outputs.begin (), midi_outputs.end (), is_dawport);

        if (pi == midi_inputs.end() || po == midi_inputs.end()) {
	        return;
        }

        if (!_daw_in->connected()) {
	        AudioEngine::instance()->connect (_daw_in->name(), *pi);
        }

        if (!_daw_out->connected()) {
	        AudioEngine::instance()->connect (_daw_out->name(), *po);
        }
}

int
LaunchPadX::ports_acquire ()
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
LaunchPadX::ports_release ()
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
LaunchPadX::daw_write (const MidiByteArray& data)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("daw write %1 %2\n", data.size(), data));
	_daw_out_port->write (&data[0], data.size(), 0);
}

void
LaunchPadX::daw_write (MIDI::byte const * data, size_t size)
{

#ifndef NDEBUG
	std::stringstream str;

	if (DEBUG_ENABLED(DEBUG::Launchpad)) {
		str << hex;
		for (size_t n = 0; n < size; ++n) {
			str << (int) data[n] << ' ';
		}
	}
#endif

	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("daw write %1 [%2]\n", size, str.str()));
	_daw_out_port->write (data, size, 0);
}

void
LaunchPadX::scroll_text (std::string const & txt, int color, bool loop, float speed)
{
	MidiByteArray msg (sysex_header);

	msg.push_back (0x32);
	msg.push_back (color);
	msg.push_back (loop ? 1: 0);

	for (std::string::size_type i = 0; i < txt.size(); ++i) {
		msg.push_back (txt[i] & 0xf7);
	}

	msg.push_back (0xf7);
	daw_write (msg);

	if (speed != 0.f) {
		msg[sysex_header.size() + 3] = (MIDI::byte) (floor (1.f + (speed * 6.f)));
		msg[sysex_header.size() + 4] = 0xf7;
		msg.resize (sysex_header.size() + 5);
		daw_write (msg);
	}
}

LaunchPadX::StripableSlot
LaunchPadX::get_stripable_slot (int x, int y) const
{
	x += scroll_x_offset;
	y += scroll_y_offset;

	if ((StripableSlotColumn::size_type) x > stripable_slots.size()) {
		return StripableSlot (-1, -1);
	}

	if ((StripableSlotRow::size_type) y > stripable_slots[x].size()) {
		return StripableSlot (-1, -1);
	}

	return stripable_slots[x][y];
}

void
LaunchPadX::stripable_selection_changed ()
{
	return;
}

void
LaunchPadX::start_press_timeout (Pad& pad)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	pad.timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &LaunchPadX::long_press_timeout), pad.id));
	timeout->attach (main_loop()->get_context());
}

void
LaunchPadX::maybe_start_press_timeout (Pad& pad)
{
	if (pad.on_long_press == &LaunchPadX::relax) {
		return;
	}
	start_press_timeout (pad);
}

bool
LaunchPadX::long_press_timeout (int pad_id)
{
	PadMap::iterator p = pad_map.find (pad_id);
	if (p == pad_map.end()) {
		/* impossible */
		return false;
	}
	Pad& pad (p->second);

	(this->*pad.on_long_press) (pad);

	return false; /* don't get called again */
}

void
LaunchPadX::left_press (Pad& pad)
{
	const int shift = (_session_pressed ? 9 : 1);
	if (scroll_x_offset >= shift) {
		scroll_x_offset -= shift;
	}
	viewport_changed ();
}

void
LaunchPadX::right_press (Pad& pad)
{
	const int shift = (_session_pressed ? 9 : 1);
	scroll_x_offset += shift;
	viewport_changed ();
}

void
LaunchPadX::session_press (Pad& pad)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("session press, mode %1\n", _session_mode));

	if (_session_mode == SessionMode) {
		_session_mode = MixerMode;
	} else {
		_session_mode = SessionMode;
	}
	display_session_layout ();
}

void
LaunchPadX::session_release (Pad& pad)
{
}

void
LaunchPadX::note_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadX::custom_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadX::cue_press (Pad& pad, int row)
{
	session->trigger_cue_row (row);
}


void
LaunchPadX::rh0_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 0 + scroll_y_offset);
		}
	}
}

void
LaunchPadX::rh1_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 1 + scroll_y_offset);
		} else {
			pan_press (pad);
		}
	}
}

void
LaunchPadX::rh2_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 2 + scroll_y_offset);
		} else {
			send_a_press (pad);
		}
	}
}

void
LaunchPadX::rh3_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 3 + scroll_y_offset);
		} else {
			send_b_press (pad);
		}
	}
}

void
LaunchPadX::rh4_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 4 + scroll_y_offset);
		} else {
			stop_clip_press (pad);
		}
	}
}

void
LaunchPadX::rh5_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 5 + scroll_y_offset);
		} else {
			mute_press (pad);
		}
	}
}

void
LaunchPadX::rh6_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 6 + scroll_y_offset);
		} else {
			solo_press (pad);
		}
	}
}

void
LaunchPadX::rh7_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		if (_session_mode == SessionMode) {
			cue_press (pad, 7 + scroll_y_offset);
		} else {
			record_arm_press (pad);
		}
	}
}

void
LaunchPadX::stop_clip_press (Pad& pad)
{

}

void
LaunchPadX::fader_long_press (Pad&)
{
	revert_layout_on_fader_release = true;
}

void
LaunchPadX::fader_release (Pad&)
{
	if (revert_layout_on_fader_release) {
		set_layout (pre_fader_layout);
		revert_layout_on_fader_release = false;
	}
}

void
LaunchPadX::volume_press (Pad& pad)
{
	if (_current_layout == Fader && current_fader_bank == VolumeFaders) {
		set_layout (SessionLayout);
		return;
	}
	set_layout (Fader, VolumeFaders);
}

void
LaunchPadX::pan_press (Pad& pad)
{
	if (_current_layout == Fader && current_fader_bank == PanFaders) {
		set_layout (SessionLayout);
		return;
	}
	set_layout (Fader, PanFaders);
}

void
LaunchPadX::send_a_press (Pad& pad)
{
	if (_current_layout == Fader && current_fader_bank == SendFaders) {
		set_layout (SessionLayout);
		return;
	}
	set_layout (Fader, SendFaders);
}

void
LaunchPadX::send_b_press (Pad& pad)
{
	if (_current_layout == Fader && current_fader_bank == SendFaders) {
		set_layout (SessionLayout);
		return;
	}
	set_layout (Fader, SendFaders);
}

void
LaunchPadX::mute_press (Pad& pad)
{
	std::shared_ptr<Stripable> s = session->selection().first_selected_stripable();
	if (s) {
		std::shared_ptr<AutomationControl> ac = s->mute_control();
		if (ac) {
			ac->set_value (!ac->get_value(), PBD::Controllable::UseGroup);
		}
	}
}

void
LaunchPadX::solo_press (Pad& pad)
{
	std::shared_ptr<Stripable> s = session->selection().first_selected_stripable();
	if (s) {
		std::shared_ptr<AutomationControl> ac = s->solo_control();
		if (ac) {
			session->set_control (ac, !ac->get_value(), PBD::Controllable::UseGroup);
		}
	}
}

void
LaunchPadX::solo_long_press (Pad& pad)
{
	cancel_all_solo ();
	/* Pad was used for long press, do not invoke release action */
	consumed.insert (pad.id);
}

void
LaunchPadX::record_arm_press (Pad& pad)
{
	std::shared_ptr<Stripable> s = session->selection().first_selected_stripable();
	if (s) {
		std::shared_ptr<AutomationControl> ac = s->rec_enable_control();
		if (ac) {
			ac->set_value (!ac->get_value(), PBD::Controllable::UseGroup);
		}
	}
}

void
LaunchPadX::capture_midi_press (Pad& pad)
{
	set_record_enable (!get_record_enabled());
}

void
LaunchPadX::down_press (Pad& pad)
{
	const int shift = (_session_pressed ? 9 : 1);

	if (scroll_y_offset >= shift) {
		scroll_y_offset -= shift;
	}
}

void
LaunchPadX::up_press (Pad& pad)
{
	const int shift = (_session_pressed ? 9 : 1);
	scroll_y_offset += shift;
}

void
LaunchPadX::pad_press (Pad& pad, int velocity)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("pad press on %1, %2 => %3 vel %4\n", pad.x, pad.y, pad.id, velocity));
	session->bang_trigger_at (pad.x, pad.y, velocity / 127.0f);
	start_press_timeout (pad);
}

void
LaunchPadX::pad_long_press (Pad& pad)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("pad long press on %1, %2 => %3\n", pad.x, pad.y, pad.id));
	session->unbang_trigger_at (pad.x, pad.y);
	/* Pad was used for long press, do not invoke release action */
	consumed.insert (pad.id);
}

void
LaunchPadX::trigger_property_change (PropertyChange pc, Trigger* t)
{
	int x = t->box().order();
	int y = t->index();

	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("prop change %1 for trigger at %2, %3\n", pc, x, y));

	if (y > scroll_y_offset + 7) {
		/* not visible at present */
		return;
	}

	if (x > scroll_x_offset + 7) {
		/* not visible at present */
		return;
	}

	/* name property change is sent when slots are loaded or unloaded */

	PropertyChange our_interests;
	our_interests.add (Properties::running);
	our_interests.add (Properties::name);;

	if (pc.contains (our_interests)) {

		int pid = (11 + ((7 - y) * 10)) + x;
		MidiByteArray msg;
		std::shared_ptr<Route> r = session->get_remote_nth_route (scroll_x_offset + x);

		if (!r || !t->region()) {
			msg.push_back (0x90);
			msg.push_back (pid);
			msg.push_back (0x0);
			daw_write (msg);
			return;
		}

		switch (t->state()) {
		case Trigger::Stopped:
			msg.push_back (0x90);
			msg.push_back (pid);
			msg.push_back (find_closest_palette_color (r->presentation_info().color()));
			break;

		case Trigger::WaitingToStart:
			msg.push_back (0x91); /* channel 1=> pulsing */
			msg.push_back (pid);
			msg.push_back (0x17); // find_closest_palette_color (r->presentation_info().color()));
			break;

		case Trigger::Running:
			/* choose contrasting color from the base one */
			msg.push_back (0x90);
			msg.push_back (pid);
			msg.push_back (find_closest_palette_color (HSV(r->presentation_info().color()).opposite()));
			break;

		case Trigger::WaitingForRetrigger:
		case Trigger::WaitingToStop:
		case Trigger::WaitingToSwitch:
		case Trigger::Stopping:
			msg.push_back (0x91);
			msg.push_back (pid);
			msg.push_back (find_closest_palette_color (HSV(r->presentation_info().color()).opposite()));
		}

		daw_write (msg);
	}
}

void
LaunchPadX::map_triggers ()
{
	for (int x = 0; x < 8; ++x) {
		map_triggerbox (x);
	}
}

void
LaunchPadX::map_triggerbox (int x)
{
	MIDI::byte msg[3];

	msg[0] = 0x90;

	std::shared_ptr<Route> r = session->get_remote_nth_route (scroll_x_offset + x);
	int palette_index;

	if (r) {
		palette_index = find_closest_palette_color (r->presentation_info().color());
	} else {
		palette_index = 0x0;
	}

	for (int y = 0; y < 8; ++y) {

		int xp = x + scroll_x_offset;
		int yp = y + scroll_y_offset;

		int pid = (11 + ((7 - y) * 10)) + x;
		msg[1] = pid;

		TriggerPtr t = session->trigger_at (xp, yp);

		if (!t || !t->region()) {
			msg[2] = 0x0;
		} else {
			msg[2] = palette_index;
		}

		daw_write (msg, 3);
	}
}

void
LaunchPadX::build_color_map ()
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
LaunchPadX::find_closest_palette_color (uint32_t color)
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
LaunchPadX::viewport_changed ()
{
	route_connections.drop_connections ();

	for (int n = 0; n < 8; ++n) {
		std::shared_ptr<Route> r = session->get_remote_nth_route (scroll_x_offset + n);
		if (r) {
			r->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&LaunchPadX::viewport_changed, this), this);
			r->presentation_info().PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&LaunchPadX::route_property_change, this, _1, n), this);
		} else {
			if (n == 0) {
				/* not even the first stripable ... so do nothing */
			}
		}
	}


	switch (_current_layout) {
	case SessionLayout:
		map_triggers ();
		break;
	case Fader:
		map_faders ();
		break;
	default:
		break;
	}

	stripable_selection_changed ();
}

void
LaunchPadX::route_property_change (PropertyChange const & pc, int col)
{
	if (pc.contains (Properties::color)) {
		map_triggerbox (col);
	}


	if (pc.contains (Properties::selected)) {
	}
}

void
LaunchPadX::setup_faders (FaderBank bank)
{
	MidiByteArray msg (sysex_header);

	msg.push_back (1); /* fader bank command */
	msg.push_back (bank);
	switch (bank) {
	case PanFaders:
		msg.push_back (1); /* vertical orientation */
		break;
	default:
		msg.push_back (0); /* vertical orientation */
		break;
	}
	for (int n = 0; n < 8; ++n) {
		msg.push_back (n);         /* fader number */
		switch (bank) {
		case PanFaders:
			msg.push_back (1); /* bipolar */
			break;
		default:
			msg.push_back (0); /* unipolar */
			break;
		}
		msg.push_back (0x20+n);       /* CC number */
		msg.push_back (random() % 127); /* color */
	}

	msg.push_back (0xf7);
	daw_write (msg);
}

void
LaunchPadX::fader_move (int cc, int val)
{
	std::shared_ptr<Route> r;

	switch (current_fader_bank) {
	case SendFaders:
	case DeviceFaders:
		r = std::dynamic_pointer_cast<Route> (session->selection().first_selected_stripable());
		break;
	default:
		r = session->get_remote_nth_route (scroll_x_offset + (cc - 0x20));
		break;
	}

	if (!r) {
		return;
	}

	std::shared_ptr<AutomationControl> ac;

	switch (current_fader_bank) {
	case VolumeFaders:
		ac = r->gain_control();
		if (ac) {
			session->set_control (ac, ARDOUR::slider_position_to_gain_with_max (val/127.0, ARDOUR::Config->get_max_gain()), PBD::Controllable::NoGroup);
		}
		break;
	case PanFaders:
		ac = r->pan_azimuth_control();
		if (ac) {
			session->set_control (ac, val/127.0, PBD::Controllable::NoGroup);
		}
		break;
	case SendFaders:
		ac = r->send_level_controllable (scroll_x_offset + (cc - 0x20));
		if (ac) {
			session->set_control (ac, ARDOUR::slider_position_to_gain_with_max (val/127.0, ARDOUR::Config->get_max_gain()), PBD::Controllable::NoGroup);
		}
		break;
	default:
		break;
	}
}

void
LaunchPadX::map_faders ()
{
	MIDI::byte msg[3];
	msg[0] = 0xb4;

	control_connections.drop_connections ();

	for (int n = 0; n < 8; ++n) {
		std::shared_ptr<Route> r;


		switch (current_fader_bank) {
		case SendFaders:
		case DeviceFaders:
			r = std::dynamic_pointer_cast<Route> (session->selection().first_selected_stripable());
			break;
		default:
			r = session->get_remote_nth_route (scroll_x_offset + n);
			break;
		}

		std::shared_ptr<AutomationControl> ac;

		msg[1] = 0x20 + n;

		if (!r) {
			switch (current_fader_bank) {
			case PanFaders:
				msg[2] = 63; /* neutral position is halfway across */
				break;
			default:
				msg[2] = 0;  /* neutral position is at bottom */
				break;
			}
			daw_write (msg, 3);
			continue;
		}

		switch (current_fader_bank) {
		case VolumeFaders:
			ac = r->gain_control();
			if (ac) {
				msg[2] = (MIDI::byte) (ARDOUR::gain_to_slider_position_with_max (ac->get_value(), ARDOUR::Config->get_max_gain()) * 127.0);
			} else {
				msg[2] = 0;
			}
			break;
		case PanFaders:
			ac = r->pan_azimuth_control ();
			if (ac) {
				msg[2] = (MIDI::byte) (ac->get_value() * 127.0);
			} else {
				msg[2] = 0;
			}
			break;
		case SendFaders:
			ac = r->send_level_controllable (n);
			if (ac) {
				msg[2] = (MIDI::byte) (ARDOUR::gain_to_slider_position_with_max (ac->get_value(), ARDOUR::Config->get_max_gain()) * 127.0);
			} else {
				msg[2] = 0;
			}
			break;
		default:
			msg[2] = 0;
			break;
		}

		if (ac) {
			ac->Changed.connect (control_connections, invalidator (*this), boost::bind (&LaunchPadX::automation_control_change, this, n, std::weak_ptr<AutomationControl> (ac)), this);
		}

		daw_write (msg, 3);
	}
}

void
LaunchPadX::automation_control_change (int n, std::weak_ptr<AutomationControl> wac)
{
	std::shared_ptr<AutomationControl> ac = wac.lock();
	if (!ac) {
		return;
	}

	MIDI::byte msg[3];
	msg[0] = 0xb4;
	msg[1] = 0x20 + n;

	switch (current_fader_bank) {
	case VolumeFaders:
	case SendFaders:
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
