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

#include <gtkmm/cellrenderercombo.h>

#include "pbd/file_utils.h"

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ui_config.h"
#include "midi_tracker_editor.h"
#include "note_player.h"
#include "tooltips.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using Timecode::BBT_Time;

///////////////////////
// MidiTrackerMatrix //
///////////////////////

MidiTrackerMatrix::MidiTrackerMatrix(ARDOUR::Session* session,
                                     boost::shared_ptr<ARDOUR::MidiRegion> region,
                                     boost::shared_ptr<ARDOUR::MidiModel> midi_model,
                                     uint16_t rpb)
	: _session(session), _region(region), _midi_model(midi_model),
	  _conv(_session->tempo_map(), _region->position())
{
	set_rows_per_beat(rpb);
	update_matrix();
}

void MidiTrackerMatrix::set_rows_per_beat(uint16_t rpb)
{
	rows_per_beat = rpb;
	beats_per_row = Evoral::Beats(1.0 / rows_per_beat);
	_ticks_per_row = BBT_Time::ticks_per_beat/rows_per_beat;
}

void MidiTrackerMatrix::update_matrix()
{
	first_beats = find_first_row_beats();
	last_beats = find_last_row_beats();
	nrows = find_nrows();

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
			Evoral::Beats on_time = (*inote)->time();
			Evoral::Beats off_time = (*inote)->end_time();
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

Evoral::Beats MidiTrackerMatrix::find_first_row_beats()
{	
	return _conv.from (_region->first_frame()).snap_to (beats_per_row);
}

Evoral::Beats MidiTrackerMatrix::find_last_row_beats()
{
	return _conv.from (_region->last_frame()).snap_to (beats_per_row);
}

uint32_t MidiTrackerMatrix::find_nrows()
{
	return (last_beats - first_beats).to_double() * rows_per_beat;
}

framepos_t MidiTrackerMatrix::frame_at_row(uint32_t irow)
{
	return _conv.to (beats_at_row(irow));
}

Evoral::Beats MidiTrackerMatrix::beats_at_row(uint32_t irow)
{
	return first_beats + (irow*1.0) / rows_per_beat;
}

uint32_t MidiTrackerMatrix::row_at_beats(Evoral::Beats beats)
{
	Evoral::Beats half_row(0.5/rows_per_beat);
	return (beats - first_beats + half_row).to_double() * rows_per_beat;
}

uint32_t MidiTrackerMatrix::row_at_beats_min_delay(Evoral::Beats beats)
{
	Evoral::Beats tpr_minus_1 = Evoral::Beats::ticks(_ticks_per_row - 1);
	return (beats - first_beats + tpr_minus_1).to_double() * rows_per_beat;
}

uint32_t MidiTrackerMatrix::row_at_beats_max_delay(Evoral::Beats beats)
{
	return (beats - first_beats).to_double() * rows_per_beat;
}
