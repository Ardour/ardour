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
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types_convert.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "gtkmm2ext/colors.h"

#include "gui.h"
#include "lppro.h"

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

#define NOVATION          0x1235
#define LAUNCHPADPROMK3   0x0123

bool
LaunchPadPro::available ()
{
	bool rv = LIBUSB_SUCCESS == libusb_init (0);
	if (rv) {
		libusb_exit (0);
	}
	return rv;
}

bool
LaunchPadPro::match_usb (uint16_t vendor, uint16_t device)
{
	return vendor == NOVATION && device == LAUNCHPADPROMK3;
}

bool
LaunchPadPro::probe (std::string& i, std::string& o)
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;
	AudioEngine::instance()->get_ports ("", DataType::MIDI, PortFlags (IsOutput|IsTerminal), midi_inputs);
	AudioEngine::instance()->get_ports ("", DataType::MIDI, PortFlags (IsInput|IsTerminal), midi_outputs);

	auto has_lppro = [](string const& s) {
		std::string pn = AudioEngine::instance()->get_hardware_port_name_by_name (s);
		return pn.find ("Launchpad Pro MK3 MIDI 1") != string::npos;
	};

	auto pi = std::find_if (midi_inputs.begin (), midi_inputs.end (), has_lppro);
	auto po = std::find_if (midi_outputs.begin (), midi_outputs.end (), has_lppro);

	if (pi == midi_inputs.end () || po == midi_outputs.end ()) {
		return false;
	}

	i = *pi;
	o = *po;
	return true;
}

LaunchPadPro::LaunchPadPro (ARDOUR::Session& s)
	: MIDISurface (s, X_("Novation Launchpad Pro"), X_("Launchpad Pro"), false)
	, _daw_out_port (nullptr)
	, _gui (nullptr)
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
}

LaunchPadPro::~LaunchPadPro ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "push2 control surface object being destroyed\n");

	stop_event_loop ();

	MIDISurface::drop ();
}

int
LaunchPadPro::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose("Launchpad Pro::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		if (device_acquire ()) {
			return -1;
		}

		if ((_connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {
			std::cerr << "LPP claims connection state is " << _connection_state << std::endl;
			begin_using_device ();
		} else {
			/* begin_using_device () will get called once we're connected */
		}

	} else {
		/* Control Protocol Manager never calls us with false, but
		 * insteads destroys us.
		 */
	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::Launchpad, string_compose("Launchpad Pro::set_active done with yn: '%1'\n", yn));

	return 0;
}

void
LaunchPadPro::run_event_loop ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "start event loop\n");
	BaseUI::run ();
}

void
LaunchPadPro::stop_event_loop ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "stop event loop\n");
	BaseUI::quit ();
}

int
LaunchPadPro::begin_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "begin using device\n");

	set_device_mode (Programmer);
	// all_pads_off ();
	all_pads_on ();
#if 0
	init_buttons (true);
	init_touch_strip (false);
	reset_pad_colors ();
	splash ();

	/* catch current selection, if any so that we can wire up the pads if appropriate */
	stripable_selection_changed ();

	request_pressure_mode ();
#endif

	return MIDISurface::begin_using_device ();
}

int
LaunchPadPro::stop_using_device ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "stop using device\n");

	if (!_in_use) {
		DEBUG_TRACE (DEBUG::Launchpad, "nothing to do, device not in use\n");
		return 0;
	}

	set_device_mode (Standalone);

#if 0
	init_buttons (false);
	strip_buttons_off ();

	for (auto & pad : _xy_pad_map) {
		pad->set_color (LED::Black);
		pad->set_state (LED::NoTransition);
		write (pad->state_msg());
	}

#endif
	return MIDISurface::stop_using_device ();
}

XMLNode&
LaunchPadPro::get_state() const
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
LaunchPadPro::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("LaunchPadPro::set_state: active %1\n", active()));

	int retval = 0;

	if (MIDISurface::set_state (node, version)) {
		return -1;
	}

	return retval;
}

std::string
LaunchPadPro::input_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Novation
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_("system:midi_capture_1319078870");
#else
	return X_("Launchpad Pro MK3 MIDI 1 in");
#endif
}

std::string
LaunchPadPro::input_daw_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Novation
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_("system:midi_capture_1319078870");
#else
	return X_("Launchpad Pro MK3 MIDI 3 in");
#endif
}

std::string
LaunchPadPro::output_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Novation
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_("system:midi_playback_3409210341");
#else
	return X_("Launchpad Pro MK3 MIDI 1 out");
#endif
}

std::string
LaunchPadPro::output_daw_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Novation
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_("system:midi_playback_3409210341");
#else
	return X_("Launchpad Pro MK3 MIDI 3 out");
#endif
}

void
LaunchPadPro::stripable_selection_changed ()
{
}

void
LaunchPadPro::build_color_map ()
{
	/* RGB values taken from using color picker on PDF of LP manual, page 10 */

	static int novation_color_chart_right_side[] = {
		0x0,
		0xb3b3b3,
		0xdddddd,
		0xffffff,
		0xffb3b3,
		0xff6161,
		0xdd6161,
		0xb36161,
		0xfff3d5,
		0xffb361,
		0xdd8c61,
		0xb37661,
		0xffeea1,
		0xffff61,
		0xdddd61,
		0xb3b361,
		0xddffa1,
		0xc2ff61,
		0xa1dd61,
		0x81b361,
		0xc2ffb3,
		0x61ff61,
		0x61dd61,
		0x61b361,
		0xc2ffc2,
		0x61ff8c,
		0x61dd76,
		0x61b36b,
		0xc2ffcc,
		0x61ffcc,
		0x61dda1,
		0x61b381,
		0xc2fff3,
		0x61ffe9,
		0x61ddc2,
		0x61b396,
		0xc2f3ff,
		0x61eeff,
		0x61c7dd,
		0x61a1b3,
		0xc2ddff,
		0x61c7ff,
		0x61a1dd,
		0x6181b3,
		0xa18cff,
		0x6161ff,
		0x6161dd,
		0x6161b3,
		0xccb3ff,
		0xa161ff,
		0x8161dd,
		0x7661b3,
		0xffb3ff,
		0xff61ff,
		0xdd61dd,
		0xb361b3,
		0xffb3d5,
		0xff61c2,
		0xdd61a1,
		0xb3618c,
		0xff7661,
		0xe9b361,
		0xddc261,
		0xa1a161,
	};

	for (size_t n = 0; n < sizeof (novation_color_chart_right_side) / sizeof (novation_color_chart_right_side[0]); ++n) {
		int color = novation_color_chart_right_side[n];
		std::pair<int,int> p (n, color);
		color_map.insert (p);
	}

	assert (color_map.size() == 64);
}

void
LaunchPadPro::build_pad_map ()
{
#define EDGE_PAD(id) if (!(pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id)))).second)) abort()

	EDGE_PAD (Shift);

	EDGE_PAD (Left);
	EDGE_PAD (Right);
	EDGE_PAD (Session);
	EDGE_PAD (Note);
	EDGE_PAD (Chord);
	EDGE_PAD (Custom);
	EDGE_PAD (Sequencer);
	EDGE_PAD (Projects);

	EDGE_PAD (Patterns);
	EDGE_PAD (Steps);
	EDGE_PAD (PatternSettings);
	EDGE_PAD (Velocity);
	EDGE_PAD (Probability);
	EDGE_PAD (Mutation);
	EDGE_PAD (MicroStep);
	EDGE_PAD (PrintToClip);

	EDGE_PAD (StopClip);
	EDGE_PAD (Device);
	EDGE_PAD (Sends);
	EDGE_PAD (Pan);
	EDGE_PAD (Volume);
	EDGE_PAD (Solo);
	EDGE_PAD (Mute);
	EDGE_PAD (RecordArm);

	EDGE_PAD (CaptureMIDI);
	EDGE_PAD (Play);
	EDGE_PAD (FixedLength);
	EDGE_PAD (Quantize);
	EDGE_PAD (Duplicate);
	EDGE_PAD (Clear);
	EDGE_PAD (Down);
	EDGE_PAD (Up);

	EDGE_PAD (Lower1);
	EDGE_PAD (Lower2);
	EDGE_PAD (Lower3);
	EDGE_PAD (Lower4);
	EDGE_PAD (Lower5);
	EDGE_PAD (Lower6);
	EDGE_PAD (Lower7);
	EDGE_PAD (Lower8);

	/* Now add the 8x8 central pad grid */

	for (int row = 0; row < 8; ++row) {
		for (int col = 0; col < 8; ++col) {
			int pid = (11 + (row * 10)) + col;
			std::pair<int,Pad> p (pid, Pad (pid, row, col));
			if (!pad_map.insert (p).second) abort();
		}
	}

	std::cerr << "pad map is " << pad_map.size() << std::endl;

	/* The +1 is for the shift pad at upper left */
	assert (pad_map.size() == (64 + (5 * 8) + 1));
}

LaunchPadPro::Pad*
LaunchPadPro::pad_by_id (int pid)
{
	PadMap::iterator p = pad_map.find (pid);
	if (p == pad_map.end()) {
		return nullptr;
	}
	return &p->second;
}

void
LaunchPadPro::light_pad (int pad_id, int color, Pad::ColorMode mode)
{
	Pad* pad = pad_by_id (pad_id);
	if (!pad) {
		return;
	}
	pad->set (color, mode);
	write (pad->state_msg());
}

void
LaunchPadPro::pad_off (int pad_id)
{
	Pad* pad = pad_by_id (pad_id);
	if (!pad) {
		return;
	}
	pad->set (0, Pad::Static);
	write (pad->state_msg());
}

void
LaunchPadPro::all_pads_off ()
{
	for (PadMap::iterator p = pad_map.begin(); p != pad_map.end(); ++p) {
		Pad& pad (p->second);
		pad.set (13, Pad::Static);
		write (pad.state_msg());
	}
}

void
LaunchPadPro::all_pads_on ()
{
	for (PadMap::iterator p = pad_map.begin(); p != pad_map.end(); ++p) {
		Pad& pad (p->second);
		pad.set (random() % color_map.size(), Pad::Static);
		write (pad.state_msg());
	}
}

void
LaunchPadPro::set_device_mode (DeviceMode m)
{
	/* LP Pro MK3 programming manual, pages 14 and 18 */
	MidiByteArray msg (9, 0xf0, 0x00, 0x20, 0x29, 0x2, 0xe, 0x10, 0x0, 0xf7);

	switch (m) {
	case Standalone:
		/* no edit necessary */
		break;
	case DAW:
		msg[7] = 0x1;
		break;
	case LiveSession:
		msg[6] = 0xe;
		msg[7] = 0x0;
		break;
	case Programmer:
		msg[6] = 0xe;
		msg[7] = 0x1;
		break;
	}


	if (m == Programmer) {
		std::cerr << "Send to port 1 " << msg << " to enter mode " << m << std::endl;
		write (msg);
	} else {
		std::cerr << "back to live mode\n";
		MidiByteArray first_msg (9, 0xf0, 0x00, 0x20, 0x29, 0x2, 0xe, 0xe, 0x0, 0xf7);
		write (first_msg);
	}
}

void
LaunchPadPro::handle_midi_sysex (MIDI::Parser&, MIDI::byte* raw_bytes, size_t sz)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Sysex, %1 bytes\n", sz));
}

void
LaunchPadPro::handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));

}

void
LaunchPadPro::handle_midi_note_on_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	if (ev->velocity == 0) {
		handle_midi_note_off_message (parser, ev);
		return;
	}

	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Note On %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));
}

void
LaunchPadPro::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Note Off %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));
}

void
LaunchPadPro::port_registration_handler ()
{
	MIDISurface::port_registration_handler ();
	connect_daw_ports ();
}

void
LaunchPadPro::connect_daw_ports ()
{
	if (!_daw_in || !_daw_out) {
		/* ports not registered yet */
		return;
	}

	if (_daw_in->connected() && _daw_out->connected()) {
		/* don't waste cycles here */
		return;
	}

	std::vector<std::string> in;
	std::vector<std::string> out;

	AudioEngine::instance()->get_ports (string_compose (".*%1", input_daw_port_name()), DataType::MIDI, PortFlags (IsPhysical|IsOutput), in);
	AudioEngine::instance()->get_ports (string_compose (".*%1", output_daw_port_name()), DataType::MIDI, PortFlags (IsPhysical|IsInput), out);

	if (!in.empty() && !out.empty()) {
		if (!_daw_in->connected()) {
			AudioEngine::instance()->connect (_daw_in->name(), in.front());
		}
		if (!_daw_out->connected()) {
			AudioEngine::instance()->connect (_daw_out->name(), out.front());
		}
	}
}

int
LaunchPadPro::ports_acquire ()
{
	int ret = MIDISurface::ports_acquire ();

	if (!ret) {
		_daw_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, string_compose (X_("%1 daw in"), port_name_prefix), true);
		if (_daw_in) {
			_daw_in_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_daw_in).get();
			_daw_out = AudioEngine::instance()->register_output_port (DataType::MIDI, string_compose (X_("%1 daw out"), port_name_prefix), true);
		}
		if (_daw_out) {
			_daw_out_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();
			return 0;
		}

		connect_to_port_parser (*_daw_in_port);

		ret = -1;
	}

	return ret;
}

void
LaunchPadPro::ports_release ()
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
LaunchPadPro::daw_write (const MidiByteArray& data)
{
	/* immediate delivery */
	_daw_out_port->write (&data[0], data.size(), 0);
}

void
LaunchPadPro::daw_write (MIDI::byte const * data, size_t size)
{
	_daw_out_port->write (data, size, 0);
}
