/*
	Copyright (C) 2006,2007 John Anderson

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

#include <sstream>

#include <glibmm/main.h>
#include <boost/shared_array.hpp>

#include "mackie_port.h"

#include "mackie_control_exception.h"
#include "mackie_control_protocol.h"
#include "mackie_midi_builder.h"
#include "controls.h"
#include "surface.h"

#include "fader.h"
#include "button.h"
#include "strip.h"
#include "pot.h"
#include "control_group.h"

#include "midi++/types.h"
#include "midi++/port.h"

#include "ardour/debug.h"
#include "ardour/rc_configuration.h"

#include "i18n.h"

using namespace std;
using namespace Mackie;
using namespace ARDOUR;
using namespace PBD;

// The MCU sysex header
MidiByteArray mackie_sysex_hdr  (5, MIDI::sysex, 0x0, 0x0, 0x66, 0x10);

// The MCU extender sysex header
MidiByteArray mackie_sysex_hdr_xt  (5, MIDI::sysex, 0x0, 0x0, 0x66, 0x11);

MackiePort::MackiePort (MackieControlProtocol & mcp, MIDI::Port & input_port, MIDI::Port & output_port, int number, port_type_t port_type)
	: SurfacePort (input_port, output_port, number)
	, _mcp (mcp)
	, _port_type (port_type)
	, _emulation (none)
	, _initialising (true)
	, _connected (false)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::MackiePort\n");
}

MackiePort::~MackiePort()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::~MackiePort\n");
	close();
	DEBUG_TRACE (DEBUG::MackieControl, "~MackiePort finished\n");
}

int MackiePort::strips() const
{
	if  (_port_type == mcu)
	{
		switch  (_emulation)
		{
			// BCF2000 only has 8 faders, so reserve one for master
			case bcf2000: return 7;
			case mackie: return 8;
			case none:
			default:
				throw MackieControlException ("MackiePort::strips: don't know what emulation we're using");
		}
	}
	else
	{
		// must be an extender, ie no master fader
		return 8;
	}
}

// should really be in MackiePort
void MackiePort::open()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackiePort::open %1\n", *this));
	
	input_port().parser()->sysex.connect_same_thread (sysex_connection, boost::bind (&MackiePort::handle_midi_sysex, this, _1, _2, _3));
		     
	// make sure the device is connected
	init();
}

void MackiePort::close()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::close\n");
	
	// disconnect signals

	sysex_connection.disconnect();
	ScopedConnectionList::drop_connections ();
	_connected = false;

	// TODO emit a "closing" signal?
}

const MidiByteArray & MackiePort::sysex_hdr() const
{
	switch  (_port_type)
	{
		case mcu: return mackie_sysex_hdr;
		case ext: return mackie_sysex_hdr_xt;
	}
	cout << "MackiePort::sysex_hdr _port_type not known" << endl;
	return mackie_sysex_hdr;
}

MidiByteArray calculate_challenge_response (MidiByteArray::iterator begin, MidiByteArray::iterator end)
{
	MidiByteArray l;
	back_insert_iterator<MidiByteArray> back  (l);
	copy (begin, end, back);
	
	MidiByteArray retval;
	
	// this is how to calculate the response to the challenge.
	// from the Logic docs.
	retval <<  (0x7f &  (l[0] +  (l[1] ^ 0xa) - l[3]));
	retval <<  (0x7f &  ( (l[2] >> l[3]) ^  (l[0] + l[3])));
	retval <<  (0x7f &  ((l[3] -  (l[2] << 2)) ^  (l[0] | l[1])));
	retval <<  (0x7f &  (l[1] - l[2] +  (0xf0 ^  (l[3] << 4))));
	
	return retval;
}

// not used right now
MidiByteArray MackiePort::host_connection_query (MidiByteArray & bytes)
{
	MidiByteArray response;

	// handle host connection query
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host connection query: %1\n", bytes));
	
	if  (bytes.size() != 18) {
		finalise_init (false);
		cerr << "expecting 18 bytes, read " << bytes << " from " << input_port().name() << endl;
		return response;
	}

	// build and send host connection reply
	response << 0x02;
	copy (bytes.begin() + 6, bytes.begin() + 6 + 7, back_inserter (response));
	response << calculate_challenge_response (bytes.begin() + 6 + 7, bytes.begin() + 6 + 7 + 4);
	return response;
}

// not used right now
MidiByteArray MackiePort::host_connection_confirmation (const MidiByteArray & bytes)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host_connection_confirmation: %1\n", bytes));
	
	// decode host connection confirmation
	if  (bytes.size() != 14) {
		finalise_init (false);
		ostringstream os;
		os << "expecting 14 bytes, read " << bytes << " from " << input_port().name();
		throw MackieControlException (os.str());
	}
	
	// send version request
	return MidiByteArray (2, 0x13, 0x00);
}

void MackiePort::probe_emulation (const MidiByteArray &)
{
#if 0
	cout << "MackiePort::probe_emulation: " << bytes.size() << ", " << bytes << endl;

	MidiByteArray version_string;
	for  (int i = 6; i < 11; ++i) version_string << bytes[i];
	cout << "version_string: " << version_string << endl;
#endif
	
	// TODO investigate using serial number. Also, possibly size of bytes might
	// give an indication. Also, apparently MCU sends non-documented messages
	// sometimes.
	if (!_initialising)
	{
		//cout << "MackiePort::probe_emulation out of sequence." << endl;
		return;
	}

	finalise_init (true);
}

void MackiePort::init()
{
	DEBUG_TRACE (DEBUG::MackieControl,  "MackiePort::init\n");

	init_mutex.lock();
	_initialising = true;

	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::init lock acquired\n");

	// emit pre-init signal
	init_event();
	
	// kick off initialisation. See docs in header file for init()
	
	// bypass the init sequence because sometimes the first
	// message doesn't get to the unit, and there's no way
	// to do a timed lock in Glib.
	//write_sysex  (MidiByteArray  (2, 0x13, 0x00));
	
	finalise_init (true);
}

void MackiePort::finalise_init (bool yn)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::finalise_init\n");

	bool emulation_ok = false;
	
	// probing doesn't work very well, so just use a config variable
	// to set the emulation mode
	// TODO This might have to be specified on a per-port basis
	// in the config file
	// if an mcu and a bcf are needed to work as one surface
	if  (_emulation == none) {

		// TODO same as code in mackie_control_protocol.cc
		if  (ARDOUR::Config->get_mackie_emulation() == "bcf") {
			_emulation = bcf2000;
			emulation_ok = true;
		} else if  (ARDOUR::Config->get_mackie_emulation() == "mcu")  {
			_emulation = mackie;
			emulation_ok = true;
		} else {
			cout << "unknown mackie emulation: " << ARDOUR::Config->get_mackie_emulation() << endl;
			emulation_ok = false;
		}
	}
	
	yn = yn && emulation_ok;
	
	SurfacePort::active (yn);

	if (yn) {
		active_event();
		
		// start handling messages from controls
		connect_to_signals ();
	}

	_initialising = false;
	init_cond.signal();
	init_mutex.unlock();

	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::finalise_init lock released\n");
}

void MackiePort::connect_to_signals ()
{
	if (!_connected) {

		MIDI::Parser* p = input_port().parser();

		/* V-Pot messages are Controller */
		p->controller.connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_controller_message, this, _1, _2));
		/* Button messages are NoteOn */
		p->note_on.connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_note_on_message, this, _1, _2));
		/* Fader messages are Pitchbend */
		p->channel_pitchbend[0].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 0U));
		p->channel_pitchbend[1].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 1U));
		p->channel_pitchbend[2].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 2U));
		p->channel_pitchbend[3].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 3U));
		p->channel_pitchbend[4].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 4U));
		p->channel_pitchbend[5].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 5U));
		p->channel_pitchbend[6].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 6U));
		p->channel_pitchbend[7].connect_same_thread (*this, boost::bind (&MackiePort::handle_midi_pitchbend_message, this, _1, _2, 7U));
		
		_connected = true;
	}
}

bool MackiePort::wait_for_init()
{
	Glib::Mutex::Lock lock (init_mutex);
	while (_initialising) {
		DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::wait_for_active waiting\n");
		init_cond.wait (init_mutex);
		DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::wait_for_active released\n");
	}
	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::wait_for_active returning\n");
	return SurfacePort::active();
}

void MackiePort::handle_midi_sysex (MIDI::Parser &, MIDI::byte * raw_bytes, size_t count)
{
	MidiByteArray bytes (count, raw_bytes);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handle_midi_sysex: %1\n", bytes));

	switch (bytes[5])
	{
		case 0x01:
			write_sysex (host_connection_query (bytes));
			break;
		case 0x03:
			// not used right now
			write_sysex (host_connection_confirmation (bytes));
			break;
		case 0x04:
			inactive_event ();
			cout << "host connection error" << bytes << endl;
			break;
		case 0x14:
			probe_emulation (bytes);
			break;
		default:
			cout << "unknown sysex: " << bytes << endl;
	}
}

void
MackiePort::handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t pb, uint32_t fader_id)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handle_midi pitchbend on port %3 (number %4), fader = %1 value = %2\n", 
							   (8*number()) + fader_id, pb, *this, number()));

	Control* control = _mcp.surface().faders[(8*number()) + fader_id];

	if (control) {
		float midi_pos = pb >> 4; // only the top 10 bytes are used
		_mcp.handle_control_event (*this, *control, midi_pos / 1023.0);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "fader not found\n");
	}
}

void 
MackiePort::handle_midi_note_on_message (MIDI::Parser &, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackiePort::handle_note_on %1 = %2\n", ev->note_number, ev->velocity));

	Control* control = _mcp.surface().buttons[(8*number()) + ev->note_number];

	if (control) {
		ControlState control_state (ev->velocity == 0x7f ? press : release);
		control->set_in_use (control_state.button_state == press);
		control_event (*this, *control, control_state);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "button not found\n");
	}
}

void 
MackiePort::handle_midi_controller_message (MIDI::Parser &, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackiePort::handle_midi_controller %1 = %2\n", ev->controller_number, ev->value));

	Control* control = _mcp.surface().pots[(8*number()) + ev->controller_number];

	if (!control && ev->controller_number == Control::jog_base_id) {
		control = _mcp.surface().controls_by_name["jog"];
	}

	if (control) {
		ControlState state;
		
		// bytes[2] & 0b01000000 (0x40) give sign
		state.sign = (ev->value & 0x40) == 0 ? 1 : -1; 
		// bytes[2] & 0b00111111 (0x3f) gives delta
		state.ticks = (ev->value & 0x3f);
		if (state.ticks == 0) {
			/* euphonix and perhaps other devices send zero
			   when they mean 1, we think.
			*/
			state.ticks = 1;
		}
		state.delta = float (state.ticks) / float (0x3f);
		
		/* Pots only emit events when they move, not when they
		   stop moving. So to get a stop event, we need to use a timeout.
		*/
		
		control->set_in_use (true);
		_mcp.add_in_use_timeout (*this, *control, control);

		control_event (*this, *control, state);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "pot not found\n");
	}
}

void
MackiePort::control_event (SurfacePort& sp, Control& c, const ControlState& cs)
{
	_mcp.handle_control_event (sp, c, cs);
}
