/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_EVENT_HPP
#define EVORAL_EVENT_HPP

#include <stdint.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <evoral/midi_events.h>
#ifdef EVENT_WITH_XML
	#include <pbd/xml++.h>
#endif

/** If this is not defined, all methods of MidiEvent are RT safe
 * but MidiEvent will never deep copy and (depending on the scenario)
 * may not be usable in STL containers, signals, etc. 
 */
#define EVENT_ALLOW_ALLOC 1

//#define EVENT_WITH_XML

namespace Evoral {


/** Identical to jack_midi_event_t, but with double timestamp
 *
 * time is either a frame time (from/to Jack) or a beat time (internal
 * tempo time, used in MidiModel) depending on context.
 */
struct Event {
#ifdef EVENT_ALLOW_ALLOC
	Event(double t=0, uint32_t s=0, uint8_t* b=NULL, bool owns_buffer=false);
	
	/** Copy \a copy.
	 * 
	 * If \a owns_buffer is true, the buffer will be copied and this method
	 * is NOT REALTIME SAFE.  Otherwise both events share a buffer and
	 * memory management semantics are the caller's problem.
	 */
	Event(const Event& copy, bool owns_buffer);
	
#ifdef EVENT_WITH_XML
	/** Event from XML ala http://www.midi.org/dtds/MIDIEvents10.dtd
	 */
	Event(const XMLNode& event);
	
	/** Event to XML ala http://www.midi.org/dtds/MIDIEvents10.dtd
	 */
	boost::shared_ptr<XMLNode> to_xml() const;
#endif
	
	~Event();

	inline const Event& operator=(const Event& copy) {
		_time = copy._time;
		if (_owns_buffer) {
			if (copy._buffer) {
				if (copy._size > _size) {
					_buffer = (uint8_t*)::realloc(_buffer, copy._size);
				}
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

	inline void shallow_copy(const Event& copy) {
		if (_owns_buffer) {
			free(_buffer);
			_buffer = false;
			_owns_buffer = false;
		}

		_time = copy._time;
		_size = copy._size;
		_buffer = copy._buffer;
	}
	
	inline void set(uint8_t* buf, size_t size, double t) {
		if (_owns_buffer) {
			if (_size < size) {
				_buffer = (uint8_t*) ::realloc(_buffer, size);
			}
			memcpy (_buffer, buf, size);
		} else {
			_buffer = buf;
		}

		_size = size;
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
	
	inline void set_buffer(size_t size, uint8_t* buf, bool own) {
		if (_owns_buffer) {
			free(_buffer);
			_buffer = NULL;
		}
		_size = size;
		_buffer = buf;
		_owns_buffer = own;
	}

	inline void realloc(size_t size) {
		if (_owns_buffer) {
			if (size > _size)
				_buffer = (uint8_t*) ::realloc(_buffer, size);
		} else {
			_buffer = (uint8_t*) ::malloc(size);
			_owns_buffer = true;
		}

		_size = size;
	}


#else

	inline void set_buffer(uint8_t* buf) { _buffer = buf; }

#endif // EVENT_ALLOW_ALLOC

	inline double      time()                  const { return _time; }
	inline double&     time()                        { return _time; }
	inline uint32_t    size()                  const { return _size; }
	inline uint32_t&   size()                        { return _size; }
	inline uint8_t     type()                  const { return (_buffer[0] & 0xF0); }
	inline void        set_type(uint8_t type)        { _buffer[0] =   (0x0F & _buffer[0])
	                                                                | (0xF0 & type); }
	inline uint8_t     channel()               const { return (_buffer[0] & 0x0F); }
	inline void        set_channel(uint8_t channel)  { _buffer[0] =   (0xF0 & _buffer[0])
	                                                                | (0x0F & channel); }
	inline bool        is_note_on()            const { return (type() == MIDI_CMD_NOTE_ON); }
	inline bool        is_note_off()           const { return (type() == MIDI_CMD_NOTE_OFF); }
	inline bool        is_cc()                 const { return (type() == MIDI_CMD_CONTROL); }
	inline bool        is_pitch_bender()       const { return (type() == MIDI_CMD_BENDER); }
	inline bool        is_pgm_change()         const { return (type() == MIDI_CMD_PGM_CHANGE); }
	inline bool        is_note()               const { return (is_note_on() || is_note_off()); }
	inline bool        is_aftertouch()         const { return (type() == MIDI_CMD_NOTE_PRESSURE); }
	inline bool        is_channel_aftertouch() const { return (type() == MIDI_CMD_CHANNEL_PRESSURE); }
	inline uint8_t     note()                  const { return (_buffer[1]); }
	inline uint8_t     velocity()              const { return (_buffer[2]); }
	inline uint8_t     cc_number()             const { return (_buffer[1]); }
	inline uint8_t     cc_value()              const { return (_buffer[2]); }
	inline uint8_t     pitch_bender_lsb()      const { return (_buffer[1]); }
	inline uint8_t     pitch_bender_msb()      const { return (_buffer[2]); }
	inline uint16_t    pitch_bender_value()    const { return ( ((0x7F & _buffer[2]) << 7)
	                                                           | (0x7F & _buffer[1]) ); }
	inline uint8_t     pgm_number()            const { return (_buffer[1]); }
	inline void        set_pgm_number(uint8_t number){ _buffer[1] = number; }
	inline uint8_t     aftertouch()            const { return (_buffer[1]); }
	inline uint8_t     channel_aftertouch()    const { return (_buffer[1]); }
	inline bool        is_channel_event()      const { return (0x80 <= type()) && (type() <= 0xE0);	}
	inline bool        is_smf_meta_event()     const { return _buffer[0] == 0xFF; }
	inline bool        is_sysex()              const { return    _buffer[0] == 0xF0
	                                                          || _buffer[0] == 0xF7; }
	inline const uint8_t* buffer()             const { return _buffer; }
	inline uint8_t*&      buffer()                   { return _buffer; }

private:
	double   _time;   /**< Sample index (or beat time) at which event is valid */
	uint32_t _size;   /**< Number of uint8_ts of data in \a buffer */
	uint8_t* _buffer; /**< Raw MIDI data */

#ifdef EVENT_ALLOW_ALLOC
	bool _owns_buffer; /**< Whether buffer is locally allocated */
#endif
};


} // namespace Evoral

#endif // EVORAL_EVENT_HPP

