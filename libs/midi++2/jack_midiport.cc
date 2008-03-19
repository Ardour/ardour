/*
  Copyright (C) 2006 Paul Davis 
  Written by Dave Robillard
 
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

#include <fcntl.h>
#include <cerrno>
#include <cassert>

#include <pbd/error.h>

#include <midi++/types.h>
#include <midi++/jack.h>

using namespace std;
using namespace MIDI;
using namespace PBD;

pthread_t JACK_MidiPort::_process_thread;

JACK_MidiPort::JACK_MidiPort(const XMLNode& node, jack_client_t* jack_client)
	: Port(node)
	, _jack_client(jack_client)
	, _jack_input_port(NULL)
	, _jack_output_port(NULL)
	, _last_read_index(0)
	, non_process_thread_fifo (512)
{
	int err = create_ports (node);

	if (!err) {
		_ok = true;
	} 
}

JACK_MidiPort::~JACK_MidiPort()
{
	// FIXME: remove port
}

void
JACK_MidiPort::cycle_start (nframes_t nframes)
{
	Port::cycle_start(nframes);
	assert(_nframes_this_cycle == nframes);
	_last_read_index = 0;

	void *buffer = jack_port_get_buffer (_jack_output_port, nframes);
	jack_midi_clear_buffer (buffer);
	flush (buffer);
}

int
JACK_MidiPort::write(byte * msg, size_t msglen, timestamp_t timestamp)
{
	if (!is_process_thread()) {

		Glib::Mutex::Lock lm (non_process_thread_fifo_lock);
		RingBuffer<Event>::rw_vector vec;
		
		non_process_thread_fifo.get_write_vector (&vec);

		cerr << "Non-process thread writes " << msglen << " to " << name() << endl;

		if (vec.len[0] + vec.len[1] < 1) {
			error << "no space in FIFO for non-process thread MIDI write"
			      << endmsg;
			return 0;
		}

		if (vec.len[0]) {
			vec.buf[0]->set (msg, msglen, timestamp);
		} else {
			vec.buf[1]->set (msg, msglen, timestamp);
		}

		non_process_thread_fifo.increment_write_idx (1);

		return msglen;
				
	} else {

		assert(_currently_in_cycle);
		assert(timestamp < _nframes_this_cycle);
		assert(_jack_output_port);

		// FIXME: return value correct?
		return jack_midi_event_write (jack_port_get_buffer (_jack_output_port, _nframes_this_cycle), 
					      timestamp, msg, msglen);
	}
}

void
JACK_MidiPort::flush (void* jack_port_buffer)
{
	RingBuffer<Event>::rw_vector vec;
	size_t written;

	non_process_thread_fifo.get_read_vector (&vec);

	if (vec.len[0] + vec.len[1]) {
		cerr << "Flush " << vec.len[0] + vec.len[1] << " events from non-process FIFO\n";
	}

	if (vec.len[0]) {
		Event* evp = vec.buf[0];
		
		for (size_t n = 0; n < vec.len[0]; ++n, ++evp) {
			jack_midi_event_write (jack_port_buffer,
					       (timestamp_t) evp->time(), evp->buffer(), evp->size());
		}
	}
	
	if (vec.len[1]) {
		Event* evp = vec.buf[1];

		for (size_t n = 0; n < vec.len[1]; ++n, ++evp) {
			jack_midi_event_write (jack_port_buffer,
					       (timestamp_t) evp->time(), evp->buffer(), evp->size());
		}
	}
	
	if ((written = vec.len[0] + vec.len[1]) != 0) {
		non_process_thread_fifo.increment_read_idx (written);
	}
}

int
JACK_MidiPort::read(byte * buf, size_t bufsize)
{
	assert(_currently_in_cycle);
	assert(_jack_input_port);

	jack_midi_event_t ev;

	int err = jack_midi_event_get (&ev,
				       jack_port_get_buffer(_jack_input_port, _nframes_this_cycle),
				       _last_read_index++);
	
	// XXX this doesn't handle ev.size > max

	if (!err) {
		size_t limit = min (bufsize, ev.size);
		memcpy(buf, ev.buffer, limit);
		return limit;
	} else {
		return 0;
	}
}

int
JACK_MidiPort::create_ports(const XMLNode& node)
{
	Descriptor desc (node);

	assert(!_jack_input_port);
	assert(!_jack_output_port);
	
	jack_nframes_t nframes = jack_get_buffer_size(_jack_client);

	bool ret = true;

	if (desc.mode == O_RDWR || desc.mode == O_WRONLY) {
		_jack_output_port = jack_port_register(_jack_client,
						       string(desc.tag).append("_out").c_str(),
						       JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  		jack_midi_clear_buffer(jack_port_get_buffer(_jack_output_port, nframes));
		ret = ret && (_jack_output_port != NULL);
	}
	
	if (desc.mode == O_RDWR || desc.mode == O_RDONLY) {
		_jack_input_port = jack_port_register(_jack_client,
						      string(desc.tag).append("_in").c_str(),
						      JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  		jack_midi_clear_buffer(jack_port_get_buffer(_jack_input_port, nframes));
		ret = ret && (_jack_input_port != NULL);
	}

	return ret ? 0 : -1;
}

XMLNode& 
JACK_MidiPort::get_state () const
{
	XMLNode& root (Port::get_state ());
	return root;
}

void
JACK_MidiPort::set_state (const XMLNode& node)
{
}

void
JACK_MidiPort::set_process_thread (pthread_t thr)
{
	_process_thread = thr;
}

bool
JACK_MidiPort::is_process_thread()
{
	return (pthread_self() == _process_thread);
}
