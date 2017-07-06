/*
    Copyright (C) 2010 Paul Davis
    Author: Johannes Mueller

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

#include <glib/gstdio.h>

#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeiter.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/xml++.h"

#include "ardour/template_utils.h"

#include "template_dialog.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;

TemplateDialog::TemplateDialog ()
	: ArdourDialog ("Manage Templates")
{
	Notebook* nb = manage (new Notebook);

	SessionTemplateManager* session_tm = manage (new SessionTemplateManager);
	session_tm->init ();
	nb->append_page (*session_tm, _("Session Templates"));

	RouteTemplateManager* route_tm = manage (new RouteTemplateManager);
	route_tm->init ();
	nb->append_page (*route_tm, _("Track Templates"));

	get_vbox()->pack_start (*nb);
	add_button (_("Ok"), Gtk::RESPONSE_OK);

	show_all_children ();
}

TemplateManager::TemplateManager ()
	: HBox ()
	, _remove_button (_("Remove"))
	, _rename_button (_("Rename"))
{
	_template_model = ListStore::create (_template_columns);
	_template_treeview.set_model (_template_model);

	_validated_column.set_title (_("Template Name"));
	_validated_column.pack_start (_validating_cellrenderer);
	_template_treeview.append_column (_validated_column);
	_validating_cellrenderer.property_editable() = true;

	_validated_column.set_cell_data_func (_validating_cellrenderer, sigc::mem_fun (*this, &TemplateManager::render_template_names));
	_validating_cellrenderer.signal_edited().connect (sigc::mem_fun (*this, &TemplateManager::validate_edit));
	_template_treeview.signal_cursor_changed().connect (sigc::mem_fun (*this, &TemplateManager::row_selection_changed));
	_template_treeview.signal_key_press_event().connect (sigc::mem_fun (*this, &TemplateManager::key_event));

	ScrolledWindow* sw = manage (new ScrolledWindow);
	sw->property_hscrollbar_policy() = POLICY_AUTOMATIC;
	sw->add (_template_treeview);
	sw->set_size_request (300, 200);


	VBox* vb = manage (new VBox);
	vb->set_spacing (4);
	vb->pack_start (_rename_button, false, false);
	vb->pack_start (_remove_button, false, false);

	_rename_button.set_sensitive (false);
	_rename_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::start_edit));
	_remove_button.set_sensitive (false);
	_remove_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::delete_selected_template));

	set_spacing (6);
	pack_start (*sw);
	pack_start (*vb);

	show_all_children ();
}

void
TemplateManager::setup_model (const vector<TemplateInfo>& templates)
{
	_template_model->clear ();

	for (vector<TemplateInfo>::const_iterator it = templates.begin(); it != templates.end(); ++it) {
		TreeModel::Row row;
		row = *(_template_model->append ());

		row[_template_columns.name] = it->name;
		row[_template_columns.path] = it->path;
	}
}

void
TemplateManager::row_selection_changed ()
{
	bool has_selection = false;
	if (_template_treeview.get_selection()->count_selected_rows () != 0) {
		Gtk::TreeModel::const_iterator it = _template_treeview.get_selection()->get_selected ();
		if (it) {
			has_selection = true;
		}
	}

	_rename_button.set_sensitive (has_selection);
	_remove_button.set_sensitive (has_selection);
}

void
TemplateManager::render_template_names (Gtk::CellRenderer*, const Gtk::TreeModel::iterator& it)
{
	if (it) {
		_validating_cellrenderer.property_text () = it->get_value (_template_columns.name);
	}
}

void
TemplateManager::validate_edit (const Glib::ustring& path_string, const Glib::ustring& new_name)
{
	const TreePath path (path_string);
	TreeModel::iterator current = _template_model->get_iter (path);

	if (current->get_value (_template_columns.name) == new_name) {
		return;
	}

	TreeModel::Children rows = _template_model->children ();

	bool found = false;
	for (TreeModel::Children::const_iterator it = rows.begin(); it != rows.end(); ++it) {
		if (it->get_value (_template_columns.name) == new_name) {
			found = true;
			break;
		}
	}

	if (found) {
		error << string_compose (_("Template of name \"%1\" already exists"), new_name) << endmsg;
		return;
	}


	rename_template (current, new_name);
}

void
TemplateManager::start_edit ()
{
	TreeModel::Path path;
	TreeViewColumn* col;
	_template_treeview.get_cursor (path, col);
	_template_treeview.set_cursor (path, *col, /*set_editing =*/ true);
}

bool
TemplateManager::key_event (GdkEventKey* ev)
{
	if (ev->keyval == GDK_KEY_F2) {
		start_edit ();
		return true;
	}
	if (ev->keyval == GDK_KEY_Delete) {
		delete_selected_template ();
		return true;
	}

	return false;
}

bool
TemplateManager::adjust_plugin_paths (XMLNode* node, const string& name, const string& new_name) const
{
	bool adjusted = false;

	const XMLNodeList& procs = node->children (X_("Processor"));
	XMLNodeConstIterator pit;
	for (pit = procs.begin(); pit != procs.end(); ++pit) {
		XMLNode* lv2_node = (*pit)->child (X_("lv2"));
		if (!lv2_node) {
			continue;
		}
		string template_dir;

		if (!lv2_node->get_property (X_("template-dir"), template_dir)) {
			continue;
		}

		const int suffix_pos = template_dir.size() - name.size();
		if (suffix_pos < 0) {
			cerr << "Template name\"" << name << "\" longer than template-dir \"" << template_dir << "\", WTH?" << endl;
			continue;
		}

		if (template_dir.compare (suffix_pos, template_dir.size(), name)) {
			cerr << "Template name \"" << name << "\" no suffix of template-dir \"" << template_dir << "\"" << endl;
			continue;
		}

		const string new_template_dir = template_dir.substr (0, suffix_pos) + new_name;
		lv2_node->set_property (X_("template-dir"), new_template_dir);

		adjusted = true;
	}

	return adjusted;
}

void SessionTemplateManager::init ()
{
	vector<TemplateInfo> templates;
	find_session_templates (templates);
	setup_model (templates);
}

void RouteTemplateManager::init ()
{
	vector<TemplateInfo> templates;
	find_route_templates (templates);
	setup_model (templates);
}

void
SessionTemplateManager::rename_template (TreeModel::iterator& item, const Glib::ustring& new_name_)
{
	const string old_path = item->get_value (_template_columns.path);
	const string old_name = item->get_value (_template_columns.name);
	const string new_name = string (new_name_);

	const string old_file_old_path = Glib::build_filename (old_path, old_name+".template");

	XMLTree tree;

	if (!tree.read(old_file_old_path)) {
		error << string_compose (_("Could not parse template file \"%1\"."), old_file_old_path) << endmsg;
		return;
	}
	XMLNode* root = tree.root();

	const XMLNode* const routes_node = root->child (X_("Routes"));
	if (routes_node) {
		const XMLNodeList& routes = routes_node->children (X_("Route"));
		XMLNodeConstIterator rit;
		for (rit = routes.begin(); rit != routes.end(); ++rit) {
			adjust_plugin_paths (*rit, old_name, new_name);
		}
	}

	const string new_file_old_path = Glib::build_filename (old_path, new_name+".template");

	tree.set_filename (new_file_old_path);

	if (!tree.write ()) {
		error << string_compose(_("Could not write to new template file \"%1\"."), new_file_old_path);
		return;
	}

	const string new_path = Glib::build_filename (user_template_directory (), new_name);

	if (g_rename (old_path.c_str(), new_path.c_str()) != 0) {
		error << string_compose (_("Could not rename template directory from \"%1\" to \"%2\": %3"),
					 old_path, new_path, strerror (errno)) << endmsg;
		g_unlink (new_file_old_path.c_str());
		return;
	}

	const string old_file_new_path = Glib::build_filename (new_path, old_name+".template");
	if (g_unlink (old_file_new_path.c_str())) {
		error << string_compose (X_("Could not delete old template file \"%1\": %2"),
					 old_file_new_path, strerror (errno)) << endmsg;
	}

	item->set_value (_template_columns.name, new_name);
	item->set_value (_template_columns.path, new_path);
}


void
SessionTemplateManager::delete_selected_template ()
{
	if (_template_treeview.get_selection()->count_selected_rows() == 0) {
		return;
	}

	Gtk::TreeModel::const_iterator it = _template_treeview.get_selection()->get_selected();

	if (!it) {
		return;
	}

	PBD::remove_directory (it->get_value (_template_columns.path));

	_template_model->erase (it);
	row_selection_changed ();
}

void
RouteTemplateManager::rename_template (TreeModel::iterator& item, const Glib::ustring& new_name)
{
	const string name = item->get_value (_template_columns.name);
	const string old_filepath = item->get_value (_template_columns.path);
	const string new_filepath = Glib::build_filename (user_route_template_directory(), new_name+".template");

	XMLTree tree;
	if (!tree.read (old_filepath)) {
		error << string_compose (_("Could not parse template file \"%1\"."), old_filepath) << endmsg;
		return;
	}
	tree.root()->children().front()->set_property (X_("name"), new_name);

	const bool adjusted = adjust_plugin_paths (tree.root(), name, string (new_name));

	if (adjusted) {
		const string old_state_dir = Glib::build_filename (user_route_template_directory(), name);
		const string new_state_dir = Glib::build_filename (user_route_template_directory(), new_name);
		if (g_rename (old_state_dir.c_str(), new_state_dir.c_str()) != 0) {
			error << string_compose (_("Could not rename state dir \"%1\" to \"%22\": %3"), old_state_dir, new_state_dir, strerror (errno)) << endmsg;
			return;
		}
	}

	tree.set_filename (new_filepath);

	if (!tree.write ()) {
		error << string_compose(_("Could not write new template file \"%1\"."), new_filepath) << endmsg;
		return;
	}

	if (g_unlink (old_filepath.c_str()) != 0) {
		error << string_compose (_("Could not remove old template file \"%1\": %2"), old_filepath, strerror (errno)) << endmsg;
	}

	item->set_value (_template_columns.name, string (new_name));
	item->set_value (_template_columns.path, new_filepath);
}

void
RouteTemplateManager::delete_selected_template ()
{
	if (_template_treeview.get_selection()->count_selected_rows() == 0) {
		return;
	}

	Gtk::TreeModel::const_iterator it = _template_treeview.get_selection()->get_selected();

	if (!it) {
		return;
	}

	const string file_path = it->get_value (_template_columns.path);

	if (g_unlink (file_path.c_str()) != 0) {
		error << string_compose(_("Could not delete template file \"%1\": %2"), file_path, strerror (errno)) << endmsg;
		return;
	}
	PBD::remove_directory (Glib::build_filename (user_route_template_directory (), it->get_value (_template_columns.name)));

	_template_model->erase (it);
	row_selection_changed ();
}
