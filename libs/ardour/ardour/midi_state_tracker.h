/*
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_midi_state_tracker_h__
#define __ardour_midi_state_tracker_h__

#include <glibmm/threads.h>

#include "temporal/beats.h"
#include "ardour/midi_buffer.h"

namespace Evoral {
template <typename T> class EventSink;
}

namespace ARDOUR {

class MidiSource;

/** Tracks played notes, so they can be resolved in potential stuck note
 * situations (e.g. looping, transport stop, etc).
 */
class LIBARDOUR_API MidiNoteTracker
{
public:
	MidiNoteTracker();

	void track (const MidiBuffer::const_iterator& from, const MidiBuffer::const_iterator& to);
	void track (const uint8_t* evbuf);
	void add (uint8_t note, uint8_t chn);
	void remove (uint8_t note, uint8_t chn);
	void resolve_notes (MidiBuffer& buffer, samplepos_t time);
	void resolve_notes (Evoral::EventSink<samplepos_t>& buffer, samplepos_t time);
	void resolve_notes (MidiSource& src, const Glib::Threads::Mutex::Lock& lock, Temporal::Beats time);
	void dump (std::ostream&);
	void reset ();
	bool empty() const { return _on == 0; }
	uint16_t on() const { return _on; }
	bool active (uint8_t note, uint8_t channel) {
		return _active_notes[(channel*128)+note] > 0;
	}

	template<typename Time>
	void track (const Evoral::Event<Time>& ev) {
		track (ev.buffer());
	}

private:
	uint8_t  _active_notes[128*16];
	uint16_t _on;
};


} // namespace ARDOUR

#endif // __ardour_midi_state_tracker_h__
