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

#include <gtkmm/filechooserdialog.h>
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
#include "editor.h"
#include "utils.h"

#include "pbd/i18n.h"
#include "pbd/basename.h"

#include "mixer_snapshots.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

MixerSnapshotList::MixerSnapshotList ()
    : add_template_button("Add Template")
    , add_session_template_button("Add from Session")
    , _window_packer(new VBox())
    , _button_packer(new HBox())
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

    add_template_button.signal_clicked().connect(sigc::mem_fun(*this, &MixerSnapshotList::new_snapshot));
    add_session_template_button.signal_clicked().connect(sigc::mem_fun(*this, &MixerSnapshotList::new_snapshot_from_session));

    _button_packer->pack_start(add_template_button, false, false);
    _button_packer->pack_start(add_session_template_button, false, false);
    _window_packer->pack_start(_scroller, true, true);
    _window_packer->pack_start(*_button_packer, true, true);

    _snapshot_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &MixerSnapshotList::selection_changed));
    _snapshot_display.signal_button_press_event().connect (sigc::mem_fun (*this, &MixerSnapshotList::button_press), false);
}

void
MixerSnapshotList::set_session (Session* s)
{
    SessionHandlePtr::set_session (s);

    redisplay ();
}

void MixerSnapshotList::new_snapshot() {
    ArdourWidgets::Prompter prompter (true);
    prompter.set_name ("Prompter");
    prompter.set_title (_("New Mixer Sanpshot"));
    prompter.set_prompt (_("Sanpshot Name:"));
    prompter.set_initial_text (_session->name());
    prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);


    string name;
    if (prompter.run() == RESPONSE_ACCEPT) {
        prompter.get_result(name);
        if (name.length()) {
            RouteList rl = PublicEditor::instance().get_selection().tracks.routelist();
            _session->snapshot_manager().create_snapshot(name, rl, false);
            redisplay();
        }
    }
}

void MixerSnapshotList::new_snapshot_from_session() {
    FileChooserDialog session_selector(_("Open Session"), FILE_CHOOSER_ACTION_OPEN);

    session_selector.add_button(Stock::CANCEL, RESPONSE_CANCEL);
    session_selector.add_button(Stock::OPEN, RESPONSE_ACCEPT);
    session_selector.set_current_folder(Glib::path_get_dirname(_session->path()));

    int response = session_selector.run();
    session_selector.hide();

    if (response != RESPONSE_ACCEPT) {
        return;
    }

    string session_path = session_selector.get_filename();
    string name = basename_nosuffix(session_path);
    _session->snapshot_manager().create_snapshot(name, session_path, false);
    redisplay();
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
        TreeViewColumn* col;
        TreeModel::Path path;
        int cx;
        int cy;

        _snapshot_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
        TreeModel::iterator iter = _snapshot_model->get_iter(path);

        if (iter) {
            popup_context_menu(ev->button, ev->time, iter);
            return true;
        }
        return true;
    }

    if (ev->type == GDK_2BUTTON_PRESS) {
        TreeViewColumn* col;
        TreeModel::Path path;
        int cx;
        int cy;

        _snapshot_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
        TreeModel::iterator iter = _snapshot_model->get_iter(path);

        if (iter) {
            MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
            snapshot->recall();
            return true;
        }
    }
    return false;
}


/** Pop up the snapshot display context menu.
 * @param button Button used to open the menu.
 * @param time Menu open time.
 * @param snapshot_name Name of the snapshot that the menu click was over.
 */
void
MixerSnapshotList::popup_context_menu (int button, int32_t time, TreeModel::iterator& iter)
{
    using namespace Menu_Helpers;

    MenuList& items (_menu.items());
    items.clear ();

    add_item_with_sensitivity(items, MenuElem (_("Remove"), sigc::bind(sigc::mem_fun (*this, &MixerSnapshotList::remove_snapshot), iter)), true);
    add_item_with_sensitivity (items, MenuElem (_("Rename..."), sigc::bind (sigc::mem_fun (*this, &MixerSnapshotList::rename_snapshot), iter)), true);
    add_item_with_sensitivity (items, MenuElem (_("Promote To Mixer Template"), sigc::bind (sigc::mem_fun (*this, &MixerSnapshotList::promote_snapshot), iter)), true);
    _menu.popup (button, time);
}

void MixerSnapshotList::remove_snapshot(TreeModel::iterator& iter)
{
    MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
    vector<string> choices;

    std::string prompt = string_compose (_("Do you really want to remove snapshot \"%1\" ?\n(which cannot be undone)"), snapshot->get_label());

    choices.push_back (_("No, do nothing."));
    choices.push_back (_("Yes, remove it."));

    ArdourWidgets::Choice prompter (_("Remove snapshot"), prompt, choices);

    if (prompter.run () == 1) {
        redisplay ();
    }
    printf("remove snapshot %s @ path %s\n", snapshot->get_label().c_str(), snapshot->get_path().c_str());
}

void
MixerSnapshotList::rename_snapshot(TreeModel::iterator& iter)
{
    MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
    ArdourWidgets::Prompter prompter(true);

    string new_name;

    prompter.set_name ("Prompter");
    prompter.set_title (_("Rename Snapshot"));
    prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
    prompter.set_prompt (_("New name of snapshot"));
    prompter.set_initial_text (snapshot->get_label());

    if (prompter.run() == RESPONSE_ACCEPT) {
        prompter.get_result (new_name);
        if (new_name.length()) {
            redisplay ();
        }
    }
    printf("rename snapshot %s to %s\n", snapshot->get_label().c_str(), new_name.c_str());
}

void MixerSnapshotList::promote_snapshot(TreeModel::iterator& iter)
{
    MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
    printf("promote snapshot %s to mixer template\n", snapshot->get_label().c_str());
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

    _snapshot_model->clear();

    for(MixerSnapshotManager::SnapshotList::const_iterator it = local_snapshots.begin(); it != local_snapshots.end(); it++) {
        TreeModel::Row row = *(_snapshot_model->append());
        row[_columns.name] = (*it)->get_label();
        row[_columns.snapshot] = (*it);
    }
}

