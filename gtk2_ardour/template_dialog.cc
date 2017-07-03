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
#include "pbd/i18n.h"

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
SessionTemplateManager::rename_template (TreeModel::iterator& item, const Glib::ustring& new_name)
{
	const string path = item->get_value (_template_columns.path);
	const string name = item->get_value (_template_columns.name);

	const string old_filepath = Glib::build_filename (path, name+".template");
	const string new_filepath = Glib::build_filename (path, new_name+".template");
	const string new_path = Glib::build_filename (user_template_directory (), new_name);

	if (g_rename (old_filepath.c_str(), new_filepath.c_str()) != 0) {
		error << string_compose (_("Renaming of the template file failed: %1"), strerror (errno)) << endmsg;
		return;
	}

	if (g_rename (path.c_str(), new_path.c_str()) != 0) {
		error << string_compose (_("Renaming of the template directory failed: %1"), strerror (errno)) << endmsg;
		if (g_rename (new_filepath.c_str(), old_filepath.c_str()) != 0) {
			error << string_compose (_("Couldn't even undo renaming of template file: %1. Please examine the situation using filemanager or terminal."),
						 strerror (errno)) << endmsg;
		}
		return;
	}

	item->set_value (_template_columns.name, string (new_name));
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

	const string path = it->get_value (_template_columns.path);
	const string name = it->get_value (_template_columns.name);
	const string file_path = Glib::build_filename (path, name+".template");

	if (g_unlink (file_path.c_str()) != 0) {
		error << string_compose(_("Could not delete template file \"%1\": %2"), file_path, strerror (errno)) << endmsg;
		return;
	}

	if (g_rmdir (path.c_str()) != 0) {
		error << string_compose(_("Could not delete template directory \"%1\": %2"), path, strerror (errno)) << endmsg;
	}

	_template_model->erase (it);
	row_selection_changed ();
}

void
RouteTemplateManager::rename_template (TreeModel::iterator& item, const Glib::ustring& new_name)
{
	const string old_filepath = item->get_value (_template_columns.path);
	const string new_filepath = Glib::build_filename (user_route_template_directory(), new_name+".template");

	if (g_rename (old_filepath.c_str(), new_filepath.c_str()) != 0) {
		error << string_compose (_("Renaming of the template file failed: %1"), strerror (errno)) << endmsg;
		return;
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

	_template_model->erase (it);
	row_selection_changed ();
}
