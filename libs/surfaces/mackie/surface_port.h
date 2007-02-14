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

#include <sigc++/signal.h>

#include "midi_byte_array.h"
#include "types.h"

namespace MIDI {
	class Port;
}

namespace Mackie
{

/**
	Make a relationship between a midi port and a Mackie device.
*/
class SurfacePort : public sigc::trackable
{
public:
	SurfacePort( MIDI::Port & port, int number );
	virtual ~SurfacePort() {}
	
	// when this is successful, active() should return true
	virtual void open() = 0;
	virtual void close() = 0;

	/// read bytes from the port. They'll either end up in the
	/// parser, or if that's not active they'll be returned
	MidiByteArray read();
	
	/// an easier way to output bytes via midi
	void write( const MidiByteArray & );
	
	/// write a sysex message
	void write_sysex( const MidiByteArray & mba );
	void write_sysex( MIDI::byte msg );

	// return the correct sysex header for this port
	virtual const MidiByteArray & sysex_hdr() const = 0;

	MIDI::Port & port() { return _port; }
	const MIDI::Port & port() const { return _port; }
	
	// all control notofications are sent from here
	sigc::signal<void, SurfacePort &, Control &, const ControlState &> control_event;
	
	// emitted just before the port goes into initialisation
	// where it tries to establish that its device is connected
	sigc::signal<void> init_event;
	
	// emitted when the port completes initialisation successfully
	sigc::signal<void> active_event;

	// emitted when the port goes inactive (ie a read or write failed)
	sigc::signal<void> inactive_event;
	
	// the port number - master is 0, extenders are 1,2,3,4
	virtual int number() const { return _number; }
	
	// number of strips handled by this port. Usually 8.
	virtual int strips() const = 0;

	virtual bool active() const { return _active; }
	virtual void active( bool yn ) { _active = yn; }
	
private:
	MIDI::Port & _port;
	int _number;
	bool _active;
};	

std::ostream & operator << ( std::ostream & , const SurfacePort & port );

}

#endif
