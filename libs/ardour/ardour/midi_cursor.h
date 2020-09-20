/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_midi_cursor_h__
#define __ardour_midi_cursor_h__

#include <set>

#include <boost/utility.hpp>



#include "pbd/signals.h"

#include "temporal/beats.h"
#include "evoral/Sequence.h"

#include "ardour/types.h"

namespace ARDOUR {

struct MidiCursor : public boost::noncopyable {
	MidiCursor()  {}

	void connect(PBD::Signal1<void, bool>& invalidated) {
		connections.drop_connections();
		invalidated.connect_same_thread(
			connections, boost::bind(&MidiCursor::invalidate, this, _1));
	}

	void invalidate(bool preserve_notes) {
		iter.invalidate(preserve_notes ? &active_notes : NULL);
		last_read_end = 0;
	}

	Evoral::Sequence<Temporal::Beats>::const_iterator        iter;
	std::set<Evoral::Sequence<Temporal::Beats>::WeakNotePtr> active_notes;
	timepos_t                                                last_read_end;
	PBD::ScopedConnectionList                                connections;
};

}

#endif /* __ardour_midi_cursor_h__ */
