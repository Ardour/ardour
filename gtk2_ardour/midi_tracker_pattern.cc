/*
    Copyright (C) 2015 Nil Geisweiller

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

#include <cmath>
#include <map>

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "midi_tracker_pattern.h"

using namespace std;
using namespace ARDOUR;
using Timecode::BBT_Time;

////////////////////////
// MidiTrackerPattern //
////////////////////////

MidiTrackerPattern::MidiTrackerPattern(ARDOUR::Session* session,
                                       boost::shared_ptr<ARDOUR::MidiRegion> region,
                                       boost::shared_ptr<ARDOUR::MidiModel> midi_model)
	: TrackerPattern(session, region), _midi_model(midi_model)
{
}

void MidiTrackerPattern::update_pattern()
{
	set_row_range();

	// Distribute the notes across N tracks so that no overlapping notes can
	// exist on the same track. When a note on hits, it is placed on the first
	// available track, ordered by vector index. In case several notes on are
	// hit simultaneously, then the lowest pitch one is placed on the first
	// available track, ordered by vector index.
	const MidiModel::Notes& notes = _midi_model->notes();
	MidiModel::StrictNotes strict_notes(notes.begin(), notes.end());
	std::vector<MidiModel::Notes> notes_per_track;
	for (MidiModel::StrictNotes::const_iterator note = strict_notes.begin();
	     note != strict_notes.end(); ++note) {
		int freetrack = -1;		// index of the first free track
		for (int i = 0; i < (int)notes_per_track.size(); i++) {
			if ((*notes_per_track[i].rbegin())->end_time() <= (*note)->time()) {
				freetrack = i;
				break;
			}
		}
		// No free track found, create a new one.
		if (freetrack < 0) {
			freetrack = notes_per_track.size();
			notes_per_track.push_back(MidiModel::Notes());
		}
		// Insert the note in the first free track
		notes_per_track[freetrack].insert(*note);
	}
	ntracks = notes_per_track.size();

	notes_on.clear();
	notes_on.resize(ntracks);
	notes_off.clear();
	notes_off.resize(ntracks);

	for (uint16_t itrack = 0; itrack < ntracks; ++itrack) {
		for (MidiModel::Notes::iterator inote = notes_per_track[itrack].begin();
		     inote != notes_per_track[itrack].end(); ++inote) {
			Evoral::Beats on_time = (*inote)->time() + first_beats;
			Evoral::Beats off_time = (*inote)->end_time() + first_beats;
			uint32_t row_on_max_delay = row_at_beats_max_delay(on_time);
			uint32_t row_on = row_at_beats(on_time);
			uint32_t row_off_min_delay = row_at_beats_min_delay(off_time);
			uint32_t row_off = row_at_beats(off_time);

			if (row_on == row_off && row_on != row_off_min_delay) {
				notes_on[itrack].insert(RowToNotes::value_type(row_on, *inote));
				notes_off[itrack].insert(RowToNotes::value_type(row_off_min_delay, *inote));
			} else if (row_on == row_off && row_on_max_delay != row_off) {
				notes_on[itrack].insert(RowToNotes::value_type(row_on_max_delay, *inote));
				notes_off[itrack].insert(RowToNotes::value_type(row_off, *inote));
			} else {
				notes_on[itrack].insert(RowToNotes::value_type(row_on, *inote));
				notes_off[itrack].insert(RowToNotes::value_type(row_off, *inote));
			}
		}
	}
}
