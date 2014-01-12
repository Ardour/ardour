/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#include <algorithm>
#include <glib.h>
#include <stdint.h>

#include "evoral/visibility.h"
#include "evoral/MIDIEvent.hpp"

namespace Evoral {

/** An abstract (protocol agnostic) note.
 *
 * Currently a note is defined as (on event, length, off event).
 */
template<typename Time>
class /*LIBEVORAL_API*/ Note {
public:
	Note(uint8_t chan=0, Time time=0, Time len=0, uint8_t note=0, uint8_t vel=0x40);
	Note(const Note<Time>& copy);
	~Note();

	const Note<Time>& operator=(const Note<Time>& copy);

	inline bool operator==(const Note<Time>& other) {
		return musical_time_equal (time(), other.time()) &&
			note() == other.note() &&
			musical_time_equal (length(), other.length()) &&
			velocity() == other.velocity() &&
			off_velocity() == other.off_velocity() &&
			channel()  == other.channel();
	}

	inline event_id_t id() const { return _on_event.id(); }
	void set_id (event_id_t);

	inline Time    time()         const { return _on_event.time(); }
	inline Time    end_time()     const { return _off_event.time(); }
	inline uint8_t note()         const { return _on_event.note(); }
	inline uint8_t velocity()     const { return _on_event.velocity(); }
	inline uint8_t off_velocity() const { return _off_event.velocity(); }
	inline Time    length()       const { return _off_event.time() - _on_event.time(); }
	inline uint8_t channel()      const {
		assert(_on_event.channel() == _off_event.channel());
		return _on_event.channel();
	}

private:
	inline int clamp(int val, int low, int high) {
		return std::min (std::max (val, low), high);
	}

public:
	inline void set_time(Time t) {
		_off_event.set_time(t + length());
		_on_event.set_time(t);
	}
	inline void set_note(uint8_t n) {
		const uint8_t nn = clamp(n, 0, 127);
		_on_event.buffer()[1] = nn;
		_off_event.buffer()[1] = nn;
	}
	inline void set_velocity(uint8_t n) {
		_on_event.buffer()[2] = clamp(n, 0, 127);
	}
	inline void set_off_velocity(uint8_t n) {
		_off_event.buffer()[2] = clamp(n, 0, 127);
	}
	inline void set_length(Time l) {
		_off_event.set_time(_on_event.time() + l);
	}
	inline void set_channel(uint8_t c) {
		const uint8_t cc = clamp(c, 0, 16);
		_on_event.set_channel(cc);
		_off_event.set_channel(cc);
	}

	inline       Event<Time>& on_event()        { return _on_event; }
	inline const Event<Time>& on_event()  const { return _on_event; }
	inline       Event<Time>& off_event()       { return _off_event; }
	inline const Event<Time>& off_event() const { return _off_event; }

private:
	// Event buffers are self-contained
	MIDIEvent<Time> _on_event;
	MIDIEvent<Time> _off_event;
};

} // namespace Evoral

template<typename Time>
/*LIBEVORAL_API*/ std::ostream& operator<<(std::ostream& o, const Evoral::Note<Time>& n) {
	o << "Note #" << n.id() << ": pitch = " << (int) n.note()
	  << " @ " << n.time() << " .. " << n.end_time()
	  << " velocity " << (int) n.velocity()
	  << " chn " << (int) n.channel();
	return o;
}

#include "../src/Note.impl"

#endif // EVORAL_NOTE_HPP

