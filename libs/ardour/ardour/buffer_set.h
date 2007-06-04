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

#include <cassert>
#include <vector>
#include <ardour/chan_count.h>
#include <ardour/data_type.h>

namespace ARDOUR {

class Buffer;
class AudioBuffer;
class MidiBuffer;
class PortSet;


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
	
	void attach_buffers(PortSet& ports);

	void ensure_buffers(const ChanCount& count, size_t buffer_capacity);
	void ensure_buffers(DataType type, size_t num_buffers, size_t buffer_capacity);

	const ChanCount& available() const { return _available; }
	ChanCount&       available()       { return _available; }

	const ChanCount& count() const { return _count; }
	ChanCount&       count()       { return _count; }

	void set_count(const ChanCount& count) { _count = count; }
	
	size_t buffer_capacity(DataType type) const;

	Buffer& get(DataType type, size_t i)
	{
		assert(i <= _count.get(type));
		return *_buffers[type.to_index()][i];
	}

	AudioBuffer& get_audio(size_t i)
	{
		return (AudioBuffer&)get(DataType::AUDIO, i);
	}
	
	MidiBuffer& get_midi(size_t i)
	{
		return (MidiBuffer&)get(DataType::MIDI, i);
	}

	void read_from(BufferSet& in, jack_nframes_t nframes, jack_nframes_t offset=0);

	// ITERATORS
	
	// FIXME: this is a filthy copy-and-paste mess
	// FIXME: litter these with assertions
	
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

	class iterator {
	public:

		Buffer& operator*()  { return _set.get(_type, _index); }
		Buffer* operator->() { return &_set.get(_type, _index); }
		iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const iterator& other) { return (_index == other._index); }
		bool operator!=(const iterator& other) { return (_index != other._index); }
		iterator operator=(const iterator& other) { _set = other._set; _type = other._type; _index = other._index; return *this; }

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

	/// Vector of vectors, indexed by DataType::to_index()
	std::vector<BufferVec> _buffers;

	/// Use counts (there may be more actual buffers than this)
	ChanCount _count;

	/// Available counts (number of buffers actually allocated)
	ChanCount _available;

	/// Whether we (don't) 'own' the contained buffers (otherwise we mirror a PortSet)
	bool _is_mirror;
};


} // namespace ARDOUR

#endif // __ardour_buffer_set_h__
