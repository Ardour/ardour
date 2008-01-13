/*
    Copyright (C) 2006 Paul Davis 

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

#include <cassert>
#include <iostream>

#include <ardour/midi_port.h>
#include <ardour/jack_midi_port.h>
#include <ardour/data_type.h>

using namespace ARDOUR;
using namespace std;

MidiPort::MidiPort (const std::string& name, Flags flags, bool external, nframes_t capacity)
	: Port (name, flags)
	, BaseMidiPort (name, flags)
	, PortFacade (name, flags)
{
	_buffer = new MidiBuffer (capacity);

	if (external) {
		/* external ports use the same buffer for the jack port (_ext_port)
		 * and internal ports (this) */
		_ext_port = new JackMidiPort (name, flags, _buffer);
	} else {
		/* internal ports just have a single buffer, no jack port */
		_ext_port = 0;
	}

	set_name (name);
	reset ();
}

MidiPort::~MidiPort()
{
	if (_ext_port) {
		delete _ext_port;
		_ext_port = 0;
	}
}

void
MidiPort::reset()
{
	BaseMidiPort::reset();

	if (_ext_port) {
		_ext_port->reset ();
	}
}

void
MidiPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	if (_ext_port) {
		_ext_port->cycle_start (nframes, offset);
	}
	
	if (_flags & IsInput) {
			
		if (_ext_port) {
		
			BaseMidiPort* mprt = dynamic_cast<BaseMidiPort*>(_ext_port);
			assert(mprt);
			assert(&mprt->get_midi_buffer() == _buffer);

			if (!_connections.empty()) {
				(*_mixdown) (_connections, _buffer, nframes, offset, false);
			}

		} else {
		
			if (_connections.empty()) {
				_buffer->silence (nframes, offset);
			} else {
				(*_mixdown) (_connections, _buffer, nframes, offset, true);
			}
		}

	} else {
			
		_buffer->silence (nframes, offset);
	}
}

	
void
MidiPort::cycle_end (nframes_t nframes, nframes_t offset)
{
	if (_ext_port) {
		_ext_port->cycle_end (nframes, offset);
	}
}

