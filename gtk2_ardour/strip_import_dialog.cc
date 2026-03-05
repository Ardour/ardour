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

#include <ytkmm/sizegroup.h>
#include <ytkmm/stock.h>

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"

#include "ardour/directory_names.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"

#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_spacer.h"

#include "strip_import_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourWidgets;

StripImportDialog::StripImportDialog (Session* s)
	: ArdourDialog (_("Import Track/Bus State"))
	, _add_rid (0)
	, _add_eid (0)
{
	set_session (s);

	_open_button   = manage (new Button (Stock::GO_FORWARD));
	_ok_button     = manage (new Button (Stock::OK));
	_cancel_button = manage (new Button (Stock::CANCEL));

	_progress_bar.set_no_show_all ();

	get_action_area ()->pack_start (_info_text);
	get_action_area ()->pack_end (*_cancel_button);
	get_action_area ()->pack_end (*_open_button);
	get_action_area ()->pack_end (*_ok_button);

	_cancel_button->show ();
	_open_button->show ();
	_ok_button->hide ();

	_open_button->signal_clicked ().connect (mem_fun (*this, &StripImportDialog::maybe_switch_to_import_page), false);
	_ok_button->signal_clicked ().connect (mem_fun (*this, &StripImportDialog::ok_activated), false);
	_cancel_button->signal_clicked ().connect ([this] { ArdourDialog::response (RESPONSE_CANCEL);});

	_open_button->set_sensitive (false);
	_ok_button->set_sensitive (false);

	get_vbox ()->pack_start (_page_file);
	setup_file_page ();
}

StripImportDialog::~StripImportDialog ()
{
	_notebook_connection.disconnect ();
	_chooser_connection.disconnect ();
}

/* ****************************************************************************
 * Page One .. pick file to import 
 */

void
StripImportDialog::setup_file_page ()
{
	/* file-chooser */
	_chooser.set_size_request (450, 300);
	_chooser.set_current_folder (poor_mans_glob (Config->get_default_session_parent_dir ()));

	FileFilter tracks_filter;
	tracks_filter.add_pattern (string_compose (X_("*%1"), routestate_suffix));
	tracks_filter.set_name (string_compose (_("%1 tracks"), PROGRAM_NAME));
	_chooser.add_filter (tracks_filter);

	FileFilter template_filter;
	template_filter.add_pattern (string_compose (X_("*%1"), template_suffix));
	template_filter.set_name (string_compose (_("%1 tracks"), PROGRAM_NAME));
	_chooser.add_filter (template_filter);

	FileFilter session_filter;
	session_filter.add_pattern (string_compose (X_("*%1"), ARDOUR::statefile_suffix));
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	_chooser.add_filter (session_filter);

	FileFilter all_filter;
	all_filter.add_pattern (string_compose (X_("*%1"), ARDOUR::statefile_suffix));
	all_filter.add_pattern (string_compose (X_("*%1"), template_suffix));
	all_filter.add_pattern (string_compose (X_("*%1"), routestate_suffix));
	all_filter.set_name (_("All supported files"));
	_chooser.add_filter (all_filter);
	_chooser.set_filter (all_filter);

	Gtkmm2ext::add_volume_shortcuts (_chooser);

	_chooser_connection = _chooser.signal_selection_changed ().connect (mem_fun (*this, &StripImportDialog::file_selection_changed));
	_chooser.signal_file_activated ().connect (sigc::mem_fun (*this, &StripImportDialog::maybe_switch_to_import_page));

	_notebook.append_page (_chooser, _("File"));

	guint page = 1;

	/* recent Sessions */
	ARDOUR::RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (!rs.empty ()) {
		_recent_model = TreeStore::create (_columns);

		/* setup model - compare to SessionDialog::redisplay_recent_sessions */
		for (auto const& [_, dir] : rs) {
			string dirname = dir;
			if (dirname.empty ()) {
				continue;
			}
			if (dirname[dirname.length () - 1] == '/') {
				dirname = dirname.substr (0, dirname.length () - 1);
			}
			/* check whether session still exists */
			if (!Glib::file_test (dirname, Glib::FILE_TEST_EXISTS)) {
				/* session doesn't exist */
				continue;
			}
			/* now get available states for this session */
			vector<string> state_file_names = Session::possible_states (dirname);
			if (state_file_names.empty ()) {
				/* no state file? */
				continue;
			}
			TreeModel::Row row = *(_recent_model->append ());
			if (state_file_names.size () > 1) {
				row[_columns.name] = PBD::basename_nosuffix (dirname);
				row[_columns.path] = "";
				for (auto const& snap : state_file_names) {
					Gtk::TreeModel::Row child_row = *(_recent_model->append (row.children ()));
					child_row[_columns.name]      = snap;
					child_row[_columns.path]      = Glib::build_filename (dirname, snap + statefile_suffix);
				}
			} else {
				row[_columns.name] = state_file_names.front ();
				row[_columns.path] = Glib::build_filename (dirname, state_file_names.front () + statefile_suffix);
			}
		}

		_recent_treeview.set_model (_recent_model);
		_recent_treeview.append_column (_("Session Name"), _columns.name);
		_recent_treeview.set_headers_visible (true);
		_recent_treeview.get_selection ()->set_mode (SELECTION_SINGLE);

		_recent_treeview.get_selection ()->signal_changed ().connect (sigc::bind (sigc::mem_fun (*this, &StripImportDialog::treeview_selection_changed), &_recent_treeview, Snapshot));
		_recent_treeview.signal_row_activated ().connect ([&] (const TreeModel::Path&, TreeViewColumn*) { maybe_switch_to_import_page (); });

		_recent_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);
		_recent_scroller.add (_recent_treeview);

		_notebook.append_page (_recent_scroller, _("Recent Sessions"));

		_notebook_type[page]      = Snapshot;
		_notebook_content[page++] = &_recent_treeview;
	}

	/* template list */
	vector<TemplateInfo> templates;
	find_session_templates (templates, false);

	if (templates.size () > 0) {
		_template_model = ListStore::create (_columns);
		setup_model (_template_model, templates);

		_template_treeview.set_model (_template_model);
		_template_treeview.append_column (_("Name"), _columns.name);
		_template_treeview.set_headers_visible (true);

		_template_treeview.get_selection ()->signal_changed ().connect (sigc::bind (sigc::mem_fun (*this, &StripImportDialog::treeview_selection_changed), &_template_treeview, Template));
		_template_treeview.signal_row_activated ().connect ([&] (const TreeModel::Path&, TreeViewColumn*) { maybe_switch_to_import_page (); });

		_template_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);
		_template_scroller.add (_template_treeview);

		_notebook.append_page (_template_scroller, _("Session Templates"));

		_notebook_type[page]      = Template;
		_notebook_content[page++] = &_template_treeview;
	}

	templates.clear ();
	Searchpath local_path (_session->path ());
	local_path.add_subdirectory_to_paths (routestates_dir_name);
	find_presets (local_path, templates);
	if (templates.size () > 0) {
		_local_pset_model = ListStore::create (_columns);
		setup_model (_local_pset_model, templates);

		_local_pset_treeview.set_model (_local_pset_model);
		_local_pset_treeview.append_column (_("Name"), _columns.name);
		_local_pset_treeview.set_headers_visible (true);

		_local_pset_treeview.get_selection ()->signal_changed ().connect (sigc::bind (sigc::mem_fun (*this, &StripImportDialog::treeview_selection_changed), &_local_pset_treeview, RouteState));
		_local_pset_treeview.signal_row_activated ().connect ([&] (const TreeModel::Path&, TreeViewColumn*) { maybe_switch_to_import_page (); });

		_local_pset_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);
		_local_pset_scroller.add (_local_pset_treeview);

		_notebook.append_page (_local_pset_scroller, _("Local Strip Templates"));

		_notebook_type[page]      = RouteState;
		_notebook_content[page++] = &_local_pset_treeview;
	}

	templates.clear ();
	Searchpath global_path (ardour_data_search_path ());
	global_path.add_subdirectory_to_paths (routestates_dir_name);
	find_presets (global_path, templates);
	if (templates.size () > 0) {
		_global_pset_model = ListStore::create (_columns);
		setup_model (_global_pset_model, templates);

		_global_pset_treeview.set_model (_global_pset_model);
		_global_pset_treeview.append_column (_("Name"), _columns.name);
		_global_pset_treeview.set_headers_visible (true);

		_global_pset_treeview.get_selection ()->signal_changed ().connect (sigc::bind (sigc::mem_fun (*this, &StripImportDialog::treeview_selection_changed), &_global_pset_treeview, RouteState));
		_global_pset_treeview.signal_row_activated ().connect ([&] (const TreeModel::Path&, TreeViewColumn*) { maybe_switch_to_import_page (); });

		_global_pset_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);
		_global_pset_scroller.add (_global_pset_treeview);

		_notebook.append_page (_global_pset_scroller, _("Global Strip Templates"));

		_notebook_type[page]      = RouteState;
		_notebook_content[page++] = &_global_pset_treeview;
	}

	_notebook_connection = _notebook.signal_switch_page ().connect (sigc::mem_fun (*this, &StripImportDialog::page_changed));

	_page_file.pack_start (_notebook);
	_page_file.show_all ();

	if (!rs.empty ()) {
		_notebook.set_current_page (1);
	} else if (page > 1) {
		_notebook.set_current_page (page - 1);
	}
}

void
StripImportDialog::page_changed (GtkNotebookPage*, guint page)
{
	switch (page) {
		case 0:
			file_selection_changed ();
			break;
		default:
			treeview_selection_changed (_notebook_content.at (page), _notebook_type.at (page));
			break;
	}
}

void
StripImportDialog::setup_model (Glib::RefPtr<Gtk::ListStore> model, vector<TemplateInfo> const& templates)
{
	for (auto const& t : templates) {
		TreeModel::Row row;
		row = *(model->append ());

		row[_columns.name] = t.name;
		row[_columns.path] = t.path;
	}
}

void
StripImportDialog::find_presets (Searchpath const& search_path, vector<TemplateInfo>& template_info)
{
	vector<string> templates;

	/* clang-format off */
	find_paths_matching_filter (templates,
	                            search_path,
	                            [] (const string& str, void*) { return Glib::file_test (str, Glib::FILE_TEST_IS_DIR); },
	                            0, true, true);
	/* clang-format on */

	if (templates.empty ()) {
		return;
	}

	for (vector<string>::iterator i = templates.begin (); i != templates.end (); ++i) {
		string file = session_template_dir_to_file (*i);

		TemplateInfo rti;
		rti.name = Glib::path_get_basename (*i);
		rti.path = *i;

		template_info.push_back (rti);
	}
	std::sort (template_info.begin (), template_info.end ());
}

void
StripImportDialog::file_selection_changed ()
{
	parse_route_state (_chooser.get_filename ());
}

static string
template_dir_to_file (string const& dir, string const& suffix)
{
	return Glib::build_filename (dir, Glib::path_get_basename (dir) + suffix);
}

void
StripImportDialog::treeview_selection_changed (Gtk::TreeView* treeview, SelectionType t)
{
	Gtk::TreeModel::const_iterator selection = treeview->get_selection ()->get_selected ();
	if (selection) {
		switch (t) {
			case Template:
				parse_route_state (template_dir_to_file ((*selection)[_columns.path], template_suffix));
				break;
			case RouteState:
				parse_route_state (template_dir_to_file ((*selection)[_columns.path], routestate_suffix));
				break;
			case Snapshot:
				parse_route_state ((*selection)[_columns.path]);
				break;
		}
	} else {
		parse_route_state ("");
	}
}

void
StripImportDialog::parse_route_state (std::string const& path)
{
	_extern_map.clear ();
	_path = path;

	if (path.empty ()) {
		goto out;
	}
	if (!Glib::file_test (path, Glib::FILE_TEST_IS_REGULAR)) {
		goto out;
	}

	_extern_map = _session->parse_route_state (path, _match_pbd_id);

out:
	if (_extern_map.empty ()) {
		_info_text.set_text ("");
		_info_text.hide ();
	} else {
		_info_text.set_text (string_compose (P_("%1 Track", "%1 Tracks", _extern_map.size ()), _extern_map.size ()));
		_info_text.show ();
	}
	_open_button->set_sensitive (!_extern_map.empty ());
}

void
StripImportDialog::maybe_switch_to_import_page ()
{
	if (_extern_map.empty ()) {
		return;
	}

	// TODO cleanup managed items on _page_file

	/* -> next page */
	setup_strip_import_page ();
	get_vbox ()->remove (_page_file);
	get_vbox ()->pack_start (_page_strip);

	_info_text.hide ();
	_open_button->hide ();
	_ok_button->show ();
}

/* ****************************************************************************
 * Page Two map Tracks/State 
 */

void
StripImportDialog::refill_import_table ()
{
	Gtk::Label* l;
	Gtkmm2ext::container_clear (_strip_table, true);

	_strip_table.set_spacings (3);

	Glib::RefPtr<SizeGroup> col_size_group (SizeGroup::create (SIZE_GROUP_HORIZONTAL));

	l = manage (new Label (string_compose ("<b>%1</b>", _("Local Track/Bus"))));
	l->set_use_markup ();
	_strip_table.attach (*l, 0, 1, 0, 1, Gtk::FILL, Gtk::SHRINK);
	col_size_group->add_widget (*l);

	l = manage (new Label (string_compose ("<b>%1</b>", _("External State"))));
	l->set_use_markup ();
	_strip_table.attach (*l, 2, 3, 0, 1, Gtk::FILL, Gtk::SHRINK);
	col_size_group->add_widget (*l);

	/* clang-format off */
	_strip_table.attach (*manage (new ArdourVSpacer (1.0)), 1, 2, 0, 1, SHRINK       , EXPAND | FILL, 8, 4);
	_strip_table.attach (*manage (new ArdourHSpacer (1.0)), 0, 4, 1, 2, EXPAND | FILL, SHRINK,        4, 8);
	/* clang-format on */

	const bool show_all_local_tracks = _show_all_toggle->get_active ();

	std::vector<std::pair<PBD::ID, PBD::ID>> sorted_map;

	if (show_all_local_tracks) {
		for (auto const& r : _route_map) {
			PBD::ID d (0);
			try {
				d = _import_map.at (r.first);
			} catch (...) {}
			sorted_map.push_back (make_pair (r.first, d));
		}
		for (auto const& i : _import_map) {
			if (_route_map.find (i.first) == _route_map.end ()) {
				sorted_map.push_back (i);
			}
		}
	} else {
		for (auto const& i : _import_map) {
			sorted_map.push_back (i);
		}
	}

	std::sort (sorted_map.begin (), sorted_map.end (), [=] (auto& a, auto& b) {
		try {
			return _route_map.at (a.first).pi.order () < _route_map.at (b.first).pi.order ();
		} catch (...) {
		}
		return a.first < b.first;
	});

	/* Refill table */
	int r = 1;
	for (auto& [rid, eid] : sorted_map /*_import_map*/) {
		bool is_new = _route_map.find (rid) == _route_map.end ();

		if (!is_new) {
			l = manage (new Label (_route_map.at (rid).name, 0, 0.5));
		} else {
			l = manage (new Label (_("<i>New Track</i>"), 0, 0.5));
			l->set_use_markup ();
		}

		++r;
		_strip_table.attach (*l, 0, 1, r, r + 1, EXPAND | FILL, SHRINK);

#if 0
		l = manage (new Label (_extern_map[eid], 1.0, 0.5));
		_strip_table.attach (*l, 2, 3, r, r + 1, EXPAND | FILL, SHRINK);
#else
		using namespace Menu_Helpers;
		ArdourDropdown* dd = manage (new ArdourDropdown ());
		if (show_all_local_tracks) {
			dd->add_menu_elem (MenuElem ("---", sigc::bind (sigc::mem_fun (*this, &StripImportDialog::change_mapping), dd, rid, PBD::ID (0), "---")));
		}
		for (auto& [eid, einfo] : _extern_map) {
			dd->add_menu_elem (MenuElem (Gtkmm2ext::markup_escape_text (einfo.name), sigc::bind (sigc::mem_fun (*this, &StripImportDialog::change_mapping), dd, rid, eid, einfo.name)));
		}
		assert (show_all_local_tracks || _extern_map.find (eid) != _extern_map.end ());
		try {
			dd->set_text (_extern_map.at (eid).name);
		} catch (std::out_of_range const&) {
			dd->set_text ("---");
		}
		_strip_table.attach (*dd, 2, 3, r, r + 1, EXPAND | FILL, SHRINK);
#endif

		if (show_all_local_tracks && !is_new) {
			continue;
		}
		ArdourButton* rm = manage (new ArdourButton ());
		rm->set_icon (ArdourIcon::CloseCross);
		rm->set_tweaks (ArdourButton::TrackHeader);
		rm->signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &StripImportDialog::remove_mapping), rid));
		_strip_table.attach (*rm, 3, 4, r, r + 1, Gtk::SHRINK, Gtk::SHRINK, 4, 0);
	}

	if (r > 1) {
		++r;
		_strip_table.attach (*manage (new ArdourVSpacer (1.0)), 1, 2, 2, r, SHRINK, EXPAND | FILL, 8, 4);
		++r;
		_strip_table.attach (*manage (new ArdourHSpacer (1.0)), 0, 4, r, r + 1, EXPAND | FILL, SHRINK, 4, 8);
	}

	++r;

	/* Add options */
	using namespace Menu_Helpers;

	_add_rid = PBD::ID (0);
	_add_eid = PBD::ID (0);

	int64_t next_id  = std::numeric_limits<uint64_t>::max () - 1;
	PBD::ID next_new = PBD::ID (next_id);

	while (_import_map.find (next_new) != _import_map.end ()) {
		--next_id;
		next_new = PBD::ID (next_id);
	}

	/* accumulate both dropdowns, so columns are equally spaced */
	std::vector<std::string> sizing_texts;

	_add_rid_dropdown = manage (new ArdourWidgets::ArdourDropdown ());
	_add_rid_dropdown->add_menu_elem (MenuElem (_(" -- New Track -- "), sigc::bind (sigc::mem_fun (*this, &StripImportDialog::prepare_mapping), false, next_new, _("New Track"))));
	sizing_texts.push_back (_(" -- New Track -- "));

	if (!show_all_local_tracks) {
		for (auto& [rid, rinfo] : _route_map) {
			if (_import_map.find (rid) != _import_map.end ()) {
				continue;
			}
			_add_rid_dropdown->add_menu_elem (MenuElem (Gtkmm2ext::markup_escape_text (rinfo.name), sigc::bind (sigc::mem_fun (*this, &StripImportDialog::prepare_mapping), false, rid, rinfo.name)));
			sizing_texts.push_back (rinfo.name);
		}
	}

	_add_eid_dropdown = manage (new ArdourWidgets::ArdourDropdown ());
	for (auto& [eid, einfo] : _extern_map) {
		_add_eid_dropdown->add_menu_elem (MenuElem (Gtkmm2ext::markup_escape_text (einfo.name), sigc::bind (sigc::mem_fun (*this, &StripImportDialog::prepare_mapping), true, eid, einfo.name)));
		sizing_texts.push_back (einfo.name);
	}

	_add_rid_dropdown->set_sizing_texts (sizing_texts);
	_add_eid_dropdown->set_sizing_texts (sizing_texts);
	col_size_group->add_widget (*_add_rid_dropdown);
	col_size_group->add_widget (*_add_eid_dropdown);

	_add_new_mapping = manage (new ArdourButton ());
	_add_new_mapping->set_icon (ArdourIcon::PlusSign);
	_add_new_mapping->set_tweaks (ArdourButton::TrackHeader);
	_add_new_mapping->signal_clicked.connect (sigc::mem_fun (*this, &StripImportDialog::add_mapping));

	/* clang-format off */
	_strip_table.attach (*_add_rid_dropdown, 0, 1, r, r + 1, EXPAND | FILL, SHRINK);
	_strip_table.attach (*_add_eid_dropdown, 2, 3, r, r + 1, EXPAND | FILL, SHRINK);
	_strip_table.attach (*_add_new_mapping,  3, 4, r, r + 1, Gtk::SHRINK,   SHRINK, 4, 0);
	/* clang-format on */

	bool can_add = !_add_rid_dropdown->items ().empty () && !_add_eid_dropdown->items ().empty ();

	_add_rid_dropdown->set_sensitive (can_add);
	_add_eid_dropdown->set_sensitive (can_add);
	_add_new_mapping->set_sensitive (false);

	_ok_button->set_sensitive (!_import_map.empty ());

	_strip_table.show_all ();
}

void
StripImportDialog::idle_refill_import_table ()
{
	Glib::signal_idle ().connect ([&] () { refill_import_table (); return false; }, Glib::PRIORITY_HIGH_IDLE + 10);
}

void
StripImportDialog::change_mapping (ArdourDropdown* dd, PBD::ID const& rid, PBD::ID const& eid, std::string const& name)
{
	if (eid == PBD::ID (0)) {
		_import_map.erase (rid);
	} else {
		_import_map[rid] = eid;
	}
	dd->set_text (name);

	if (_show_all_toggle->get_active ()) {
		idle_refill_import_table ();
	}
}

void
StripImportDialog::prepare_mapping (bool ext, PBD::ID const& id, std::string const& name)
{
	if (ext) {
		_add_eid_dropdown->set_text (name);
		_add_eid = id;
	} else {
		_add_rid_dropdown->set_text (name);
		_add_rid = id;
	}

	_add_new_mapping->set_sensitive (_add_rid != PBD::ID (0) && _add_eid != PBD::ID (0));
}

void
StripImportDialog::add_mapping ()
{
	assert (_add_rid != PBD::ID (0));
	assert (_add_eid != PBD::ID (0));

	_import_map[_add_rid] = _add_eid;

	idle_refill_import_table ();
}

void
StripImportDialog::remove_mapping (PBD::ID const& id)
{
	if (1 == _import_map.erase (id)) {
		idle_refill_import_table ();
	}
}

void
StripImportDialog::clear_mapping ()
{
	_import_map.clear ();
	idle_refill_import_table ();
}

void
StripImportDialog::import_all_strips (bool only_visible)
{
	_import_map.clear ();

	std::vector<std::pair<PBD::ID, PresentationInfo::order_t>> sorted_eid;

	for (auto& [eid, einfo] : _extern_map) {
		if (einfo.pi.special () || (only_visible && einfo.pi.hidden ())) {
			continue;
		}
#ifdef MIXBUS
		if (einfo.mixbus > 0) {
			continue;
		}
#endif
		sorted_eid.push_back (make_pair (eid, einfo.pi.order ()));
	}

	std::sort (sorted_eid.begin (), sorted_eid.end (), [=] (auto& a, auto& b) {
		return a.second < b.second;
	});

	int64_t next_id = std::numeric_limits<uint64_t>::max () - 1 - sorted_eid.size ();
	for (auto const& [eid, _] : sorted_eid) {
		PBD::ID next_new      = PBD::ID (next_id++);
		_import_map[next_new] = eid;
	}

	idle_refill_import_table ();
}

void
StripImportDialog::set_default_mapping (bool and_idle_update)
{
	_import_map.clear ();

	if (_match_pbd_id) {
		/* try a 1:1 mapping */
		for (auto& [eid, einfo] : _extern_map) {
			if (_route_map.find (eid) != _route_map.end ()) {
				_import_map[eid] = eid;
			}
		}
	} else {
		/* match by name */
		for (auto& [eid, einfo] : _extern_map) {
			// TODO consider building a reverse [pointer] map
			for (auto& [rid, rinfo] : _route_map) {
#ifdef MIXBUS
				if (einfo.mixbus > 0 && einfo.mixbus == rinfo.mixbus) {
					_import_map[rid] = eid;
					break;
				}
#endif
				if (einfo == rinfo) {
					_import_map[rid] = eid;
					break;
				}
			}
		}
	}
	if (and_idle_update) {
		idle_refill_import_table ();
	}
}

void
StripImportDialog::setup_strip_import_page ()
{
	_route_map.clear ();

	for (auto const& r : *_session->get_routes ()) {
		if (r->is_main_bus () && !r->is_master ()) {
			continue;
		}
#ifdef MIXBUS
		_route_map.emplace (r->id (), Session::RouteImportInfo (r->name (), r->presentation_info (), r->mixbus ()));
#else
		_route_map.emplace (r->id (), Session::RouteImportInfo (r->name (), r->presentation_info (), 0));
#endif
	}

	using namespace Menu_Helpers;
	_action = manage (new ArdourWidgets::ArdourDropdown ());
	_action->add_menu_elem (MenuElem (_("Clear Mapping"), sigc::mem_fun (*this, &StripImportDialog::clear_mapping)));
	_action->add_menu_elem (MenuElem (_("Import all as new tracks"), sigc::bind (sigc::mem_fun (*this, &StripImportDialog::import_all_strips), false)));
	_action->add_menu_elem (MenuElem (_("Import visible as new tracks"), sigc::bind (sigc::mem_fun (*this, &StripImportDialog::import_all_strips), true)));
	_action->add_menu_elem (MenuElem (_match_pbd_id ? _("Reset - auto-map by ID") : _("Reset - auto-map by name"), sigc::bind (mem_fun (*this, &StripImportDialog::set_default_mapping), true)));
	_action->set_text (_("Actions"));

	_show_all_toggle = new ArdourButton (_("Show all local tracks"), ArdourButton::led_default_elements, true);
	_show_all_toggle->set_led_left (true);
	_show_all_toggle->set_can_focus (true);
	_show_all_toggle->signal_clicked.connect (mem_fun (*this, &StripImportDialog::refill_import_table));

	_action_box.set_spacing (4);
	_action_box.pack_start (*_action, true, false);
	_action_box.pack_start (*_show_all_toggle, true, false);

	VBox* vbox = manage (new VBox ());
	vbox->pack_start (_strip_table, false, false, 4);

	_strip_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	_strip_scroller.add (*vbox);

	_page_strip.set_spacing (4);
	_page_strip.pack_start (_strip_scroller);
	_page_strip.pack_end (_progress_bar, false, false, 4);
	_page_strip.pack_end (_action_box, false, false, 4);
	_page_strip.show_all ();

	_ok_button->set_sensitive (true);

	set_default_mapping (false);
	refill_import_table ();
}

void
StripImportDialog::ok_activated ()
{
	ArdourDialog::response (RESPONSE_ACCEPT);
}

void
StripImportDialog::on_response (int response_id)
{
	_cancel_button->set_sensitive (false);
	_ok_button->set_sensitive (false);
}

bool
StripImportDialog::on_delete_event (GdkEventAny* ev)
{
	if (!_cancel_button->sensitive ()) {
		return true;
	}
	return ArdourDialog::on_delete_event (ev);
}

void
StripImportDialog::do_import ()
{
	_session->import_route_state (_path, _import_map, Session::CreateRouteGroup, this);
}

void
StripImportDialog::update_progress_gui (float p)
{
	_action_box.hide ();
	_progress_bar.show ();
	if (p == 0) {
		_progress_bar.set_text (_("Importing Track State"));
	}
	_progress_bar.set_fraction (p);
}
