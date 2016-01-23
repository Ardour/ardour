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
	: rows_per_beat(rpb), beats_per_row(1.0/rows_per_beat),
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

///////////////////////
// MidiTrackerEditor //
///////////////////////

static const gchar *_beats_per_row_strings[] = {
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/28"),
	N_("Beats/24"),
	N_("Beats/20"),
	N_("Beats/16"),
	N_("Beats/14"),
	N_("Beats/12"),
	N_("Beats/10"),
	N_("Beats/8"),
	N_("Beats/7"),
	N_("Beats/6"),
	N_("Beats/5"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats/2"),
	N_("Beats"),
	0
};

#define COMBO_TRIANGLE_WIDTH 25

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

	// Beats per row combo
	beats_per_row_strings =  I18N (_beats_per_row_strings);
	build_beats_per_row_menu ();

	register_actions ();

	setup_tooltips ();
	setup_toolbar ();
	setup_matrix ();
	setup_scroller ();
	set_beats_per_row_to (SnapToBeatDiv4);

	redisplay_model ();

	midi_model->ContentsChanged.connect (content_connection, invalidator (*this),
	                                     boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());

	vbox.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (toolbar, false, false);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

MidiTrackerEditor::~MidiTrackerEditor ()
{
}

void
MidiTrackerEditor::register_actions ()
{
	Glib::RefPtr<ActionGroup> beats_per_row_actions = ActionGroup::create (X_("BeatsPerRow"));
	RadioAction::Group beats_per_row_choice_group;

	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-onetwentyeighths"), _("Beats Per Row to One Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv128)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixtyfourths"), _("Beats Per Row to Sixty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv64)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirtyseconds"), _("Beats Per Row to Thirty Seconds"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv32)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyeighths"), _("Beats Per Row to Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv28)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyfourths"), _("Beats Per Row to Twenty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv24)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentieths"), _("Beats Per Row to Twentieths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv20)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-asixteenthbeat"), _("Beats Per Row to Sixteenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv16)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fourteenths"), _("Beats Per Row to Fourteenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv14)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twelfths"), _("Beats Per Row to Twelfths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv12)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-tenths"), _("Beats Per Row to Tenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv10)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-eighths"), _("Beats Per Row to Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv8)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sevenths"), _("Beats Per Row to Sevenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv7)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixths"), _("Beats Per Row to Sixths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv6)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fifths"), _("Beats Per Row to Fifths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv5)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-quarters"), _("Beats Per Row to Quarters"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv4)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirds"), _("Beats Per Row to Thirds"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv3)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-halves"), _("Beats Per Row to Halves"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv2)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-beat"), _("Beats Per Row to Beat"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeat)));

	ActionManager::add_action_group (beats_per_row_actions);
}

void
MidiTrackerEditor::redisplay_model ()
{
	view.set_model (Glib::RefPtr<Gtk::ListStore>(0));
	model->clear ();

	if (_session) {

		MidiTrackerMatrix mtm(_session, region, midi_model, rows_per_beat);
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

void
MidiTrackerEditor::setup_matrix ()
{
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

	view.show ();
}

void
MidiTrackerEditor::setup_toolbar ()
{
	beats_per_row_selector.show ();
	toolbar.pack_start (beats_per_row_selector, false, false);
	toolbar.show ();
}

void
MidiTrackerEditor::setup_scroller ()
{
	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	scroller.show ();
}

void
MidiTrackerEditor::build_beats_per_row_menu ()
{
	using namespace Menu_Helpers;

	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv128 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv128)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv64 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv64)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv32 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv32)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv28 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv28)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv24 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv24)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv20 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv20)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv16 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv16)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv14 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv14)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv12 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv12)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv10 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv10)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv8 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv8)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv7 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv7)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv6 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv6)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv5 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv5)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv4 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv4)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv3 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv3)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv2 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv2)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeat - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeat)));

	set_size_request_to_display_given_text (beats_per_row_selector, beats_per_row_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
MidiTrackerEditor::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats Per Row"));
}

void MidiTrackerEditor::set_beats_per_row_to (SnapType st)
{
	unsigned int snap_ind = (int)st - (int)SnapToBeatDiv128;

	string str = beats_per_row_strings[snap_ind];

	if (str != beats_per_row_selector.get_text()) {
		beats_per_row_selector.set_text (str);
	}

	switch (st) {
	case SnapToBeatDiv128: rows_per_beat = 128; break;
	case SnapToBeatDiv64: rows_per_beat = 64; break;
	case SnapToBeatDiv32: rows_per_beat = 32; break;
	case SnapToBeatDiv28: rows_per_beat = 28; break;
	case SnapToBeatDiv24: rows_per_beat = 24; break;
	case SnapToBeatDiv20: rows_per_beat = 20; break;
	case SnapToBeatDiv16: rows_per_beat = 16; break;
	case SnapToBeatDiv14: rows_per_beat = 14; break;
	case SnapToBeatDiv12: rows_per_beat = 12; break;
	case SnapToBeatDiv10: rows_per_beat = 10; break;
	case SnapToBeatDiv8: rows_per_beat = 8; break;
	case SnapToBeatDiv7: rows_per_beat = 7; break;
	case SnapToBeatDiv6: rows_per_beat = 6; break;
	case SnapToBeatDiv5: rows_per_beat = 5; break;
	case SnapToBeatDiv4: rows_per_beat = 4; break;
	case SnapToBeatDiv3: rows_per_beat = 3; break;
	case SnapToBeatDiv2: rows_per_beat = 2; break;
	case SnapToBeat: rows_per_beat = 1; break;
	default:
		/* relax */
		break;
	}

	redisplay_model ();
}

void MidiTrackerEditor::beats_per_row_selection_done (SnapType snaptype)
{
	RefPtr<RadioAction> ract = beats_per_row_action (snaptype);
	if (ract) {
		ract->set_active ();
	}
}

RefPtr<RadioAction>
MidiTrackerEditor::beats_per_row_action (SnapType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::SnapToBeatDiv128:
		action = "beats-per-row-onetwentyeighths";
		break;
	case Editing::SnapToBeatDiv64:
		action = "beats-per-row-sixtyfourths";
		break;
	case Editing::SnapToBeatDiv32:
		action = "beats-per-row-thirtyseconds";
		break;
	case Editing::SnapToBeatDiv28:
		action = "beats-per-row-twentyeighths";
		break;
	case Editing::SnapToBeatDiv24:
		action = "beats-per-row-twentyfourths";
		break;
	case Editing::SnapToBeatDiv20:
		action = "beats-per-row-twentieths";
		break;
	case Editing::SnapToBeatDiv16:
		action = "beats-per-row-asixteenthbeat";
		break;
	case Editing::SnapToBeatDiv14:
		action = "beats-per-row-fourteenths";
		break;
	case Editing::SnapToBeatDiv12:
		action = "beats-per-row-twelfths";
		break;
	case Editing::SnapToBeatDiv10:
		action = "beats-per-row-tenths";
		break;
	case Editing::SnapToBeatDiv8:
		action = "beats-per-row-eighths";
		break;
	case Editing::SnapToBeatDiv7:
		action = "beats-per-row-sevenths";
		break;
	case Editing::SnapToBeatDiv6:
		action = "beats-per-row-sixths";
		break;
	case Editing::SnapToBeatDiv5:
		action = "beats-per-row-fifths";
		break;
	case Editing::SnapToBeatDiv4:
		action = "beats-per-row-quarters";
		break;
	case Editing::SnapToBeatDiv3:
		action = "beats-per-row-thirds";
		break;
	case Editing::SnapToBeatDiv2:
		action = "beats-per-row-halves";
		break;
	case Editing::SnapToBeat:
		action = "beats-per-row-beat";
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible beats-per-row", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("BeatsPerRow"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "MidiTrackerEditor::beats_per_row_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
MidiTrackerEditor::beats_per_row_chosen (SnapType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = beats_per_row_action (type);

	if (ract && ract->get_active()) {
		set_beats_per_row_to (type);
	}
}
