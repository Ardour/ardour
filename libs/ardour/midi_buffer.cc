/*
    Copyright (C) 2006-2007 Paul Davis 
	Author: Dave Robillard
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <iostream>
#include <ardour/midi_buffer.h>

#ifdef __x86_64__
static const int CPU_CACHE_ALIGN = 64;
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

using namespace std;
using namespace ARDOUR;


// FIXME: mirroring for MIDI buffers?
MidiBuffer::MidiBuffer(size_t capacity)
	: Buffer(DataType::MIDI, capacity)
	, _events(0)
	, _data(0)
{
	if (capacity) {
		resize(_capacity);
		silence(_capacity);
	}
}
	
MidiBuffer::~MidiBuffer()
{
	if (_events) {
		free(_events);
	}
	if (_data) {
		free(_data);
	}
}

void
MidiBuffer::resize(size_t size)
{
	assert(size > 0);

	if (size < _capacity) {
		return;
	}

	free(_data);
	free(_events);

	_size = 0;
	_capacity = size;

#ifdef NO_POSIX_MEMALIGN
	_events = (Evoral::MIDIEvent *) malloc(sizeof(Evoral::MIDIEvent) * _capacity);
	_data = (uint8_t *) malloc(sizeof(uint8_t) * _capacity * MAX_EVENT_SIZE);
#else
	posix_memalign((void**)&_events, CPU_CACHE_ALIGN, sizeof(Evoral::Event) * _capacity);
	posix_memalign((void**)&_data, CPU_CACHE_ALIGN, sizeof(uint8_t) * _capacity * MAX_EVENT_SIZE);
#endif	
	assert(_data);
	assert(_events);
}

void
MidiBuffer::copy(const MidiBuffer& copy)
{
	assert(_capacity >= copy._capacity);
	_size = 0;

	for (size_t i = 0; i < copy.size(); ++i)
		push_back(copy[i]);
}


/** Read events from @a src starting at time @a offset into the START of this buffer, for
 * time duration @a nframes.  Relative time, where 0 = start of buffer.
 *
 * Note that offset and nframes refer to sample time, NOT buffer offsets or event counts.
 */
void
MidiBuffer::read_from(const Buffer& src, nframes_t nframes, nframes_t offset)
{
	assert(src.type() == DataType::MIDI);
	assert(&src != this);

	const MidiBuffer& msrc = (MidiBuffer&)src;
	
	assert(_capacity >= msrc.size());

	if (offset == 0) {
		clear();
		assert(_size == 0);
	}
	
	// FIXME: slow
	for (size_t i = 0; i < msrc.size(); ++i) {
		const Evoral::MIDIEvent& ev = msrc[i];
		//cout << "MidiBuffer::read_from event type: " << int(ev.type())
		//		<< " time: " << ev.time() << " buffer size: " << _size << endl;
		if (ev.time() < offset) {
			//cout << "MidiBuffer::read_from skipped event before " << offset << endl;
		} else if (ev.time() < (nframes + offset)) {
			//cout << "MidiBuffer::read_from appending event" << endl;
			push_back(ev);
		} else {
			//cerr << "MidiBuffer::read_from skipped event after "
			//	<< nframes << " + " << offset << endl;
		}
	}

	_silent = src.silent();
}


/** Push an event into the buffer.
 *
 * Note that the raw MIDI pointed to by ev will be COPIED and unmodified.
 * That is, the caller still owns it, if it needs freeing it's Not My Problem(TM).
 * Realtime safe.
 * @return false if operation failed (not enough room)
 */
bool
MidiBuffer::push_back(const Evoral::MIDIEvent& ev)
{
	if (_size == _capacity) {
		cerr << "MidiBuffer::push_back failed (buffer is full)" << endl;
		return false;
	}

	uint8_t* const write_loc = _data + (_size * MAX_EVENT_SIZE);

	memcpy(write_loc, ev.buffer(), ev.size());
	_events[_size] = ev;
	_events[_size].set_buffer(ev.size(), write_loc, false);

	/*cerr << "MidiBuffer: pushed @ " << _events[_size].time()
		<< " size = " << _size << endl;
	for (size_t i = 0; i < _events[_size].size(); ++i) {
		printf("%X ", _events[_size].buffer()[i]);
	}
	printf("\n");*/

	++_size;
	_silent = false;

	return true;
}


/** Push an event into the buffer.
 *
 * Note that the raw MIDI pointed to by ev will be COPIED and unmodified.
 * That is, the caller still owns it, if it needs freeing it's Not My Problem(TM).
 * Realtime safe.
 * @return false if operation failed (not enough room)
 */
bool
MidiBuffer::push_back(const jack_midi_event_t& ev)
{
	if (_size == _capacity) {
		cerr << "MidiBuffer::push_back failed (buffer is full)" << endl;
		return false;
	}

	uint8_t* const write_loc = _data + (_size * MAX_EVENT_SIZE);

	memcpy(write_loc, ev.buffer, ev.size);
	_events[_size].time() = (double)ev.time;
	_events[_size].set_buffer(ev.size, write_loc, false);

	/*cerr << "MidiBuffer: pushed @ " << _events[_size].time()
		<< " size = " << _size << endl;
	for (size_t i = 0; i < _events[_size].size(); ++i) {
		printf("%X ", _events[_size].buffer()[i]);
	}
	printf("\n");*/
	
	++_size;
	_silent = false;

	return true;
}


/** Reserve space for a new event in the buffer.
 *
 * This call is for copying MIDI directly into the buffer, the data location
 * (of sufficient size to write \a size bytes) is returned, or 0 on failure.
 * This call MUST be immediately followed by a write to the returned data
 * location, or the buffer will be corrupted and very nasty things will happen.
 */
uint8_t*
MidiBuffer::reserve(double time, size_t size)
{
	if (size > MAX_EVENT_SIZE) {
		cerr << "WARNING: Failed to reserve " << size << " bytes for event";
		return 0;
	}

	if (_size == _capacity) {
		return 0;
	}

	uint8_t* const write_loc = _data + (_size * MAX_EVENT_SIZE);

	_events[_size].time() = time;
	_events[_size].set_buffer(size, write_loc, false);
	++_size;

	//cerr << "MidiBuffer: reserved, size = " << _size << endl;

	_silent = false;

	return write_loc;
}


void
MidiBuffer::silence(nframes_t dur, nframes_t offset)
{
	// FIXME use parameters
	if (offset != 0)
		cerr << "WARNING: MidiBuffer::silence w/ offset != 0 (not implemented)" << endl;

	memset(_events, 0, sizeof(Evoral::Event) * _capacity);
	memset(_data, 0, sizeof(uint8_t) * _capacity * MAX_EVENT_SIZE);
	_size = 0;
	_silent = true;
}

bool
MidiBuffer::merge_in_place(const MidiBuffer &other)
{
	if (other.size() == 0) {
		return true;
	}

	if (this->size() == 0) {
		copy(other);
		return true;
	}

	{
		MidiBuffer merge_buffer(0);
		Evoral::MIDIEvent onstack_events[_capacity];
		uint8_t	onstack_data[_capacity * MAX_EVENT_SIZE];
		merge_buffer._events = onstack_events;
		merge_buffer._data = onstack_data;
		merge_buffer._size = 0;

		bool retval = merge_buffer.merge(*this, other);

		copy(merge_buffer);

		// set pointers to zero again, so destructor
		// does not end in calling free() for memory
		// on the stack;
		merge_buffer._events = 0;
		merge_buffer._data = 0;

		return retval;
	}
}

/** Clear, and merge \a a and \a b into this buffer.
 *
 * FIXME: This is slow.
 *
 * \return true if complete merge was successful
 */
bool
MidiBuffer::merge(const MidiBuffer& a, const MidiBuffer& b)
{
	_size = 0;
	
	if (this == &a) {
	    merge_in_place(b);
	}

	if (this == &b) {
	    merge_in_place(a);
	}

	size_t a_index = 0;
	size_t b_index = 0;
	size_t count = a.size() + b.size();

	while (count > 0) {
		
		if (size() == capacity()) {
			cerr << "WARNING: MIDI buffer overrun, events lost!" << endl;
			return false;
		}
		
		if (a_index == a.size()) {
			push_back(b[b_index]);
			++b_index;
		} else if (b_index == b.size()) {
			push_back(a[a_index]);
			++a_index;
		} else {
			const Evoral::MIDIEvent& a_ev = a[a_index];
			const Evoral::MIDIEvent& b_ev = b[b_index];

			if (a_ev.time() <= b_ev.time()) {
				push_back(a_ev);
				++a_index;
			} else {
				push_back(b_ev);
				++b_index;
			}
		}

		--count;
	}

	return true;
}

