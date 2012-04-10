/*
    Copyright (C) 1998-2010 Paul Barton-Davis 
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

#ifndef  __libmidi_port_h__
#define  __libmidi_port_h__

#include <string>
#include <iostream>

#include <jack/types.h>

#include "pbd/xml++.h"
#include "pbd/crossthread.h"
#include "pbd/signals.h"
#include "pbd/ringbuffer.h"

#include "evoral/Event.hpp"
#include "evoral/EventRingBuffer.hpp"

#include "midi++/types.h"
#include "midi++/parser.h"

namespace MIDI {

class Channel;
class PortRequest;

class Port {
  public:
	enum Flags {
		IsInput = JackPortIsInput,
		IsOutput = JackPortIsOutput,
	};
	
	Port (std::string const &, Flags, jack_client_t *);
	Port (const XMLNode&, jack_client_t *);
	~Port ();

	XMLNode& get_state () const;
	void set_state (const XMLNode&);

	// FIXME: make Manager a friend of port so these can be hidden?

	/* Only for use by MidiManager.  Don't ever call this. */
	void cycle_start (pframes_t nframes);
	/* Only for use by MidiManager.  Don't ever call this. */
	void cycle_end ();

	/** Write a message to port.
	 * @param msg Raw MIDI message to send
	 * @param msglen Size of @a msg
	 * @param timestamp Time stamp in frames of this message (relative to cycle start)
	 * @return number of bytes successfully written
	 */
	int write (byte *msg, size_t msglen, timestamp_t timestamp);

	/** Read raw bytes from a port.
	 * @param buf memory to store read data in
	 * @param bufsize size of @a buf
	 * @return number of bytes successfully read, negative if error
	 */
	int read (byte *buf, size_t bufsize);

	void parse (framecnt_t timestamp);
	
	/** Write a message to port.
	 * @return true on success.
	 * FIXME: describe semantics here
	 */
	int midimsg (byte *msg, size_t len, timestamp_t timestamp) {
		return !(write (msg, len, timestamp) == (int) len);
	} 

	bool clock (timestamp_t timestamp);

	/* select(2)/poll(2)-based I/O */

	/** Get the file descriptor for port.
	 * @return File descriptor, or -1 if not selectable. 
	 */
	int selectable () const {
		return xthread.selectable();
	}

	Channel *channel (channel_t chn) { 
		return _channel[chn&0x7F];
	}
	
	Parser* parser () {
		return _parser;
	}
	
	const char *name () const   { return _tagname.c_str(); }
	bool   ok ()   const        { return _ok; }

	bool centrally_parsed() const { return _centrally_parsed; }
	void set_centrally_parsed(bool yn) { _centrally_parsed = yn; }

	bool receives_input () const {
		return _flags == IsInput;
	}

	bool sends_output () const {
		return _flags == IsOutput;
	}

	struct Descriptor {
	    std::string tag;
	    Flags flags;

	    Descriptor (const XMLNode&);
	    XMLNode& get_state();
	};

	pframes_t nframes_this_cycle() const { return _nframes_this_cycle; }

	void reestablish (jack_client_t *);
	void reconnect ();

	static void set_process_thread (pthread_t);
	static pthread_t get_process_thread () { return _process_thread; }
	static bool is_process_thread();

	static std::string state_node_name;
	
	static PBD::Signal0<void> MakeConnections;
	static PBD::Signal0<void> JackHalted;

private:	
	bool              _ok;
	bool              _currently_in_cycle;
	pframes_t         _nframes_this_cycle;
	std::string       _tagname;
	size_t            _number;
	Channel*          _channel[16];
	Parser*           _parser;
	jack_client_t*    _jack_client;
	jack_port_t*      _jack_port;
	framecnt_t        _last_read_index;
	timestamp_t       _last_write_timestamp;
	RingBuffer< Evoral::Event<double> > output_fifo;
	Evoral::EventRingBuffer<timestamp_t> input_fifo;
	Glib::Mutex output_fifo_lock;
	CrossThreadChannel xthread;
	Flags             _flags;
	bool              _centrally_parsed;


	int create_port ();

	/** Channel used to signal to the MidiControlUI that input has arrived */
	
	std::string _connections;
	PBD::ScopedConnection connect_connection;
	PBD::ScopedConnection halt_connection;
	void flush (void* jack_port_buffer);
	void jack_halted ();
	void make_connections ();
	void init (std::string const &, Flags);

	static pthread_t _process_thread;

};

struct PortSet {
    PortSet (std::string str) : owner (str) { }
    
    std::string owner;
    std::list<XMLNode> ports;
};

std::ostream & operator << ( std::ostream & os, const Port & port );

} // namespace MIDI

#endif // __libmidi_port_h__
