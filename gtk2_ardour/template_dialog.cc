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
	: ArdourDialog (_("Manage Templates"))
	, _remove_button (_("Remove"))
	, _rename_button (_("Rename"))
{
	_session_template_model = ListStore::create (_session_template_columns);
	setup_session_templates ();
	_session_template_treeview.set_model (_session_template_model);

	_validated_column.set_title (_("Template Name"));
	_validated_column.pack_start (_validating_cellrenderer);
	_session_template_treeview.append_column (_validated_column);
	_validating_cellrenderer.property_editable() = true;

	_validated_column.set_cell_data_func (_validating_cellrenderer, sigc::mem_fun (*this, &TemplateDialog::render_template_names));
	_validating_cellrenderer.signal_edited().connect (sigc::mem_fun (*this, &TemplateDialog::validate_edit));
	_session_template_treeview.signal_cursor_changed().connect (sigc::mem_fun (*this, &TemplateDialog::row_selection_changed));
	_session_template_treeview.signal_key_press_event().connect (sigc::mem_fun (*this, &TemplateDialog::key_event));

	ScrolledWindow* sw = manage (new ScrolledWindow);
	sw->add (_session_template_treeview);
	sw->set_size_request (300, 200);


	VBox* vb = manage (new VBox);
	vb->pack_start (_rename_button, false, false);
	vb->pack_start (_remove_button, false, false);

	_rename_button.set_sensitive (false);
	_rename_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateDialog::start_edit));
	_remove_button.set_sensitive (false);
	_remove_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateDialog::delete_selected_template));

	HBox* hb = manage (new HBox);
	hb->pack_start (*sw);
	hb->pack_start (*vb);

	get_vbox()->pack_start (*hb);

	show_all_children ();

	add_button (_("Ok"), Gtk::RESPONSE_OK);
}

void
TemplateDialog::setup_session_templates ()
{
	vector<TemplateInfo> templates;
	find_session_templates (templates);

	_session_template_model->clear ();

	for (vector<TemplateInfo>::iterator it = templates.begin(); it != templates.end(); ++it) {
		TreeModel::Row row;
		row = *(_session_template_model->append ());

		row[_session_template_columns.name] = it->name;
		row[_session_template_columns.path] = it->path;
	}
}

void
TemplateDialog::row_selection_changed ()
{
	bool has_selection = false;
	if (_session_template_treeview.get_selection()->count_selected_rows () != 0) {
		Gtk::TreeModel::const_iterator it = _session_template_treeview.get_selection()->get_selected ();
		if (it) {
			has_selection = true;
		}
	}

	_rename_button.set_sensitive (has_selection);
	_remove_button.set_sensitive (has_selection);
}

void
TemplateDialog::render_template_names (Gtk::CellRenderer*, const Gtk::TreeModel::iterator& it)
{
	if (it) {
		_validating_cellrenderer.property_text () = it->get_value (_session_template_columns.name);
	}
}

void
TemplateDialog::validate_edit (const Glib::ustring& path_string, const Glib::ustring& new_name)
{
	const TreePath path (path_string);
	TreeModel::iterator current = _session_template_model->get_iter (path);

	if (current->get_value (_session_template_columns.name) == new_name) {
		return;
	}

	TreeModel::Children rows = _session_template_model->children ();

	bool found = false;
	for (TreeModel::Children::const_iterator it = rows.begin(); it != rows.end(); ++it) {
		if (it->get_value (_session_template_columns.name) == new_name) {
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
TemplateDialog::start_edit ()
{
	TreeModel::Path path;
	TreeViewColumn* col;
	_session_template_treeview.get_cursor (path, col);
	_session_template_treeview.set_cursor (path, *col, /*set_editing =*/ true);
}

void
TemplateDialog::delete_selected_template ()
{
	if (_session_template_treeview.get_selection()->count_selected_rows() == 0) {
		return;
	}

	Gtk::TreeModel::const_iterator it = _session_template_treeview.get_selection()->get_selected();

	if (!it) {
		return;
	}

	const string path = it->get_value (_session_template_columns.path);
	const string name = it->get_value (_session_template_columns.name);
	const string file_path = Glib::build_filename (path, name+".template");

	if (g_unlink (file_path.c_str()) != 0) {
		error << string_compose(_("Could not delete template file \"%1\": %2"), file_path, strerror (errno)) << endmsg;
		return;
	}

	if (g_rmdir (path.c_str()) != 0) {
		error << string_compose(_("Could not delete template directory \"%1\": %2"), path, strerror (errno)) << endmsg;
	}

	_session_template_model->erase (it);
	row_selection_changed ();
}

bool
TemplateDialog::key_event (GdkEventKey* ev)
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

void
TemplateDialog::rename_template (TreeModel::iterator& item, const Glib::ustring& new_name)
{
	const string path = item->get_value (_session_template_columns.path);
	const string name = item->get_value (_session_template_columns.name);

	const string old_filepath = Glib::build_filename (path, name+".template");
	const string new_filepath = Glib::build_filename (path, new_name+".template");
	const string new_path = Glib::build_filename (user_template_directory(), new_name);

	cout << old_filepath << " " << new_filepath << endl;
	cout << path << " " << new_path << endl;

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

	item->set_value (_session_template_columns.name, string(new_name));
	item->set_value (_session_template_columns.path, new_path);
}
