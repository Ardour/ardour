#include <iostream>

#include "ardour/filesystem_paths.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_directory.h"

#include <glib.h>
#include <glibmm.h>
#include <glibmm/datetime.h>

#include <gtkmm/table.h>

#include "widgets/tooltips.h"
#include "widgets/choice.h"
#include "widgets/prompter.h"

#include "mixer_snapshot_dialog.h"

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/gstdio_compat.h"
#include "pbd/i18n.h"

using namespace Gtk;
using namespace PBD;
using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;

struct ColumnInfo {
	int           index;
	int           sort_idx;
	AlignmentEnum al;
	const char*   label;
	const char*   tooltip;
};

MixerSnapshotDialog::MixerSnapshotDialog()
    : ArdourDialog(_("this is a dialog"), true, false)
{
    global_model = Gtk::ListStore::create(_columns);
    local_model  = Gtk::ListStore::create(_columns);
    
    bootstrap_display_and_model(global_display, global_model, true);
    bootstrap_display_and_model(local_display, local_model,  false);

    // global_display.set_focus_on_click();
    // local_display.set_focus_on_click();

    // global_display.signal_cursor_changed().connect(sigc::mem_fun(*this, &MixerSnapshotDialog::callback));
    // local_display.signal_cursor_changed().connect(sigc::mem_fun(*this, &MixerSnapshotDialog::callback));

    // global_display.get_selection()->signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::selection_changed), true), false);
    // local_display.get_selection()->signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::selection_changed), false), false);

    global_display.signal_button_press_event().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::button_press), true), false);
    local_display.signal_button_press_event().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::button_press), false), false);
}

MixerSnapshotDialog::~MixerSnapshotDialog() 
{

}

void MixerSnapshotDialog::set_session(Session* s)
{
    if(s)
        ArdourDialog::set_session(s);

    refill();
}


bool MixerSnapshotDialog::button_press(GdkEventButton* ev, bool global)
{
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
    Button* btn_del  = manage(new Button("Delete"));
    Button* btn_load = manage(new Button("New From Session"));
    add_remove->pack_start(*btn_add,  true, true);
    add_remove->pack_start(*btn_del,  true, true);
    add_remove->pack_start(*btn_load, true, true);

    VBox* vbox = manage(new VBox);
    vbox->set_homogeneous();
    vbox->pack_start(*add_remove);
	vbox->set_size_request(800, -1);

    if(global) {
        btn_add->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::new_snapshot), true));

        global_scroller.set_border_width(10);
	    global_scroller.set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	    global_scroller.add(global_display);

        Table* table = manage(new Table(3, 3));
        table->set_size_request(-1, 400);
        table->attach(global_scroller, 0, 3, 0, 5                         );
        table->attach(*vbox,           2, 3, 6, 8, FILL|EXPAND, FILL, 5, 5);
        get_vbox()->pack_start(*table);
    } else {
        btn_add->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &MixerSnapshotDialog::new_snapshot), false));

        local_scroller.set_border_width(10);
	    local_scroller.set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	    local_scroller.add(local_display);
        
        Table* table = manage(new Table(3, 3));
        table->set_size_request(-1, 400);
        table->attach(local_scroller,  0, 3, 0, 5                         );
        table->attach(*vbox,           2, 3, 6, 8, FILL|EXPAND, FILL, 5, 5);
        get_vbox()->pack_start(*table);
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
    MixerSnapshot* snap = new MixerSnapshot(_session);

    ArdourWidgets::Prompter prompter(true);

	string new_name;

	prompter.set_name("New Mixer Snapshot Prompter");
	prompter.set_title(_("Mixer Snapshot Name:"));
	prompter.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	prompter.set_prompt(_("Set Mixer Snapshot Name"));
	prompter.set_initial_text(snap->label);

	if (prompter.run() == RESPONSE_ACCEPT) {
		prompter.get_result(new_name);
		if (new_name.length() > 0) {
			snap->label = new_name;
            snap->snap();
            snap->write(global);
			refill();
		}
	}
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
        row[_columns.n_groups]     = snap->get_groups().size();;
        row[_columns.has_specials] = true;

        GStatBuf gsb;
		g_stat(path.c_str(), &gsb);
        Glib::DateTime gdt(Glib::DateTime::create_now_local(gsb.st_mtime));

        row[_columns.timestamp] = gsb.st_mtime;
        row[_columns.date]      = gdt.format ("%F %H:%M");;
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
        row[_columns.n_groups]     = snap->get_groups().size();;
        row[_columns.has_specials] = true;

        GStatBuf gsb;
		g_stat(path.c_str(), &gsb);
        Glib::DateTime gdt(Glib::DateTime::create_now_local(gsb.st_mtime));

        row[_columns.timestamp] = gsb.st_mtime;
        row[_columns.date]      = gdt.format ("%F %H:%M");;
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
        snap->write(global);
    }
    
}

int MixerSnapshotDialog::run() {
    show_all();
    return 0;
}