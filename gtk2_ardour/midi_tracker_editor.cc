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

#include "ui_config.h"
#include "midi_tracker_editor.h"
#include "note_player.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using Timecode::BBT_Time;

///////////////////////
// MidiTrackerMatrix //
///////////////////////

MidiTrackerMatrix::MidiTrackerMatrix(ARDOUR::Session* session,
                                     boost::shared_ptr<ARDOUR::MidiRegion> region,
                                     boost::shared_ptr<ARDOUR::MidiModel> midi_model,
                                     uint16_t rpb)
	: rows_per_beat(rpb), snap(1.0/rows_per_beat),
	  _ticks_per_row(BBT_Time::ticks_per_beat/rows_per_beat),
	  _session(session), _region(region), _midi_model(midi_model),
	  _conv(_session->tempo_map(), _region->position())
{
	updateMatrix();
}

void MidiTrackerMatrix::updateMatrix()
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
	return _conv.from (_region->first_frame()).snap_to (snap);
}

Evoral::Beats MidiTrackerMatrix::find_last_row_beats()
{
	return _conv.from (_region->last_frame()).snap_to (snap);
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

///////////////////////
// MidiTrackerEditor //
///////////////////////

const std::string MidiTrackerEditor::note_off_str = "===";
const std::string MidiTrackerEditor::undefined_str = "***";

MidiTrackerEditor::MidiTrackerEditor (ARDOUR::Session* s, boost::shared_ptr<MidiRegion> r, boost::shared_ptr<MidiTrack> tr)
	: ArdourWindow (r->name())
	, region (r)
	, track (tr)
	, midi_model (region->midi_source(0)->model())

{
	/* We do not handle nested sources/regions. Caller should have tackled this */

	if (r->max_source_level() > 0) {
		throw failed_constructor();
	}

	set_session (s);

	edit_column = -1;
	editing_renderer = 0;
	editing_editable = 0;

	model = ListStore::create (columns);
	view.set_model (model);

	Gtk::TreeViewColumn viewcolumn_time (_("Time"), columns.time);
	Gtk::CellRenderer* cellrenderer_time = viewcolumn_time.get_first_cell_renderer ();		
	viewcolumn_time.add_attribute(cellrenderer_time->property_cell_background (), columns._color);
	view.append_column (viewcolumn_time);
	for (size_t i = 0; i < GUI_NUMBER_OF_TRACKS; i++) {
		stringstream ss_note;
		stringstream ss_ch;
		stringstream ss_vel;
		stringstream ss_delay;
		ss_note << "Note" << i;
		ss_ch << "Ch" << i;
		ss_vel << "Vel" << i;
		ss_delay << "Delay" << i;

		Gtk::TreeViewColumn viewcolumn_note (_(ss_note.str().c_str()), columns.note_name[i]);
		Gtk::TreeViewColumn viewcolumn_channel (_(ss_ch.str().c_str()), columns.channel[i]);
		Gtk::TreeViewColumn viewcolumn_velocity (_(ss_vel.str().c_str()), columns.velocity[i]);
		Gtk::TreeViewColumn viewcolumn_delay (_(ss_delay.str().c_str()), columns.delay[i]);

		Gtk::CellRenderer* cellrenderer_note = viewcolumn_note.get_first_cell_renderer ();		
		Gtk::CellRenderer* cellrenderer_channel = viewcolumn_channel.get_first_cell_renderer ();		
		Gtk::CellRenderer* cellrenderer_velocity = viewcolumn_velocity.get_first_cell_renderer ();		
		Gtk::CellRenderer* cellrenderer_delay = viewcolumn_delay.get_first_cell_renderer ();		

		viewcolumn_note.add_attribute(cellrenderer_note->property_cell_background (), columns._color);
		viewcolumn_channel.add_attribute(cellrenderer_channel->property_cell_background (), columns._color);
		viewcolumn_velocity.add_attribute(cellrenderer_velocity->property_cell_background (), columns._color);
		viewcolumn_delay.add_attribute(cellrenderer_delay->property_cell_background (), columns._color);

		view.append_column (viewcolumn_note);
		view.append_column (viewcolumn_channel);
		view.append_column (viewcolumn_velocity);
		view.append_column (viewcolumn_delay);
	}

	view.set_headers_visible (true);
	view.set_rules_hint (true);
	view.set_grid_lines (TREE_VIEW_GRID_LINES_BOTH);
	view.get_selection()->set_mode (SELECTION_MULTIPLE);
	
	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	redisplay_model ();

	midi_model->ContentsChanged.connect (content_connection, invalidator (*this),
	                                     boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());

	view.show ();
	scroller.show ();
	vbox.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

MidiTrackerEditor::~MidiTrackerEditor ()
{
}

void
MidiTrackerEditor::redisplay_model ()
{
	view.set_model (Glib::RefPtr<Gtk::ListStore>(0));
	model->clear ();

	if (_session) {

		MidiTrackerMatrix mtm(_session, region, midi_model, 8 /* TODO: user defined */);
		TreeModel::Row row;
		
		// Generate each row
		for (uint32_t irow = 0; irow < mtm.nrows; irow++) {
			row = *(model->append());
			Evoral::Beats row_beats = mtm.beats_at_row(irow);
			uint32_t row_frame = mtm.frame_at_row(irow);

			// Time
			Timecode::BBT_Time row_bbt;
			_session->tempo_map().bbt_time(row_frame, row_bbt);
			stringstream ss;
			print_padded(ss, row_bbt);
			row[columns.time] = ss.str();

			// If the row is on a beat the color differs
			row[columns._color] = row_beats == row_beats.round_up_to_beat() ?
				"#202020" : "#101010";

			// TODO: don't dismiss off-beat rows near the region boundaries

			for (size_t i = 0; i < (size_t)mtm.ntracks; i++) {

				// // Fill with blank
				// row[columns.channel[i]] = "--";
				// row[columns.note_name[i]] = "----";
				// row[columns.velocity[i]] = "---";
				// row[columns.delay[i]] = "-----";
				
				size_t notes_off_count = mtm.notes_off[i].count(irow);
				size_t notes_on_count = mtm.notes_on[i].count(irow);

				if (notes_on_count > 0 || notes_off_count > 0) {
					MidiTrackerMatrix::RowToNotes::const_iterator i_off = mtm.notes_off[i].find(irow);
					MidiTrackerMatrix::RowToNotes::const_iterator i_on = mtm.notes_on[i].find(irow);

					// Determine whether the row is defined
					bool undefined = (notes_off_count > 1 || notes_on_count > 1)
						|| (notes_off_count == 1 && notes_on_count == 1
						    && i_off->second->end_time() != i_on->second->time());

					if (undefined) {
						row[columns.note_name[i]] = undefined_str;
					} else {
						// Notes off
						MidiTrackerMatrix::RowToNotes::const_iterator i_off = mtm.notes_off[i].find(irow);
						if (i_off != mtm.notes_off[i].end()) {
							boost::shared_ptr<NoteType> note = i_off->second;
							row[columns.channel[i]] = to_string (note->channel() + 1);
							row[columns.note_name[i]] = note_off_str;
							row[columns.velocity[i]] = to_string ((int)note->velocity());
							int64_t delay_ticks = (note->end_time() - row_beats).to_relative_ticks();
							if (delay_ticks != 0)
								row[columns.delay[i]] = to_string(delay_ticks);
						}

						// Notes on
						MidiTrackerMatrix::RowToNotes::const_iterator i_on = mtm.notes_on[i].find(irow);
						if (i_on != mtm.notes_on[i].end()) {
							boost::shared_ptr<NoteType> note = i_on->second;
							row[columns.channel[i]] = to_string (note->channel() + 1);
							row[columns.note_name[i]] = Evoral::midi_note_name (note->note());
							row[columns.velocity[i]] = to_string ((int)note->velocity());
							int64_t delay_ticks = (note->time() - row_beats).to_relative_ticks();
							if (delay_ticks != 0)
								row[columns.delay[i]] = to_string (delay_ticks);
							// Keep the note around for playing it
							row[columns._note[i]] = note;
						}
					}
				}
			}
		}
	}

	view.set_model (model);
}
