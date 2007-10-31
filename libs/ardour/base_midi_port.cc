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

#include <ardour/base_midi_port.h>
#include <ardour/data_type.h>

using namespace ARDOUR;
using namespace std;

BaseMidiPort::BaseMidiPort (const std::string& name, Flags flags)
	: Port (name, flags)
	, _buffer (0) 
	, _own_buffer (false)
{
	_type = DataType::MIDI;
	reset();
}

BaseMidiPort::~BaseMidiPort()
{
	if (_own_buffer && _buffer) {
		delete _buffer;
	}
}

void
BaseMidiPort::default_mixdown (const set<Port*>& ports, MidiBuffer* dest, nframes_t cnt, nframes_t offset, bool first_overwrite)
{
	set<Port*>::const_iterator p = ports.begin();

	if (first_overwrite) {
		dest->read_from ((dynamic_cast<BaseMidiPort*>(*p))->get_midi_buffer(), cnt, offset);
		p++;
	}

	// XXX DAVE: this is just a guess

	for (; p != ports.end(); ++p) {
		dest->merge (*dest, (dynamic_cast<BaseMidiPort*>(*p))->get_midi_buffer());
	}
}

void 
BaseMidiPort::set_mixdown_function (void (*func)(const set<Port*>&, MidiBuffer*, nframes_t, nframes_t, bool))
{
	/* caller should (but not must) hold process lock since this is an atomic operation on most platforms
	   and even if its not, it doesn't really matter.
	*/

	_mixdown = func;
}
