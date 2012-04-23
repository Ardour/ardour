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
#include "midi++/port_base.h"

namespace MIDI {

class Channel;
class PortRequest;

class Port : public PortBase {
  public:
	Port (std::string const &, PortBase::Flags, jack_client_t *);
	Port (const XMLNode&, jack_client_t *);
	~Port ();

	XMLNode& get_state () const;
	void set_state (const XMLNode&);

	void cycle_start (pframes_t nframes);
	void cycle_end ();

	void parse (framecnt_t timestamp);
	int write (byte *msg, size_t msglen, timestamp_t timestamp);
	int read (byte *buf, size_t bufsize);
	void drain (int check_interval_usecs);
	int selectable () const { return xthread.selectable(); }

	pframes_t nframes_this_cycle() const { return _nframes_this_cycle; }

	void reestablish (jack_client_t *);
	void reconnect ();

	static void set_process_thread (pthread_t);
	static pthread_t get_process_thread () { return _process_thread; }
	static bool is_process_thread();

	static PBD::Signal0<void> MakeConnections;
	static PBD::Signal0<void> JackHalted;

private:	
	bool              _currently_in_cycle;
	pframes_t         _nframes_this_cycle;
	jack_client_t*    _jack_client;
	jack_port_t*      _jack_port;
	timestamp_t       _last_write_timestamp;
	RingBuffer< Evoral::Event<double> > output_fifo;
	Evoral::EventRingBuffer<timestamp_t> input_fifo;
	Glib::Mutex output_fifo_lock;
	CrossThreadChannel xthread;

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

} // namespace MIDI

#endif // __libmidi_port_h__
