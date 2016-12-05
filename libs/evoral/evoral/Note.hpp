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
#include "evoral/Event.hpp"

namespace Evoral {

/** An abstract (protocol agnostic) note.
 *
 * Currently a note is defined as (on event, length, off event).
 */
template<typename Time>
#ifdef COMPILER_MSVC
class LIBEVORAL_LOCAL Note : public MultiEvent<Time,2>
#else
class LIBEVORAL_TEMPLATE_API Note : public MultiEvent<Time,2>
#endif
{
public:
	using MultiEvent<Time,2>::_events; /* C++ template arcana */
	using MultiEvent<Time,2>::set_event;

	Note (EventPointer<Time> const & on_event, EventPointer<Time> const & off_event);
	Note (uint8_t chan, Time time, Time length, uint8_t note, uint8_t velocity = 64);
	Note (Note<Time> const & other);

	~Note();

	const Note<Time>& operator=(const Note<Time>& copy);

	inline bool operator==(const Note<Time>& other) {
		return time() == other.time() &&
			note() == other.note() &&
			length() == other.length() &&
			velocity() == other.velocity() &&
			off_velocity() == other.off_velocity() &&
			channel()  == other.channel();
	}

	inline Time    time()         const { return _events[0]->time(); }
	inline Time    end_time()     const { return _events[1]->time(); }
	inline uint8_t note()         const { return _events[0]->note(); }
	inline uint8_t velocity()     const { return _events[0]->velocity(); }
	inline uint8_t off_velocity() const { return _events[1]->velocity(); }
	inline Time    length()       const { return _events[1]->time() - _events[0]->time(); }
	inline uint8_t channel()      const {
		assert(_events[0]->channel() == _events[1]->channel());
		return _events[0]->channel();
	}

	bool operator== (Note const& other) const {
		/* Compare contents of both on and off events */
		return (*_events[0] == *other.on_event()) &&
			(*_events[1] == *other.off_event());
	}

private:
	inline int clamp(int val, int low, int high) {
		return std::min (std::max (val, low), high);
	}

public:
	inline void set_time(Time t) {
		_events[1]->set_time(t + length());

	}
	inline void set_note(uint8_t n) {
		const uint8_t nn = clamp(n, 0, 127);
		_events[0]->buffer()[1] = nn;
		_events[1]->buffer()[1] = nn;
	}
	inline void set_velocity(uint8_t n) {
		_events[0]->buffer()[2] = clamp(n, 0, 127);
	}
	inline void set_off_velocity(uint8_t n) {
		_events[1]->buffer()[2] = clamp(n, 0, 127);
	}
	inline void set_length(Time l) {
		_events[1]->set_time(_events[0]->time() + l);
	}
	inline void set_channel(uint8_t c) {
		const uint8_t cc = clamp(c, 0, 16);
		_events[0]->set_channel(cc);
		_events[1]->set_channel(cc);
	}

	inline       EventPointer<Time>& on_event()        { return _events[0]; }
	inline const EventPointer<Time>& on_event()  const { return _events[0]; }
	inline       EventPointer<Time>& off_event()       { return _events[1]; }
	inline const EventPointer<Time>& off_event() const { return _events[1]; }
};

template<typename T>
struct LowerNoteValueComparator
{
	bool operator() (T const & thing, uint8_t val) const {
		return thing->note() < val;
	}
	bool operator() (T const & a, T const & b) const {
		return a->note() < b->note();
	}
};

template<typename Time>
/*LIBEVORAL_API*/ std::ostream& operator<<(std::ostream& o, const Evoral::Note<Time>& n) {
	o << "Note #" << n.id() << ": pitch = " << (int) n.note()
	  << " @ " << n.time() << " .. " << n.end_time()
	  << " velocity " << (int) n.velocity()
	  << " chn " << (int) n.channel();
	return o;
}

template<typename Time>
class NotePointer : public MultiEventPointer<Note<Time> >
{
  public:
	NotePointer () {}
	NotePointer (Note<Time>* n) : MultiEventPointer<Note<Time> > (n) {}
	NotePointer (NotePointer const & other);
	NotePointer (uint8_t chan, Time time, Time length, uint8_t note, uint8_t velocity)
		: MultiEventPointer<Note<Time> > (new Note<Time> (chan, time, length, note, velocity))
	{
	}

	~NotePointer () { }

	NotePointer& copy() const {
		/* XXX need pools for all this */
		return *new NotePointer<Time> (new Note<Time> (*(this->get())));
	}
};

} // namespace Evoral

#ifdef COMPILER_MSVC
#include "../src/Note.impl"
#endif

#endif // EVORAL_NOTE_HPP
