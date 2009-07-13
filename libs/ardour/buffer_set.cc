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


#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <algorithm>
#include "ardour/buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/midi_buffer.h"
#include "ardour/port.h"
#include "ardour/port_set.h"
#include "ardour/audioengine.h"
#ifdef HAVE_SLV2
#include "ardour/lv2_plugin.h"
#include "ardour/lv2_event_buffer.h"
#endif

namespace ARDOUR {

/** Create a new, empty BufferSet */
BufferSet::BufferSet()
	: _is_mirror(false)
	, _is_silent(false)
{
	for (size_t i=0; i < DataType::num_types; ++i) {
		_buffers.push_back(BufferVec());
	}

	_count.reset();
	_available.reset();
}

BufferSet::~BufferSet()
{
	clear();
}

/** Destroy all contained buffers.
 */
void
BufferSet::clear()
{
	if (!_is_mirror) {
		for (std::vector<BufferVec>::iterator i = _buffers.begin(); i != _buffers.end(); ++i) {
			for (BufferVec::iterator j = (*i).begin(); j != (*i).end(); ++j) {
				delete *j;
			}
			(*i).clear();
		}
	}
	_buffers.clear();
	_count.reset();
	_available.reset();
}

/** Make this BufferSet a direct mirror of a PortSet's buffers.
 */
void
BufferSet::attach_buffers(PortSet& ports, nframes_t nframes, nframes_t offset)
{
	clear();

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		_buffers.push_back(BufferVec());
		BufferVec& v = _buffers[*t];

		for (PortSet::iterator p = ports.begin(*t); p != ports.end(*t); ++p) {
			assert(p->type() == *t);
			v.push_back(&(p->get_buffer(nframes, offset)));
		}
	}
	
	_count = ports.count();
	_available = ports.count();

	_is_mirror = true;
}

/** Ensure that there are @a num_buffers buffers of type @a type available,
 * each of size at least @a buffer_size
 */
void
BufferSet::ensure_buffers(DataType type, size_t num_buffers, size_t buffer_capacity)
{
	assert(type != DataType::NIL);
	assert(type < _buffers.size());

	if (num_buffers == 0) {
		return;
	}

	// The vector of buffers of the type we care about
	BufferVec& bufs = _buffers[type];
	
	// If we're a mirror just make sure we're ok
	if (_is_mirror) {
		assert(_count.get(type) >= num_buffers);
		assert(bufs[0]->type() == type);
		return;
	}

	// If there's not enough or they're too small, just nuke the whole thing and
	// rebuild it (so I'm lazy..)
	if (bufs.size() < num_buffers
			|| (bufs.size() > 0 && bufs[0]->capacity() < buffer_capacity)) {

		// Nuke it
		for (BufferVec::iterator i = bufs.begin(); i != bufs.end(); ++i) {
			delete (*i);
		}
		bufs.clear();

		// Rebuild it
		for (size_t i = 0; i < num_buffers; ++i) {
			bufs.push_back(Buffer::create(type, buffer_capacity));
		}
	
		_available.set(type, num_buffers);
		_count.set (type, num_buffers);
	}

#ifdef HAVE_SLV2
	// Ensure enough low level MIDI format buffers are available for conversion
	// in both directions (input & output, out-of-place)
	if (type == DataType::MIDI && _lv2_buffers.size() < _buffers[type].size() * 2 + 1) {
		while (_lv2_buffers.size() < _buffers[type].size() * 2) {
			_lv2_buffers.push_back(std::make_pair(false, new LV2EventBuffer(buffer_capacity)));
		}
	}
#endif

	// Post-conditions
	assert(bufs[0]->type() == type);
	assert(bufs.size() >= num_buffers);
	assert(bufs.size() == _available.get(type));
	assert(bufs[0]->capacity() >= buffer_capacity);
}

/** Ensure that the number of buffers of each type @a type matches @a chns
 * and each buffer is of size at least @a buffer_capacity
 */
void
BufferSet::ensure_buffers(const ChanCount& chns, size_t buffer_capacity)
{
	if (chns == ChanCount::ZERO) {
		return;
	}

	// If we're a mirror just make sure we're ok
	if (_is_mirror) {
		assert(_count >= chns);
		return;
	}

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

		// The vector of buffers of this type
		BufferVec& bufs = _buffers[*t];

		uint32_t nbufs = chns.get (*t);
		
		if (nbufs == 0) {
			// Nuke it
			for (BufferVec::iterator i = bufs.begin(); i != bufs.end(); ++i) {
				delete (*i);
			}
			bufs.clear();
			continue;
		}

		// If there's not enough or they're too small, just nuke the whole thing and
		// rebuild it (so I'm lazy..)
		if (bufs.size() < nbufs
		    || (bufs.size() > 0 && bufs[0]->capacity() < buffer_capacity)) {
			
			// Nuke it
			for (BufferVec::iterator i = bufs.begin(); i != bufs.end(); ++i) {
				delete (*i);
			}
			bufs.clear();
			
			// Rebuild it
			for (size_t i = 0; i < nbufs; ++i) {
				bufs.push_back(Buffer::create(*t, buffer_capacity));
			}
			
			_available.set (*t, nbufs);
		}
		
#ifdef HAVE_SLV2
		// Ensure enough low level MIDI format buffers are available for conversion
		// in both directions (input & output, out-of-place)
		if (*t == DataType::MIDI && _lv2_buffers.size() < _buffers[DataType::MIDI].size() * 2 + 1) {
			while (_lv2_buffers.size() < _buffers[DataType::MIDI].size() * 2) {
				_lv2_buffers.push_back(std::make_pair(false, new LV2EventBuffer(buffer_capacity)));
			}
		}
#endif
		
		// Post-conditions
		assert(bufs[0]->type() == *t);
		assert(bufs.size() == _available.get(*t));
		assert(bufs[0]->capacity() >= buffer_capacity);
	}
	
	assert (available() == chns);
}

/** Get the capacity (size) of the available buffers of the given type.
 *
 * All buffers of a certain type always have the same capacity.
 */
size_t
BufferSet::buffer_capacity(DataType type) const
{
	assert(_available.get(type) > 0);
	return _buffers[type][0]->capacity();
}

Buffer&
BufferSet::get(DataType type, size_t i)
{
	assert(i < _available.get(type));
	return *_buffers[type][i];
}

#ifdef HAVE_SLV2

LV2EventBuffer&
BufferSet::get_lv2_midi(bool input, size_t i)
{
	MidiBuffer& mbuf = get_midi(i);
	LV2Buffers::value_type b = _lv2_buffers.at(i * 2 + (input ? 0 : 1));
	LV2EventBuffer* ebuf = b.second;
	
	ebuf->reset();
	if (input) {
		for (MidiBuffer::iterator e = mbuf.begin(); e != mbuf.end(); ++e) {
			const Evoral::MIDIEvent<nframes_t> ev(*e, false);
			uint32_t type = LV2Plugin::midi_event_type();
			ebuf->append(ev.time(), 0, type, ev.size(), ev.buffer());
		}
	}
	return *ebuf;
}

void
BufferSet::flush_lv2_midi(bool input, size_t i)
{
	MidiBuffer& mbuf = get_midi(i);
	LV2Buffers::value_type b = _lv2_buffers.at(i * 2 + (input ? 0 : 1));
	LV2EventBuffer* ebuf = b.second;

	mbuf.silence(0, 0);
	for (ebuf->rewind(); ebuf->is_valid(); ebuf->increment()) {
		uint32_t frames;
		uint32_t subframes;
		uint16_t type;
		uint16_t size;
		uint8_t* data;
		ebuf->get_event(&frames, &subframes, &type, &size, &data);
		mbuf.push_back(frames, size, data);
	}
}

#endif

// FIXME: make 'in' const
void
BufferSet::read_from (BufferSet& in, nframes_t nframes)
{
	assert(available() >= in.count());

	// Copy all buffers 1:1
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		BufferSet::iterator o = begin(*t);
		for (BufferSet::iterator i = in.begin(*t); i != in.end(*t); ++i, ++o) {
			o->read_from (*i, nframes);
		}
	}

	set_count(in.count());
}

// FIXME: make 'in' const
void
BufferSet::merge_from (BufferSet& in, nframes_t nframes)
{
	/* merge all input buffers into out existing buffers.

	   NOTE: if "in" contains more buffers than this set,
	   we will drop the extra buffers.

	*/

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		BufferSet::iterator o = begin(*t);
		for (BufferSet::iterator i = in.begin(*t); i != in.end(*t) && o != end (*t); ++i, ++o) {
			o->merge_from (*i, nframes);
		}
	}
}

void
BufferSet::silence (nframes_t nframes, nframes_t offset)
{
	for (std::vector<BufferVec>::iterator i = _buffers.begin(); i != _buffers.end(); ++i) {
		for (BufferVec::iterator b = i->begin(); b != i->end(); ++b) {
			(*b)->silence (nframes, offset);
		}
	}
}

} // namespace ARDOUR

