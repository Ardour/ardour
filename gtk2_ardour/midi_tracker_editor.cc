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
	, buttons (1, 1)
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

	view.signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::key_press), false);
	view.signal_key_release_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::key_release), false);
	view.signal_scroll_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::scroll_event), false);

	view.append_column (_("Time"), columns.time);
	for (size_t i = 0; i < GUI_NUMBER_OF_TRACKS; i++) {
		stringstream ss_note;
		stringstream ss_ch;
		stringstream ss_vel;
		stringstream ss_delay;
		ss_note << "Note-" << i;
		ss_ch << "Ch-" << i;
		ss_vel << "Vel-" << i;
		ss_delay << "Delay-" << i;
		view.append_column (_(ss_note.str().c_str()), columns.note_name[i]);
		view.append_column (_(ss_ch.str().c_str()), columns.channel[i]);
		view.append_column (_(ss_vel.str().c_str()), columns.velocity[i]);
		view.append_column (_(ss_delay.str().c_str()), columns.delay[i]);
	}

	view.set_headers_visible (true);
	view.set_rules_hint (true);
	view.set_grid_lines (TREE_VIEW_GRID_LINES_BOTH);
	view.get_selection()->set_mode (SELECTION_MULTIPLE);
	view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &MidiTrackerEditor::selection_changed));

	for (int i = 0; i < TRACKER_COLNUM_COUNT; ++i) {
		CellRendererText* renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (i));

		TreeViewColumn* col = view.get_column (i);
		col->set_data (X_("colnum"), GUINT_TO_POINTER(i));

		renderer->property_editable() = true;

		renderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_started), i));
		renderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
		renderer->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::edited));
	}

	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	redisplay_model ();

	midi_model->ContentsChanged.connect (content_connection, invalidator (*this),
	                                     boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());

	buttons.attach (sound_notes_button, 0, 1, 0, 1);
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action ("Editor", "sound-midi-notes");
	if (act) {
		gtk_activatable_set_related_action (GTK_ACTIVATABLE (sound_notes_button.gobj()), act->gobj());	
	}

	view.show ();
	scroller.show ();
	buttons.show ();
	vbox.show ();
	sound_notes_button.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (buttons, false, false);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

MidiTrackerEditor::~MidiTrackerEditor ()
{
}

bool
MidiTrackerEditor::scroll_event (GdkEventScroll* ev)
{
	TreeModel::Path path;
	TreeViewColumn* col;
	int cellx;
	int celly;
	int idelta = 0;
	double fdelta = 0;
	MidiModel::NoteDiffCommand::Property prop (MidiModel::NoteDiffCommand::NoteNumber);
	bool apply = false;
	bool was_selected = false;
	char const * opname;

	if (!view.get_path_at_pos (ev->x, ev->y, path, col, cellx, celly)) {
		return false;
	}
	
	if (view.get_selection()->count_selected_rows() == 0) {
		was_selected = false;
	} else if (view.get_selection()->is_selected (path)) {
		was_selected = true;
	} else {
		was_selected = false;
	}
	
	int colnum = GPOINTER_TO_UINT (col->get_data (X_("colnum")));

	switch (colnum) {
	case TIME_COLNUM:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			fdelta = 1/64.0;
		} else {
			fdelta = 1/4.0;
		}
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			fdelta = -fdelta;
		}
		prop = MidiModel::NoteDiffCommand::StartTime;
		opname = _("edit note time");
		apply = true;
		break;
	case NOTE_COLNUM:
		idelta = 1;
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			idelta = -idelta;
		}
		prop = MidiModel::NoteDiffCommand::NoteNumber;
		opname = _("edit note name");
		apply = true;
		break;
	case CHANNEL_COLNUM:
		idelta = 1;
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			idelta = -idelta;
		}
		prop = MidiModel::NoteDiffCommand::Channel;
		opname = _("edit note channel");
		apply = true;
		break;
	case VELOCITY_COLNUM:
		idelta = 1;
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			idelta = -idelta;
		}
		prop = MidiModel::NoteDiffCommand::Velocity;
		opname = _("edit note velocity");
		apply = true;
		break;

	case DELAY_COLNUM:
		opname = _("edit note delay");
		apply = true;
		break;

	default:
		break;
	}


	if (apply) {

		MidiModel::NoteDiffCommand* cmd = midi_model->new_note_diff_command (opname);
		vector<TreeModel::Path> previous_selection;

		if (was_selected) {

			/* use selection */
			
			TreeView::Selection::ListHandle_Path rows = view.get_selection()->get_selected_rows ();
			TreeModel::iterator iter;
			boost::shared_ptr<NoteType> note;
			
			for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {

				previous_selection.push_back (*i);

				if ((iter = model->get_iter (*i))) {
					
					note = (*iter)[columns._note[0]];
					
					switch (prop) {
					case MidiModel::NoteDiffCommand::StartTime:
						if (note->time() + fdelta >= 0) {
							cmd->change (note, prop, note->time() + fdelta);
						} else {
							cmd->change (note, prop, Evoral::Beats());
						}
						break;
					case MidiModel::NoteDiffCommand::Velocity:
						cmd->change (note, prop, (uint8_t) (note->velocity() + idelta));
						break;
					case MidiModel::NoteDiffCommand::Length:
						if (note->length().to_double() + fdelta >=
						    Evoral::Beats::tick().to_double()) {
							cmd->change (note, prop, note->length() + fdelta);
						} else {
							cmd->change (note, prop, Evoral::Beats::tick());
						}
						break;
					case MidiModel::NoteDiffCommand::Channel:
						cmd->change (note, prop, (uint8_t) (note->channel() + idelta));
						break;
					case MidiModel::NoteDiffCommand::NoteNumber:
						cmd->change (note, prop, (uint8_t) (note->note() + idelta));
						break;
					default:
						continue;
					}
				}
			}

		} else {

			/* just this row */
			
			TreeModel::iterator iter;
			iter = model->get_iter (path);

			previous_selection.push_back (path);

			if (iter) {
				boost::shared_ptr<NoteType> note = (*iter)[columns._note[0]];
				
				switch (prop) {
				case MidiModel::NoteDiffCommand::StartTime:
					if (note->time() + fdelta >= 0) {
						cmd->change (note, prop, note->time() + fdelta);
					} else {
						cmd->change (note, prop, Evoral::Beats());
					}
					break;
				case MidiModel::NoteDiffCommand::Velocity:
					cmd->change (note, prop, (uint8_t) (note->velocity() + idelta));
					break;
				case MidiModel::NoteDiffCommand::Length:
					if (note->length() + fdelta >=
					    Evoral::Beats::tick().to_double()) {
						cmd->change (note, prop, note->length() + fdelta);
					} else {
						cmd->change (note, prop, Evoral::Beats::tick());
					}
					break;
				case MidiModel::NoteDiffCommand::Channel:
					cmd->change (note, prop, (uint8_t) (note->channel() + idelta));
					break;
				case MidiModel::NoteDiffCommand::NoteNumber:
					cmd->change (note, prop, (uint8_t) (note->note() + idelta));
					break;
				default:
					break;
				}
			}
		}

		midi_model->apply_command (*_session, cmd);

		/* reset selection to be as it was before we rebuilt */
		
		for (vector<TreeModel::Path>::iterator i = previous_selection.begin(); i != previous_selection.end(); ++i) {
			view.get_selection()->select (*i);
		}
	}

	return true;
}

bool
MidiTrackerEditor::key_press (GdkEventKey* ev)
{
	bool ret = false;
	TreeModel::Path path;
	TreeViewColumn* col;
	int colnum;

	switch (ev->keyval) {
	case GDK_Tab:
		if (edit_column > 0) {
			colnum = edit_column;
			path = edit_path;
			if (editing_editable) {
				editing_editable->editing_done ();
			}
			if (colnum >= 5) {
				/* wrap to next line */
				colnum = 0;
				path.next();
			} else {
				colnum++;
			}
			col = view.get_column (colnum);
			view.set_cursor (path, *col, true);
			ret = true;
		}
		break;
		
	case GDK_Up:
	case GDK_uparrow:
		if (edit_column > 0) {
			colnum = edit_column;
			path = edit_path;
			if (editing_editable) {
				editing_editable->editing_done ();
			}
			path.prev ();
			col = view.get_column (colnum);
			view.set_cursor (path, *col, true);
			ret = true;
		}
		break;

	case GDK_Down:
	case GDK_downarrow:
		if (edit_column > 0) {
			colnum = edit_column;
			path = edit_path;
			if (editing_editable) {
				editing_editable->editing_done ();
			}
			path.next ();
			col = view.get_column (colnum);
			view.set_cursor (path, *col, true);
			ret = true;
		}
		break;

	case GDK_Escape:
		stop_editing (true);
		break;
		
	}

	return ret;
}

bool
MidiTrackerEditor::key_release (GdkEventKey* ev)
{
	bool ret = false;
	TreeModel::Path path;
	TreeViewColumn* col;
	TreeModel::iterator iter;
	MidiModel::NoteDiffCommand* cmd;
	boost::shared_ptr<NoteType> note;
	boost::shared_ptr<NoteType> copy;

	switch (ev->keyval) {
	case GDK_Insert:
		/* add a new note to the model, based on the note at the cursor
		 * pos
		 */
		view.get_cursor (path, col);
		iter = model->get_iter (path);
		cmd = midi_model->new_note_diff_command (_("insert new note"));
		note = (*iter)[columns._note[0]];
		copy.reset (new NoteType (*note.get()));
		cmd->add (copy);
		midi_model->apply_command (*_session, cmd);
		/* model has been redisplayed by now */
		path.next ();
		/* select, start editing column 2 (note) */
		col = view.get_column (2);
		view.set_cursor (path, *col, true);
		break;

	case GDK_Delete:
	case GDK_BackSpace:
		if (edit_column < 0) {
			delete_selected_note ();
		}
		ret = true;
		break;

	case GDK_z:
		if (_session && Gtkmm2ext::Keyboard::modifier_state_contains (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
			_session->undo (1);
			ret = true;
		}
		break;
		
	case GDK_r:
		if (_session && Gtkmm2ext::Keyboard::modifier_state_contains (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
			_session->redo (1);
			ret = true;
		}
		break;

	default:
		break;
	}

	return ret;
}

void
MidiTrackerEditor::delete_selected_note ()
{
	Glib::RefPtr<TreeSelection> selection = view.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (rows.empty()) {
		return;
	}

	typedef vector<boost::shared_ptr<NoteType> > Notes;
	Notes to_delete;

	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {
		TreeIter iter;

		if ((iter = model->get_iter (*i))) {
			boost::shared_ptr<NoteType> note = (*iter)[columns._note[0]];
			to_delete.push_back (note);
		}
	}

	MidiModel::NoteDiffCommand* cmd = midi_model->new_note_diff_command (_("delete notes (from list)"));

	for (Notes::iterator i = to_delete.begin(); i != to_delete.end(); ++i) {
		cmd->remove (*i);
	}

	midi_model->apply_command (*_session, cmd);
}

void
MidiTrackerEditor::stop_editing (bool cancelled)
{
	if (!cancelled) {
		if (editing_editable) {
			editing_editable->editing_done ();
		}
	} else {
		if (editing_renderer) {
			editing_renderer->stop_editing (cancelled);
		}
	}
}

void
MidiTrackerEditor::editing_started (CellEditable* ed, const string& path, int colno)
{
	edit_path = TreePath (path);
	edit_column = colno;
	editing_renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (colno));
	editing_editable = ed;

	if (ed) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::key_press), false);
			e->signal_key_release_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::key_release), false);
		}
	}
}

void
MidiTrackerEditor::editing_canceled ()
{
	edit_path.clear ();
	edit_column = -1;
	editing_renderer = 0;
	editing_editable = 0;
}

void
MidiTrackerEditor::edited (const std::string& path, const std::string& text)
{
	TreeModel::iterator iter = model->get_iter (path);

	if (!iter || text.empty()) {
		return;
	}

	boost::shared_ptr<NoteType> note = (*iter)[columns._note[0]];
	MidiModel::NoteDiffCommand::Property prop (MidiModel::NoteDiffCommand::NoteNumber);

	int    ival;
	bool   apply = false;
	int    idelta = 0;
	double fdelta = 0;
	char const * opname;
	switch (edit_column) {
	case 0: // start
		break;
	case 1: // channel
		// correct ival for zero-based counting after scan
		if (sscanf (text.c_str(), "%d", &ival) == 1 && --ival != note->channel()) {
			idelta = ival - note->channel();
			prop = MidiModel::NoteDiffCommand::Channel;
			opname = _("change note channel");
			apply = true;
		}
		break;
	case 2: // note
		if (sscanf (text.c_str(), "%d", &ival) == 1 && ival != note->note()) {
			idelta = ival - note->note();
			prop = MidiModel::NoteDiffCommand::NoteNumber;
			opname = _("change note number");
			apply = true;
		}
		break;
	case 3: // name
		break;
	case 4: // velocity
		if (sscanf (text.c_str(), "%d", &ival) == 1 && ival != note->velocity()) {
			idelta = ival - note->velocity();
			prop = MidiModel::NoteDiffCommand::Velocity;
			opname = _("change note velocity");
			apply = true;
		}
		break;
	default:
		break;
	}

	if (apply) {

		MidiModel::NoteDiffCommand* cmd = midi_model->new_note_diff_command (opname);

		TreeView::Selection::ListHandle_Path rows = view.get_selection()->get_selected_rows ();
		
		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {
			if ((iter = model->get_iter (*i))) {

				note = (*iter)[columns._note[0]];
				
				switch (prop) {
				case MidiModel::NoteDiffCommand::Velocity:
					cmd->change (note, prop, (uint8_t) (note->velocity() + idelta));
					break;
				case MidiModel::NoteDiffCommand::Length:
					cmd->change (note, prop, note->length() + fdelta);
					break;
				case MidiModel::NoteDiffCommand::Channel:
					cmd->change (note, prop, (uint8_t) (note->channel() + idelta));
					break;
				case MidiModel::NoteDiffCommand::NoteNumber:
					cmd->change (note, prop, (uint8_t) (note->note() + idelta));
					break;
				default:
					continue;
				}
			}
		}

		midi_model->apply_command (*_session, cmd);

		/* model has been redisplayed by now */
		/* keep selected row(s), move cursor there, don't continue editing */
		
		TreeViewColumn* col = view.get_column (edit_column);
		view.set_cursor (edit_path, *col, 0);

		/* reset edit info, since we're done */

		edit_path.clear ();
		edit_column = -1;
		editing_renderer = 0;
		editing_editable = 0;
	}
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
			// ss << row_bbt;
			print_padded(ss, row_bbt);
			row[columns.time] = ss.str();

			// TODO: don't dismiss off-beat rows near the region boundaries

			for (size_t i = 0; i < (size_t)mtm.ntracks; i++) {
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
							row[columns.channel[i]] = note->channel() + 1;
							row[columns.note_name[i]] = note_off_str;
							row[columns.velocity[i]] = note->velocity();
							int64_t delay_ticks = (note->end_time() - row_beats).to_relative_ticks();
							row[columns.delay[i]] = delay_ticks;
						}

						// Notes on
						MidiTrackerMatrix::RowToNotes::const_iterator i_on = mtm.notes_on[i].find(irow);
						if (i_on != mtm.notes_on[i].end()) {
							boost::shared_ptr<NoteType> note = i_on->second;
							row[columns.channel[i]] = note->channel() + 1;
							row[columns.note_name[i]] = Evoral::midi_note_name (note->note());
							row[columns.velocity[i]] = note->velocity();
							row[columns.delay[i]] = (note->time() - row_beats).to_relative_ticks();
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
MidiTrackerEditor::selection_changed ()
{
	if (!UIConfiguration::instance().get_sound_midi_notes()) {
		return;
	}

	TreeModel::Path path;
	TreeModel::iterator iter;
	boost::shared_ptr<NoteType> note;
	TreeView::Selection::ListHandle_Path rows = view.get_selection()->get_selected_rows ();

	NotePlayer* player = new NotePlayer (track);

	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {
		if ((iter = model->get_iter (*i))) {
			note = (*iter)[columns._note[0]];
			player->add (note);
		}
	}

	player->play ();
}
