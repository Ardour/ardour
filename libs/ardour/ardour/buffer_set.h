/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cassert>
#include <vector>
#include "ardour/chan_count.h"
#include "ardour/data_type.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT
#include "evoral/Event.h"
struct _VstEvents;
typedef struct _VstEvents VstEvents;
struct _VstMidiEvent;
typedef struct _VstMidiEvent VstMidiEvent;
#endif


namespace ARDOUR {

class Buffer;
class AudioBuffer;
class MidiBuffer;
class PortSet;

struct LV2_Evbuf;

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
class LIBARDOUR_API BufferSet
{
public:
	BufferSet();
	~BufferSet();

	void clear();

	void attach_buffers (PortSet const& ports);
	void get_backend_port_addresses (PortSet&, samplecnt_t);

	/* the capacity here is a size_t and has a different interpretation depending
	   on the DataType of the buffers. for audio, its a sample count. for MIDI
	   its a byte count.
	*/

	void ensure_buffers(DataType type, size_t num_buffers, size_t buffer_capacity);
	void ensure_buffers(const ChanCount& chns, size_t buffer_capacity);

	/* Returns true if Buffer::silent_data() is true for all buffers */
	bool silent_data() const;

	const ChanCount& available() const { return _available; }
	ChanCount&       available()       { return _available; }

	const ChanCount& count() const { return _count; }
	ChanCount&       count()       { return _count; }

	void silence (samplecnt_t nframes, samplecnt_t offset);
	bool is_mirror() const { return _is_mirror; }

	void set_count(const ChanCount& count) { assert(count <= _available); _count = count; }

	size_t buffer_capacity(DataType type) const;

	AudioBuffer& get_audio(size_t i) {
		return (AudioBuffer&)get_available (DataType::AUDIO, i);
	}
	const AudioBuffer& get_audio(size_t i) const {
		return (const AudioBuffer&)get_available(DataType::AUDIO, i);
	}

	MidiBuffer& get_midi(size_t i) {
		return (MidiBuffer&)get_available(DataType::MIDI, i);
	}
	const MidiBuffer& get_midi(size_t i) const {
		return (const MidiBuffer&)get_available(DataType::MIDI, i);
	}

	Buffer&       get_available(DataType type, size_t i);
	const Buffer& get_available(DataType type, size_t i) const;

	/** Get a MIDI buffer translated into an LV2 MIDI buffer for use with
	 * plugins.  The index here corresponds directly to MIDI buffer numbers
	 * (i.e. the index passed to get_midi), translation back and forth will
	 * happen as needed.
	 */
	LV2_Evbuf* get_lv2_midi(bool input, size_t i);

	/** ensure minimum size of LV2 Atom port buffer */
	void ensure_lv2_bufsize(bool input, size_t i, size_t buffer_capacity);

	/** Flush modified LV2 event output buffers back to Ardour buffers */
	void flush_lv2_midi(bool input, size_t i, pframes_t, samplecnt_t);

	/** Forward plugin MIDI output to to Ardour buffers */
	void forward_lv2_midi(LV2_Evbuf*, size_t, pframes_t, samplecnt_t);

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT
	VstEvents* get_vst_midi (size_t);
#endif

	void read_from(const BufferSet& in, samplecnt_t nframes);
	void read_from(const BufferSet& in, samplecnt_t nframes, DataType);
	void merge_from(const BufferSet& in, samplecnt_t nframes);

	template <typename BS, typename B>
	class iterator_base {
	public:
		iterator_base(const iterator_base& other)
			: _set(other._set), _type(other._type), _index(other._index) {}
		B& operator*()  { return (B&)_set.get_available(_type, _index); }
		B* operator->() { return &(B&)_set.get_available(_type, _index); }
		iterator_base<BS,B>& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const iterator_base<BS,B>& other) { return (_index == other._index); }
		bool operator!=(const iterator_base<BS,B>& other) { return (_index != other._index); }
		iterator_base<BS,B> operator=(const iterator_base<BS,B>& other) {
			_set = other._set; _type = other._type; _index = other._index; return *this;
		}

	private:
		friend class BufferSet;

		iterator_base(BS& list, DataType type, size_t index)
			: _set(list), _type(type), _index(index) {}

		BS&      _set;
		DataType _type;
		size_t   _index;
	};

	typedef iterator_base<BufferSet, Buffer> iterator;
	iterator begin(DataType type) { return iterator(*this, type, 0); }
	iterator end(DataType type)   { return iterator(*this, type, _count.get(type)); }

	typedef iterator_base<const BufferSet, const Buffer> const_iterator;
	const_iterator begin(DataType type) const { return const_iterator(*this, type, 0); }
	const_iterator end(DataType type)   const { return const_iterator(*this, type, _count.get(type)); }

	typedef iterator_base<BufferSet, AudioBuffer> audio_iterator;
	audio_iterator audio_begin() { return audio_iterator(*this, DataType::AUDIO, 0); }
	audio_iterator audio_end()   { return audio_iterator(*this, DataType::AUDIO, _count.n_audio()); }

	typedef iterator_base<BufferSet, MidiBuffer> midi_iterator;
	midi_iterator midi_begin() { return midi_iterator(*this, DataType::MIDI, 0); }
	midi_iterator midi_end()   { return midi_iterator(*this, DataType::MIDI, _count.n_midi()); }

private:
	typedef std::vector<Buffer*> BufferVec;

	/// Vector of vectors, indexed by DataType
	std::vector<BufferVec> _buffers;

	/// LV2 MIDI buffers (for conversion to/from MIDI buffers)
	typedef std::vector< std::pair<bool, LV2_Evbuf*> > LV2Buffers;
	LV2Buffers _lv2_buffers;

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT
	class VSTBuffer {
	public:
		VSTBuffer (size_t);
		~VSTBuffer ();

		void clear ();
		void push_back (Evoral::Event<samplepos_t> const &);
		VstEvents* events () const {
			return _events;
		}

	private:
		/* prevent copy construction */
		VSTBuffer (VSTBuffer const &);

		VstEvents* _events; /// the parent VSTEvents struct
		VstMidiEvent* _midi_events; ///< storage area for VSTMidiEvents
		size_t _capacity;
	};

	typedef std::vector<VSTBuffer*> VSTBuffers;
	VSTBuffers _vst_buffers;
#endif

	/// Use counts (there may be more actual buffers than this)
	ChanCount _count;

	/// Available counts (number of buffers actually allocated)
	ChanCount _available;

	/// False if we 'own' the contained buffers, if true we mirror a PortSet)
	bool _is_mirror;
};


} // namespace ARDOUR

