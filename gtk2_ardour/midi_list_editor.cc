/*
    Copyright (C) 2009 Paul Davis

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

#include "midi_list_editor.h"
#include "note_player.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR;
using Timecode::BBT_Time;

MidiListEditor::MidiListEditor (Session* s, boost::shared_ptr<MidiRegion> r, boost::shared_ptr<MidiTrack> tr)
	: ArdourWindow (r->name())
	, region (r)
	, track (tr)
{
	/* We do not handle nested sources/regions. Caller should have tackled this */

	if (r->max_source_level() > 0) {
		throw failed_constructor();
	}

	set_session (s);

	edit_column = -1;
	editing_renderer = 0;

	model = ListStore::create (columns);
	view.set_model (model);

	note_length_model = ListStore::create (note_length_columns);
	TreeModel::Row row;
	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat;
	row[note_length_columns.name] = _("Whole");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat/2;
	row[note_length_columns.name] = _("Half");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat/3;
	row[note_length_columns.name] = _("Triplet");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat/4;
	row[note_length_columns.name] = _("Quarter");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat/8;
	row[note_length_columns.name] = _("Eighth");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat;
	row[note_length_columns.name] = _("Sixteenth");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat/32;
	row[note_length_columns.name] = _("Thirty-second");

	row = *(note_length_model->append());
	row[note_length_columns.ticks] = BBT_Time::ticks_per_beat/64;
	row[note_length_columns.name] = _("Sixty-fourth");

	view.signal_key_press_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_press), false);
	view.signal_key_release_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_release), false);

	view.append_column (_("Start"), columns.start);
	view.append_column (_("Channel"), columns.channel);
	view.append_column (_("Num"), columns.note);
	view.append_column (_("Name"), columns.note_name);
	view.append_column (_("Vel"), columns.velocity);

	/* use a combo renderer for length, so that we can offer a selection
	   of pre-defined note lengths. we still allow edited values with
	   arbitrary length (in ticks).
	 */

	Gtk::TreeViewColumn* lenCol = Gtk::manage (new Gtk::TreeViewColumn (_("Length")));
	Gtk::CellRendererCombo* comboCell = Gtk::manage(new Gtk::CellRendererCombo);
	lenCol->pack_start(*comboCell);
	lenCol->add_attribute (comboCell->property_text(), columns.length);

	comboCell->property_model() = note_length_model;
	comboCell->property_text_column() = 1;
	comboCell->property_has_entry() = false;

	view.append_column (*lenCol);
	view.append_column (_("End"), columns.end);
	view.set_headers_visible (true);
	view.set_rules_hint (true);
	view.get_selection()->set_mode (SELECTION_MULTIPLE);
	view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &MidiListEditor::selection_changed));

	for (int i = 0; i < 7; ++i) {
		CellRendererText* renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (i));

		renderer->property_editable() = true;

		renderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiListEditor::editing_started), i));
		renderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiListEditor::editing_canceled));
		renderer->signal_edited().connect (sigc::mem_fun (*this, &MidiListEditor::edited));
	}

	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	redisplay_model ();

	region->midi_source(0)->model()->ContentsChanged.connect (content_connection, invalidator (*this), 
								  boost::bind (&MidiListEditor::redisplay_model, this), gui_context());

	view.show ();
	scroller.show ();
	buttons.show ();
	vbox.show ();

	vbox.pack_start (buttons, false, false);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

MidiListEditor::~MidiListEditor ()
{
}

bool
MidiListEditor::key_press (GdkEventKey* ev)
{
	bool ret = false;
	TreeModel::Path path;
	TreeViewColumn* col;

	switch (ev->keyval) {
	case GDK_Tab:
		if (edit_column > 0) {
			if (edit_column >= 6) {
				edit_column = 0;
				edit_path.next();
			} else {
				edit_column++;
			}
			col = view.get_column (edit_column);
			path = edit_path;
			view.set_cursor (path, *col, true);
			ret = true;
		}
		break;
		
	case GDK_Up:
		if (edit_column > 0) {
			edit_path.prev ();
			col = view.get_column (edit_column);
			path = edit_path;
			view.set_cursor (path, *col, true);
			ret = true;
		}
		break;

	case GDK_Down:
		if (edit_column > 0) {
			edit_path.next ();
			col = view.get_column (edit_column);
			path = edit_path;
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
MidiListEditor::key_release (GdkEventKey* ev)
{
	bool ret = false;
	TreeModel::Path path;
	TreeViewColumn* col;
	TreeModel::iterator iter;
	TreeModel::Row row;
	MidiModel::NoteDiffCommand* cmd;
	boost::shared_ptr<MidiModel> m (region->midi_source(0)->model());
	boost::shared_ptr<NoteType> note;
	boost::shared_ptr<NoteType> copy;

	switch (ev->keyval) {
	case GDK_Insert:
		/* add a new note to the model, based on the note at the cursor
		 * pos
		 */
		view.get_cursor (path, col);
		iter = model->get_iter (path);
		cmd = m->new_note_diff_command (_("insert new note"));
		note = (*iter)[columns._note];
		copy.reset (new NoteType (*note.get()));
		cmd->add (copy);
		m->apply_command (*_session, cmd);
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
MidiListEditor::delete_selected_note ()
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
			boost::shared_ptr<NoteType> note = (*iter)[columns._note];
			to_delete.push_back (note);
		}
	}

	boost::shared_ptr<MidiModel> m (region->midi_source(0)->model());
	MidiModel::NoteDiffCommand* cmd = m->new_note_diff_command (_("delete notes (from list)"));

	for (Notes::iterator i = to_delete.begin(); i != to_delete.end(); ++i) {
		cmd->remove (*i);
	}

	m->apply_command (*_session, cmd);
}

void
MidiListEditor::stop_editing (bool cancelled)
{
	if (editing_renderer) {
		editing_renderer->stop_editing (cancelled);
	}
}

void
MidiListEditor::editing_started (CellEditable*, const string& path, int colno)
{
	cerr << "start editing at [" << path << "] col " << colno << endl;
	edit_path = TreePath (path);
	edit_column = colno;
	editing_renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (colno));
}

void
MidiListEditor::editing_canceled ()
{
	cerr << "editing cancelled with edit_column = " << edit_column << " path \"" << edit_path.to_string() << "\"\n";
	edit_path.clear ();
	edit_column = -1;
	editing_renderer = 0;
}

void
MidiListEditor::edited (const std::string& path, const std::string& text)
{
	TreeModel::iterator iter = model->get_iter (path);

	if (!iter || text.empty()) {
		return;
	}

	cerr << "Edited " << path << " col " << edit_column << " to \"" << text << "\"\n";

	boost::shared_ptr<NoteType> note = (*iter)[columns._note];
	boost::shared_ptr<MidiModel> m (region->midi_source(0)->model());
	MidiModel::NoteDiffCommand* cmd;

	cmd = m->new_note_diff_command (_("insert new note"));

	double fval;
	int    ival;
	bool   apply = false;

	switch (edit_column) {
	case 0: // start
		break;
	case 1: // channel
		// correct ival for zero-based counting after scan
		if (sscanf (text.c_str(), "%d", &ival) == 1 && --ival != note->note()) {
			cmd->change (note, MidiModel::NoteDiffCommand::NoteNumber, (uint8_t) ival);
			apply = true;
			cerr << "channel differs " << (int) ival << " vs. " << (int) note->channel() << endl;
		}
		break;
	case 2: // note
		if (sscanf (text.c_str(), "%d", &ival) == 1 && ival != note->note()) {
			cmd->change (note, MidiModel::NoteDiffCommand::NoteNumber, (uint8_t) ival);
			apply = true;
			cerr << "note number differs " << (int) ival << " vs. " << (int) note->note() << endl;
		}
		break;
	case 3: // name
		break;
	case 4: // velocity
		if (sscanf (text.c_str(), "%d", &ival) == 1 && ival != note->velocity()) {
			cmd->change (note, MidiModel::NoteDiffCommand::Velocity, (uint8_t) ival);
			apply = true;
			cerr << "velocity differs " << (int) ival << " vs. " << (int) note->velocity() << endl;
		}
		break;
	case 5: // length
		if (sscanf (text.c_str(), "%d", &ival) == 1) {
			fval = (double) ival / Timecode::BBT_Time::ticks_per_beat;
		
			if (fval != note->length()) {
				cmd->change (note, MidiModel::NoteDiffCommand::Length, fval);
				apply = true;
				cerr << "length differs: " << fval << " vs. " << note->length() << endl;
			}
		}
		break;
	case 6: // end
		break;
	default:
		break;
	}

	if (apply) {
		cerr << "Apply change\n";
		m->apply_command (*_session, cmd);
	} else {
		cerr << "No change\n";
	}
	
	/* model has been redisplayed by now */
	/* keep selected row(s), move cursor there, to allow us to continue editing */

	TreeViewColumn* col = view.get_column (edit_column);
	view.set_cursor (edit_path, *col, 0);
}

void
MidiListEditor::redisplay_model ()
{
	view.set_model (Glib::RefPtr<Gtk::ListStore>(0));
	model->clear ();

	if (_session) {

		BeatsFramesConverter conv (_session->tempo_map(), region->position());
		MidiModel::Notes notes = region->midi_source(0)->model()->notes();
		TreeModel::Row row;
		stringstream ss;

		for (MidiModel::Notes::iterator i = notes.begin(); i != notes.end(); ++i) {
			row = *(model->append());
			row[columns.channel] = (*i)->channel() + 1;
			row[columns.note_name] = Evoral::midi_note_name ((*i)->note());
			row[columns.note] = (*i)->note();
			row[columns.velocity] = (*i)->velocity();

			Timecode::BBT_Time bbt;
			double dur;

			_session->tempo_map().bbt_time (conv.to ((*i)->time()), bbt);

			ss.str ("");
			ss << bbt;
			row[columns.start] = ss.str();

			bbt.bars = 0;
			dur = (*i)->end_time() - (*i)->time();
			bbt.beats = floor (dur);
			bbt.ticks = (uint32_t) lrint (fmod (dur, 1.0) * Timecode::BBT_Time::ticks_per_beat);
			
			row[columns.length] = lrint ((*i)->length() * Timecode::BBT_Time::ticks_per_beat);

			_session->tempo_map().bbt_time (conv.to ((*i)->end_time()), bbt);
			
			ss.str ("");
			ss << bbt;
			row[columns.end] = ss.str();

			row[columns._note] = (*i);
		}
	}

	view.set_model (model);
}

void
MidiListEditor::selection_changed ()
{
	if (!Config->get_sound_midi_notes()) {
		return;
	}

	TreeModel::Path path;
	TreeModel::iterator iter;
	boost::shared_ptr<NoteType> note;
	TreeView::Selection::ListHandle_Path rows = view.get_selection()->get_selected_rows ();

	NotePlayer* player = new NotePlayer (track);

	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {
		if (iter = model->get_iter (*i)) {
			note = (*iter)[columns._note];		
			player->add (note);
		}
	}

	player->play ();
}
