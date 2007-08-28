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

#ifndef __ardour_midi_event_h__
#define __ardour_midi_event_h__

#include <ardour/types.h>
#include <ardour/midi_events.h>
#include <stdint.h>

/** If this is not defined, all methods of MidiEvent are RT safe
 * but MidiEvent will never deep copy and (depending on the scenario)
 * may not be usable in STL containers, signals, etc. */
#define MIDI_EVENT_ALLOW_ALLOC 1

namespace ARDOUR {


/** Identical to jack_midi_event_t, but with double timestamp
 *
 * time is either a frame time (from/to Jack) or a beat time (internal
 * tempo time, used in MidiModel) depending on context.
 */
struct MidiEvent {
#ifdef MIDI_EVENT_ALLOW_ALLOC
	MidiEvent(double t=0, uint32_t s=0, Byte* b=NULL, bool owns_buffer=false)
		: _time(t)
		, _size(s)
		, _buffer(b)
		, _owns_buffer(owns_buffer)
	{
		if (owns_buffer) {
			_buffer = (Byte*)malloc(_size);
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
	MidiEvent(const MidiEvent& copy, bool owns_buffer)
		: _time(copy._time)
		, _size(copy._size)
		, _buffer(copy._buffer)
		, _owns_buffer(owns_buffer)
	{
		if (owns_buffer) {
			_buffer = (Byte*)malloc(_size);
			if (copy._buffer)
				memcpy(_buffer, copy._buffer, _size);
			else
				memset(_buffer, 0, _size);
		}
	}
	
	~MidiEvent() {
		if (_owns_buffer)
			free(_buffer);
	}

	inline const MidiEvent& operator=(const MidiEvent& copy) {
		_time = copy._time;
		if (!_owns_buffer) {
			_buffer = copy._buffer;
		} else if (copy._buffer) {
			if (!_buffer || _size < copy._size)
				_buffer = (Byte*)realloc(_buffer, copy._size);
			memcpy(_buffer, copy._buffer, copy._size);
		} else {
			free(_buffer);
			_buffer = NULL;
		}
		_size = copy._size;
		return *this;
	}

	inline bool owns_buffer() const { return _owns_buffer; }
	
	inline void set_buffer(Byte* buf) {
		if (_owns_buffer) {
			free(_buffer);
			_buffer = NULL;
		}
		_buffer = buf;
		_owns_buffer = false;
	}

#else

	inline void set_buffer(Byte* buf) { _buffer = buf; }

#endif // MIDI_EVENT_ALLOW_ALLOC

	inline double      time()        const { return _time; }
	inline double&     time()              { return _time; }
	inline uint32_t    size()        const { return _size; }
	inline uint32_t&   size()              { return _size; }
	inline uint8_t     type()        const { return (_buffer[0] & 0xF0); }
	inline uint8_t     channel()     const { return (_buffer[0] & 0x0F); }
	inline bool        is_note_on()  const { return (type() == MIDI_CMD_NOTE_ON); }
	inline bool        is_note_off() const { return (type() == MIDI_CMD_NOTE_OFF); }
	inline bool        is_cc()       const { return (type() == MIDI_CMD_CONTROL); }
	inline bool        is_note()     const { return (is_note_on() || is_note_off()); }
	inline uint8_t     note()        const { return (_buffer[1]); }
	inline uint8_t     velocity()    const { return (_buffer[2]); }
	inline uint8_t     cc_number()   const { return (_buffer[1]); }
	inline uint8_t     cc_value()    const { return (_buffer[2]); }
	inline const Byte* buffer()      const { return _buffer; }
	inline Byte*&      buffer()           { return _buffer; }

private:
	double   _time;   /**< Sample index (or beat time) at which event is valid */
	uint32_t _size;   /**< Number of bytes of data in \a buffer */
	Byte*    _buffer; /**< Raw MIDI data */

#ifdef MIDI_EVENT_ALLOW_ALLOC
	bool _owns_buffer; /**< Whether buffer is locally allocated */
#endif
};


} // namespace ARDOUR

#endif /* __ardour_midi_event_h__ */
