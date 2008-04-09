/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#ifndef __libmidipp_midi_event_h__
#define __libmidipp_midi_event_h__

#include <stdint.h>
#include <cstdlib>
#include <cstring>
#include <assert.h>

#include <midi++/types.h>
#include <midi++/events.h>

/** If this is not defined, all methods of MidiEvent are RT safe
 * but MidiEvent will never deep copy and (depending on the scenario)
 * may not be usable in STL containers, signals, etc. 
 */
#define MIDI_EVENT_ALLOW_ALLOC 1

namespace MIDI {


/** Identical to jack_midi_event_t, but with double timestamp
 *
 * time is either a frame time (from/to Jack) or a beat time (internal
 * tempo time, used in MidiModel) depending on context.
 */
struct Event {
#ifdef MIDI_EVENT_ALLOW_ALLOC
	Event(double t=0, uint32_t s=0, uint8_t* b=NULL, bool owns_buffer=false)
		: _time(t)
		, _size(s)
		, _buffer(b)
		, _owns_buffer(owns_buffer)
	{
		if (owns_buffer) {
			_buffer = (uint8_t*)malloc(_size);
			if (b)
				memcpy(_buffer, b, _size);
			else
				memset(_buffer, 0, _size);
		}
	}
	
	/** Copy \a copy.
	 * 
	 * If \a owns_buffer is true, the buffer will be copied and this method
	 * is NOT REALTIME SAFE.  Otherwise both events share a buffer and
	 * memory management semantics are the caller's problem.
	 */
	Event(const Event& copy, bool owns_buffer)
		: _time(copy._time)
		, _size(copy._size)
		, _buffer(copy._buffer)
		, _owns_buffer(owns_buffer)
	{
		if (owns_buffer) {
			_buffer = (uint8_t*)malloc(_size);
			if (copy._buffer)
				memcpy(_buffer, copy._buffer, _size);
			else
				memset(_buffer, 0, _size);
		}
	}
	
	~Event() {
		if (_owns_buffer)
			free(_buffer);
	}

	inline const Event& operator=(const Event& copy) {
		_time = copy._time;
		if (_owns_buffer) {
			if (copy._buffer) {
				if (!_buffer || _size < copy._size)
					_buffer = (uint8_t*)::realloc(_buffer, copy._size);
				memcpy(_buffer, copy._buffer, copy._size);
			} else {
				free(_buffer);
				_buffer = NULL;
			}
		} else {
			_buffer = copy._buffer;
		}

		_size = copy._size;
		return *this;
	}
	
	inline void set (uint8_t* msg, size_t msglen, timestamp_t t) {
		if (_owns_buffer) {
			if (_size < msglen) {
				free (_buffer);
				_buffer = (uint8_t*) malloc (msglen);
			}
		} else {
			_buffer = (uint8_t*) malloc (msglen);
			_owns_buffer = true;
		}

		memcpy (_buffer, msg, msglen);
		_time = t;
	}

	inline bool operator==(const Event& other) const {
		if (_time != other._time)
			return false;

		if (_size != other._size)
			return false;

		if (_buffer == other._buffer)
			return true;

		for (size_t i=0; i < _size; ++i)
			if (_buffer[i] != other._buffer[i])
				return false;

		return true;
	}
	
	inline bool operator!=(const Event& other) const { return ! operator==(other); }

	inline bool owns_buffer() const { return _owns_buffer; }
	
	inline void set_buffer(uint8_t* buf, bool own) {
		if (_owns_buffer) {
			free(_buffer);
			_buffer = NULL;
		}
		_buffer = buf;
		_owns_buffer = own;
	}

	inline void realloc(size_t size) {
		assert(_owns_buffer);
		_buffer = (uint8_t*) ::realloc(_buffer, size);
	}

#else

	inline void set_buffer(uint8_t* buf) { _buffer = buf; }

#endif // MIDI_EVENT_ALLOW_ALLOC

	inline double      time()        const { return _time; }
	inline double&     time()              { return _time; }
	inline uint32_t    size()        const { return _size; }
	inline uint32_t&   size()              { return _size; }
	inline uint8_t     type()        const { return (_buffer[0] & 0xF0); }
	inline uint8_t     channel()     const { return (_buffer[0] & 0x0F); }
	inline void        set_channel(uint8_t channel)   { _buffer[0] = (0xF0 & _buffer[0]) | channel; }
        inline bool        is_note_on()  const { return (type() == MIDI_CMD_NOTE_ON); }
        inline bool        is_note_off() const { return (type() == MIDI_CMD_NOTE_OFF); }
        inline bool        is_cc()       const { return (type() == MIDI_CMD_CONTROL); }
	inline bool        is_note()     const { return (is_note_on() || is_note_off()); }
	inline uint8_t     note()        const { return (_buffer[1]); }
	inline uint8_t     velocity()    const { return (_buffer[2]); }
	inline uint8_t     cc_number()   const { return (_buffer[1]); }
	inline uint8_t     cc_value()    const { return (_buffer[2]); }
	inline const uint8_t* buffer()      const { return _buffer; }
	inline uint8_t*&      buffer()            { return _buffer; }

private:
	double   _time;   /**< Sample index (or beat time) at which event is valid */
	uint32_t _size;   /**< Number of uint8_ts of data in \a buffer */
	uint8_t*    _buffer; /**< Raw MIDI data */

#ifdef MIDI_EVENT_ALLOW_ALLOC
	bool _owns_buffer; /**< Whether buffer is locally allocated */
#endif
};


}

#endif /* __libmidipp_midi_event_h__ */
