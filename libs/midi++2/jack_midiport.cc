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
 
  $Id: alsa_sequencer_midiport.cc 244 2006-01-06 04:59:17Z essej $
*/

#include <fcntl.h>
#include <cerrno>
#include <cassert>

#include <midi++/types.h>
#include <midi++/jack.h>
#include <midi++/port_request.h>

using namespace std;
using namespace MIDI;

JACK_MidiPort::JACK_MidiPort(PortRequest & req, jack_client_t* jack_client)
	: Port(req)
	, _jack_client(jack_client)
	, _jack_input_port(NULL)
	, _jack_output_port(NULL)
	, _last_read_index(0)
{
	int err = create_ports(req);

	if (!err) {
		req.status = PortRequest::OK;
		_ok = true;
	} else {
		req.status = PortRequest::Unknown;
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
	jack_midi_clear_buffer(jack_port_get_buffer(_jack_output_port, nframes), nframes);
}

int
JACK_MidiPort::write(byte * msg, size_t msglen, timestamp_t timestamp)
{
	assert(_currently_in_cycle);
	assert(timestamp < _nframes_this_cycle);
	assert(_jack_output_port);

	// FIXME: return value correct?
	return jack_midi_event_write (
		jack_port_get_buffer(_jack_output_port, _nframes_this_cycle),
		timestamp, msg, msglen, _nframes_this_cycle);
}

int
JACK_MidiPort::read(byte * buf, size_t max, timestamp_t timestamp)
{
	assert(_currently_in_cycle);
	assert(timestamp < _nframes_this_cycle);
	assert(_jack_input_port);

	jack_midi_event_t ev;

	int err = jack_midi_event_get (&ev,
		jack_port_get_buffer(_jack_input_port, _nframes_this_cycle),
		_last_read_index++, _nframes_this_cycle);
	
	if (!err) {
		memcpy(buf, ev.buffer, ev.size);
		return ev.size;
	} else {
		return 0;
	}
}

int
JACK_MidiPort::create_ports(PortRequest & req)
{
	assert(!_jack_input_port);
	assert(!_jack_output_port);
	
	jack_nframes_t nframes = jack_get_buffer_size(_jack_client);

	bool ret = true;

	if (req.mode == O_RDWR || req.mode == O_WRONLY) {
		_jack_output_port = jack_port_register(_jack_client,
			string(req.tagname).append("_out").c_str(),
			JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  		jack_midi_reset_new_port(
			jack_port_get_buffer(_jack_output_port, nframes), nframes);
		ret = ret && (_jack_output_port != NULL);
	}

	if (req.mode == O_RDWR || req.mode == O_RDONLY) {
		_jack_input_port = jack_port_register(_jack_client,
			string(req.tagname).append("_in").c_str(),
			JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  		jack_midi_reset_new_port(
			jack_port_get_buffer(_jack_input_port, nframes), nframes);
		ret = ret && (_jack_input_port != NULL);
	}

	return ret ? 0 : -1;
}

