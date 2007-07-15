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

namespace ARDOUR {


/** Identical to jack_midi_event_t, but with double timestamp
 *
 * time is either a frame time (from/to Jack) or a beat time (internal
 * tempo time, used in MidiModel) depending on context.
 */
struct MidiEvent {
	MidiEvent(double t=0, size_t s=0, Byte* b=NULL)
		: time(t), size(s), buffer(b)
	{}

	inline uint8_t type()     const { return (buffer[0] & 0xF0); }
	inline uint8_t note()     const { return (buffer[1]); }
	inline uint8_t velocity() const { return (buffer[2]); }

	double time;   /**< Sample index (or beat time) at which event is valid */
	size_t size;   /**< Number of bytes of data in \a buffer */
	Byte*  buffer; /**< Raw MIDI data */
};


} // namespace ARDOUR

#endif /* __ardour_midi_event_h__ */
