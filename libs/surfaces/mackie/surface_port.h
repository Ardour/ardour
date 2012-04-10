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
#ifndef surface_port_h
#define surface_port_h

#include <midi++/types.h>
#include <glibmm/thread.h>

#include "pbd/signals.h"
#include "midi_byte_array.h"
#include "types.h"

namespace MIDI {
	class Port;
	class Parser;
}

class MackieControlProtocol;

namespace Mackie
{

class Surface;

/**
	Make a relationship between a midi port and a Mackie device.
*/

class SurfacePort 
{
public:
	SurfacePort (Mackie::Surface&, MIDI::Port& input_port, MIDI::Port& output_port);
	virtual ~SurfacePort();
	
	void open();
	void close();

	/// read bytes from the port. They'll either end up in the
	/// parser, or if that's not active they'll be returned
	MidiByteArray read();
	
	/// an easier way to output bytes via midi
	void write (const MidiByteArray&);
	
	MIDI::Port& input_port() { return *_input_port; }
	const MIDI::Port& input_port() const { return *_input_port; }
	MIDI::Port& output_port() { return *_output_port; }
	const MIDI::Port& output_port() const { return *_output_port; }

	// emitted when the port goes inactive (ie a read or write failed)
	PBD::Signal0<void> inactive_event;
	
	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);

	bool active() const { return _active; }

protected:
	MidiByteArray host_connection_query (MidiByteArray& bytes);
	MidiByteArray host_connection_confirmation (const MidiByteArray& bytes);

private:
	Mackie::Surface* _surface;
	MIDI::Port*      _input_port;
	MIDI::Port*      _output_port;
	bool             _active;

	PBD::ScopedConnection sysex_connection;
};	

std::ostream& operator <<  (std::ostream& , const SurfacePort& port);

}

#endif
