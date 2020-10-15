/*
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#include <cmath>
#include <map>

#include <gtkmm/cellrenderercombo.h>

#include "evoral/midi_util.h"
#include "evoral/Note.h"

#include "ardour/beats_samples_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"

#include "midi_list_editor.h"
#include "note_player.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using Temporal::BBT_Time;

static map<int32_t,std::string> note_length_map;

static void
fill_note_length_map ()
{
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/1, _("Whole")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/2, _("Half")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/3, _("Triplet")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/4, _("Quarter")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/8, _("Eighth")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/16, _("Sixteenth")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/32, _("Thirty-second")));
	note_length_map.insert (make_pair<int32_t,string> (Temporal::ticks_per_beat/64, _("Sixty-fourth")));
}

MidiListEditor::MidiListEditor (Session* s, boost::shared_ptr<MidiRegion> r, boost::shared_ptr<MidiTrack> tr)
	: ArdourWindow (r->name())
	, buttons (1, 1)
	, region (r)
	, track (tr)
{
	if (note_length_map.empty()) {
		fill_note_length_map ();
	}

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

	note_length_model = ListStore::create (note_length_columns);
	TreeModel::Row row;

	for (std::map<int,string>::iterator i = note_length_map.begin(); i != note_length_map.end(); ++i) {
		row = *(note_length_model->append());
		row[note_length_columns.ticks] = i->first;
		row[note_length_columns.name] = i->second;
	}

	view.signal_key_press_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_press), false);
	view.signal_key_release_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_release), false);
	view.signal_scroll_event().connect (sigc::mem_fun (*this, &MidiListEditor::scroll_event), false);

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

	view.set_headers_visible (true);
	view.set_rules_hint (true);
	view.get_selection()->set_mode (SELECTION_MULTIPLE);
	view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &MidiListEditor::selection_changed));

	for (int i = 0; i < 6; ++i) {
		CellRendererText* renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (i));

		TreeViewColumn* col = view.get_column (i);
		col->set_data (X_("colnum"), GUINT_TO_POINTER(i));

		renderer->property_editable() = true;

		renderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiListEditor::editing_started), i));
		renderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiListEditor::editing_canceled));
		renderer->signal_edited().connect (sigc::mem_fun (*this, &MidiListEditor::edited));
	}

	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	redisplay_model ();

	region->midi_source(0)->model()->ContentsChanged.connect (content_connections, invalidator (*this),
	                                                          boost::bind (&MidiListEditor::redisplay_model, this), gui_context());
	region->PropertyChanged.connect (content_connections, invalidator (*this),
	                                 boost::bind (&MidiListEditor::redisplay_model, this), gui_context());

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

MidiListEditor::~MidiListEditor ()
{
}

bool
MidiListEditor::scroll_event (GdkEventScroll* ev)
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
	case 0:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			fdelta = 1/64.0;
		} else {
			fdelta = 1/4.0;
		}
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			fdelta = -fdelta;
		}
		prop = MidiModel::NoteDiffCommand::StartTime;
		opname = _("edit note start");
		apply = true;
		break;
	case 1:
		idelta = 1;
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			idelta = -idelta;
		}
		prop = MidiModel::NoteDiffCommand::Channel;
		opname = _("edit note channel");
		apply = true;
		break;
	case 2:
	case 3:
		idelta = 1;
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			idelta = -idelta;
		}
		prop = MidiModel::NoteDiffCommand::NoteNumber;
		opname = _("edit note number");
		apply = true;
		break;

	case 4:
		idelta = 1;
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			idelta = -idelta;
		}
		prop = MidiModel::NoteDiffCommand::Velocity;
		opname = _("edit note velocity");
		apply = true;
		break;

	case 5:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			fdelta = 1/64.0;
		} else {
			fdelta = 1/4.0;
		}
		if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_LEFT) {
			fdelta = -fdelta;
		}
		prop = MidiModel::NoteDiffCommand::Length;
		opname = _("edit note length");
		apply = true;
		break;

	default:
		break;
	}


	if (apply) {

		boost::shared_ptr<MidiModel> m (region->midi_source(0)->model());
		MidiModel::NoteDiffCommand* cmd = m->new_note_diff_command (opname);
		vector<TreeModel::Path> previous_selection;

		if (was_selected) {

			/* use selection */

			TreeView::Selection::ListHandle_Path rows = view.get_selection()->get_selected_rows ();
			TreeModel::iterator iter;
			boost::shared_ptr<NoteType> note;

			for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {

				previous_selection.push_back (*i);

				if ((iter = model->get_iter (*i))) {

					note = (*iter)[columns._note];

					switch (prop) {
					case MidiModel::NoteDiffCommand::StartTime:
						if (note->time() + fdelta >= Temporal::Beats()) {
							cmd->change (note, prop, note->time() + fdelta);
						} else {
							cmd->change (note, prop, Temporal::Beats());
						}
						break;
					case MidiModel::NoteDiffCommand::Velocity:
						cmd->change (note, prop, (uint8_t) (note->velocity() + idelta));
						break;
					case MidiModel::NoteDiffCommand::Length:
						if (note->length() + fdelta >= Temporal::Beats::one_tick()) {
							cmd->change (note, prop, note->length() + fdelta);
						} else {
							cmd->change (note, prop, Temporal::Beats::one_tick());
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
				boost::shared_ptr<NoteType> note = (*iter)[columns._note];

				switch (prop) {
				case MidiModel::NoteDiffCommand::StartTime:
					if (note->time() + fdelta >= Temporal::Beats()) {
						cmd->change (note, prop, note->time() + fdelta);
					} else {
						cmd->change (note, prop, Temporal::Beats());
					}
					break;
				case MidiModel::NoteDiffCommand::Velocity:
					cmd->change (note, prop, (uint8_t) (note->velocity() + idelta));
					break;
				case MidiModel::NoteDiffCommand::Length:
					if (note->length() + fdelta >= Temporal::Beats::one_tick()) {
						cmd->change (note, prop, note->length() + fdelta);
					} else {
						cmd->change (note, prop, Temporal::Beats::one_tick());
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

		m->apply_command (*_session, cmd);

		/* reset selection to be as it was before we rebuilt */

		for (vector<TreeModel::Path>::iterator i = previous_selection.begin(); i != previous_selection.end(); ++i) {
			view.get_selection()->select (*i);
		}
	}

	return true;
}

bool
MidiListEditor::key_press (GdkEventKey* ev)
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
MidiListEditor::key_release (GdkEventKey* ev)
{
	bool ret = false;
	TreeModel::Path path;
	TreeViewColumn* col;
	TreeModel::iterator iter;
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
MidiListEditor::editing_started (CellEditable* ed, const string& path, int colno)
{
	edit_path = TreePath (path);
	edit_column = colno;
	editing_renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (colno));
	editing_editable = ed;

	if (ed) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_press), false);
			e->signal_key_release_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_release), false);
		}
	}
}

void
MidiListEditor::editing_canceled ()
{
	edit_path.clear ();
	edit_column = -1;
	editing_renderer = 0;
	editing_editable = 0;
}

void
MidiListEditor::edited (const std::string& path, const std::string& text)
{
	TreeModel::iterator iter = model->get_iter (path);

	if (!iter || text.empty()) {
		return;
	}

	boost::shared_ptr<NoteType> note = (*iter)[columns._note];
	MidiModel::NoteDiffCommand::Property prop (MidiModel::NoteDiffCommand::NoteNumber);

	double fval;
	int    ival;
	bool   apply = false;
	int    idelta = 0;
	Temporal::Beats delta;
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
		ival = ParameterDescriptor::midi_note_num (text);
		if (ival < 128) {
			idelta = ival - note->note();
			prop = MidiModel::NoteDiffCommand::NoteNumber;
			opname = _("change note number");
			apply = true;
		}
		break;
	case 4: // velocity
		if (sscanf (text.c_str(), "%d", &ival) == 1 && ival != note->velocity()) {
			idelta = ival - note->velocity();
			prop = MidiModel::NoteDiffCommand::Velocity;
			opname = _("change note velocity");
			apply = true;
		}
		break;
	case 5: // length

		if (sscanf (text.c_str(), "%lf", &fval) == 1) {

			/* numeric value entered */

			if (text.find ('.') == string::npos && text.find (',') == string::npos) {
				/* integral => units are ticks */
				fval = fval / Temporal::ticks_per_beat;
			} else {
				/* non-integral => beats, so use as-is */
			}

		} else {

			/* assume its text from the combo. look for the map
			 * entry for the actual note ticks
			 */

			uint64_t len_ticks = note->length().to_ticks();
			std::map<int,string>::iterator x = note_length_map.find (len_ticks);

			if (x == note_length_map.end()) {

				/* tick length not in map - was
				 * displaying numeric value ... use new value
				 * from note length map, and convert to beats.
				 */

				for (x = note_length_map.begin(); x != note_length_map.end(); ++x) {
					if (x->second == text) {
						break;
					}
				}

				if (x != note_length_map.end()) {
					fval = x->first / Temporal::ticks_per_beat;
				}

			} else {

				fval = -1.0;

				if (text != x->second) {

					/* get ticks for the newly selected
					 * note length
					 */

					for (x = note_length_map.begin(); x != note_length_map.end(); ++x) {
						if (x->second == text) {
							break;
						}
					}

					if (x != note_length_map.end()) {
						/* convert to beats */
						fval = (double) x->first / Temporal::ticks_per_beat;
					}
				}
			}
		}

		if (fval > 0.0) {
			delta = fval - note->length();
			prop = MidiModel::NoteDiffCommand::Length;
			opname = _("change note length");
			apply = true;
		}
		break;

	default:
		break;
	}

	if (apply) {

		boost::shared_ptr<MidiModel> m (region->midi_source(0)->model());
		MidiModel::NoteDiffCommand* cmd = m->new_note_diff_command (opname);

		TreeView::Selection::ListHandle_Path rows = view.get_selection()->get_selected_rows ();

		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {
			if ((iter = model->get_iter (*i))) {

				note = (*iter)[columns._note];

				switch (prop) {
				case MidiModel::NoteDiffCommand::Velocity:
					cmd->change (note, prop, (uint8_t) (note->velocity() + idelta));
					break;
				case MidiModel::NoteDiffCommand::Length:
					cmd->change (note, prop, note->length() + delta);
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

		m->apply_command (*_session, cmd);

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
MidiListEditor::redisplay_model ()
{
	view.set_model (Glib::RefPtr<Gtk::ListStore>(0));
	model->clear ();

	if (_session) {

		boost::shared_ptr<MidiModel> m (region->midi_source(0)->model());
		TreeModel::Row row;
		stringstream ss;

		MidiModel::Notes::const_iterator i = m->note_lower_bound (region->nt_start().beats());
		Temporal::Beats end_time = (region->nt_start() + region->nt_length()).beats();

		for (; i != m->notes().end() && (*i)->time() < end_time; ++i) {
			row = *(model->append());
			row[columns.channel] = (*i)->channel() + 1;
			row[columns.note_name] = ParameterDescriptor::midi_note_name ((*i)->note());
			row[columns.note] = (*i)->note();
			row[columns.velocity] = (*i)->velocity();

#warning NUTEMPO needs ::bbt() method for timeline types
			// Temporal::BBT_Time bbt (((region->position() + (*i)->time()).earlier (start)).bbt());
			Temporal::BBT_Time bbt;

			ss.str ("");
			ss << bbt;
			row[columns.start] = ss.str();

			bbt.bars = 0;
			const Temporal::Beats dur = (*i)->end_time() - (*i)->time();
			bbt.beats = dur.get_beats ();
			bbt.ticks = dur.get_ticks ();

			uint64_t len_ticks = (*i)->length().to_ticks();
			std::map<int,string>::iterator x = note_length_map.find (len_ticks);

			if (x != note_length_map.end()) {
				row[columns.length] = x->second;
			} else {
				ss.str ("");
				ss << len_ticks;
				row[columns.length] = ss.str();
			}

			row[columns._note] = (*i);
		}
	}

	view.set_model (model);
}

void
MidiListEditor::selection_changed ()
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
			note = (*iter)[columns._note];
			player->add (note);
		}
	}

	player->play ();
}
