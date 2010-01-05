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

#include "evoral/midi_util.h"
#include "ardour/midi_region.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "midi_list_editor.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR;

MidiListEditor::MidiListEditor (Session* s, boost::shared_ptr<MidiRegion> r)
	: ArdourDialog (r->name(), false, false)
	, region (r)
{
	set_session (s);

	model = ListStore::create (columns);
	view.set_model (model);

	view.signal_key_press_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_press));
	view.signal_key_release_event().connect (sigc::mem_fun (*this, &MidiListEditor::key_release));

	view.append_column (_("Start"), columns.start);
	view.append_column (_("Channel"), columns.channel);
	view.append_column (_("Num"), columns.note);
	view.append_column (_("Name"), columns.note_name);
	view.append_column (_("Vel"), columns.velocity);
	view.append_column (_("Length"), columns.length);
	view.append_column (_("End"), columns.end);
	view.set_headers_visible (true);
	view.set_name (X_("MidiListView"));
	view.set_rules_hint (true);

	for (int i = 0; i < 6; ++i) {
		CellRendererText* renderer = dynamic_cast<CellRendererText*>(view.get_column_cell_renderer (i));
		renderer->property_editable() = true;
		renderer->signal_edited().connect (sigc::mem_fun (*this, &MidiListEditor::edited));
	}

	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	redisplay_model ();

	view.show ();
	scroller.show ();

	get_vbox()->pack_start (scroller);
	set_size_request (400, 400);
}

MidiListEditor::~MidiListEditor ()
{
}

bool
MidiListEditor::key_press (GdkEventKey* ev)
{
	return true;
}

bool
MidiListEditor::key_release (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Delete:
		delete_selected_note ();
		break;
	default:
		break;
	}

	return true;
}

void
MidiListEditor::delete_selected_note ()
{
	Glib::RefPtr<TreeSelection> selection = view.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (rows.empty()) {
		return;
	}

	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();
	TreeIter iter;

	/* selection mode is single, so rows.begin() is it */

	if ((iter = model->get_iter (*i))) {
		boost::shared_ptr<NoteType> note = (*iter)[columns._note];
		cerr << "Would have deleted " << *note << endl;
	}

}
void
MidiListEditor::edited (const Glib::ustring& path, const Glib::ustring& /* text */)
{
	TreeModel::iterator iter = model->get_iter (path);

	cerr << "Edit at " << path << endl;

	if (!iter) {
		return;
	}

	boost::shared_ptr<NoteType> note = (*iter)[columns._note];

	cerr << "Edited " << *note << endl;

	redisplay_model ();
	
	/* keep selected row(s), move cursor there, to allow us to continue editing */
}

void
MidiListEditor::redisplay_model ()
{
	view.set_model (Glib::RefPtr<Gtk::ListStore>(0));
	model->clear ();

	if (_session) {
		
		MidiModel::Notes notes = region->midi_source(0)->model()->notes();
		TreeModel::Row row;
		stringstream ss;
		
		for (MidiModel::Notes::iterator i = notes.begin(); i != notes.end(); ++i) {
			row = *(model->append());
			row[columns.channel] = (*i)->channel() + 1;
			row[columns.note_name] = Evoral::midi_note_name ((*i)->note());
			row[columns.note] = (*i)->note();
			row[columns.velocity] = (*i)->velocity();
			
			BBT_Time bbt;
			double dur;
			bbt.bars = 0;
			bbt.beats = (uint32_t) floor ((*i)->time());
			bbt.ticks = (uint32_t) lrint (fmod ((*i)->time(), 1.0) * Meter::ticks_per_beat);

			_session->tempo_map().bbt_time (region->position(), bbt);
			
			ss.str ("");
			ss << bbt;
			row[columns.start] = ss.str();

			bbt.bars = 0;
			dur = (*i)->end_time() - (*i)->time();
			bbt.beats = floor (dur);
			bbt.ticks = (uint32_t) lrint (fmod (dur, 1.0) * Meter::ticks_per_beat);
			
			_session->tempo_map().bbt_duration_at (region->position(), bbt, 0);

			ss.str ("");
			ss << bbt;
			row[columns.length] = ss.str();
			
			bbt.bars = 0;
			bbt.beats = (uint32_t) floor ((*i)->end_time());
			bbt.ticks = (uint32_t) lrint (fmod ((*i)->end_time(), 1.0) * Meter::ticks_per_beat);

			_session->tempo_map().bbt_time (region->position(), bbt);
			
			ss.str ("");
			ss << bbt;
			row[columns.end] = ss.str();
			
			row[columns._note] = (*i);
		}
	}

	view.set_model (model);
}
