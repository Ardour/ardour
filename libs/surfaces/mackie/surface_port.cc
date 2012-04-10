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
#include <cstring>
#include <cerrno>

#include <sigc++/sigc++.h>
#include <boost/shared_array.hpp>

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/manager.h"

#include "ardour/debug.h"
#include "ardour/rc_configuration.h"

#include "controls.h"
#include "mackie_control_exception.h"
#include "surface.h"
#include "surface_port.h"


#include "i18n.h"

using namespace std;
using namespace Mackie;
using namespace PBD;

/** @param input_port Input MIDI::Port; this object takes responsibility for removing it from
 *  the MIDI::Manager and destroying it.
 *  @param output_port Output MIDI::Port; responsibility similarly taken.
 */
SurfacePort::SurfacePort (Surface& s, MIDI::Port & input_port, MIDI::Port & output_port)
	: _surface (&s)
	, _input_port (&input_port)
	, _output_port (&output_port)
	, _active (false)
{
}

SurfacePort::~SurfacePort()
{
	close ();

	MIDI::Manager* mm = MIDI::Manager::instance ();
	
	if (_input_port) {
		mm->remove_port (_input_port);
		delete _input_port;
	}

	if (_output_port) {
		mm->remove_port (_output_port);
		delete _output_port;
	}
}

// wrapper for one day when strerror_r is working properly
string fetch_errmsg (int error_number)
{
	char * msg = strerror (error_number);
	return msg;
}
	
MidiByteArray SurfacePort::read()
{
	const int max_buf_size = 512;
	MIDI::byte buf[max_buf_size];
	MidiByteArray retval;

	// check active. Mainly so that the destructor
	// doesn't destroy the mutex while it's still locked
	if  (!active()) {
		return retval;
	}
	
	// return nothing read if the lock isn't acquired

	// read port and copy to return value
	int nread = input_port().read (buf, sizeof (buf));

	if (nread >= 0) {
		retval.copy (nread, buf);
		if ((size_t) nread == sizeof (buf)) {
#ifdef PORT_DEBUG
			cout << "SurfacePort::read recursive" << endl;
#endif
			retval << read();
		}
	} else {
		if  (errno != EAGAIN) {
			ostringstream os;
			os << "Surface: error reading from port: " << input_port().name();
			os << ": " << errno << fetch_errmsg (errno);

			cout << os.str() << endl;
			inactive_event();
			throw MackieControlException (os.str());
		}
	}
#ifdef PORT_DEBUG
	cout << "SurfacePort::read: " << retval << endl;
#endif
	return retval;
}

void SurfacePort::write (const MidiByteArray & mba)
{
	if (mba.empty()) {
		return;
	}

	if (!active()) return;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("port %1 write %2\n", output_port().name(), mba));
	
	int count = output_port().write (mba.bytes().get(), mba.size(), 0);

	if  (count != (int)mba.size()) {
		if  (errno == 0) {
			cout << "port overflow on " << output_port().name() << ". Did not write all of " << mba << endl;
		} else if  (errno != EAGAIN) {
			ostringstream os;
			os << "Surface: couldn't write to port " << output_port().name();
			os << ", error: " << fetch_errmsg (errno) << "(" << errno << ")";
			
			cout << os.str() << endl;
			inactive_event();
		}
	}
}


void SurfacePort::open()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("SurfacePort::open %1\n", *this));
	input_port().parser()->sysex.connect_same_thread (sysex_connection, boost::bind (&SurfacePort::handle_midi_sysex, this, _1, _2, _3));
	_active = true;
}

void SurfacePort::close()
{
	DEBUG_TRACE (DEBUG::MackieControl, "SurfacePort::close\n");
	sysex_connection.disconnect();

	if (_surface) {
		// faders to minimum
		_surface->write_sysex (0x61);
		// All LEDs off
		_surface->write_sysex (0x62);
		// Reset (reboot into offline mode)
		_surface->write_sysex (0x63);
	}

	_active = false;
}

void 
SurfacePort::handle_midi_sysex (MIDI::Parser &, MIDI::byte * raw_bytes, size_t count)
{
	MidiByteArray bytes (count, raw_bytes);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handle_midi_sysex: %1\n", bytes));

	switch (bytes[5])
	{
		case 0x01:
			_surface->write_sysex (host_connection_query (bytes));
			break;
		case 0x03:
			// not used right now
			_surface->write_sysex (host_connection_confirmation (bytes));
			break;
		case 0x04:
			inactive_event ();
			cout << "host connection error" << bytes << endl;
			break;
		case 0x14:
			// probe_emulation (bytes);
			break;
		default:
			cout << "unknown sysex: " << bytes << endl;
	}
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
MidiByteArray SurfacePort::host_connection_query (MidiByteArray & bytes)
{
	MidiByteArray response;

	// handle host connection query
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host connection query: %1\n", bytes));
	
	if  (bytes.size() != 18) {
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
MidiByteArray SurfacePort::host_connection_confirmation (const MidiByteArray & bytes)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host_connection_confirmation: %1\n", bytes));
	
	// decode host connection confirmation
	if  (bytes.size() != 14) {
		ostringstream os;
		os << "expecting 14 bytes, read " << bytes << " from " << input_port().name();
		throw MackieControlException (os.str());
	}
	
	// send version request
	return MidiByteArray (2, 0x13, 0x00);
}


ostream & Mackie::operator <<  (ostream & os, const SurfacePort & port)
{
	os << "{ ";
	os << "name: " << port.input_port().name() << " " << port.output_port().name();
	os << "; ";
	os << " }";
	return os;
}

