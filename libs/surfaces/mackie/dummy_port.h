/*
	Copyright (C) 2008 John Anderson

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
#ifndef dummy_port_h
#define dummy_port_h

#include "surface_port.h"

#include "midi_byte_array.h"

namespace MIDI {
	class Port;
}

namespace Mackie
{

/**
	A Dummy Port, to catch things that shouldn't be sent.
*/
class DummyPort : public SurfacePort
{
public:
	DummyPort();
	virtual ~DummyPort();
	
	// when this is successful, active() should return true
	virtual void open();
	
	// subclasses should call this before doing their own close
	virtual void close();

	/// read bytes from the port. They'll either end up in the
	/// parser, or if that's not active they'll be returned
	virtual MidiByteArray read();
	
	/// an easier way to output bytes via midi
	virtual void write( const MidiByteArray & );

	virtual const MidiByteArray & sysex_hdr() const;
	virtual int strips() const;

};	

}

#endif
