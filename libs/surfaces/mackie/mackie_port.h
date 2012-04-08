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
#ifndef mackie_port_h
#define mackie_port_h

#include <midi++/types.h>
#include <glibmm/thread.h>

#include "pbd/signals.h"

#include "surface_port.h"
#include "midi_byte_array.h"
#include "types.h"

namespace MIDI {
	class Port;
	class Parser;
}

class MackieControlProtocol;

namespace Mackie
{

class MackiePort : public SurfacePort
{
public:
	enum port_type_t { mcu, ext };
	enum emulation_t { none, mackie, bcf2000 };
	
	MackiePort (MackieControlProtocol & mcp, MIDI::Port & input_port, MIDI::Port & output_port, int number, port_type_t = mcu);
	~MackiePort();

	virtual void open();
	virtual void close();

	/// MCU and extenders have different sysex headers
	virtual const MidiByteArray & sysex_hdr() const;

	/// Handle device initialisation
	void handle_midi_sysex( MIDI::Parser &, MIDI::byte *, size_t count );
	void handle_midi_pitchbend_message (MIDI::Parser &, MIDI::pitchbend_t, uint32_t channel_id);
	void handle_midi_controller_message (MIDI::Parser &, MIDI::EventTwoBytes*);
	
	/// return the number of strips associated with this port
	virtual int strips() const;

	/// Block until the port has finished initialising, and then return
	/// whether the intialisation succeeded
	bool wait_for_init();
	
	emulation_t emulation() const { return _emulation; }
	
	/// Connect the any signal from the parser to handle_midi_any
	/// unless it's already connected
	void connect_to_signals ();

protected:
	/**
		The initialisation sequence is fairly complex. First a lock is acquired
		so that a condition can be used to signal the end of the init process.
		Then a sysex is sent to the device. The response to the sysex
		is handled by a switch in handle_midi_sysex which calls one of the
		other methods.
		
		However, windows DAWs ignore the documented init sequence and so we
		do too. Thanks to Essox for helping with this.
		
		So we use the version firmware to figure out what device is on
		the other end of the cable.
	*/
	void init();

	/**
		Once the device is initialised, finalise_init(true) is called, which
		releases the lock and signals the condition, and starts handling incoming
		messages. finalise_init(false) will also release the lock but doesn't
		start handling messages.
	*/
	void finalise_init( bool yn );

	MidiByteArray host_connection_query( MidiByteArray & bytes );
	MidiByteArray host_connection_confirmation( const MidiByteArray & bytes );

	/**
		Will set _emulation to what it thinks is correct, based
		on responses from the device. Or get/set parameters. Or
		environment variables. Or existence of a file.
	*/
	void probe_emulation( const MidiByteArray & bytes );

private:
	MackieControlProtocol & _mcp;
	port_type_t _port_type;
	PBD::ScopedConnection any_connection;
	PBD::ScopedConnection sysex_connection;
	emulation_t _emulation;

	bool _initialising;
	bool _connected;
	Glib::Cond init_cond;
	Glib::Mutex init_mutex;
};

}

#endif
