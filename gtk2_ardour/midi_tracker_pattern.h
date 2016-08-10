/*
    Copyleft (C) 2015 Nil Geisweiller

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

#ifndef __ardour_gtk2_midi_tracker_pattern_h_
#define __ardour_gtk2_midi_tracker_pattern_h_

#include <gtkmm/treeview.h>
#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/bindings.h"

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "ardour_dropdown.h"
#include "ardour_window.h"
#include "editing.h"

#include "tracker_pattern.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

/**
 * Data structure holding the pattern of events for the tracker
 * representation. Plus some goodies method to generate a tracker pattern given
 * a midi region.
 */
class MidiTrackerPattern : public TrackerPattern {
public:
	// Holds a note and its associated track number (a maximum of 4096
	// tracks should be more than enough).
	typedef Evoral::Note<Evoral::Beats> NoteType;
	typedef std::multimap<uint32_t, boost::shared_ptr<NoteType> > RowToNotes;
	typedef std::pair<RowToNotes::const_iterator, RowToNotes::const_iterator> NotesRange;

	MidiTrackerPattern(ARDOUR::Session* session,
	                   boost::shared_ptr<ARDOUR::MidiRegion> region,
	                   boost::shared_ptr<ARDOUR::MidiModel> midi_model,
	                   uint16_t rpb);

	// Build or rebuild the pattern (implement TrackerPattern::update_pattern)
	void update_pattern();

	// Number of tracker tracks of that midi track (determined by the number of
	// overlapping notes)
	uint16_t ntracks;

	// Map row index to notes on for each track
	std::vector<RowToNotes> notes_on;

	// Map row index to notes off (basically the same corresponding notes on)
	// for each track
	std::vector<RowToNotes> notes_off;

private:
	boost::shared_ptr<ARDOUR::MidiModel> _midi_model;
};

#endif /* __ardour_gtk2_midi_tracker_pattern_h_ */
