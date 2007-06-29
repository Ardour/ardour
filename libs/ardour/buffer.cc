/*
    Copyright (C) 2006 Paul Davis 
    
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

#include <algorithm>
#include <iostream>
using std::cerr; using std::endl;

#include <ardour/buffer.h>
#include <ardour/audio_buffer.h>
#include <ardour/midi_buffer.h>

#ifdef __x86_64__
static const int CPU_CACHE_ALIGN = 64;
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

namespace ARDOUR {


Buffer*
Buffer::create(DataType type, size_t capacity)
{
	if (type == DataType::AUDIO)
		return new AudioBuffer(capacity);
	else if (type == DataType::MIDI)
		return new MidiBuffer(capacity);
	else
		return NULL;
}


AudioBuffer::AudioBuffer(size_t capacity)
	: Buffer(DataType::AUDIO, capacity)
	, _owns_data(false)
	, _data(NULL)
{
	_size = capacity; // For audio buffers, size = capacity (always)
	if (capacity > 0) {
#ifdef NO_POSIX_MEMALIGN
		_data =  (Sample *) malloc(sizeof(Sample) * capacity);
#else
		posix_memalign((void**)&_data, CPU_CACHE_ALIGN, sizeof(Sample) * capacity);
#endif	
		assert(_data);
		_owns_data = true;
		clear();
	}
}

AudioBuffer::~AudioBuffer()
{
	if (_owns_data)
		free(_data);
}

// FIXME: mirroring for MIDI buffers?
MidiBuffer::MidiBuffer(size_t capacity)
	: Buffer(DataType::MIDI, capacity)
//	, _owns_data(true)
	, _events(NULL)
	, _data(NULL)
{
	assert(capacity > 0);

	_size = 0;

#ifdef NO_POSIX_MEMALIGN
	_events =  (MidiEvent *) malloc(sizeof(MidiEvent) * capacity);
	_data =  (Byte *) malloc(sizeof(Byte) * capacity * MAX_EVENT_SIZE);
#else
	posix_memalign((void**)&_events, CPU_CACHE_ALIGN, sizeof(MidiEvent) * capacity);
	posix_memalign((void**)&_data, CPU_CACHE_ALIGN, sizeof(Byte) * capacity * MAX_EVENT_SIZE);
#endif	
	assert(_data);
	assert(_events);
	silence(_capacity);
}

MidiBuffer::~MidiBuffer()
{
	free(_events);
	free(_data);
}


/** Read events from @a src starting at time @a offset into the START of this buffer, for
 * time direction @a nframes.  Relative time, where 0 = start of buffer.
 *
 * Note that offset and nframes refer to sample time, NOT buffer offsets or event counts.
 */
void
MidiBuffer::read_from(const Buffer& src, nframes_t nframes, nframes_t offset)
{
	assert(src.type() == DataType::MIDI);
	const MidiBuffer& msrc = (MidiBuffer&)src;

	assert(_capacity >= src.size());

	clear();
	assert(_size == 0);

	// FIXME: slow
	for (size_t i=0; i < src.size(); ++i) {
		const MidiEvent& ev = msrc[i];
		if (ev.time >= offset && ev.time < offset+nframes) {
			push_back(ev);
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
MidiBuffer::push_back(const MidiEvent& ev)
{
	if (_size == _capacity)
		return false;

	Byte* const write_loc = _data + (_size * MAX_EVENT_SIZE);

	memcpy(write_loc, ev.buffer, ev.size);
	_events[_size] = ev;
	_events[_size].buffer = write_loc;
	++_size;

	//cerr << "MidiBuffer: pushed, size = " << _size << endl;

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
	if (_size == _capacity)
		return false;

	Byte* const write_loc = _data + (_size * MAX_EVENT_SIZE);

	memcpy(write_loc, ev.buffer, ev.size);
	_events[_size].time = (double)ev.time;
	_events[_size].size = ev.size;
	_events[_size].buffer = write_loc;
	++_size;

	//cerr << "MidiBuffer: pushed, size = " << _size << endl;

	_silent = false;

	return true;
}


/** Reserve space for a new event in the buffer.
 *
 * This call is for copying MIDI directly into the buffer, the data location
 * (of sufficient size to write \a size bytes) is returned, or NULL on failure.
 * This call MUST be immediately followed by a write to the returned data
 * location, or the buffer will be corrupted and very nasty things will happen.
 */
Byte*
MidiBuffer::reserve(double time, size_t size)
{
	assert(size < MAX_EVENT_SIZE);

	if (_size == _capacity)
		return NULL;

	Byte* const write_loc = _data + (_size * MAX_EVENT_SIZE);

	_events[_size].time = time;
	_events[_size].size = size;
	_events[_size].buffer = write_loc;
	++_size;

	//cerr << "MidiBuffer: reserved, size = " << _size << endl;

	_silent = false;

	return write_loc;
}


void
MidiBuffer::silence(nframes_t dur, nframes_t offset)
{
	// FIXME use parameters
	assert(offset == 0);
	//assert(dur == _capacity);

	memset(_events, 0, sizeof(MidiEvent) * _capacity);
	memset(_data, 0, sizeof(Byte) * _capacity * MAX_EVENT_SIZE);
	_size = 0;
	_silent = true;
}


} // namespace ARDOUR

