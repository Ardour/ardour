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

#include "widgets/tooltips.h"
#include "widgets/choice.h"
#include "widgets/prompter.h"
#include "widgets/popup.h"

#include "ardour_ui.h"
#include "editor.h"
#include "utils.h"
#include "mixer_snapshots.h"
#include "gui_thread.h"

#include "pbd/i18n.h"
#include "pbd/basename.h"
#include "pbd/gstdio_compat.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;

struct ColumnInfo {
    int           index;
    int           sort_idx;
    AlignmentEnum al;
    const char*   label;
    const char*   tooltip;
};

MixerSnapshotList::MixerSnapshotList (bool global)
    : add_template_button("Add Snapshot")
    , add_session_template_button("Add from External")
    , _window_packer(new VBox())
    , _button_packer(new HBox())
    , _bug_user(true)
    , _global(global)
{
    _snapshot_model = ListStore::create (_columns);
    _snapshot_display.set_model (_snapshot_model);
    _snapshot_display.append_column (_("Mixer Snapshots (double-click to load)"), _columns.name);
    _snapshot_display.set_size_request (75, -1);
    _snapshot_display.set_headers_visible (true);
    _snapshot_display.set_reorderable (false);

    _scroller.add (_snapshot_display);
    _scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

    add_template_button.signal_clicked().connect(sigc::mem_fun(*this, &MixerSnapshotList::new_snapshot));
    add_session_template_button.signal_clicked().connect(sigc::mem_fun(*this, &MixerSnapshotList::new_snapshot_from_session));

    if(_global) {
        bootstrap_display_and_model();
    } else {
        _button_packer->pack_start(add_template_button, true, true);
        add_session_template_button.set_size_request(-1, 50);
        _button_packer->pack_start(add_session_template_button, true, true);
        _window_packer->pack_start(_scroller, true, true);
        _window_packer->pack_start(*_button_packer, false, true);
    }

    _snapshot_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &MixerSnapshotList::selection_changed));
    _snapshot_display.signal_button_press_event().connect (sigc::mem_fun (*this, &MixerSnapshotList::button_press), false);
}

void MixerSnapshotList::bootstrap_display_and_model()
{
    TreeView& display = _snapshot_display;
    Glib::RefPtr<ListStore> model = _snapshot_model;

    display.append_column(_("# Tracks"), _columns.n_tracks);
    display.append_column(_("# VCAs"),   _columns.n_vcas);
    display.append_column(_("# Groups"), _columns.n_groups);
    display.append_column(_("Date"),     _columns.date);
    display.append_column(_("Version"),  _columns.version);

    //newest snaps should be at the top
    model->set_sort_column(4, SORT_DESCENDING);

    ColumnInfo ci[] = {
        { 0,  0,  ALIGN_LEFT,   _("Name"),     _("") },
        { 1,  1,  ALIGN_CENTER, _("# Tracks"), _("") },
        { 2,  2,  ALIGN_CENTER, _("# VCAs"),   _("") },
        { 3,  3,  ALIGN_CENTER, _("# Groups"), _("") },
        { 4,  6,  ALIGN_LEFT,   _("Date"),     _("") },
        { 5,  5,  ALIGN_LEFT,   _("Version"),  _("") },
        { -1,-1,  ALIGN_CENTER, 0, 0 }
    };

    for (int i = 0; ci[i].index >= 0; ++i) {
        ColumnInfo info = ci[i];

        TreeViewColumn* column = display.get_column(info.index);

        Label* label = manage(new Label (info.label));
        label->set_alignment(info.al);
        set_tooltip(*label, info.tooltip);
        column->set_widget(*label);
        label->show();

        column->set_sort_column(info.sort_idx);
        column->set_expand(false);
        column->set_alignment(info.al);

        //...and this sets the alignment for the data cells
        CellRendererText* rend = dynamic_cast<CellRendererText*>(display.get_column_cell_renderer(info.index));
        if (rend) {
            rend->property_xalign() = (info.al == ALIGN_RIGHT ? 1.0 : (info.al == ALIGN_LEFT ? 0.0 : 0.5));
        }
    }
}

void MixerSnapshotList::set_session (Session* s)
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
            _session->snapshot_manager().create_snapshot(name, rl, _global);
            redisplay();
        }
    }
}

void MixerSnapshotList::new_snapshot_from_session() {
    FileChooserDialog session_selector(_("New Snapshot from Session, Template or Other:"), FILE_CHOOSER_ACTION_OPEN);

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
    _session->snapshot_manager().create_snapshot(name, session_path, _global);
    redisplay();
}

/* A new snapshot has been selected. */
void MixerSnapshotList::selection_changed ()
{
    if (_snapshot_display.get_selection()->count_selected_rows() == 0) {
        return;
    }

    TreeModel::iterator i = _snapshot_display.get_selection()->get_selected();
    _snapshot_display.set_sensitive (false);
    _snapshot_display.set_sensitive (true);
}

bool MixerSnapshotList::button_press (GdkEventButton* ev)
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

            MixerSnapshotSubstitutionDialog* sub_dialog = new MixerSnapshotSubstitutionDialog(snapshot);
            sub_dialog->signal_response().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotList::substitution_dialog_response), sub_dialog));
            sub_dialog->show_all();
            sub_dialog->set_position(WIN_POS_MOUSE);
            sub_dialog->present();

            return true;
        }
    }
    return false;
}

void MixerSnapshotList::substitution_dialog_response(int response, MixerSnapshotSubstitutionDialog* dialog)
{
    if(!dialog) {
        return;
    }

    if(response != RESPONSE_ACCEPT) {
        delete dialog;
        return;
    }

    MixerSnapshot* snapshot = dialog->get_snapshot();
    if(!snapshot) {
        delete dialog;
        return;
    }

    vector<MixerSnapshot::State> clean = snapshot->get_routes();
    vector<MixerSnapshot::State> dirty;

    const string selection_text = dialog->get_selection_combo_active_text();
    if(selection_text != " --- ") {
        //set all selected routes to the selection's state
        if(snapshot->route_state_exists(selection_text)) {
            XMLNode node (snapshot->get_route_state_by_name(selection_text).node);

            RouteList rl = PublicEditor::instance().get_selection().tracks.routelist();
            for(RouteList::const_iterator i = rl.begin(); i != rl.end(); i++) {
                if((*i)->is_monitor() || (*i)->is_master() || (*i)->is_auditioner()) {
                    continue;
                }
                XMLNode copy (node);
                MixerSnapshot::State new_state {
                    string(),
                    (*i)->name(),
                    copy
                };
                dirty.push_back(new_state);
            }
        }
    } else {
        //make standard substitutions
        vector<route_combo>::const_iterator p;
        vector<route_combo> pairs = dialog->get_substitutions();
        for(p = pairs.begin(); p != pairs.end(); p++) {
            boost::shared_ptr<Route> route = (*p).first;
            ComboBoxText*            cb    = (*p).second;

            if(!route || !cb) {
                continue;
            }

            if(route->is_monitor() || route->is_master() || route->is_auditioner()) {
                continue;
            }

            const string name = route->name();
            const string at   = cb->get_active_text();

            //do not recall this state
            if(at == " --- ") {
                continue;
            }

            const bool route_state_exists = snapshot->route_state_exists(name);
            const bool subst_state_exists = snapshot->route_state_exists(at);
            if(route_state_exists && subst_state_exists) {
                XMLNode copy (snapshot->get_route_state_by_name(at).node);
                MixerSnapshot::State new_state {
                    string(),
                    name,
                    copy
                };
                dirty.push_back(new_state);
                continue;
            } else if(!route_state_exists && subst_state_exists) {
                XMLNode copy (snapshot->get_route_state_by_name(at).node);
                MixerSnapshot::State new_state {
                    string(),
                    name,
                    copy
                };
                dirty.push_back(new_state);
                continue;
            }
        }
    }

    delete dialog;

    snapshot->set_route_states(dirty);
    snapshot->recall();
    snapshot->set_route_states(clean);
}


/* Pop up the snapshot display context menu.
 * @param button Button used to open the menu.
 * @param time Menu open time.
 * @param snapshot_name Name of the snapshot that the menu click was over.
 */
void MixerSnapshotList::popup_context_menu (int button, int32_t time, TreeModel::iterator& iter)
{
    using namespace Menu_Helpers;

    MenuList& items (_menu.items());
    items.clear();

    add_item_with_sensitivity(items, MenuElem(_("Remove"), sigc::bind(sigc::mem_fun(*this, &MixerSnapshotList::remove_snapshot), iter)), true);
    add_item_with_sensitivity (items, MenuElem(_("Rename..."), sigc::bind(sigc::mem_fun(*this, &MixerSnapshotList::rename_snapshot), iter)), true);
    if(!_global) {
        add_item_with_sensitivity (items, MenuElem (_("Promote To Mixer Template"), sigc::bind(sigc::mem_fun(*this, &MixerSnapshotList::promote_snapshot), iter)), true);
    }
    _menu.popup (button, time);
}

void MixerSnapshotList::remove_snapshot(TreeModel::iterator& iter)
{
    MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
    vector<string> choices;

    std::string prompt = string_compose(_("Do you really want to remove snapshot \"%1\" ?\n(this cannot be undone)"), snapshot->get_label());

    choices.push_back(_("No, do nothing."));
    choices.push_back(_("Yes, remove it."));
    choices.push_back(_("Yes, and don't ask again."));

    ArdourWidgets::Choice prompter (_("Remove snapshot"), prompt, choices);

    if(_bug_user) {
        switch(prompter.run()) {
            case 0:
                break;
            case 1:
                //remove
                if(_session->snapshot_manager().remove_snapshot(snapshot)) {
                    _snapshot_model->erase((*iter));
                }
                break;
            case 2:
                //remove and switch bug_user
                if(_session->snapshot_manager().remove_snapshot(snapshot)) {
                    _snapshot_model->erase((*iter));
                    _bug_user = false;
                }
                break;
            default:
                break;
        }
    } else {
        if(_session->snapshot_manager().remove_snapshot(snapshot)) {
            _snapshot_model->erase((*iter));
        }
    }
}

void MixerSnapshotList::rename_snapshot(TreeModel::iterator& iter)
{
    MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
    ArdourWidgets::Prompter prompter(true);

    string new_name;

    prompter.set_name("Prompter");
    prompter.set_title(_("Rename Snapshot"));
    prompter.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
    prompter.set_prompt(_("New name of snapshot"));
    prompter.set_initial_text(snapshot->get_label());

    if (prompter.run() == RESPONSE_ACCEPT) {
        prompter.get_result(new_name);
        if (new_name.length()) {
            //notify the user that this is about to be overwritten
            TreeModel::const_iterator jter = get_row_by_name(new_name);
            if(jter) {
                const string name = (*jter)[_columns.name];
                const string prompt = string_compose(
                    _("Do you really want to overwrite snapshot \"%1\" ?\n(this cannot be undone)"),
                    name
                );

                vector<string> choices;
                choices.push_back(_("No, do nothing."));
                choices.push_back(_("Yes, overwrite it."));

                ArdourWidgets::Choice prompter (_("Overwrite Snapshot"), prompt, choices);

                if(prompter.run() == 1) {
                    remove_row(jter);
                } else {
                    return;
                }
            }
            //remove any row with this new name (we're overwriting this)
            if(_session->snapshot_manager().rename_snapshot(snapshot, new_name)) {
                if (new_name.length() > 45) {
                    new_name = new_name.substr(0, 45);
                    new_name.append("...");
                }
                //set this row's name to the new name
                (*iter)[_columns.name] = new_name;
            }
        }
    }
}

void MixerSnapshotList::promote_snapshot(TreeModel::iterator& iter)
{
    MixerSnapshot* snapshot = (*iter)[_columns.snapshot];

    //let the user know that this was successful.
    if(_session->snapshot_manager().promote(snapshot)) {
        const string label = snapshot->get_label();

        const string notification = string_compose(
            _("Snapshot \"%1\" is now available to all sessions.\n"),
            label
        );

        //not leaked, self-deleting
        PopUp* notify = new PopUp(WIN_POS_MOUSE, 2000, true);
        notify->set_text(notification);
        notify->touch();
    }
}

TreeModel::const_iterator MixerSnapshotList::get_row_by_name(const std::string& name)
{
    TreeModel::const_iterator iter;
    TreeModel::Children rows = _snapshot_model->children();
    for(iter = rows.begin(); iter != rows.end(); iter++) {
        const string row_name = (*iter)[_columns.name];
        if(row_name == name) {
            break;
        }
    }
    return iter;
}

bool MixerSnapshotList::remove_row(Gtk::TreeModel::const_iterator& iter)
{
    if(iter) {
        const string name = (*iter)[_columns.name];
        MixerSnapshot* snapshot = (*iter)[_columns.snapshot];
        _snapshot_model->erase((*iter));
        if(snapshot) {
            _session->snapshot_manager().remove_snapshot(snapshot);
        }
        return true;
    }
    return false;
}

void MixerSnapshotList::redisplay ()
{
    if (!_session) {
        return;
    }

    SnapshotList active_list;
    if(_global) {
        active_list = _session->snapshot_manager().get_global_snapshots();
    } else if(!_global) {
        active_list = _session->snapshot_manager().get_local_snapshots();
    }

    if(active_list.empty()) {
        return;
    }

    _snapshot_model->clear();
    for(SnapshotList::const_iterator it = active_list.begin(); it != active_list.end(); it++) {
        // (*it)->LabelChanged.connect(connections, invalidator(*this), boost::bind(&MixerSnapshotList::test_func, this, _1), gui_context());
        TreeModel::Row row = *(_snapshot_model->append());
        row[_columns.name] = (*it)->get_label();
        row[_columns.snapshot] = (*it);

        //additional information for the global snapshots
        if(_global) {
            row[_columns.n_tracks]  = (*it)->get_routes().size();
            row[_columns.n_vcas]    = (*it)->get_vcas().size();
            row[_columns.n_groups]  = (*it)->get_groups().size();

            GStatBuf gsb;
            g_stat((*it)->get_path().c_str(), &gsb);
            Glib::DateTime gdt(Glib::DateTime::create_now_local(gsb.st_mtime));

            row[_columns.timestamp] = gsb.st_mtime;;
            row[_columns.date]      = gdt.format("%F %H:%M");
            row[_columns.version]   = (*it)->get_last_modified_with();
        }
    }
}

