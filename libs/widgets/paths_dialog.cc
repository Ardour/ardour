/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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
#include <cstdio>

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/liststore.h>
#include <gtkmm/stock.h>
#include <gtkmm/treestore.h>

#include "pbd/i18n.h"
#include "pbd/pathexpand.h"

#include "gtkmm2ext/utils.h"

#include "widgets/paths_dialog.h"

using namespace Gtk;
using namespace std;
using namespace ArdourWidgets;

PathsDialog::PathsDialog (Gtk::Window& parent, std::string title, std::string current_paths, std::string default_paths)
	: Dialog (title, parent, true)
	, paths_list_view(1, false, Gtk::SELECTION_SINGLE)
	, add_path_button(_("Add"))
	, remove_path_button(_("Delete"))
	, set_default_button(_("Reset to Default"))
	, _default_paths(default_paths)
{
	set_name ("PathsDialog");
	set_skip_taskbar_hint (true);
	set_resizable (true);
	set_size_request (400, -1);

	paths_list_view.set_border_width (4);

	add_path_button.signal_clicked().connect (sigc::mem_fun (*this, &PathsDialog::add_path));
	remove_path_button.signal_clicked().connect (sigc::mem_fun (*this, &PathsDialog::remove_path));
	set_default_button.signal_clicked().connect (sigc::mem_fun (*this, &PathsDialog::set_default));
	remove_path_button.set_sensitive(false);

	paths_list_view.set_column_title(0,"Path");

	std::vector <std::string> a = PBD::parse_path(current_paths);
	for(vector<std::string>::const_iterator i = a.begin(); i != a.end(); ++i) {
		paths_list_view.append_text(*i);
	}

	paths_list_view.get_selection()->signal_changed().connect (mem_fun (*this, &PathsDialog::selection_changed));

	VBox *vbox = manage (new VBox);
	vbox->pack_start (add_path_button, false, false);
	vbox->pack_start (remove_path_button, false, false);
	vbox->pack_start (set_default_button, false, false);

	/* Overall layout */
	HBox *hbox = manage (new HBox);
	hbox->pack_start (*vbox, false, false);
	hbox->pack_start (paths_list_view, true, true); // TODO, wrap in scroll-area ?!
	hbox->set_spacing (4);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*hbox, true, true);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_ACCEPT);

	show_all_children ();
}

PathsDialog::~PathsDialog ()
{
}

void
PathsDialog::on_show() {
	Dialog::on_show ();
}

std::string
PathsDialog::get_serialized_paths() {
	std::string path;
	for (unsigned int i = 0; i < paths_list_view.size(); ++i) {
		if (i > 0) path += G_SEARCHPATH_SEPARATOR;
		path += paths_list_view.get_text(i, 0);
	}
	return path;
}

void
PathsDialog::selection_changed () {
	std::vector<int> selection = paths_list_view.get_selected();
	if (selection.size() > 0) {
		remove_path_button.set_sensitive(true);
	} else {
		remove_path_button.set_sensitive(false);
	}
}

void
PathsDialog::add_path() {
	Gtk::FileChooserDialog d (_("Add folder to search path"), Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	Gtkmm2ext::add_volume_shortcuts (d);

	std::vector<int> selection = paths_list_view.get_selected();
	if (selection.size() == 1 ) {
		d.set_current_folder(paths_list_view.get_text(selection.at(0), 0));
	}

	d.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	d.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
	ResponseType r = (ResponseType) d.run ();
	if (r == Gtk::RESPONSE_OK) {
		std::string dir = d.get_filename();
		if (Glib::file_test (dir, Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
			bool dup = false;
			for (unsigned int i = 0; i < paths_list_view.size(); ++i) {
				if (paths_list_view.get_text(i, 0) == dir) {
					dup = true;
					break;
				}
			}
			if (!dup) {
				paths_list_view.prepend_text(dir);
			}
		}
	}
}

void
PathsDialog::remove_path() {
	std::vector<int> selection = paths_list_view.get_selected();
	if (selection.size() == 0 ) { return ; }

	/* Gtk::ListViewText internals to delete row(s) */
	Gtk::TreeModel::iterator row_it = paths_list_view.get_selection()->get_selected();
	Glib::RefPtr<Gtk::TreeModel> reftm = paths_list_view.get_model();
	Glib::RefPtr<Gtk::TreeStore> refStore = Glib::RefPtr<Gtk::TreeStore>::cast_dynamic(reftm);
	if(refStore) {
		refStore->erase(row_it);
		return;
	}
	Glib::RefPtr<Gtk::ListStore> refLStore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic(reftm);
	if(refLStore){
		refLStore->erase(row_it);
		return;
	}
}

void
PathsDialog::set_default() {

	paths_list_view.clear_items();
	std::vector <std::string> a = PBD::parse_path(_default_paths);
	for(vector<std::string>::const_iterator i = a.begin(); i != a.end(); ++i) {
		paths_list_view.append_text(*i);
	}
}
