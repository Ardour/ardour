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
#include "mackie_port.h"

#include "mackie_control_exception.h"
#include "mackie_control_protocol.h"
#include "mackie_midi_builder.h"
#include "controls.h"
#include "surface.h"

#include <glibmm/main.h>

#include <boost/shared_array.hpp>

#include "midi++/types.h"
#include "midi++/port.h"

#include "ardour/debug.h"
#include "ardour/rc_configuration.h"

#include "i18n.h"

#include <sstream>

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
	any_connection.disconnect();
	sysex_connection.disconnect();
	
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
	// handle host connection query
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host connection query: %1\n", bytes));
	
	if  (bytes.size() != 18) {
		finalise_init (false);
		cerr << "expecting 18 bytes, read " << bytes << " from " << input_port().name() << endl;
		return;
	}

	// build and send host connection reply
	MidiByteArray response;
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
	if  (_emulation == none)
	{
		// TODO same as code in mackie_control_protocol.cc
		if  (ARDOUR::Config->get_mackie_emulation() == "bcf")
		{
			_emulation = bcf2000;
			emulation_ok = true;
		}
		else if  (ARDOUR::Config->get_mackie_emulation() == "mcu")
		{
			_emulation = mackie;
			emulation_ok = true;
		}
		else
		{
			cout << "unknown mackie emulation: " << ARDOUR::Config->get_mackie_emulation() << endl;
			emulation_ok = false;
		}
	}
	
	yn = yn && emulation_ok;
	
	SurfacePort::active (yn);

	if (yn) {
		active_event();
		
		// start handling messages from controls
		connect_any();
	}

	_initialising = false;
	init_cond.signal();
	init_mutex.unlock();

	DEBUG_TRACE (DEBUG::MackieControl, "MackiePort::finalise_init lock released\n");
}

void MackiePort::connect_any()
{
	if (!any_connection.connected()) {
		input_port().parser()->any.connect_same_thread (any_connection, boost::bind (&MackiePort::handle_midi_any, this, _1, _2, _3));
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

Control & MackiePort::lookup_control (MIDI::byte * bytes, size_t count)
{
	// Don't instantiate a MidiByteArray here unless it's needed for exceptions.
	// Reason being that this method is called for every single incoming
	// midi event, and it needs to be as efficient as possible.

	Control * control = 0;
	MIDI::byte midi_type = bytes[0] & 0xf0; //0b11110000

	switch (midi_type) {
		// fader
		case MackieMidiBuilder::midi_fader_id:
		{
			int midi_id = bytes[0] & 0x0f;
			control = _mcp.surface().faders[midi_id];
			if  (control == 0)
			{
				MidiByteArray mba (count, bytes);
				ostringstream os;
				os << "Control for fader" << bytes << " id " << midi_id << " is null";
				throw MackieControlException (os.str());
			}
			break;
		}
			
		// button
		case MackieMidiBuilder::midi_button_id:
			control = _mcp.surface().buttons[bytes[1]];
			if  (control == 0)
			{
				MidiByteArray mba (count, bytes);
				ostringstream os;
				os << "Control for button " << mba << " is null";
				throw MackieControlException (os.str());
			}
			break;
			
		// pot (jog wheel, external control)
		case MackieMidiBuilder::midi_pot_id:
			control = _mcp.surface().pots[bytes[1]];
			if  (control == 0)
			{
				MidiByteArray mba (count, bytes);
				ostringstream os;
				os << "Control for rotary " << mba << " is null";
				throw MackieControlException (os.str());
			}
			break;
		
		default:
			MidiByteArray mba (count, bytes);
			ostringstream os;
			os << "Cannot find control for " << mba;
			throw MackieControlException (os.str());
	}
	return *control;
}

// converts midi messages into control_event signals
// it might be worth combining this with lookup_control
// because they have similar logic flows.
void MackiePort::handle_midi_any (MIDI::Parser &, MIDI::byte * raw_bytes, size_t count)
{
	MidiByteArray bytes (count, raw_bytes);
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackiePort::handle_midi_any %1\n", bytes));

	try
	{
		// ignore sysex messages
		if  (raw_bytes[0] == MIDI::sysex) return;

		// sanity checking
		if (count != 3) {
			ostringstream os;
			MidiByteArray mba (count, raw_bytes);
			os << "MackiePort::handle_midi_any needs 3 bytes, but received " << mba;
			throw MackieControlException (os.str());
		}
		
		Control & control = lookup_control (raw_bytes, count);
		control.set_in_use (true);
		
		// This handles incoming bytes. Outgoing bytes
		// are sent by the signal handlers.
		switch (control.type()) {
			// fader
			case Control::type_fader:
			{
				// only the top-order 10 bits out of 14 are used
				int midi_pos =  ( (raw_bytes[2] << 7) + raw_bytes[1]) >> 4;
				
				// in_use is set by the MackieControlProtocol::handle_strip_button
				
				// relies on implicit ControlState constructor
				control_event (*this, control, float(midi_pos) / float(0x3ff));
			}
			break;
				
			// button
			case Control::type_button:
			{
				ControlState control_state (raw_bytes[2] == 0x7f ? press : release);
				control.set_in_use (control_state.button_state == press);
				control_event (*this, control, control_state);
				
				break;
			}
				
			// pot (jog wheel, external control)
			case Control::type_pot:
			{
				ControlState state;
				
				// bytes[2] & 0b01000000 (0x40) give sign
				state.sign =  (raw_bytes[2] & 0x40) == 0 ? 1 : -1; 
				// bytes[2] & 0b00111111 (0x3f) gives delta
				state.ticks =  (raw_bytes[2] & 0x3f);
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

				control.set_in_use (true);
				add_in_use_timeout (control, &control);

				// emit the control event
				control_event (*this, control, state);
				break;
			}
			default:
				cerr << "Do not understand control type " << control;
		}
	}

	catch (MackieControlException & e) {
		MidiByteArray bytes (count, raw_bytes);
		cout << bytes << ' ' << e.what() << endl;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("finished MackiePort::handle_midi_any %1\n", bytes));
}

