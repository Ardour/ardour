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

#ifndef EVORAL_NOTE_HPP 
#define EVORAL_NOTE_HPP

#include <stdint.h>
#include <evoral/MIDIEvent.hpp>

namespace Evoral {


/** An abstract (protocol agnostic) note.
 *
 * Currently a note is defined as (on event, length, off event).
 */
class Note {
public:
	Note(uint8_t chan=0, EventTime time=0, EventLength len=0, uint8_t note=0, uint8_t vel=0x40);
	Note(const Note& copy);
	~Note();

	const Note& operator=(const Note& copy);

	inline bool operator==(const Note& other) {
		return time() == other.time() && 
	         note() == other.note() && 
	         length() == other.length() &&
	         velocity() == other.velocity() &&
	         channel()  == other.channel();
	}

	inline EventTime   time()     const { return _on_event.time(); }
	inline EventTime   end_time() const { return _off_event.time(); }
	inline uint8_t     note()     const { return _on_event.note(); }
	inline uint8_t     velocity() const { return _on_event.velocity(); }
	inline EventLength length()   const { return _off_event.time() - _on_event.time(); }
	inline uint8_t     channel()  const {
		assert(_on_event.channel() == _off_event.channel()); 
	    return _on_event.channel(); 
	}

	inline void set_time(EventTime t)     { _off_event.time() = t + length(); _on_event.time() = t; }
	inline void set_note(uint8_t n)       { _on_event.buffer()[1] = n; _off_event.buffer()[1] = n; }
	inline void set_velocity(uint8_t n)   { _on_event.buffer()[2] = n; }
	inline void set_length(EventLength l) { _off_event.time() = _on_event.time() + l; }
	inline void set_channel(uint8_t c)    { _on_event.set_channel(c);  _off_event.set_channel(c); }

	inline       Event& on_event()        { return _on_event; }
	inline const Event& on_event()  const { return _on_event; }
	inline       Event& off_event()       { return _off_event; }
	inline const Event& off_event() const { return _off_event; }

private:
	// Event buffers are self-contained
	MIDIEvent _on_event;
	MIDIEvent _off_event;
};


} // namespace Evoral

#endif // EVORAL_NOTE_HPP

