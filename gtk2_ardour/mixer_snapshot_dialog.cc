#include <iostream>
#include <stdio.h>

#include "ardour/filesystem_paths.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_directory.h"

#include <glib.h>
#include <glibmm.h>
#include <glibmm/datetime.h>
#include <glibmm/fileutils.h>

#include <gtkmm/table.h>
#include <gtkmm/filechooserdialog.h>

#include "editor.h"
#include "mixer_snapshot_dialog.h"
#include "utils.h"

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/gstdio_compat.h"
#include "pbd/i18n.h"

#include "widgets/tooltips.h"
#include "widgets/choice.h"
#include "widgets/prompter.h"

using namespace Gtk;
using namespace PBD;
using namespace std;
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

MixerSnapshotDialog::MixerSnapshotDialog(Session* s)
    : ArdourWindow(_("Mixer Snapshot Manager:"))
{
    global_model = Gtk::ListStore::create(_columns);
    local_model  = Gtk::ListStore::create(_columns);

    bootstrap_display_and_model(global_display, global_model, true);
    bootstrap_display_and_model(local_display, local_model,  false);

    add(top_level_view_box);

    global_display.signal_button_press_event().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::button_press), true), false);
    local_display.signal_button_press_event().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::button_press), false), false);

    set_session(s);
}

void MixerSnapshotDialog::set_session(Session* s)
{
    if(s)
        ArdourWindow::set_session(s);

    refill();
}


bool MixerSnapshotDialog::button_press(GdkEventButton* ev, bool global)
{
    if (ev->button == 3) {

        Gtk::TreeModel::Path path;
        Gtk::TreeViewColumn* col;
        int cx;
        int cy;

        TreeModel::iterator iter;
        if(global) {
            global_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
            iter = global_model->get_iter(path);
        } else {
            local_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
            iter = local_model->get_iter(path);
        }

        if (iter) {
            TreeModel::Row row = *(iter);
            popup_context_menu(ev->button, ev->time, row[_columns.full_path]);
            return true;
        }
    };

    if (ev->type == GDK_2BUTTON_PRESS) {

        TreeModel::iterator iter;
        if(global) {
            iter = global_display.get_selection()->get_selected();
        } else {
            iter = local_display.get_selection()->get_selected();
        }

        global_display.get_selection()->unselect_all();
        local_display.get_selection()->unselect_all();

        if(iter) {
            MixerSnapshot* s = (*iter)[_columns.snapshot];
            s->recall();
            return true;
        }
    }
    return false;
}

void MixerSnapshotDialog::popup_context_menu(int btn, int64_t time, string path)
{
    using namespace Menu_Helpers;
    MenuList& items(menu.items());
    items.clear();
    add_item_with_sensitivity(items, MenuElem(_("Remove"), sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::remove_snapshot), path)), true);
    add_item_with_sensitivity(items, MenuElem(_("Rename..."), sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::rename_snapshot), path)), true);
    menu.popup(btn, time);
}

void MixerSnapshotDialog::remove_snapshot(const string path)
{
    ::g_remove(path.c_str());
    refill();
}

void MixerSnapshotDialog::rename_snapshot(const string old_path)
{
    string dir_name  = Glib::path_get_dirname(old_path);

    Prompter prompt(true);
    prompt.set_name("Rename MixerSnapshot Prompter");
    prompt.set_title(_("New Snapshot Name:"));
    prompt.add_button(Stock::SAVE, RESPONSE_ACCEPT);
    prompt.set_prompt(_("Rename Mixer Snapshot:"));
    prompt.set_initial_text(basename_nosuffix(old_path));

    if (prompt.run() == RESPONSE_ACCEPT) {
        string new_label;
        prompt.get_result(new_label);
        if (new_label.length() > 0) {
            string new_path = Glib::build_filename(dir_name, new_label + ".xml");
            ::g_rename(old_path.c_str(), new_path.c_str());
            refill();
        }
    }
}

bool MixerSnapshotDialog::bootstrap_display_and_model(Gtkmm2ext::DnDTreeView<string>& display, Glib::RefPtr<ListStore> model, bool global)
{
    if(!model) {
        return false;
    }

    display.set_model(model);

    display.append_column(_("Fav"),            _columns.favorite);
    display.append_column(_("Name"),           _columns.name);
    display.append_column(_("# Tracks"),       _columns.n_tracks);
    display.append_column(_("# VCAs"),         _columns.n_vcas);
    display.append_column(_("# Groups"),       _columns.n_groups);
    display.append_column(_("Special Tracks"), _columns.has_specials);
    display.append_column(_("Date"),           _columns.date);
    display.append_column(_("Version"),        _columns.version);

    //newest snaps should be at the top
    model->set_sort_column(6, SORT_DESCENDING);

    display.set_headers_visible(true);
    display.set_headers_clickable(true);
    display.set_reorderable(false);
    display.set_rules_hint(true);
    display.add_object_drag(_columns.name.index(), "");
    display.set_drag_column(_columns.name.index());

    CellRendererToggle* fav_cell = dynamic_cast<CellRendererToggle*>(display.get_column_cell_renderer(0));
    fav_cell->property_activatable() = true;
    fav_cell->property_radio() = true;
    fav_cell->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::fav_cell_action), global));

    HBox* add_remove = manage(new HBox);
    Button* btn_add  = manage(new Button("New"));
    Button* btn_new  = manage(new Button("New From Session"));
    add_remove->pack_start(*btn_add,  true, true, 50);
    add_remove->pack_start(*btn_new, true, true, 45);

    VBox* vbox = manage(new VBox);
    vbox->set_homogeneous();
    vbox->pack_start(*add_remove);
    vbox->set_size_request(800, -1);

    if(global) {
        btn_add->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::new_snapshot), true));
        btn_new->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::new_snap_from_session), true));

        global_scroller.set_border_width(10);
        global_scroller.set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
        global_scroller.add(global_display);

        Table* table = manage(new Table(3, 3));
        table->set_size_request(-1, 400);
        table->attach(global_scroller, 0, 3, 0, 5                         );
        table->attach(*vbox,           2, 3, 6, 8, FILL|EXPAND, FILL, 5, 5);
        top_level_view_box.pack_start(*table);
    } else {
        btn_add->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::new_snapshot), false));
        btn_new->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::new_snap_from_session), false));

        local_scroller.set_border_width(10);
        local_scroller.set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
        local_scroller.add(local_display);

        Table* table = manage(new Table(3, 3));
        table->set_size_request(-1, 400);
        table->attach(local_scroller,  0, 3, 0, 5                         );
        table->attach(*vbox,           2, 3, 6, 8, FILL|EXPAND, FILL, 5, 5);
        top_level_view_box.pack_start(*table);
    }

    ColumnInfo ci[] = {
        { 0,  0,  ALIGN_CENTER,  _("Favorite"),       _("") },
        { 1,  1,  ALIGN_LEFT,    _("Name"),           _("") },
        { 2,  2,  ALIGN_CENTER,  _("# Tracks"),       _("") },
        { 3,  3,  ALIGN_CENTER,  _("# VCAs"),         _("") },
        { 4,  4,  ALIGN_CENTER,  _("# Groups"),       _("") },
        { 5,  5,  ALIGN_CENTER,  _("Special Tracks"), _("") },
        { 6,  8,  ALIGN_LEFT,    _("Date"),           _("") },
        { 7,  7,  ALIGN_LEFT,    _("Version"),        _("") },
        { -1,-1,  ALIGN_CENTER, 0, 0 }
    };

    for (int i = 0; ci[i].index >= 0; ++i) {
        ColumnInfo info = ci[i];

        TreeViewColumn* col = display.get_column(info.index);

        Label* label = manage(new Label (info.label));
        label->set_alignment(info.al);
        set_tooltip(*label, info.tooltip);
        col->set_widget(*label);
        label->show();

        col->set_sort_column(info.sort_idx);
        col->set_expand(false);
        col->set_alignment(info.al);

        //...and this sets the alignment for the data cells
        CellRendererText* rend = dynamic_cast<CellRendererText*>(display.get_column_cell_renderer(info.index));
        if (rend) {
            rend->property_xalign() = (info.al == ALIGN_RIGHT ? 1.0 : (info.al == ALIGN_LEFT ? 0.0 : 0.5));
        }
    }
    return true;
}

void MixerSnapshotDialog::new_snapshot(bool global)
{
    if(!_session)
        return;

    string path = Glib::build_filename(user_config_directory(-1), "mixer_snapshots/");
    if(!Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS & Glib::FILE_TEST_IS_DIR))
        ::g_mkdir(path.c_str(), 0775);

    path = Glib::build_filename(_session->session_directory().root_path(), "mixer_snapshots/");
    if(!Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS & Glib::FILE_TEST_IS_DIR))
        ::g_mkdir(path.c_str(), 0775);

    MixerSnapshot* snap = new MixerSnapshot(_session);

    Prompter prompt(true);
    prompt.set_name("New Mixer Snapshot Prompter");
    prompt.set_title(_("Mixer Snapshot Name:"));
    prompt.add_button(Stock::SAVE, RESPONSE_ACCEPT);
    prompt.set_prompt(_("Set Mixer Snapshot Name"));
    prompt.set_initial_text(_session->name());

    RouteList rl = PublicEditor::instance().get_selection().tracks.routelist();

    CheckButton* sel = new CheckButton(_("Selected Tracks Only:"));
    sel->set_active(!rl.empty());
    sel->show();
    prompt.get_vbox()->pack_start(*sel);

    if(prompt.run() == RESPONSE_ACCEPT) {
        string new_label;
        prompt.get_result(new_label);
        if (new_label.length() > 0) {
            snap->label = new_label;

            string path = "";
            if(global) {
                path = Glib::build_filename(user_config_directory(-1), "mixer_snapshots/", snap->label + ".xml");
            } else {
                path = Glib::build_filename(_session->session_directory().root_path(), "mixer_snapshots/", snap->label + ".xml");
            }

            if(!rl.empty() && sel->get_active())
                snap->snap(rl);
            else
                snap->snap();

            snap->write(path);
            refill();
        }
    }
}

void MixerSnapshotDialog::new_snap_from_session(bool global)
{
    string testpath = Glib::build_filename(user_config_directory(-1), "mixer_snapshots/");
    if(!Glib::file_test(testpath.c_str(), Glib::FILE_TEST_EXISTS & Glib::FILE_TEST_IS_DIR))
        ::g_mkdir(testpath.c_str(), 0775);

    testpath = Glib::build_filename(_session->session_directory().root_path(), "mixer_snapshots/");
    if(!Glib::file_test(testpath.c_str(), Glib::FILE_TEST_EXISTS & Glib::FILE_TEST_IS_DIR))
        ::g_mkdir(testpath.c_str(), 0775);

    Gtk::FileChooserDialog session_selector(_("Open Session"), FILE_CHOOSER_ACTION_OPEN);
    string session_parent_dir = Glib::path_get_dirname(_session->path());
    session_selector.add_button(Stock::CANCEL, RESPONSE_CANCEL);
    session_selector.add_button(Stock::OPEN, RESPONSE_ACCEPT);
    session_selector.set_current_folder(session_parent_dir);

    int response = session_selector.run();
    session_selector.hide();

    if (response == RESPONSE_CANCEL) {
        return;
    }

    string session_path = session_selector.get_filename();

    MixerSnapshot* snapshot = new MixerSnapshot(_session, session_path);

    snapshot->label = basename_nosuffix(session_path);

    string path = "";
    if(global) {
        path = Glib::build_filename(user_config_directory(-1), "mixer_snapshots/", snapshot->label + ".xml");
    } else {
        path = Glib::build_filename(_session->session_directory().root_path(), "mixer_snapshots/", snapshot->label + ".xml");
    }

    snapshot->write(path);
    refill();
}

void MixerSnapshotDialog::refill()
{
    global_model->clear();

    string global_directory = Glib::build_filename(user_config_directory(-1), "mixer_snapshots/");

    vector<string> files;
    find_files_matching_pattern(files, global_directory, "*.xml");

    for(vector<string>::iterator i = files.begin(); i != files.end(); i++) {
        string path = *(i);
        string name = basename_nosuffix(*(i));

        MixerSnapshot* snap = new MixerSnapshot(_session, path);
        snap->label = name;

        TreeModel::Row row = *(global_model->append());
        if (name.length() > 48) {
            name = name.substr (0, 48);
            name.append("...");
        }

        row[_columns.name]         = name;
        row[_columns.favorite]     = snap->favorite;
        row[_columns.version]      = snap->get_last_modified_with();
        row[_columns.n_tracks]     = snap->get_routes().size();
        row[_columns.n_vcas]       = snap->get_vcas().size();
        row[_columns.n_groups]     = snap->get_groups().size();
        row[_columns.has_specials] = snap->has_specials();

        GStatBuf gsb;
        g_stat(path.c_str(), &gsb);
        Glib::DateTime gdt(Glib::DateTime::create_now_local(gsb.st_mtime));

        row[_columns.timestamp] = gsb.st_mtime;
        row[_columns.date]      = gdt.format ("%F %H:%M");
        row[_columns.full_path] = path;
        row[_columns.snapshot]  = snap;
    }

    local_model->clear();
    files.clear();

    string local_directory = Glib::build_filename(_session->session_directory().root_path(), "mixer_snapshots/");
    find_files_matching_pattern(files, local_directory, "*.xml");

    for(vector<string>::iterator i = files.begin(); i != files.end(); i++) {
        string path = *(i);
        string name = basename_nosuffix(*(i));

        MixerSnapshot* snap = new MixerSnapshot(_session, path);
        snap->label = name;

        TreeModel::Row row = *(local_model->append());
        if (name.length() > 48) {
            name = name.substr (0, 48);
            name.append("...");
        }

        row[_columns.name]         = name;
        row[_columns.favorite]     = snap->favorite;
        row[_columns.version]      = snap->get_last_modified_with();
        row[_columns.n_tracks]     = snap->get_routes().size();
        row[_columns.n_vcas]       = snap->get_vcas().size();
        row[_columns.n_groups]     = snap->get_groups().size();
        row[_columns.has_specials] = snap->has_specials();

        GStatBuf gsb;
        g_stat(path.c_str(), &gsb);
        Glib::DateTime gdt(Glib::DateTime::create_now_local(gsb.st_mtime));

        row[_columns.timestamp] = gsb.st_mtime;
        row[_columns.date]      = gdt.format ("%F %H:%M");
        row[_columns.full_path] = path;
        row[_columns.snapshot]  = snap;
    }
}

void MixerSnapshotDialog::fav_cell_action(const string& path, bool global)
{
    TreeModel::iterator iter;
    if(global)
        iter = global_model->get_iter(path);
    else
        iter = local_model->get_iter(path);

    if(iter) {
        MixerSnapshot* snap = (*iter)[_columns.snapshot];
        snap->favorite = !snap->favorite;
        (*iter)[_columns.favorite] = snap->favorite;
        snap->write((*iter)[_columns.full_path]);
    }
}