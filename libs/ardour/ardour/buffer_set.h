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

#ifndef __ardour_buffer_set_h__
#define __ardour_buffer_set_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cassert>
#include <vector>
#include "ardour/chan_count.h"
#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {

class Buffer;
class AudioBuffer;
class MidiBuffer;
class PortSet;
#ifdef HAVE_SLV2
class LV2EventBuffer;
#endif

/** A set of buffers of various types.
 *
 * These are mainly accessed from Session and passed around as scratch buffers
 * (eg as parameters to run() methods) to do in-place signal processing.
 *
 * There are two types of counts associated with a BufferSet - available,
 * and the 'use count'.  Available is the actual number of allocated buffers
 * (and so is the maximum acceptable value for the use counts).
 *
 * The use counts are how things determine the form of their input and inform
 * others the form of their output (eg what they did to the BufferSet).
 * Setting the use counts is realtime safe.
 */
class BufferSet
{
public:
	BufferSet();
	~BufferSet();

	void clear();
	
	void attach_buffers(PortSet& ports, nframes_t nframes, nframes_t offset = 0);

	void ensure_buffers(DataType type, size_t num_buffers, size_t buffer_capacity);
	void ensure_buffers(const ChanCount& chns, size_t buffer_capacity);

	const ChanCount& available() const { return _available; }
	ChanCount&       available()       { return _available; }

	const ChanCount& count() const { return _count; }
	ChanCount&       count()       { return _count; }

	void is_silent(bool yn) { _is_silent = yn; }
	bool is_silent() const  { return _is_silent; }
	void silence (nframes_t nframes, nframes_t offset);
	bool is_mirror() const { return _is_mirror; } 

	void set_count(const ChanCount& count) { assert(count <= _available); _count = count; }
	
	size_t buffer_capacity(DataType type) const;

	Buffer& get(DataType type, size_t i);

	AudioBuffer& get_audio(size_t i) {
		return (AudioBuffer&)get(DataType::AUDIO, i);
	}
	
	MidiBuffer& get_midi(size_t i) {
		return (MidiBuffer&)get(DataType::MIDI, i);
	}

#ifdef HAVE_SLV2
	/** Get a MIDI buffer translated into an LV2 MIDI buffer for use with plugins.
	 * The index here corresponds directly to MIDI buffer numbers (i.e. the index
	 * passed to get_midi), translation back and forth will happen as needed */
	LV2EventBuffer& get_lv2_midi(bool input, size_t i);

	/** Flush modified LV2 event output buffers back to Ardour buffers */
	void flush_lv2_midi(bool input, size_t i);
#endif

	void read_from(BufferSet& in, nframes_t nframes);
	void merge_from(BufferSet& in, nframes_t nframes);

	// ITERATORS
	// FIXME: possible to combine these?  templates?
	
	class audio_iterator {
	public:
		AudioBuffer& operator*()  { return _set.get_audio(_index); }
		AudioBuffer* operator->() { return &_set.get_audio(_index); }
		audio_iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const audio_iterator& other) { return (_index == other._index); }
		bool operator!=(const audio_iterator& other) { return (_index != other._index); }

	private:
		friend class BufferSet;

		audio_iterator(BufferSet& list, size_t index) : _set(list), _index(index) {}

		BufferSet& _set;
		size_t    _index;
	};

	audio_iterator audio_begin() { return audio_iterator(*this, 0); }
	audio_iterator audio_end()   { return audio_iterator(*this, _count.n_audio()); }

	class midi_iterator {
	public:
		MidiBuffer& operator*()  { return _set.get_midi(_index); }
		MidiBuffer* operator->() { return &_set.get_midi(_index); }
		midi_iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const midi_iterator& other) { return (_index == other._index); }
		bool operator!=(const midi_iterator& other) { return (_index != other._index); }

	private:
		friend class BufferSet;

		midi_iterator(BufferSet& list, size_t index) : _set(list), _index(index) {}

		BufferSet& _set;
		size_t    _index;
	};

	midi_iterator midi_begin() { return midi_iterator(*this, 0); }
	midi_iterator midi_end()   { return midi_iterator(*this, _count.n_midi()); }

	class iterator {
	public:
		Buffer& operator*()  { return _set.get(_type, _index); }
		Buffer* operator->() { return &_set.get(_type, _index); }
		iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const iterator& other) { return (_index == other._index); }
		bool operator!=(const iterator& other) { return (_index != other._index); }
		iterator operator=(const iterator& other) {
			_set = other._set; _type = other._type; _index = other._index; return *this;
		}

	private:
		friend class BufferSet;

		iterator(BufferSet& list, DataType type, size_t index)
			: _set(list), _type(type), _index(index) {}

		BufferSet& _set;
		DataType   _type;
		size_t     _index;
	};

	iterator begin(DataType type) { return iterator(*this, type, 0); }
	iterator end(DataType type)   { return iterator(*this, type, _count.get(type)); }
	
private:
	typedef std::vector<Buffer*> BufferVec;

	/// Vector of vectors, indexed by DataType
	std::vector<BufferVec> _buffers;
	
#ifdef HAVE_SLV2
	/// LV2 MIDI buffers (for conversion to/from MIDI buffers)
	typedef std::vector< std::pair<bool, LV2EventBuffer*> > LV2Buffers;
	LV2Buffers _lv2_buffers;
#endif

	/// Use counts (there may be more actual buffers than this)
	ChanCount _count;

	/// Available counts (number of buffers actually allocated)
	ChanCount _available;

	/// False if we 'own' the contained buffers, if true we mirror a PortSet)
	bool _is_mirror;

	/// Whether the buffer set should be considered silent
	bool _is_silent;
};


} // namespace ARDOUR

#endif // __ardour_buffer_set_h__
