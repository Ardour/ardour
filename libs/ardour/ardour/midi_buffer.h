/*
    Copyright (C) 2006-2009 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_midi_buffer_h__
#define __ardour_midi_buffer_h__

#include "evoral/EventSink.hpp"
#include "evoral/midi_util.h"
#include "evoral/types.hpp"

#include "midi++/event.h"

#include "ardour/buffer.h"
#include "ardour/parameter_types.h"

namespace ARDOUR {


/** Buffer containing 8-bit unsigned char (MIDI) data. */
class LIBARDOUR_API MidiBuffer : public Buffer, public Evoral::EventSink<framepos_t>
{
public:
	typedef framepos_t TimeType;

	MidiBuffer(size_t capacity);
	~MidiBuffer();

	void silence (framecnt_t nframes, framecnt_t offset = 0);
	void read_from (const Buffer& src, framecnt_t nframes, frameoffset_t dst_offset = 0, frameoffset_t src_offset = 0);
	void merge_from (const Buffer& src, framecnt_t nframes, frameoffset_t dst_offset = 0, frameoffset_t src_offset = 0);

	void copy(const MidiBuffer& copy);
	void copy(MidiBuffer const * const);

	bool     push_back(const Evoral::Event<TimeType>& event);
	bool     push_back(TimeType time, size_t size, const uint8_t* data);

	uint8_t* reserve (TimeType time, size_t object_size);

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
		iterator_base<BufferType, EventType>(BufferType& b, framecnt_t o)
		: buffer(&b), offset(o) { }

		iterator_base<BufferType, EventType>(const iterator_base<BufferType,EventType>& o)
		: buffer (o.buffer), offset(o.offset) { }

		inline iterator_base<BufferType,EventType> operator= (const iterator_base<BufferType,EventType>& o) {
			if (&o != this) {
				buffer = o.buffer;
				offset = o.offset;
			}
			return *this;
		}

		inline EventType* operator*() const {
			return reinterpret_cast<EventType*> (buffer->_data + offset);
		}

		inline EventType* operator*() {
			return reinterpret_cast<EventType*> (buffer->_data + offset);
		}

		inline iterator_base<BufferType, EventType>& operator++() {
			EventType const * ev = reinterpret_cast<EventType*> (buffer->_data + offset);
			assert (ev->size() > 0);
			offset += ev->aligned_size ();
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

		const size_t total_data_deleted = (*i)->aligned_size();

		if (i.offset + total_data_deleted > _size) {
			_size = 0;
			return end();
		}

		/* move data from after the erased event */
		memmove (_data + i.offset, _data + i.offset + total_data_deleted, total_data_deleted);
		_size -= total_data_deleted;

		/* all subsequent iterators are now invalid, and the one we
		 * return should refer to the event we copied, which was after
		 * the one we just erased.
		 */

		return iterator (*this, i.offset);
	}

	uint8_t const * data() const { return _data; }

	void dump (std::ostream&) const;

  private:
	friend class iterator_base< MidiBuffer, Evoral::Event<TimeType> >;
	friend class iterator_base< const MidiBuffer, const Evoral::Event<TimeType> >;

	uint8_t* _data; ///< timestamp, event, timestamp, event, ...
	pframes_t _size;
};

} // namespace ARDOUR

#endif // __ardour_midi_buffer_h__
