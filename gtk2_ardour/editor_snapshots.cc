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


#include <glib.h>
#include <glibmm.h>
#include <glibmm/datetime.h>

#include <gtkmm/liststore.h>

#include "pbd/file_utils.h"
#include "pbd/gstdio_compat.h"
#include "pbd/i18n.h"

#include "ardour/filename_extensions.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_directory.h"

#include "widgets/choice.h"
#include "widgets/prompter.h"

#include "editor_snapshots.h"
#include "ardour_ui.h"
#include "utils.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

EditorSnapshots::EditorSnapshots (Editor* e)
	: EditorComponent (e)
{
	_snap_model = ListStore::create (_columns);
	_snap_display.set_model (_snap_model);
	_snap_display.append_column (_("Snapshot (click to load)"), _columns.visible_name);
	_snap_display.append_column (_("Modified Date"), _columns.time_formatted);
	_snap_display.set_size_request (75, -1);
	_snap_display.set_headers_visible (true);
	_snap_display.set_reorderable (false);
	_snap_scroller.add (_snap_display);
	_snap_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_snap_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &EditorSnapshots::selection_changed));
	_snap_display.signal_button_press_event().connect (sigc::mem_fun (*this, &EditorSnapshots::button_press), false);
	
	_back_model = ListStore::create (_columns);
	_back_display.set_model (_back_model);
	_back_display.append_column (_("Auto-Backup (click to load)"), _columns.visible_name);
	_back_display.append_column (_("Modified Date"), _columns.time_formatted);
	_back_display.set_size_request (75, -1);
	_back_display.set_headers_visible (true);
	_back_display.set_reorderable (false);
	_back_scroller.add (_back_display);
	_back_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_back_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &EditorSnapshots::backup_selection_changed));
	
	_pane.add(_snap_scroller);
if(Profile->get_mixbus()) {
	_pane.add(_back_scroller);
}
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
	if (_snap_display.get_selection()->count_selected_rows() > 0) {

		TreeModel::iterator i = _snap_display.get_selection()->get_selected();

		std::string snap_name = (*i)[_columns.real_name];

		if (snap_name.length() == 0) {
			return;
		}

		if (_session->snap_name() == snap_name) {
			return;
		}

		_snap_display.set_sensitive (false);
		ARDOUR_UI::instance()->load_session (_session->path(), string (snap_name));
		_snap_display.set_sensitive (true);
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
		_snap_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
		Gtk::TreeModel::iterator iter = _snap_model->get_iter (path);
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
	ArdourWidgets::Prompter prompter(true);

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

	ArdourWidgets::Choice prompter (_("Remove snapshot"), prompt, choices);

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

	//fill the snapshots pane
	{
		vector<std::string> state_file_paths;

		get_state_files_in_directory (_session->session_directory().root_path(),
									  state_file_paths);

		if (state_file_paths.empty()) {
			return;
		}

		vector<string> state_file_names (get_file_names_no_extension(state_file_paths));

		_snap_model->clear ();

		for (vector<string>::iterator i = state_file_names.begin(); i != state_file_names.end(); ++i)
		{
			string statename = (*i);
			TreeModel::Row row = *(_snap_model->append());

			/* this lingers on in case we ever want to change the visible
			   name of the snapshot.
			*/

			string display_name;
			display_name = statename;

			if (statename == _session->snap_name()) {
				_snap_display.get_selection()->select(row);
			}

			std::string s = Glib::build_filename (_session->path(), statename + ARDOUR::statefile_suffix);

			GStatBuf gsb;
			g_stat (s.c_str(), &gsb);
			Glib::DateTime gdt(Glib::DateTime::create_now_local (gsb.st_mtime));

			row[_columns.visible_name] = display_name;
			row[_columns.real_name] = statename;
			row[_columns.time_formatted] = gdt.format ("%F %H:%M");
		}
	}
	
	//fill the backup pane
	{
		vector<std::string> state_file_paths;

		get_state_files_in_directory (_session->session_directory().backup_path(),
									  state_file_paths);

		if (state_file_paths.empty()) {
			return;
		}

		vector<string> state_file_names (get_file_names_no_extension(state_file_paths));

		_back_model->clear ();

		for (vector<string>::iterator i = state_file_names.begin(); i != state_file_names.end(); ++i)
		{
			string statename = (*i);
			TreeModel::Row row = *(_back_model->append());

			/* this lingers on in case we ever want to change the visible
			   name of the snapshot.
			*/

			string display_name;
			display_name = statename;

			std::string s = Glib::build_filename (_session->path(), statename + ARDOUR::statefile_suffix);

			GStatBuf gsb;
			g_stat (s.c_str(), &gsb);
			Glib::DateTime gdt(Glib::DateTime::create_now_local (gsb.st_mtime));

			row[_columns.visible_name] = display_name;
			row[_columns.real_name] = statename;
			row[_columns.time_formatted] = gdt.format ("%F %H:%M");
		}
	}
	
}

/** A new backup has been selected.
 */
void
EditorSnapshots::backup_selection_changed ()
{
	if (_back_display.get_selection()->count_selected_rows() > 0) {

		TreeModel::iterator i = _back_display.get_selection()->get_selected();

		std::string back_name = (*i)[_columns.real_name];

		//copy the backup file to the session root folder, so we can open it
		std::string back_path = _session->session_directory().backup_path() + G_DIR_SEPARATOR + back_name + ARDOUR::statefile_suffix;
		std::string copy_path = _session->session_directory().root_path() + G_DIR_SEPARATOR + back_name + ARDOUR::statefile_suffix;
		PBD::copy_file (back_path, copy_path);
		
		//now open the copy
		_snap_display.set_sensitive (false);
		ARDOUR_UI::instance()->load_session (_session->path(), string (back_name));
		_snap_display.set_sensitive (true);
	}
}

