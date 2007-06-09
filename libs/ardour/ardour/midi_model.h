
/*
    Copyright (C) 2006 Paul Davis
	Written by Dave Robillard, 2006

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

#ifndef __ardour_midi_model_h__ 
#define __ardour_midi_model_h__

#include <boost/utility.hpp>
#include <ardour/types.h>
#include <ardour/buffer.h>

namespace ARDOUR {

/** A dynamically resizable collection of MIDI events, sorted by time
 */
class MidiModel : public boost::noncopyable {
public:
	MidiModel(size_t size=0);
	~MidiModel();

	void clear() { _events.clear(); }

	/** Resizes vector if necessary (NOT realtime safe) */
	void append(const MidiBuffer& data);
	
	/** Resizes vector if necessary (NOT realtime safe) */
	void append(double time, size_t size, Byte* in_buffer);
	
	inline const MidiEvent& event_at(unsigned i) const { return _events[i]; }

	inline size_t n_events() const { return _events.size(); }

private:
	std::vector<MidiEvent> _events;
};

} /* namespace ARDOUR */

#endif /* __ardour_midi_model_h__ */

