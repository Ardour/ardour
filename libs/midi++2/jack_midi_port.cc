/*
    Copyright (C) 1998 Paul Barton-Davis
    
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

    $Id$
*/
#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <errno.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/convert.h"
#include "pbd/strsplit.h"
#include "pbd/stacktrace.h"

#include "midi++/types.h"
#include "midi++/jack_midi_port.h"
#include "midi++/channel.h"

using namespace MIDI;
using namespace std;
using namespace PBD;

pthread_t JackMIDIPort::_process_thread;
Signal0<void> JackMIDIPort::JackHalted;
Signal0<void> JackMIDIPort::MakeConnections;

JackMIDIPort::JackMIDIPort (string const & name, Flags flags, jack_client_t* jack_client)
	: Port (name, flags)
	, _currently_in_cycle (false)
	, _nframes_this_cycle (0)
	, _jack_client (jack_client)
	, _jack_port (0)
	, output_fifo (512)
	, input_fifo (1024)
	, xthread (true)
{
	assert (jack_client);
	init (name, flags);
}

JackMIDIPort::JackMIDIPort (const XMLNode& node, jack_client_t* jack_client)
	: Port (node)
	, _currently_in_cycle (false)
	, _nframes_this_cycle (0)
	, _jack_client (jack_client)
	, _jack_port (0)
	, output_fifo (512)
	, input_fifo (1024)
	, xthread (true)
{
	assert (jack_client);

	Descriptor desc (node);
	init (desc.tag, desc.flags);
	set_state (node);
}

void
JackMIDIPort::init (string const & name, Flags flags)
{
	if (!create_port ()) {
		_ok = true;
	}

	MakeConnections.connect_same_thread (connect_connection, boost::bind (&JackMIDIPort::make_connections, this));
	JackHalted.connect_same_thread (halt_connection, boost::bind (&JackMIDIPort::jack_halted, this));
}


JackMIDIPort::~JackMIDIPort ()
{
	for (int i = 0; i < 16; i++) {
		delete _channel[i];
	}

	if (_jack_port) {
		if (_jack_client) {
			jack_port_unregister (_jack_client, _jack_port);
			_jack_port = 0;
		}
	}
}

void
JackMIDIPort::parse (framecnt_t timestamp)
{
	byte buf[512];

	/* NOTE: parsing is done (if at all) by initiating a read from 
	   the port. Each port implementation calls on the parser
	   once it has data ready.
	*/
	
	_parser->set_timestamp (timestamp);

	while (1) {
		
		// cerr << "+++ READ ON " << name() << endl;

		int nread = read (buf, sizeof (buf));

		// cerr << "-- READ (" << nread << " ON " << name() << endl;
		
		if (nread > 0) {
			if ((size_t) nread < sizeof (buf)) {
				break;
			} else {
				continue;
			}
		} else if (nread == 0) {
			break;
		} else if (errno == EAGAIN) {
			break;
		} else {
			fatal << "Error reading from MIDI port " << name() << endmsg;
			/*NOTREACHED*/
		}
	}
}

void
JackMIDIPort::cycle_start (pframes_t nframes)
{
	assert (_jack_port);
	
	_currently_in_cycle = true;
	_nframes_this_cycle = nframes;

	assert(_nframes_this_cycle == nframes);

	if (sends_output()) {
		void *buffer = jack_port_get_buffer (_jack_port, nframes);
		jack_midi_clear_buffer (buffer);
		flush (buffer);	
	}
	
	if (receives_input()) {
		void* jack_buffer = jack_port_get_buffer(_jack_port, nframes);
		const pframes_t event_count = jack_midi_get_event_count(jack_buffer);

		jack_midi_event_t ev;
		timestamp_t cycle_start_frame = jack_last_frame_time (_jack_client);

		for (pframes_t i = 0; i < event_count; ++i) {
			jack_midi_event_get (&ev, jack_buffer, i);
			input_fifo.write (cycle_start_frame + ev.time, (Evoral::EventType) 0, ev.size, ev.buffer);
		}	
		
		if (event_count) {
			xthread.wakeup ();
		}
	}
}

void
JackMIDIPort::cycle_end ()
{
	if (sends_output()) {
		flush (jack_port_get_buffer (_jack_port, _nframes_this_cycle));
	}

	_currently_in_cycle = false;
	_nframes_this_cycle = 0;
}

void
JackMIDIPort::jack_halted ()
{
	_jack_client = 0;
	_jack_port = 0;
}

void
JackMIDIPort::drain (int check_interval_usecs)
{
	RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0} };

	if (is_process_thread()) {
		error << "Process thread called MIDI::JackMIDIPort::drain() - this cannot work" << endmsg;
		return;
	}

	while (1) {
		output_fifo.get_write_vector (&vec);
		if (vec.len[0] + vec.len[1] >= output_fifo.bufsize() - 1) {
			break;
		}
		usleep (check_interval_usecs);
	}
}

int
JackMIDIPort::write(byte * msg, size_t msglen, timestamp_t timestamp)
{
	int ret = 0;

	if (!sends_output()) {
		return ret;
	}
	
	if (!is_process_thread()) {

		Glib::Mutex::Lock lm (output_fifo_lock);
		RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0} };
		
		output_fifo.get_write_vector (&vec);

		if (vec.len[0] + vec.len[1] < 1) {
			error << "no space in FIFO for non-process thread MIDI write" << endmsg;
			return 0;
		}

		if (vec.len[0]) {
                        if (!vec.buf[0]->owns_buffer()) {
                                vec.buf[0]->set_buffer (0, 0, true);
                        }
			vec.buf[0]->set (msg, msglen, timestamp);
		} else {
                        if (!vec.buf[1]->owns_buffer()) {
                                vec.buf[1]->set_buffer (0, 0, true);
                        }
			vec.buf[1]->set (msg, msglen, timestamp);
		}

		output_fifo.increment_write_idx (1);
		
		ret = msglen;

	} else {

		// XXX This had to be temporarily commented out to make export work again
		if (!(timestamp < _nframes_this_cycle)) {
			std::cerr << "attempting to write MIDI event of " << msglen << " bytes at time "
				  << timestamp << " of " << _nframes_this_cycle
				  << " (this will not work - needs a code fix)"
				  << std::endl;
		}

		if (_currently_in_cycle) {
			if (timestamp == 0) {
				timestamp = _last_write_timestamp;
			} 

			if (jack_midi_event_write (jack_port_get_buffer (_jack_port, _nframes_this_cycle), 
						timestamp, msg, msglen) == 0) {
				ret = msglen;
				_last_write_timestamp = timestamp;

			} else {
				ret = 0;
				cerr << "write of " << msglen << " failed, port holds "
					<< jack_midi_get_event_count (jack_port_get_buffer (_jack_port, _nframes_this_cycle))
					<< endl;
			}
		} else {
			cerr << "write to JACK midi port failed: not currently in a process cycle." << endl;
			PBD::stacktrace (cerr, 20);
		}
	}

	if (ret > 0 && _parser) {
		// ardour doesn't care about this and neither should your app, probably
		// output_parser->raw_preparse (*output_parser, msg, ret);
		for (int i = 0; i < ret; i++) {
			_parser->scanner (msg[i]);
		}
		// ardour doesn't care about this and neither should your app, probably
		// output_parser->raw_postparse (*output_parser, msg, ret);
	}	

	return ret;
}

void
JackMIDIPort::flush (void* jack_port_buffer)
{
	RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0 } };
	size_t written;

	output_fifo.get_read_vector (&vec);

	if (vec.len[0] + vec.len[1]) {
		// cerr << "Flush " << vec.len[0] + vec.len[1] << " events from non-process FIFO\n";
	}

	if (vec.len[0]) {
		Evoral::Event<double>* evp = vec.buf[0];
		
		for (size_t n = 0; n < vec.len[0]; ++n, ++evp) {
			jack_midi_event_write (jack_port_buffer,
					       (timestamp_t) evp->time(), evp->buffer(), evp->size());
		}
	}
	
	if (vec.len[1]) {
		Evoral::Event<double>* evp = vec.buf[1];

		for (size_t n = 0; n < vec.len[1]; ++n, ++evp) {
			jack_midi_event_write (jack_port_buffer,
					       (timestamp_t) evp->time(), evp->buffer(), evp->size());
		}
	}
	
	if ((written = vec.len[0] + vec.len[1]) != 0) {
		output_fifo.increment_read_idx (written);
	}
}

int
JackMIDIPort::read (byte *, size_t)
{
	if (!receives_input()) {
		return 0;
	}
	
	timestamp_t time;
	Evoral::EventType type;
	uint32_t size;
	byte buffer[input_fifo.capacity()];

	while (input_fifo.read (&time, &type, &size, buffer)) {
		_parser->set_timestamp (time);
		for (uint32_t i = 0; i < size; ++i) {
			_parser->scanner (buffer[i]);
		}
	}

	return 0;
}

int
JackMIDIPort::create_port ()
{
	_jack_port = jack_port_register(_jack_client, _tagname.c_str(), JACK_DEFAULT_MIDI_TYPE, _flags, 0);
	return _jack_port == 0 ? -1 : 0;
}

XMLNode& 
JackMIDIPort::get_state () const
{
	XMLNode& root = Port::get_state ();
	
#if 0
	byte device_inquiry[6];

	device_inquiry[0] = 0xf0;
	device_inquiry[0] = 0x7e;
	device_inquiry[0] = 0x7f;
	device_inquiry[0] = 0x06;
	device_inquiry[0] = 0x02;
	device_inquiry[0] = 0xf7;
	
	write (device_inquiry, sizeof (device_inquiry), 0);
#endif

	if (_jack_port) {
		
		const char** jc = jack_port_get_connections (_jack_port);
		string connection_string;
		if (jc) {
			for (int i = 0; jc[i]; ++i) {
				if (i > 0) {
					connection_string += ',';
				}
				connection_string += jc[i];
			}
			free (jc);
		}
		
		if (!connection_string.empty()) {
			root.add_property ("connections", connection_string);
		}
	} else {
		if (!_connections.empty()) {
			root.add_property ("connections", _connections);
		}
	}

	return root;
}

void
JackMIDIPort::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("tag")) == 0 || prop->value() != _tagname) {
		return;
	}
	
	Port::set_state (node);

	if ((prop = node.property ("connections")) != 0) {
		_connections = prop->value ();
	}
}

void
JackMIDIPort::make_connections ()
{
	if (!_connections.empty()) {
		vector<string> ports;
		split (_connections, ports, ',');
		for (vector<string>::iterator x = ports.begin(); x != ports.end(); ++x) {
			if (_jack_client) {
				if (receives_input()) {
					jack_connect (_jack_client, (*x).c_str(), jack_port_name (_jack_port));
				} else {
					jack_connect (_jack_client, jack_port_name (_jack_port), (*x).c_str());
				}
				/* ignore failures */
			}
		}
	}

	connect_connection.disconnect ();
}

void
JackMIDIPort::set_process_thread (pthread_t thr)
{
	_process_thread = thr;
}

bool
JackMIDIPort::is_process_thread()
{
	return (pthread_self() == _process_thread);
}

void
JackMIDIPort::reestablish (jack_client_t* jack)
{
	_jack_client = jack;
	int const r = create_port ();

	if (r) {
		PBD::error << "could not reregister ports for " << name() << endmsg;
	}
}

void
JackMIDIPort::reconnect ()
{
	make_connections ();
}
