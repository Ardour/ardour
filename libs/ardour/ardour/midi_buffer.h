/*
 * Copyright (C) 2007-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_buffer_h__
#define __ardour_midi_buffer_h__

#include "evoral/Event.h"
#include "evoral/EventSink.h"
#include "evoral/midi_util.h"
#include "evoral/types.h"

#include "ardour/buffer.h"
#include "ardour/parameter_types.h"

namespace ARDOUR {


/** Buffer containing 8-bit unsigned char (MIDI) data. */
class LIBARDOUR_API MidiBuffer : public Buffer, public Evoral::EventSink<samplepos_t>
{
public:
	typedef samplepos_t TimeType;

	MidiBuffer(size_t capacity);
	~MidiBuffer();

	void silence (samplecnt_t nframes, samplecnt_t offset = 0);
	void read_from (const Buffer& src, samplecnt_t nframes, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0);
	void merge_from (const Buffer& src, samplecnt_t nframes, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0);

	void copy(const MidiBuffer& copy);
	void copy(MidiBuffer const * const);

	void skip_to (TimeType when);

	bool push_back(const Evoral::Event<TimeType>& event);
	bool push_back(TimeType time, Evoral::EventType event_type, size_t size, const uint8_t* data);

	uint8_t* reserve(TimeType time, Evoral::EventType event_type, size_t size);

	void resize(size_t);
	size_t size() const { return _size; }
	bool empty() const { return _size == 0; }

	bool insert_event(const Evoral::Event<TimeType>& event);
	bool merge_in_place(const MidiBuffer &other);

	/** EventSink interface for non-RT use (export, bounce). */
	uint32_t write(TimeType time, Evoral::EventType type, uint32_t size, const uint8_t* buf);

	template<typename BufferType, typename EventType>
		class iterator_base
	{
	public:
		iterator_base<BufferType, EventType>(BufferType& b, samplecnt_t o)
			: buffer(&b), offset(o) {}

		iterator_base<BufferType, EventType>(const iterator_base<BufferType,EventType>& o)
			: buffer (o.buffer), offset(o.offset) {}

		inline iterator_base<BufferType,EventType> operator= (const iterator_base<BufferType,EventType>& o) {
			if (&o != this) {
				buffer = o.buffer;
				offset = o.offset;
			}
			return *this;
		}

		inline EventType operator*() const {
			uint8_t* ev_start = buffer->_data + offset + sizeof(TimeType) + sizeof (Evoral::EventType);
			int event_size = Evoral::midi_event_size(ev_start);
			assert(event_size >= 0);
			return EventType(
					*((Evoral::EventType*)(buffer->_data + offset + sizeof(TimeType))),
					*((TimeType*)(buffer->_data + offset)),
					event_size, ev_start);
		}

		inline EventType operator*() {
			uint8_t* ev_start = buffer->_data + offset + sizeof(TimeType) + sizeof (Evoral::EventType);
			int event_size = Evoral::midi_event_size(ev_start);
			assert(event_size >= 0);
			return EventType(
					*(reinterpret_cast<Evoral::EventType*>((uintptr_t)(buffer->_data + offset + sizeof(TimeType)))),
					*(reinterpret_cast<TimeType*>((uintptr_t)(buffer->_data + offset))),
					event_size, ev_start);
		}

		inline TimeType * timeptr() {
			return reinterpret_cast<TimeType*>((uintptr_t)(buffer->_data + offset));
		}

		inline Evoral::EventType * event_type_ptr() {
			return reinterpret_cast<Evoral::EventType*>((uintptr_t)(buffer->_data + offset + sizeof(TimeType)));
		}

		inline iterator_base<BufferType, EventType>& operator++() {
			uint8_t* ev_start = buffer->_data + offset + sizeof(TimeType) + sizeof (Evoral::EventType);
			int event_size = Evoral::midi_event_size(ev_start);
			assert(event_size >= 0);
			offset += align32 (sizeof(TimeType) + sizeof (Evoral::EventType) + event_size);
			return *this;
		}

		inline bool operator!=(const iterator_base<BufferType, EventType>& other) const {
			return (buffer != other.buffer) || (offset != other.offset);
		}

		inline bool operator==(const iterator_base<BufferType, EventType>& other) const {
			return (buffer == other.buffer) && (offset == other.offset);
		}

		BufferType*     buffer;
		size_t          offset;
	};

	typedef iterator_base< MidiBuffer, Evoral::Event<TimeType> >             iterator;
	typedef iterator_base< const MidiBuffer, const Evoral::Event<TimeType> > const_iterator;

	iterator begin() { return iterator(*this, 0); }
	iterator end()   { return iterator(*this, _size); }

	const_iterator begin() const { return const_iterator(*this, 0); }
	const_iterator end()   const { return const_iterator(*this, _size); }

	iterator erase(const iterator& i) {
		assert (i.buffer == this);
		uint8_t* ev_start = _data + i.offset + sizeof (TimeType) + sizeof (Evoral::EventType);
		int event_size = Evoral::midi_event_size (ev_start);

		if (event_size < 0) {
			/* unknown size, sysex: return end() */
			return end();
		}

		size_t total_data_deleted = align32 (sizeof(TimeType) + sizeof (Evoral::EventType) + event_size);

		if (i.offset + total_data_deleted > _size) {
			_size = 0;
			return end();
		}

		/* we need to avoid the temporary malloc that memmove would do,
		   so copy by hand. remember: this is small amounts of data ...
		*/
		size_t a, b;
		for (a = i.offset, b = i.offset + total_data_deleted; b < _size; ++b, ++a) {
			_data[a] = _data[b];
		}

		_size -= total_data_deleted;

		/* all subsequent iterators are now invalid, and the one we
		 * return should refer to the event we copied, which was after
		 * the one we just erased.
		 */

		return iterator (*this, i.offset);
	}

	/**
	 * returns true if the message with the second argument as its MIDI
	 * status byte should preceed the message with the first argument as
	 * its MIDI status byte.
	 */
	static bool second_simultaneous_midi_byte_is_first (uint8_t, uint8_t);

private:
	friend class iterator_base< MidiBuffer, Evoral::Event<TimeType> >;
	friend class iterator_base< const MidiBuffer, const Evoral::Event<TimeType> >;

	static size_t align32 (size_t s) {
#if defined(__arm__) || defined(__aarch64__)
		return ((s - 1) | 3) + 1;
#else
		return s;
#endif
	}

	uint8_t* _data; ///< [timestamp, event-type, event]*
	pframes_t _size;
};

} // namespace ARDOUR

#endif // __ardour_midi_buffer_h__
