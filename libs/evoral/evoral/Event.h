/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#ifndef EVORAL_EVENT_HPP
#define EVORAL_EVENT_HPP

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sstream>
#include <stdint.h>

#include "evoral/midi_events.h"
#include "evoral/types.h"
#include "evoral/visibility.h"

/** If this is not defined, all methods of MidiEvent are RT safe
 * but MidiEvent will never deep copy and (depending on the scenario)
 * may not be usable in STL containers, signals, etc.
 */
#define EVORAL_EVENT_ALLOC 1

namespace Evoral {

LIBEVORAL_API event_id_t event_id_counter();
LIBEVORAL_API event_id_t next_event_id();
LIBEVORAL_API void       init_event_id_counter(event_id_t n);

/** An event (much like a type generic jack_midi_event_t)
 *
 * Template parameter Time is the type of the time stamp used for this event.
 */
template<typename Time>
class LIBEVORAL_API Event {
public:
#ifdef EVORAL_EVENT_ALLOC
	Event(EventType type=NO_EVENT, Time time=Time(), uint32_t size=0, uint8_t* buf=NULL, bool alloc=false);

	Event(EventType type, Time time, uint32_t size, const uint8_t* buf);

	/** Copy \a copy.
	 *
	 * If \a alloc is true, the buffer will be copied and this method
	 * is NOT REALTIME SAFE.  Otherwise both events share a buffer and
	 * memory management semantics are the caller's problem.
	 */
	Event(const Event& copy, bool alloc);

	~Event();

	void assign(const Event& other);

	void set(const uint8_t* buf, uint32_t size, Time t);

	inline bool operator==(const Event& other) const {
		if (_type != other._type || _time != other._time || _size != other._size) {
			return false;
		}
		return !memcmp(_buf, other._buf, _size);
	}

	inline bool operator!=(const Event& other) const { return ! operator==(other); }

	inline bool owns_buffer() const { return _owns_buf; }

	/** set event data (e.g. midi data)
	 * @param size number of bytes
	 * @param buf raw 8bit data
	 * @param own set to true if the buffer owns the data (copy, allocate/free) or false to reference previously allocated data.
	 */
	inline void set_buffer(uint32_t size, uint8_t* buf, bool own) {
		if (_owns_buf) {
			free(_buf);
			_buf = NULL;
		}
		_size     = size;
		_buf      = buf;
		_owns_buf = own;
	}

	inline void realloc(uint32_t size) {
		if (_owns_buf) {
			if (size > _size)
				_buf = (uint8_t*) ::realloc(_buf, size);
		} else {
			_buf = (uint8_t*) ::malloc(size);
			_owns_buf = true;
		}

		_size = size;
	}

	inline void clear() {
		_type = NO_EVENT;
		_time = Time();
		_size = 0;
		_buf  = NULL;
	}

#endif // EVORAL_EVENT_ALLOC

	inline EventType      event_type()    const { return _type; }
	inline Time           time()          const { return _time; }
	inline uint32_t       size()          const { return _size; }
	inline const uint8_t* buffer()        const { return _buf; }
	inline uint8_t*       buffer()              { return _buf; }

	inline bool           is_midi ()      const { return _type == LIVE_MIDI_EVENT || _type == MIDI_EVENT; }
	inline bool           is_live_midi () const { return _type == LIVE_MIDI_EVENT; }

	inline void set_event_type(EventType t) { _type = t; }

	inline void set_time(Time t) { _time = t; }

	inline event_id_t id() const           { return _id; }
	inline void       set_id(event_id_t n) { _id = n; }

	/* The following methods are type specific and only make sense for the
	   correct event type.  It is the caller's responsibility to only call
	   methods which make sense for the given event type.  Currently this means
	   they all only make sense for MIDI, but built-in support may be added for
	   other protocols in the future, or the internal representation may change
	   to be protocol agnostic. */

	uint8_t  type()                const { return _buf[0] & 0xF0; }
	uint8_t  channel()             const { return _buf[0] & 0x0F; }
	bool     is_note_on()          const { return type() == MIDI_CMD_NOTE_ON; }
	bool     is_note_off()         const { return type() == MIDI_CMD_NOTE_OFF; }
	bool     is_note()             const { return is_note_on() || is_note_off(); }
	bool     is_poly_pressure()    const { return type() == MIDI_CMD_NOTE_PRESSURE; }
	bool     is_channel_pressure() const { return type() == MIDI_CMD_CHANNEL_PRESSURE; }
	bool     is_cc()               const { return type() == MIDI_CMD_CONTROL; }
	bool     is_pgm_change()       const { return type() == MIDI_CMD_PGM_CHANGE; }
	bool     is_pitch_bender()     const { return type() == MIDI_CMD_BENDER; }
	bool     is_channel_event()    const { return (0x80 <= type()) && (type() <= 0xE0); }
	bool     is_smf_meta_event()   const { return _buf[0] == 0xFF; }
	bool     is_sysex()            const { return _buf[0] == 0xF0 || _buf[0] == 0xF7; }
	bool     is_spp()              const { return _buf[0] == 0xF2 && size() == 1; }
	bool     is_mtc_quarter()      const { return _buf[0] == 0xF1 && size() == 1; }
	bool     is_mtc_full()         const { return (size() == 10 &&
	                                               _buf[0] == 0xF0 && _buf[1] == 0x7F &&
	                                               _buf[3] == 0x01 && _buf[4] == 0x01); }

	uint8_t  note()               const { return _buf[1]; }
	uint8_t  velocity()           const { return _buf[2]; }
	uint8_t  poly_note()          const { return _buf[1]; }
	uint8_t  poly_pressure()      const { return _buf[2]; }
	uint8_t  channel_pressure()   const { return _buf[1]; }
	uint8_t  cc_number()          const { return _buf[1]; }
	uint8_t  cc_value()           const { return _buf[2]; }
	uint8_t  pgm_number()         const { return _buf[1]; }
	uint8_t  pitch_bender_lsb()   const { return _buf[1]; }
	uint8_t  pitch_bender_msb()   const { return _buf[2]; }
	uint16_t pitch_bender_value() const { return ((0x7F & _buf[2]) << 7 | (0x7F & _buf[1])); }

	void set_channel(uint8_t channel)  { _buf[0] = (0xF0 & _buf[0]) | (0x0F & channel); }
	void set_type(uint8_t type)        { _buf[0] = (0x0F & _buf[0]) | (0xF0 & type); }
	void set_note(uint8_t num)         { _buf[1] = num; }
	void set_velocity(uint8_t val)     { _buf[2] = val; }
	void set_cc_number(uint8_t num)    { _buf[1] = num; }
	void set_cc_value(uint8_t val)     { _buf[2] = val; }
	void set_pgm_number(uint8_t num)   { _buf[1] = num; }

	void scale_velocity (float factor) {
		factor = std::max (factor, 0.0f);
		set_velocity (std::min (127L, lrintf (velocity() * factor)));
	}

	uint16_t value() const {
		switch (type()) {
		case MIDI_CMD_CONTROL:
			return cc_value();
		case MIDI_CMD_BENDER:
			return pitch_bender_value();
		case MIDI_CMD_NOTE_PRESSURE:
			return poly_pressure();
		case MIDI_CMD_CHANNEL_PRESSURE:
			return channel_pressure();
		case MIDI_CMD_PGM_CHANGE:
			return pgm_number();
		default:
			return 0;
		}
	}

protected:
	EventType  _type;      ///< Type of event (application relative, NOT MIDI 'type')
	Time       _time;      ///< Time stamp of event
	uint32_t   _size;      ///< Size of buffer in bytes
	uint8_t*   _buf;       ///< Event contents (e.g. raw MIDI data)
	event_id_t _id;        ///< Unique event ID
#ifdef EVORAL_EVENT_ALLOC
	bool       _owns_buf;  ///< Whether buffer is locally allocated
#endif
};

template<typename Time>
/*LIBEVORAL_API*/ std::ostream& operator<<(std::ostream& o, const Evoral::Event<Time>& ev) {
	o << "Event #" << ev.id() << " type = " << ev.event_type() << " @ " << ev.time();
	o << std::hex;
	for (uint32_t n = 0; n < ev.size(); ++n) {
		o << ' ' << (int) ev.buffer()[n];
	}
	o << std::dec;
	return o;
}

} // namespace Evoral

#endif // EVORAL_EVENT_HPP

