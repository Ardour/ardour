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
#include "ardour/triggerbox.h"
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
static const std::vector<MIDI::byte> sysex_header ({ 0xf0, 0x00, 0x20, 0x29, 0x2, 0xe });

const LaunchPadPro::PadID LaunchPadPro::all_pad_ids[] = {
	Shift, Left, Right, Session, Note, Chord, Custom, Sequencer, Projects,
	Patterns, Steps, PatternSettings, Velocity, Probability, Mutation, MicroStep, PrintToClip,
	StopClip, Device, Sends, Pan, Volume, Solo, Mute, RecordArm,
	CaptureMIDI, Play, FixedLength, Quantize, Duplicate, Clear, Down, Up,
	Lower1, Lower2, Lower3, Lower4, Lower5, Lower6, Lower7, Lower8,
};

const LaunchPadPro::Layout LaunchPadPro::AllLayouts[] = {
	SessionLayout, Fader, ChordLayout, CustomLayout, NoteLayout, Scale, SequencerSettings,
	SequencerSteps, SequencerVelocity, SequencerPatternSettings, SequencerProbability, SequencerMutation,
	SequencerMicroStep, SequencerProjects, SequencerPatterns, SequencerTempo, SequencerSwing, ProgrammerLayout, Settings, CustomSettings
};

bool
LaunchPadPro::available ()
{
	/* no preconditions other than the device being present */
	return true;
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
	: MIDISurface (s, X_("Novation Launchpad Pro"), X_("Launchpad Pro"), true)
	, logo_color (4)
	, scroll_x_offset  (0)
	, scroll_y_offset  (0)
	, _daw_out_port (nullptr)
	, _gui (nullptr)
	, _current_layout (SessionLayout)
	, _shift_pressed (false)
{
	run_event_loop ();
	port_setup ();

	std::string  pn_in, pn_out;
	if (probe (pn_in, pn_out)) {
		_async_in->connect (pn_in);
		_async_out->connect (pn_out);
	}

	connect_daw_ports ();

	build_pad_map ();

	Trigger::TriggerPropertyChange.connect (trigger_connections, invalidator (*this), boost::bind (&LaunchPadPro::trigger_property_change, this, _1, _2, _3), this);

	session->RecordStateChanged.connect(session_connections, invalidator(*this), boost::bind (&LaunchPadPro::record_state_changed, this), this);
	session->TransportStateChange.connect(session_connections, invalidator(*this), boost::bind (&LaunchPadPro::transport_state_changed, this), this);

}

LaunchPadPro::~LaunchPadPro ()
{
	DEBUG_TRACE (DEBUG::Launchpad, "push2 control surface object being destroyed\n");

	stop_event_loop ();

	MIDISurface::drop ();
}

void
LaunchPadPro::transport_state_changed ()
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
LaunchPadPro::record_state_changed ()
{
}

int
LaunchPadPro::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose("Launchpad Pro::set_active init with yn: %1\n", yn));

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

	connect_to_port_parser (*_daw_in_port);

	/* Connect DAW input port to event loop */

	AsyncMIDIPort* asp;

	asp = dynamic_cast<AsyncMIDIPort*> (_daw_in_port);
	asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &MIDISurface::midi_input_handler), _daw_in_port));
	asp->xthread().attach (main_loop()->get_context());

	light_logo ();

	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (1000); // milliseconds
	timeout->connect (sigc::mem_fun (*this, &LaunchPadPro::light_logo));
	timeout->attach (main_loop()->get_context());

	set_device_mode (DAW);
	set_layout (SessionLayout);

	/* catch current selection, if any so that we can wire up the pads if appropriate */
	stripable_selection_changed ();

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

	all_pads_out ();
	set_device_mode (Standalone);

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
	return X_("Launchpad Pro MK3 MIDI 1");
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
	return X_("Launchpad Pro MK3 MIDI 3");
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
	return X_("Launchpad Pro MK3 MIDI 1");
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
	return X_("Launchpad Pro MK3 MIDI 3");
#endif
}

void
LaunchPadPro::build_pad_map ()
{
#define EDGE_PAD0(id) if (!(pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), &LaunchPadPro::relax))).second)) abort()
#define EDGE_PAD(id, press) if (!(pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), (press)))).second)) abort()
#define EDGE_PAD2(id, press, release) if (!(pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), (press), (release)))).second)) abort()
#define EDGE_PAD3(id, press, release, long_press) if (!(pad_map.insert (make_pair<int,Pad> ((id),  Pad ((id), (press), (release), (long_press)))).second)) abort()

	EDGE_PAD2 (Shift, &LaunchPadPro::shift_press, &LaunchPadPro::shift_release);

	EDGE_PAD0 (Left);
	EDGE_PAD0 (Right);
	EDGE_PAD0 (Session);
	EDGE_PAD0 (Note);
	EDGE_PAD0 (Chord);
	EDGE_PAD0 (Custom);
	EDGE_PAD0 (Sequencer);
	EDGE_PAD0 (Projects);

	EDGE_PAD (Patterns, &LaunchPadPro::patterns_press);
	EDGE_PAD (Steps, &LaunchPadPro::steps_press);
	EDGE_PAD (PatternSettings, &LaunchPadPro::pattern_settings_press);
	EDGE_PAD (Velocity, &LaunchPadPro::velocity_press);
	EDGE_PAD (Probability, &LaunchPadPro::probability_press);
	EDGE_PAD (Mutation, &LaunchPadPro::mutation_press);
	EDGE_PAD (MicroStep, &LaunchPadPro::microstep_press);
	EDGE_PAD (PrintToClip, &LaunchPadPro::print_to_clip_press);

	EDGE_PAD (StopClip, &LaunchPadPro::stop_clip_press);
	EDGE_PAD0 (Device);
	EDGE_PAD0 (Sends);
	EDGE_PAD0 (Pan);
	EDGE_PAD0 (Volume);
	EDGE_PAD0 (Solo);
	EDGE_PAD0 (Mute);
	EDGE_PAD0 (RecordArm);

	EDGE_PAD0 (CaptureMIDI);
	EDGE_PAD (Play, &LaunchPadPro::play_press);
	EDGE_PAD0 (FixedLength);
	EDGE_PAD0 (Quantize);
	EDGE_PAD0 (Duplicate);
	EDGE_PAD0 (Clear);
	EDGE_PAD0 (Down);
	EDGE_PAD0 (Up);

	EDGE_PAD0 (Lower1);
	EDGE_PAD0 (Lower2);
	EDGE_PAD0 (Lower3);
	EDGE_PAD0 (Lower4);
	EDGE_PAD0 (Lower5);
	EDGE_PAD0 (Lower6);
	EDGE_PAD0 (Lower7);
	EDGE_PAD0 (Lower8);

	/* Now add the 8x8 central pad grid */

	for (int row = 0; row < 8; ++row) {
		for (int col = 0; col < 8; ++col) {
			int pid = (11 + (row * 10)) + col;
			std::pair<int,Pad> p (pid, Pad (pid, col, 7 - row, &LaunchPadPro::pad_press, &LaunchPadPro::relax, &LaunchPadPro::pad_long_press));
			if (!pad_map.insert (p).second) abort();
		}
	}

	/* The +1 is for the shift pad at upper left */
	assert (pad_map.size() == (64 + (5 * 8) + 1));
}

void
LaunchPadPro::all_pads_out ()
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
LaunchPadPro::light_logo ()
{
	MIDI::byte msg[3];

	msg[0] = 0x90;
	msg[1] = 0x63;

	logo_color++;

	if (logo_color > 60) {
		logo_color = 4;
	}

	msg[2] = logo_color;

	daw_write (msg, 3);

	return true;
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
	daw_write (pad->state_msg());
}

void
LaunchPadPro::pad_off (int pad_id)
{
	Pad* pad = pad_by_id (pad_id);
	if (!pad) {
		return;
	}
	pad->set (0, Pad::Static);
	daw_write (pad->state_msg());
}

void
LaunchPadPro::all_pads_off ()
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
LaunchPadPro::all_pads_on (int color)
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
LaunchPadPro::set_layout (Layout l, int page)
{
	MidiByteArray msg (sysex_header);
	msg.push_back (0x0);
	msg.push_back (l);
	msg.push_back (page);
	msg.push_back (0x0);
	msg.push_back (0xf7);
	std::cerr << "switch to layout " << l << " using " << msg << std::endl;
	daw_write (msg);
}

void
LaunchPadPro::set_device_mode (DeviceMode m)
{
	/* LP Pro MK3 programming manual, pages 14 and 18 */
	MidiByteArray standalone_or_daw (sysex_header);
	MidiByteArray live_or_programmer (sysex_header);

	switch (m) {
	case Standalone:
		std::cerr << "entering standalone mode\n";
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
		write (standalone_or_daw);
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
		write (standalone_or_daw);
		break;

	case Programmer:
		std::cerr << "entering programmer mode\n";
		live_or_programmer.push_back (0xe);
		live_or_programmer.push_back (0x1);
		live_or_programmer.push_back (0xf7);
		/* enter "programmer" state */
		write (live_or_programmer);
		break;
	}
}

void
LaunchPadPro::handle_midi_sysex (MIDI::Parser& parser, MIDI::byte* raw_bytes, size_t sz)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("Sysex, %1 bytes parser %2\n", sz, &parser));

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
			if (_current_layout == SessionLayout) {
				display_session_layout ();
			}
		} else {
			std::cerr << "ignore illegal layout index " << (int) raw_bytes[1] << std::endl;
		}
		break;
	default:
		break;
	}
}

void
LaunchPadPro::display_session_layout ()
{
	MIDI::byte msg[3];
	msg[0] = 0x90;

	msg[1] = Patterns;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = Steps;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = PatternSettings;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = Velocity;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = Probability;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = Mutation;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = MicroStep;
	msg[2] = 0x27;
	daw_write (msg, 3);
	msg[1] = PrintToClip;
	msg[2] = 0x27;
	daw_write (msg, 3);

	msg[1] = Play;
	msg[2] = 17;
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


	msg[1] = Lower1;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower2;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower3;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower4;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower5;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower6;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower7;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Lower8;
	msg[2] = 2;
	daw_write (msg, 3);

	msg[1] = StopClip;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Device;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Sends;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Pan;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Volume;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Solo;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = Mute;
	msg[2] = 2;
	daw_write (msg, 3);
	msg[1] = RecordArm;
	msg[2] = 2;
	daw_write (msg, 3);
}

void
LaunchPadPro::handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));

	if (_current_layout != SessionLayout) {
		return;
	}

	PadMap::iterator p = pad_map.find (ev->controller_number);
	if (p == pad_map.end()) {
		return;
	}

	Pad& pad (p->second);
	set<int>::iterator c = consumed.find (pad.id);

	if (c == consumed.end()) {
		if (ev->value) {
			(this->*pad.on_press) (pad);
		} else {
			(this->*pad.on_release) (pad);
		}
	} else {
		consumed.erase (c);
	}
}

void
LaunchPadPro::handle_midi_note_on_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
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
	(this->*pad.on_press) (pad);
}

void
LaunchPadPro::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
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
		(this->*pad.on_release) (pad);

	} else {
		/* used for long press */
		consumed.erase (c);
	}

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
		std::cerr << "no daw port registered\n";
		return;
	}

	if (_daw_in->connected() && _daw_out->connected()) {
		/* don't waste cycles here */
		std::cerr << "daw port already connected\n";
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
			_daw_out_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_daw_out).get();
			return 0;
		}

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
	_daw_out_port->write (&data[0], data.size(), 0);
}

void
LaunchPadPro::daw_write (MIDI::byte const * data, size_t size)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("daw write %1\n", size));
	_daw_out_port->write (data, size, 0);
}

void
LaunchPadPro::scroll_text (std::string const & txt, int color, bool loop, float speed)
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

LaunchPadPro::StripableSlot
LaunchPadPro::get_stripable_slot (int x, int y) const
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
LaunchPadPro::stripable_selection_changed ()
{
	std::shared_ptr<MidiPort> pad_port = std::dynamic_pointer_cast<AsyncMIDIPort>(_async_in)->shadow_port();
	std::shared_ptr<MidiTrack> current_midi_track = _current_pad_target.lock();
	std::shared_ptr<MidiTrack> new_pad_target;
	StripableNotificationList const & selected (last_selected());

	/* See if there's a MIDI track selected */

	for (StripableNotificationList::const_iterator si = selected.begin(); si != selected.end(); ++si) {

		new_pad_target = std::dynamic_pointer_cast<MidiTrack> ((*si).lock());

		if (new_pad_target) {
			break;
		}
	}

	if (current_midi_track != new_pad_target) {

		/* disconnect from pad port, if appropriate */

		if (current_midi_track && pad_port) {

			/* XXX this could possibly leave dangling MIDI notes.
			 *
			 * A general libardour fix is required. It isn't obvious
			 * how note resolution can be done unless disconnecting
			 * becomes "slow" (i.e. deferred for as long as it takes
			 * to resolve notes).
			 */
			current_midi_track->input()->disconnect (current_midi_track->input()->nth(0), pad_port->name(), this);
		}

		/* now connect the pad port to this (newly) selected midi
		 * track, if indeed there is one.
		 */

		if (new_pad_target && pad_port) {
			new_pad_target->input()->connect (new_pad_target->input()->nth (0), pad_port->name(), this);
			_current_pad_target = new_pad_target;
			// _selection_color = get_color_index (new_pad_target->presentation_info().color());
			// _contrast_color = get_color_index (Gtkmm2ext::HSV (new_pad_target->presentation_info().color()).opposite().color());
		} else {
			// _current_pad_target.reset ();
			// _selection_color = LED::Green;
			// _contrast_color = LED::Green;
		}

		// reset_pad_colors ();
	}

	// TrackMixLayout* tml = dynamic_cast<TrackMixLayout*> (_track_mix_layout);
	// assert (tml);

	// tml->set_stripable (first_selected_stripable());
}

bool
LaunchPadPro::pad_filter (MidiBuffer& in, MidiBuffer& out) const
{
	/* This filter is called asynchronously from a realtime process
	   context. It must use atomics to check state, and must not block.
	*/

	if (_current_layout != NoteLayout) {
		return false;
	}

	bool matched = false;

	for (MidiBuffer::iterator ev = in.begin(); ev != in.end(); ++ev) {
		if ((*ev).is_note_on() || (*ev).is_note_off()) {
			out.push_back (*ev);
			matched = true;
		}
	}

	return matched;
}

void
LaunchPadPro::start_press_timeout (Pad& pad)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	pad.timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &LaunchPadPro::long_press_timeout), pad.id));
	timeout->attach (main_loop()->get_context());
}

void
LaunchPadPro::maybe_start_press_timeout (Pad& pad)
{
	if (pad.on_long_press == &LaunchPadPro::relax) {
		return;
	}
	start_press_timeout (pad);
}

bool
LaunchPadPro::long_press_timeout (int pad_id)
{
	PadMap::iterator p = pad_map.find (pad_id);
	if (p == pad_map.end()) {
		/* impossible */
		return false;
	}
	Pad& pad (p->second);
	(this->*pad.on_long_press) (pad);

	/* Pad was used for long press, do not invoke release action */
	consumed.insert (pad.id);

	return false; /* don't get called again */
}

void
LaunchPadPro::shift_press (Pad& pad)
{
	_shift_pressed = true;
}

void
LaunchPadPro::shift_release (Pad& pad)
{
	_shift_pressed = false;
}

void
LaunchPadPro::left_press (Pad& pad)
{
	if (scroll_x_offset) {
		scroll_x_offset--;
	}
}

void
LaunchPadPro::right_press (Pad& pad)
{
	scroll_x_offset++;
}

void
LaunchPadPro::session_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadPro::note_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadPro::chord_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadPro::custom_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadPro::sequencer_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadPro::projects_press (Pad& pad)
{
	/* handled by device */
}

void
LaunchPadPro::patterns_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (0 + scroll_y_offset);
	}
}

void
LaunchPadPro::steps_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (1 + scroll_y_offset);
	}
}

void
LaunchPadPro::pattern_settings_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (2 + scroll_y_offset);
	}
}

void
LaunchPadPro::velocity_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (3 + scroll_y_offset);
	}
}

void
LaunchPadPro::probability_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (4 + scroll_y_offset);
	}
}

void
LaunchPadPro::mutation_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (5 + scroll_y_offset);
	}
}


void
LaunchPadPro::microstep_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (6 + scroll_y_offset);
	}
}

void
LaunchPadPro::print_to_clip_press (Pad& pad)
{
	if (_current_layout == SessionLayout) {
		session->trigger_cue_row (7  + scroll_y_offset);
	}
}

void
LaunchPadPro::stop_clip_press (Pad& pad)
{
	session->trigger_stop_all (_shift_pressed);
}

void
LaunchPadPro::device_press (Pad& pad)
{
}

void
LaunchPadPro::sends_press (Pad& pad)
{
}

void
LaunchPadPro::pan_press (Pad& pad)
{
}

void
LaunchPadPro::volume_press (Pad& pad)
{
}

void
LaunchPadPro::solo_press (Pad& pad)
{
}

void
LaunchPadPro::mute_press (Pad& pad)
{
}

void
LaunchPadPro::record_arm_press (Pad& pad)
{
}

void
LaunchPadPro::capture_midi_press (Pad& pad)
{
}

void
LaunchPadPro::play_press (Pad& pad)
{
	toggle_roll (false, true);
}

void
LaunchPadPro::fixed_length_press (Pad& pad)
{
}

void
LaunchPadPro::quantize_press (Pad& pad)
{
}

void
LaunchPadPro::duplicate_press (Pad& pad)
{
}

void
LaunchPadPro::clear_press (Pad& pad)
{
}

void
LaunchPadPro::down_press (Pad& pad)
{
	if (scroll_y_offset) {
		scroll_y_offset--;
	}
}

void
LaunchPadPro::up_press (Pad& pad)
{
	scroll_y_offset++;
}

void
LaunchPadPro::lower1_press (Pad& pad)
{
}

void
LaunchPadPro::lower2_press (Pad& pad)
{
}

void
LaunchPadPro::lower3_press (Pad& pad)
{
}

void
LaunchPadPro::lower4_press (Pad& pad)
{
}

void
LaunchPadPro::lower5_press (Pad& pad)
{
}

void
LaunchPadPro::lower6_press (Pad& pad)
{
}

void
LaunchPadPro::lower7_press (Pad& pad)
{
}

void
LaunchPadPro::lower8_press (Pad& pad)
{
}

void
LaunchPadPro::pad_press (Pad& pad)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("pad press on %1, %2 => %3\n", pad.x, pad.y, pad.id));
	session->bang_trigger_at (pad.x, pad.y);
	start_press_timeout (pad);
}

void
LaunchPadPro::pad_long_press (Pad& pad)
{
	DEBUG_TRACE (DEBUG::Launchpad, string_compose ("pad long press on %1, %2 => %3\n", pad.x, pad.y, pad.id));
	session->unbang_trigger_at (pad.x, pad.y);
}

void
LaunchPadPro::trigger_property_change (PropertyChange pc, int x, int y)
{
	TriggerPtr trigger (session->trigger_at (x, y));
	if (!trigger) {
		return;
	}


	if (pc.contains (Properties::running)) {

		int pid = (11 + ((7 - y) * 10)) + x;

		PadMap::iterator p = pad_map.find (pid);
		if (p == pad_map.end()) {
			return;
		}

		MIDI::byte msg[3];
		msg[0] = 0x90;
		msg[1] = p->second.id;

		switch (trigger->state()) {
		case Trigger::Stopped:
			msg[2] = 0;
			break;
		case Trigger::WaitingToStart:
			msg[0] |= 1;
			msg[2] = 0x27;
			break;
		case Trigger::Running:
		case Trigger::WaitingForRetrigger:
		case Trigger::WaitingToStop:
		case Trigger::WaitingToSwitch:
			msg[2] = 0x27;
			break;
		default:
			msg[2] = 0;
		}

		daw_write (msg, 3);
	}
}
