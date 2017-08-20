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

#include <map>
#include <cerrno>

#include <glib/gstdio.h>

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/frame.h>
#include <gtkmm/notebook.h>
#include <gtkmm/separator.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stock.h>
#include <gtkmm/treeiter.h>

#include "pbd/basename.h"
#include "pbd/error.h"
#include "pbd/file_archive.h"
#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/gui_thread.h"

#include "ardour/filesystem_paths.h"
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
	nb->append_page (*session_tm, _("Session Templates"));

	RouteTemplateManager* route_tm = manage (new RouteTemplateManager);
	nb->append_page (*route_tm, _("Track Templates"));

	get_vbox()->pack_start (*nb);
	add_button (_("Ok"), Gtk::RESPONSE_OK);

	get_vbox()->show_all();

	session_tm->init ();
	route_tm->init ();

	session_tm->TemplatesImported.connect (*this, invalidator (*this), boost::bind (&RouteTemplateManager::init, route_tm), gui_context ());
	route_tm->TemplatesImported.connect (*this, invalidator (*this), boost::bind (&SessionTemplateManager::init, session_tm), gui_context ());
}

TemplateManager::TemplateManager ()
	: HBox ()
	, ProgressReporter ()
	, _save_desc (_("Save Description"))
	, _desc_dirty (false)
	, _remove_button (_("Remove"))
	, _rename_button (_("Rename"))
	, _export_all_templates_button (_("Export all"))
	, _import_template_set_button (_("Import"))
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

	VBox* vb_btns = manage (new VBox);
	vb_btns->set_spacing (4);
	vb_btns->pack_start (_rename_button, false, false);
	vb_btns->pack_start (_remove_button, false, false);
	vb_btns->pack_start (_save_desc, false, false);

	_rename_button.set_sensitive (false);
	_rename_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::start_edit));
	_remove_button.set_sensitive (false);
	_remove_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::delete_selected_template));

	vb_btns->pack_start (*(manage (new VSeparator ())));

	vb_btns->pack_start (_export_all_templates_button, false, false);
	vb_btns->pack_start (_import_template_set_button, false, false);

	_export_all_templates_button.set_sensitive (false);
	_export_all_templates_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::export_all_templates));

	_import_template_set_button.set_sensitive (true);
	_import_template_set_button.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::import_template_set));

	set_spacing (6);

	VBox* vb = manage (new VBox);
	vb->pack_start (*sw);
	vb->pack_start (_progress_bar);

	Frame* desc_frame = manage (new Frame (_("Description")));

	_description_editor.set_wrap_mode (Gtk::WRAP_WORD);
	_description_editor.set_size_request (300,400);
	_description_editor.set_border_width (6);

	_save_desc.set_sensitive (false);
	_save_desc.signal_clicked().connect (sigc::mem_fun (*this, &TemplateManager::save_template_desc));

	_description_editor.get_buffer()->signal_changed().connect (sigc::mem_fun (*this, &TemplateManager::set_desc_dirty));

	desc_frame->add (_description_editor);

	pack_start (*vb);
	pack_start (*desc_frame);
	pack_start (*vb_btns);

	show_all_children ();
	_progress_bar.hide ();
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
		row[_template_columns.description] = it->description;
	}

	_export_all_templates_button.set_sensitive (!templates.empty ());
}

void
TemplateManager::row_selection_changed ()
{
	bool has_selection = false;
	if (_template_treeview.get_selection()->count_selected_rows () != 0) {
		Gtk::TreeModel::const_iterator it = _template_treeview.get_selection()->get_selected ();
		if (it) {
			has_selection = true;
			const string desc = it->get_value (_template_columns.description);
			_description_editor.get_buffer()->set_text (desc);
			_desc_dirty = false;
			_save_desc.set_sensitive (false);
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

void
TemplateManager::set_desc_dirty ()
{
	_desc_dirty = true;
	_save_desc.set_sensitive (true);
}

void
TemplateManager::save_template_desc ()
{
	const Gtk::TreeModel::const_iterator it = _template_treeview.get_selection()->get_selected ();
	const string file_path = template_file (it);

	const string desc_txt = _description_editor.get_buffer()->get_text ();
	it->set_value (_template_columns.description, desc_txt);

	XMLTree tree;

	if (!tree.read(file_path)) {
		error << string_compose (_("Could not parse template file \"%1\"."), file_path) << endmsg;
		return;
	}

	tree.root()->remove_nodes (X_("description"));
	XMLNode* desc = new XMLNode (X_("description"));

	XMLNode* dn = new XMLNode (X_("content"), desc_txt);
	desc->add_child_nocopy (*dn);

	tree.root()->add_child_nocopy (*desc);

	if (!tree.write ()) {
		error << string_compose(X_("Could not write to template file \"%1\"."), file_path) << endmsg;
		return;
	}

	_save_desc.set_sensitive (false);
	_desc_dirty = false;
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

static
bool accept_all_files (string const &, void *)
{
	return true;
}

static void _set_progress (Progress* p, size_t n, size_t t)
{
	p->set_progress (float (n) / float(t));
}


void
TemplateManager::export_all_templates ()
{
	GError* err = NULL;
	char* td = g_dir_make_tmp ("ardour-templates-XXXXXX", &err);

	if (!td) {
		error << string_compose(_("Could not make tmpdir: %1"), err->message) << endmsg;
		return;
	}
	const string tmpdir (td);
	g_free (td);
	g_clear_error (&err);

	FileChooserDialog dialog(_("Save Exported Template Archive"), FILE_CHOOSER_ACTION_SAVE);
	dialog.set_filename (X_("templates.tar.xz"));

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	FileFilter archive_filter;
	archive_filter.add_pattern (X_("*.tar.xz"));
	archive_filter.set_name (_("Template archives"));
	dialog.add_filter (archive_filter);

	int result = dialog.run ();

	if (result != RESPONSE_OK || !dialog.get_filename().length()) {
		PBD::remove_directory (tmpdir);
		return;
	}

	string filename = dialog.get_filename ();
	if (filename.compare (filename.size () - 7, 7, ".tar.xz")) {
		filename += ".tar.xz";
	}

	if (g_file_test (filename.c_str(), G_FILE_TEST_EXISTS)) {
		ArdourDialog dlg (_("File exists"), true);
		Label msg (string_compose (_("The file %1 already exists."), filename));
		dlg.get_vbox()->pack_start (msg);
		msg.show ();
		dlg.add_button (_("Overwrite"), RESPONSE_ACCEPT);
		dlg.add_button (_("Cancel"), RESPONSE_REJECT);
		dlg.set_default_response (RESPONSE_REJECT);

		result = dlg.run ();

		if (result == RESPONSE_REJECT) {
			PBD::remove_directory (tmpdir);
			return;
		}
	}

	PBD::copy_recurse (templates_dir (), Glib::build_filename (tmpdir, Glib::path_get_basename (templates_dir ())));

	vector<string> files;
	PBD::find_files_matching_regex (files, tmpdir, string ("\\.template$"), /* recurse = */ true);

	vector<string>::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		const string bn = PBD::basename_nosuffix (*it);
		const string old_path = Glib::build_filename (templates_dir (), bn);
		const string new_path = Glib::build_filename ("$TEMPLATEDIR", bn);

		XMLTree tree;
		if (!tree.read (*it)) {
			continue;
		}
		if (adjust_xml_tree (tree, old_path, new_path)) {
			tree.write (*it);
		}
	}

	find_files_matching_filter (files, tmpdir, accept_all_files, 0, false, true, true);

	std::map<std::string, std::string> filemap;
	for (it = files.begin(); it != files.end(); ++it) {
		filemap[*it] = it->substr (tmpdir.size()+1, it->size() - tmpdir.size() - 1);
	}

	_current_action = _("Exporting templates");

	PBD::FileArchive ar (filename);
	PBD::ScopedConnectionList progress_connection;
	ar.progress.connect_same_thread (progress_connection, boost::bind (&_set_progress, this, _1, _2));
	ar.create (filemap);

	PBD::remove_directory (tmpdir);
}

void
TemplateManager::import_template_set ()
{
	FileChooserDialog dialog (_("Import template archives"));
	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	FileFilter archive_filter;
	archive_filter.add_pattern (X_("*.tar.xz"));
	archive_filter.set_name (_("Template archives"));
	dialog.add_filter (archive_filter);

	int result = dialog.run ();

	if (result != RESPONSE_OK || !dialog.get_filename().length()) {
		return;
	}

	_current_action = _("Importing templates");

	FileArchive ar (dialog.get_filename ());
	PBD::ScopedConnectionList progress_connection;
	ar.progress.connect_same_thread (progress_connection, boost::bind (&_set_progress, this, _1, _2));
	ar.inflate (user_config_directory ());

	vector<string> files;
	PBD::find_files_matching_regex (files, templates_dir (), string ("\\.template$"), /* recurse = */ true);

	vector<string>::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		const string bn = PBD::basename_nosuffix (*it);
		const string old_path = Glib::build_filename ("$TEMPLATEDIR", bn);
		const string new_path = Glib::build_filename (templates_dir (), bn);

		XMLTree tree;
		if (!tree.read (*it)) {
			continue;
		}
		if (adjust_xml_tree (tree, old_path, new_path)) {
			tree.write (*it);
		}
	}

	init ();
	TemplatesImported (); /* emit signal */
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

void
TemplateManager::update_progress_gui (float p)
{
	if (p >= 1.0) {
		_progress_bar.hide ();
		return;
	}

	_progress_bar.show ();
	_progress_bar.set_text (_current_action);
	_progress_bar.set_fraction (p);
}


void SessionTemplateManager::init ()
{
	vector<TemplateInfo> templates;
	find_session_templates (templates, /* read_xml = */ true);
	setup_model (templates);

	_progress_bar.hide ();
}

void RouteTemplateManager::init ()
{
	vector<TemplateInfo> templates;
	find_route_templates (templates);
	setup_model (templates);

	_progress_bar.hide ();
}

#include <cerrno>

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

string
SessionTemplateManager::templates_dir () const
{
	return user_template_directory ();
}


string
SessionTemplateManager::template_file (const TreeModel::const_iterator& item) const
{
	const string path = item->get_value (_template_columns.path);
	const string name = item->get_value (_template_columns.name);
	return Glib::build_filename (path, name+".template");
}

bool
SessionTemplateManager::adjust_xml_tree (XMLTree& tree, const std::string& old_name, const std::string& new_name) const
{
	bool adjusted = false;
	XMLNode* root = tree.root();

	const XMLNode* const routes_node = root->child (X_("Routes"));
	if (routes_node) {
		const XMLNodeList& routes = routes_node->children (X_("Route"));
		XMLNodeConstIterator rit;
		for (rit = routes.begin(); rit != routes.end(); ++rit) {
			if (adjust_plugin_paths (*rit, old_name, new_name)) {
				adjusted = true;
			}
		}
	}

	return adjusted;
}


void
RouteTemplateManager::rename_template (TreeModel::iterator& item, const Glib::ustring& new_name)
{
	const string old_name = item->get_value (_template_columns.name);
	const string old_filepath = item->get_value (_template_columns.path);
	const string new_filepath = Glib::build_filename (user_route_template_directory(), new_name+".template");

	XMLTree tree;
	if (!tree.read (old_filepath)) {
		error << string_compose (_("Could not parse template file \"%1\"."), old_filepath) << endmsg;
		return;
	}
	tree.root()->set_property (X_("name"), new_name);
	tree.root()->children().front()->set_property (X_("name"), new_name);

	const bool adjusted = adjust_plugin_paths (tree.root(), old_name, string (new_name));

	const string old_state_dir = Glib::build_filename (user_route_template_directory(), old_name);
	const string new_state_dir = Glib::build_filename (user_route_template_directory(), new_name);

	if (adjusted) {
		if (g_file_test (old_state_dir.c_str(), G_FILE_TEST_EXISTS)) {
			if (g_rename (old_state_dir.c_str(), new_state_dir.c_str()) != 0) {
				error << string_compose (_("Could not rename state dir \"%1\" to \"%22\": %3"), old_state_dir, new_state_dir, strerror (errno)) << endmsg;
				return;
			}
		}
	}

	tree.set_filename (new_filepath);

	if (!tree.write ()) {
		error << string_compose(_("Could not write new template file \"%1\"."), new_filepath) << endmsg;
		if (adjusted) {
			g_rename (new_state_dir.c_str(), old_state_dir.c_str());
		}
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

string
RouteTemplateManager::templates_dir () const
{
	return user_route_template_directory ();
}


string
RouteTemplateManager::template_file (const TreeModel::const_iterator& item) const
{
	return item->get_value (_template_columns.path);
}

bool
RouteTemplateManager::adjust_xml_tree (XMLTree& tree, const std::string& old_name, const std::string& new_name) const
{
	return adjust_plugin_paths (tree.root(), old_name, string (new_name));
}
