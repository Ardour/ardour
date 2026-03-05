/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#pragma once
#include <string>

#include <ytkmm/box.h>
#include <ytkmm/filechooserwidget.h>
#include <ytkmm/liststore.h>
#include <ytkmm/notebook.h>
#include <ytkmm/progressbar.h>
#include <ytkmm/scrolledwindow.h>
#include <ytkmm/table.h>
#include <ytkmm/treestore.h>
#include <ytkmm/treeview.h>

#include "pbd/id.h"

#include "ardour/search_paths.h"
#include "ardour/session.h"

#include "ardour/template_utils.h"
#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace ArdourWidgets
{
	class ArdourButton;
	class ArdourDropdown;
}

class StripImportDialog : public ArdourDialog, public ProgressReporter
{
public:
	StripImportDialog (ARDOUR::Session*);
	~StripImportDialog ();

	void do_import ();

protected:
	void on_response (int);
	bool on_delete_event (GdkEventAny*);

private:
	enum SelectionType {
		Template,
		RouteState,
		Snapshot
	};

	void page_changed (GtkNotebookPage*, guint);
	void setup_file_page ();
	void file_selection_changed ();
	void treeview_selection_changed (Gtk::TreeView*, SelectionType);
	void parse_route_state (std::string const&);
	void setup_model (Glib::RefPtr<Gtk::ListStore>, std::vector<ARDOUR::TemplateInfo> const&);
	void find_presets (PBD::Searchpath const&, std::vector<ARDOUR::TemplateInfo>&);

	void setup_strip_import_page ();
	void refill_import_table ();
	void idle_refill_import_table ();
	void maybe_switch_to_import_page ();
	void add_mapping ();
	void change_mapping (ArdourWidgets::ArdourDropdown*, PBD::ID const&, PBD::ID const&, std::string const&);
	void prepare_mapping (bool, PBD::ID const&, std::string const&);
	void remove_mapping (PBD::ID const&);
	void clear_mapping ();
	void import_all_strips (bool only_visible);
	void set_default_mapping (bool and_idle_update);
	void update_sensitivity_ok ();
	void ok_activated ();
	void update_progress_gui (float);

	struct SessionTemplateColumns : public Gtk::TreeModel::ColumnRecord {
		SessionTemplateColumns ()
		{
			add (name);
			add (path);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	SessionTemplateColumns       _columns;
	Glib::RefPtr<Gtk::TreeStore> _recent_model;
	Glib::RefPtr<Gtk::ListStore> _template_model;
	Glib::RefPtr<Gtk::ListStore> _local_pset_model;
	Glib::RefPtr<Gtk::ListStore> _global_pset_model;

	std::map<guint, Gtk::TreeView*> _notebook_content;
	std::map<guint, SelectionType>  _notebook_type;

	Gtk::VBox _page_file;
	Gtk::VBox _page_strip;

	Gtk::Notebook          _notebook;
	Gtk::FileChooserWidget _chooser;
	Gtk::Button*           _open_button;
	Gtk::Button*           _ok_button;
	Gtk::Button*           _cancel_button;
	Gtk::Label             _info_text;
	Gtk::ScrolledWindow    _recent_scroller;
	Gtk::TreeView          _recent_treeview;
	Gtk::ScrolledWindow    _template_scroller;
	Gtk::TreeView          _template_treeview;
	Gtk::ScrolledWindow    _local_pset_scroller;
	Gtk::TreeView          _local_pset_treeview;
	Gtk::ScrolledWindow    _global_pset_scroller;
	Gtk::TreeView          _global_pset_treeview;

	Gtk::Table                     _strip_table;
	Gtk::ScrolledWindow            _strip_scroller;
	ArdourWidgets::ArdourDropdown* _add_rid_dropdown;
	ArdourWidgets::ArdourDropdown* _add_eid_dropdown;
	ArdourWidgets::ArdourButton*   _add_new_mapping;
	ArdourWidgets::ArdourDropdown* _action;
	ArdourWidgets::ArdourButton*   _show_all_toggle;

	Gtk::HBox        _action_box;
	Gtk::ProgressBar _progress_bar;

	bool                       _match_pbd_id;
	std::string                _path;
	std::map<PBD::ID, PBD::ID> _import_map;

	std::map<PBD::ID, ARDOUR::Session::RouteImportInfo> _extern_map;
	std::map<PBD::ID, ARDOUR::Session::RouteImportInfo> _route_map;

	PBD::ID _add_rid;
	PBD::ID _add_eid;

	sigc::connection _notebook_connection;
	sigc::connection _chooser_connection;
};
