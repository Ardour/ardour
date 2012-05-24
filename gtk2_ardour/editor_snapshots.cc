/*
    Copyright (C) 2000-2009 Paul Davis

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

#include <gtkmm/liststore.h>
#include "gtkmm2ext/choice.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_directory.h"
#include "editor_snapshots.h"
#include "ardour_ui.h"
#include "i18n.h"
#include "utils.h"
#include "prompter.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;

EditorSnapshots::EditorSnapshots (Editor* e)
	: EditorComponent (e)
{
	_model = ListStore::create (_columns);
	_display.set_model (_model);
	_display.append_column (X_("snapshot"), _columns.visible_name);
	_display.set_size_request (75, -1);
	_display.set_headers_visible (false);
	_display.set_reorderable (false);
	_scroller.add (_display);
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &EditorSnapshots::selection_changed));
	_display.signal_button_press_event().connect (sigc::mem_fun (*this, &EditorSnapshots::button_press), false);
}

void
EditorSnapshots::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	redisplay ();
}

/** A new snapshot has been selected.
 */
void
EditorSnapshots::selection_changed ()
{
	if (_display.get_selection()->count_selected_rows() > 0) {

		TreeModel::iterator i = _display.get_selection()->get_selected();

		std::string snap_name = (*i)[_columns.real_name];

		if (snap_name.length() == 0) {
			return;
		}

		if (_session->snap_name() == snap_name) {
			return;
		}

		ARDOUR_UI::instance()->load_session (_session->path(), string (snap_name));
	}
}

bool
EditorSnapshots::button_press (GdkEventButton* ev)
{
	if (ev->button == 3) {
		/* Right-click on the snapshot list. Work out which snapshot it
		   was over. */
		Gtk::TreeModel::Path path;
		Gtk::TreeViewColumn* col;
		int cx;
		int cy;
		_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
		Gtk::TreeModel::iterator iter = _model->get_iter (path);
		if (iter) {
			Gtk::TreeModel::Row row = *iter;
			popup_context_menu (ev->button, ev->time, row[_columns.real_name]);
		}
		return true;
	}

	return false;
}


/** Pop up the snapshot display context menu.
 * @param button Button used to open the menu.
 * @param time Menu open time.
 * @param snapshot_name Name of the snapshot that the menu click was over.
 */
void
EditorSnapshots::popup_context_menu (int button, int32_t time, std::string snapshot_name)
{
	using namespace Menu_Helpers;

	MenuList& items (_menu.items());
	items.clear ();

	const bool modification_allowed = (_session->snap_name() != snapshot_name && _session->name() != snapshot_name);

	add_item_with_sensitivity (items, MenuElem (_("Remove"), sigc::bind (sigc::mem_fun (*this, &EditorSnapshots::remove), snapshot_name)), modification_allowed);

	add_item_with_sensitivity (items, MenuElem (_("Rename..."), sigc::bind (sigc::mem_fun (*this, &EditorSnapshots::rename), snapshot_name)), modification_allowed);

	_menu.popup (button, time);
}

void
EditorSnapshots::rename (std::string old_name)
{
	ArdourPrompter prompter(true);

	string new_name;

	prompter.set_name ("Prompter");
	prompter.set_title (_("Rename Snapshot"));
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	prompter.set_prompt (_("New name of snapshot"));
	prompter.set_initial_text (old_name);

	if (prompter.run() == RESPONSE_ACCEPT) {
		prompter.get_result (new_name);
		if (new_name.length()) {
			_session->rename_state (old_name, new_name);
			redisplay ();
		}
	}
}


void
EditorSnapshots::remove (std::string name)
{
	vector<string> choices;

	std::string prompt = string_compose (_("Do you really want to remove snapshot \"%1\" ?\n(which cannot be undone)"), name);

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Yes, remove it."));

	Gtkmm2ext::Choice prompter (_("Remove snapshot"), prompt, choices);

	if (prompter.run () == 1) {
		_session->remove_state (name);
		redisplay ();
	}
}

void
EditorSnapshots::redisplay ()
{
	if (_session == 0) {
		return;
	}

	vector<sys::path> state_file_paths;

	get_state_files_in_directory (_session->session_directory().root_path(),
				      state_file_paths);

	if (state_file_paths.empty()) {
		return;
	}

	vector<string> state_file_names (get_file_names_no_extension(state_file_paths));

	_model->clear ();

	for (vector<string>::iterator i = state_file_names.begin(); i != state_file_names.end(); ++i)
	{
		string statename = (*i);
		TreeModel::Row row = *(_model->append());

		/* this lingers on in case we ever want to change the visible
		   name of the snapshot.
		*/

		string display_name;
		display_name = statename;

		if (statename == _session->snap_name()) {
			_display.get_selection()->select(row);
		}

		row[_columns.visible_name] = display_name;
		row[_columns.real_name] = statename;
	}
}

