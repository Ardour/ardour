/*
    Copyright (C) 2000-2019 Paul Davis

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
#include "pbd/file_utils.h"
#include "pbd/gstdio_compat.h"

#include <glibmm.h>
#include <glibmm/datetime.h>

#include <gtkmm/liststore.h>

#include "ardour/directory_names.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_directory.h"
#include "ardour/mixer_snapshot_manager.h"

#include "widgets/choice.h"
#include "widgets/prompter.h"

#include "ardour_ui.h"
#include "utils.h"

#include "pbd/i18n.h"

#include "mixer_snapshots.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

MixerSnapshotList::MixerSnapshotList ()
{
	_snapshot_model = ListStore::create (_columns);
	_snapshot_display.set_model (_snapshot_model);
	_snapshot_display.append_column (_("Mixer Snapshots (double-click to load)"), _columns.name);
//	_snapshot_display.append_column (_("Modified Date"), _columns.timestamp);
	_snapshot_display.set_size_request (75, -1);
	_snapshot_display.set_headers_visible (true);
	_snapshot_display.set_reorderable (false);
	_scroller.add (_snapshot_display);
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_snapshot_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &MixerSnapshotList::selection_changed));
	_snapshot_display.signal_button_press_event().connect (sigc::mem_fun (*this, &MixerSnapshotList::button_press), false);
}

void
MixerSnapshotList::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	redisplay ();
}

/* A new snapshot has been selected. */
void
MixerSnapshotList::selection_changed ()
{
	if (_snapshot_display.get_selection()->count_selected_rows() == 0) {
		return;
	}

	TreeModel::iterator i = _snapshot_display.get_selection()->get_selected();

	//std::string snap_path = (*i)[_columns.snap];

	_snapshot_display.set_sensitive (false);
//	ARDOUR_UI::instance()->load_session (_session->path(), string (snap_name));
	_snapshot_display.set_sensitive (true);
}

bool
MixerSnapshotList::button_press (GdkEventButton* ev)
{
	if (ev->button == 3) {
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
MixerSnapshotList::popup_context_menu (int button, int32_t time, std::string snapshot_name)
{
	using namespace Menu_Helpers;

	MenuList& items (_menu.items());
	items.clear ();

	const bool modification_allowed = (_session->snap_name() != snapshot_name && _session->name() != snapshot_name);

/*	add_item_with_sensitivity (items, MenuElem (_("Remove"), sigc::bind (sigc::mem_fun (*this, &MixerSnapshotList::remove), snapshot_name)), modification_allowed);

	add_item_with_sensitivity (items, MenuElem (_("Rename..."), sigc::bind (sigc::mem_fun (*this, &MixerSnapshotList::rename), snapshot_name)), modification_allowed);
*/

	_menu.popup (button, time);
}

void
MixerSnapshotList::rename (std::string old_name)
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
//			_session->rename_state (old_name, new_name);
			redisplay ();
		}
	}
}


void
MixerSnapshotList::remove (std::string name)
{
	vector<string> choices;

	std::string prompt = string_compose (_("Do you really want to remove snapshot \"%1\" ?\n(which cannot be undone)"), name);

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Yes, remove it."));

	ArdourWidgets::Choice prompter (_("Remove snapshot"), prompt, choices);

	if (prompter.run () == 1) {
//		_session->remove_state (name);
		redisplay ();
	}
}

void
MixerSnapshotList::redisplay ()
{
	if (_session == 0) {
		return;
	}

	MixerSnapshotManager::SnapshotList local_snapshots = _session->snapshot_manager().get_local_snapshots();

	if(local_snapshots.empty()) {
		return;
	}

	for(MixerSnapshotManager::SnapshotList::const_iterator it = local_snapshots.begin(); it != local_snapshots.end(); it++) {
		TreeModel::Row row = *(_snapshot_model->append());
		row[_columns.name] = (*it)->get_label();
		row[_columns.snapshot] = (*it);
	}
}

