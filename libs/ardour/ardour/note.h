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

#ifndef __ardour_note_h__ 
#define __ardour_note_h__

#include <stdint.h>
#include <midi++/event.h>

namespace ARDOUR {


/** A MIDI Note.
 *
 * A note is (unfortunately) special and not just another MIDI::Event as it
 * has a duration and two separate MIDI events (on and off).
 */
class Note {
public:
	Note(uint8_t chan=0, double time=0, double dur=0, uint8_t note=0, uint8_t vel=0x40);
	Note(const Note& copy);
	~Note();

	const Note& operator=(const Note& copy);

	inline bool operator==(const Note& other)
	{ return time() == other.time() && 
	         note() == other.note() && 
	         duration() == other.duration() &&
	         velocity() == other.velocity() &&
	         channel()  == other.channel();
	}

	inline double  time()     const { return _on_event.time(); }
	inline double  end_time() const { return _off_event.time(); }
	inline uint8_t note()     const { return _on_event.note(); }
	inline uint8_t velocity() const { return _on_event.velocity(); }
	inline double  duration() const { return _off_event.time() - _on_event.time(); }
	inline uint8_t channel()  const { 
		assert(_on_event.channel() == _off_event.channel()); 
	    return _on_event.channel(); 
	}

	inline void set_time(double t)      { _off_event.time() = t + duration(); _on_event.time() = t; }
	inline void set_note(uint8_t n)     { _on_event.buffer()[1] = n; _off_event.buffer()[1] = n; }
	inline void set_velocity(uint8_t n) { _on_event.buffer()[2] = n; }
	inline void set_duration(double d)  { _off_event.time() = _on_event.time() + d; }
	inline void set_channel(uint8_t c)  { _on_event.set_channel(c);  _off_event.set_channel(c); }

	inline MIDI::Event& on_event()  { return _on_event; }
	inline MIDI::Event& off_event() { return _off_event; }

	inline const MIDI::Event& on_event()  const { return _on_event; }
	inline const MIDI::Event& off_event() const { return _off_event; }

private:
	// Event buffers are self-contained
	MIDI::Event _on_event;
	MIDI::Event _off_event;
};


} // namespace ARDOUR

#endif /* __ardour_note_h__ */
