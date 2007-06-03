/*
    Copyright (C) 2006 Paul Davis 
	Written by Dave Robillard, 2006

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

#include <iostream>
#include <ardour/midi_model.h>
#include <ardour/types.h>

using namespace std;
using namespace ARDOUR;


MidiModel::MidiModel(size_t size)
: _events(size)
{
}

MidiModel::~MidiModel()
{
	for (size_t i=0; i < _events.size(); ++i)
		delete _events[i].buffer;
}


/** Append contents of \a buf to model.  NOT (even remotely) realtime safe.
 *
 * Timestamps of events in \a buf are expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 *
 * Events in buf are deep copied.
 */
void
MidiModel::append(const MidiBuffer& buf)
{
	for (size_t i=0; i < buf.size(); ++i) {
		const MidiEvent& buf_event = buf[i];
		assert(_events.empty() || buf_event.time >= _events.back().time);

		_events.push_back(buf_event);
		MidiEvent& my_event = _events.back();
		assert(my_event.time == buf_event.time);
		assert(my_event.size == buf_event.size);
		
		my_event.buffer = new Byte[my_event.size];
		memcpy(my_event.buffer, buf_event.buffer, my_event.size);
	}
}


/** Append \a in_event to model.  NOT (even remotely) realtime safe.
 *
 * Timestamps of events in \a buf are expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 *
 * Events in buf are deep copied.
 */
void
MidiModel::append(const MidiEvent& in_event)
{
	assert(_events.empty() || in_event.time >= _events.back().time);

	_events.push_back(in_event);
	MidiEvent& my_event = _events.back();
	assert(my_event.time == in_event.time);
	assert(my_event.size == in_event.size);

	my_event.buffer = new Byte[my_event.size];
	memcpy(my_event.buffer, in_event.buffer, my_event.size);
}

