/*
    Copyright (C) 2008 Paul Davis
    Author: Torben Hohn

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

#ifndef __ardour_midi_state_tracker_h__
#define __ardour_midi_state_tracker_h__

#include "ardour/midi_buffer.h"

namespace Evoral {
template <typename T> class EventSink;
}

namespace ARDOUR {

class MidiSource;

/** Tracks played notes, so they can be resolved in potential stuck note
 * situations (e.g. looping, transport stop, etc).
 */
class LIBARDOUR_API MidiStateTracker
{
public:
	MidiStateTracker();

	void track (const MidiBuffer::iterator& from, const MidiBuffer::iterator& to);
	void add (uint8_t note, uint8_t chn);
	void remove (uint8_t note, uint8_t chn);
	void resolve_notes (MidiBuffer& buffer, framepos_t time);
	void resolve_notes (Evoral::EventSink<framepos_t>& buffer, framepos_t time);
	void resolve_notes (MidiSource& src, Evoral::MusicalTime time);
	void dump (std::ostream&);
	void reset ();
	bool empty() const { return _on == 0; }
	uint16_t on() const { return _on; }
	bool active (uint8_t note, uint8_t channel) {
		return _active_notes[(channel*128)+note] > 0;
	}

private:
	void track_note_onoffs(const Evoral::MIDIEvent<MidiBuffer::TimeType>& event);

	uint8_t  _active_notes[128*16];
	uint16_t _on;
};


} // namespace ARDOUR

#endif // __ardour_midi_state_tracker_h__
