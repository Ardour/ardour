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
#include <sstream>

#include "pbd/compose.h"
#include "pbd/failed_constructor.h"

#include "ardour/buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/port.h"
#include "ardour/port_set.h"
#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#include "lv2_evbuf.h"
#endif
#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT
#include "ardour/vestige/aeffectx.h"
#endif

namespace ARDOUR {

/** Create a new, empty BufferSet */
BufferSet::BufferSet()
	: _is_mirror(false)
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

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT 
	for (VSTBuffers::iterator i = _vst_buffers.begin(); i != _vst_buffers.end(); ++i) {
		delete *i;
	}

	_vst_buffers.clear ();
#endif

}

/** Set up this BufferSet so that its data structures mirror a PortSet's buffers.
 *  This is quite expensive and not RT-safe, so it should not be called in a process context;
 *  get_backend_port_addresses() will fill in a structure set up by this method.
 *
 *  XXX: this *is* called in a process context; I'm not sure quite what `should not' means above.
 */
void
BufferSet::attach_buffers (PortSet& ports)
{
	const ChanCount& count (ports.count());

	clear ();

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		_buffers.push_back (BufferVec());
		BufferVec& v = _buffers[*t];
		v.assign (count.n (*t), (Buffer*) 0);
	}

	_count = ports.count();
	_available = ports.count();


	_is_mirror = true;
}

/** Write the backend port addresses from a PortSet into our data structures.  This
 *  call assumes that attach_buffers() has already been called for the same PortSet.
 *  Does not allocate, so RT-safe BUT you can only call Port::get_buffer() from
 *  the process() callback tree anyway, so this has to be called in RT context.
 */
void
BufferSet::get_backend_port_addresses (PortSet& ports, framecnt_t nframes)
{
	assert (_count == ports.count ());
	assert (_available == ports.count ());
	assert (_is_mirror);

	assert (_buffers.size() == DataType::num_types);

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		BufferVec& v = _buffers[*t];

		assert (v.size() == ports.num_ports (*t));

		int i = 0;
		for (PortSet::iterator p = ports.begin(*t); p != ports.end(*t); ++p) {
			v[i] = &p->get_buffer (nframes);
			++i;
		}
	}
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

#ifdef LV2_SUPPORT
	// Ensure enough low level MIDI format buffers are available for conversion
	// in both directions (input & output, out-of-place)
	if (type == DataType::MIDI && _lv2_buffers.size() < _buffers[type].size() * 2 + 1) {
		while (_lv2_buffers.size() < _buffers[type].size() * 2) {
			_lv2_buffers.push_back(
				std::make_pair(false, lv2_evbuf_new(buffer_capacity,
				                                    LV2_EVBUF_EVENT,
				                                    LV2Plugin::urids.atom_Chunk,
				                                    LV2Plugin::urids.atom_Sequence)));
		}
	}
#endif

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT
	// As above but for VST
	if (type == DataType::MIDI) {
		while (_vst_buffers.size() < _buffers[type].size()) {
			_vst_buffers.push_back (new VSTBuffer (buffer_capacity));
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
	for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
		ensure_buffers (*i, chns.get (*i), buffer_capacity);
	}
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

const Buffer&
BufferSet::get(DataType type, size_t i) const
{
	assert(i < _available.get(type));
	return *_buffers[type][i];
}

#ifdef LV2_SUPPORT

void
BufferSet::ensure_lv2_bufsize(bool input, size_t i, size_t buffer_capacity)
{
	assert(count().get(DataType::MIDI) > i);

	LV2Buffers::value_type b     = _lv2_buffers.at(i * 2 + (input ? 0 : 1));
	LV2_Evbuf*             evbuf = b.second;

	if (lv2_evbuf_get_capacity(evbuf) >= buffer_capacity) return;

	lv2_evbuf_free(b.second);
	_lv2_buffers.at(i * 2 + (input ? 0 : 1)) =
		std::make_pair(false, lv2_evbuf_new(
					buffer_capacity,
					LV2_EVBUF_EVENT,
					LV2Plugin::urids.atom_Chunk,
					LV2Plugin::urids.atom_Sequence));
}

LV2_Evbuf*
BufferSet::get_lv2_midi(bool input, size_t i, bool old_api)
{
	assert(count().get(DataType::MIDI) > i);

	LV2Buffers::value_type b     = _lv2_buffers.at(i * 2 + (input ? 0 : 1));
	LV2_Evbuf*             evbuf = b.second;

	lv2_evbuf_set_type(evbuf, old_api ? LV2_EVBUF_EVENT : LV2_EVBUF_ATOM);
	lv2_evbuf_reset(evbuf, input);
	return evbuf;
}

void
BufferSet::forward_lv2_midi(LV2_Evbuf* buf, size_t i, bool purge_ardour_buffer)
{
	MidiBuffer& mbuf  = get_midi(i);
	if (purge_ardour_buffer) {
		mbuf.silence(0, 0);
	}
	for (LV2_Evbuf_Iterator i = lv2_evbuf_begin(buf);
			 lv2_evbuf_is_valid(i);
			 i = lv2_evbuf_next(i)) {
		uint32_t frames, subframes, type, size;
		uint8_t* data;
		lv2_evbuf_get(i, &frames, &subframes, &type, &size, &data);
		if (type == LV2Plugin::urids.midi_MidiEvent) {
			mbuf.push_back(frames, size, data);
		}
	}
}

void
BufferSet::flush_lv2_midi(bool input, size_t i)
{
	MidiBuffer&            mbuf  = get_midi(i);
	LV2Buffers::value_type b     = _lv2_buffers.at(i * 2 + (input ? 0 : 1));
	LV2_Evbuf*             evbuf = b.second;

	mbuf.silence(0, 0);
	for (LV2_Evbuf_Iterator i = lv2_evbuf_begin(evbuf);
	     lv2_evbuf_is_valid(i);
	     i = lv2_evbuf_next(i)) {
		uint32_t frames;
		uint32_t subframes;
		uint32_t type;
		uint32_t size;
		uint8_t* data;
		lv2_evbuf_get(i, &frames, &subframes, &type, &size, &data);
#ifndef NDEBUG
		DEBUG_TRACE (PBD::DEBUG::LV2, string_compose ("(FLUSH) MIDI event of size %1\n", size));
		for (uint16_t x = 0; x < size; ++x) {
			DEBUG_TRACE (PBD::DEBUG::LV2, string_compose ("\tByte[%1] = %2\n", x, (int) data[x]));
		}
#endif
		if (type == LV2Plugin::urids.midi_MidiEvent) {
			// TODO: Make Ardour event buffers generic so plugins can communicate
			mbuf.push_back(frames, size, data);
		}
	}
}

#endif /* LV2_SUPPORT */

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT

VstEvents*
BufferSet::get_vst_midi (size_t b)
{
	MidiBuffer& m = get_midi (b);
	VSTBuffer* vst = _vst_buffers[b];

	vst->clear ();

	for (MidiBuffer::iterator i = m.begin(); i != m.end(); ++i) {
		vst->push_back (*i);
	}

	return vst->events();
}

BufferSet::VSTBuffer::VSTBuffer (size_t c)
  : _capacity (c)
{
	_events = static_cast<VstEvents*> (malloc (sizeof (VstEvents) + _capacity * sizeof (VstEvent *)));
	_midi_events = static_cast<VstMidiEvent*> (malloc (sizeof (VstMidiEvent) * _capacity));

	if (_events == 0 || _midi_events == 0) {
		free (_events);
		free (_midi_events);
		throw failed_constructor ();
	}

	_events->numEvents = 0;
	_events->reserved = 0;
}

BufferSet::VSTBuffer::~VSTBuffer ()
{
	free (_events);
	free (_midi_events);
}

void
BufferSet::VSTBuffer::clear ()
{
	_events->numEvents = 0;
}

void
BufferSet::VSTBuffer::push_back (Evoral::MIDIEvent<framepos_t> const & ev)
{
	if (ev.size() > 3) {
		/* XXX: this will silently drop MIDI messages longer than 3 bytes, so
		   they won't be passed to VST plugins or VSTis
		*/
		return;
	}
	int const n = _events->numEvents;
	assert (n < (int) _capacity);

	_events->events[n] = reinterpret_cast<VstEvent*> (_midi_events + n);
	VstMidiEvent* v = reinterpret_cast<VstMidiEvent*> (_events->events[n]);

	v->type = kVstMidiType;
	v->byteSize = sizeof (VstMidiEvent);
	v->deltaFrames = ev.time ();

	v->flags = 0;
	v->detune = 0;
	v->noteLength = 0;
	v->noteOffset = 0;
	v->reserved1 = 0;
	v->reserved2 = 0;
	v->noteOffVelocity = 0;
	memcpy (v->midiData, ev.buffer(), ev.size());
	v->midiData[3] = 0;

	_events->numEvents++;
}

#endif /* WINDOWS_VST_SUPPORT */

/** Copy buffers of one type from `in' to this BufferSet */
void
BufferSet::read_from (const BufferSet& in, framecnt_t nframes, DataType type)
{
	assert (available().get (type) >= in.count().get (type));

	BufferSet::iterator o = begin (type);
	for (BufferSet::const_iterator i = in.begin (type); i != in.end (type); ++i, ++o) {
		o->read_from (*i, nframes);
	}

	_count.set (type, in.count().get (type));
}

/** Copy buffers of all types from `in' to this BufferSet */
void
BufferSet::read_from (const BufferSet& in, framecnt_t nframes)
{
	assert(available() >= in.count());

	// Copy all buffers 1:1
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		read_from (in, nframes, *t);
	}
}

void
BufferSet::merge_from (const BufferSet& in, framecnt_t nframes)
{
	/* merge all input buffers into out existing buffers.

	   NOTE: if "in" contains more buffers than this set,
	   we will drop the extra buffers.

	*/

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		BufferSet::iterator o = begin(*t);
		for (BufferSet::const_iterator i = in.begin(*t); i != in.end(*t) && o != end (*t); ++i, ++o) {
			o->merge_from (*i, nframes);
		}
	}
}

void
BufferSet::silence (framecnt_t nframes, framecnt_t offset)
{
	for (std::vector<BufferVec>::iterator i = _buffers.begin(); i != _buffers.end(); ++i) {
		for (BufferVec::iterator b = i->begin(); b != i->end(); ++b) {
			(*b)->silence (nframes, offset);
		}
	}
}

void
BufferSet::set_is_silent (bool yn)
{
	for (std::vector<BufferVec>::iterator i = _buffers.begin(); i != _buffers.end(); ++i) {
		for (BufferVec::iterator b = i->begin(); b != i->end(); ++b) {
			(*b)->set_is_silent (yn);
		}
	}

}

} // namespace ARDOUR

