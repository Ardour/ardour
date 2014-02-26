/*
    Copyright (C) 2014 Robin Gareus <robin@gareus.org>

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
#include <cstdio>

#include "pbd/tokenizer.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "i18n.h"
#include "paths_dialog.h"

using namespace Gtk;
using namespace std;
using namespace ARDOUR;

PathsDialog::PathsDialog (Session* s, std::string user_paths, std::string fixed_paths)
	: ArdourDialog (_("Set Paths"), true)
	, paths_list_view(2, false, Gtk::SELECTION_SINGLE)
	, add_path_button(_("Add"))
	, remove_path_button(_("Delete"))
{
	set_session (s);
	set_name ("PathsDialog");
	set_skip_taskbar_hint (true);
	set_resizable (true);
	set_size_request (400, -1);

	paths_list_view.set_border_width (4);

	ARDOUR_UI::instance()->set_tip (add_path_button, _("Add a new search path"));
	ARDOUR_UI::instance()->set_tip (remove_path_button, _("Remove selected search path"));

	add_path_button.signal_clicked().connect (sigc::mem_fun (*this, &PathsDialog::add_path));
	remove_path_button.signal_clicked().connect (sigc::mem_fun (*this, &PathsDialog::remove_path));
	remove_path_button.set_sensitive(false);

	paths_list_view.set_column_title(0,"Type");
	paths_list_view.set_column_title(1,"Path");

	/* TODO fill in Text View */
	std::vector <std::string> a = parse_path(user_paths);
	for(vector<std::string>::const_iterator i = a.begin(); i != a.end(); ++i) {
		int row = paths_list_view.append(_("user"));
		paths_list_view.set_text(row, 1, *i);
	}
	a = parse_path(fixed_paths);
	for(vector<std::string>::const_iterator i = a.begin(); i != a.end(); ++i) {
		int row = paths_list_view.append( _("sys"));
		paths_list_view.set_text(row, 1, *i);
	}

	paths_list_view.get_selection()->signal_changed().connect (mem_fun (*this, &PathsDialog::selection_changed));

	/* Overall layout */
	HBox *hbox = manage (new HBox);
	hbox->pack_start (paths_list_view, true, true);
	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*hbox, true, true);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_ACCEPT);
	get_action_area()->pack_start (add_path_button, false, false);
	get_action_area()->pack_start (remove_path_button, false, false);

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
PathsDialog::get_serialized_paths(bool include_fixed) {
	std::string path;
	for (unsigned int i = 0; i < paths_list_view.size(); ++i) {
		if (!include_fixed && paths_list_view.get_text(i, 0) != _("user")) continue;
		if (i > 0) path += G_SEARCHPATH_SEPARATOR;
		path += paths_list_view.get_text(i, 1);
	}
	return path;
}

void
PathsDialog::selection_changed () {
	std::vector<int> selection = paths_list_view.get_selected();
	if (selection.size() > 0) {
		const int row = selection.at(0);
		if (paths_list_view.get_text(row, 0) == _("user")) {
			remove_path_button.set_sensitive(true);
			return;
		}
	}
	remove_path_button.set_sensitive(false);
}

void
PathsDialog::add_path() {
	Gtk::FileChooserDialog d (_("Add folder to search path"), Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	d.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	d.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
	ResponseType r = (ResponseType) d.run ();
	if (r == Gtk::RESPONSE_OK) {
		std::string dir = d.get_filename();
		if (Glib::file_test (dir, Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
			paths_list_view.prepend(_("user"));
			paths_list_view.set_text(0, 1, dir);
		}
	}
}

void
PathsDialog::remove_path() {
	std::vector<int> selection = paths_list_view.get_selected();
	if (selection.size() != 1) { return ; }
	const int row = selection.at(0);
	if (paths_list_view.get_text(row, 0) != _("user")) { return ; }

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

const std::vector <std::string>
PathsDialog::parse_path(std::string path, bool check_if_exists) const
{
	vector <std::string> pathlist;
	vector <std::string> tmp;
	PBD::tokenize (path, string(G_SEARCHPATH_SEPARATOR_S), std::back_inserter (tmp));

	for(vector<std::string>::const_iterator i = tmp.begin(); i != tmp.end(); ++i) {
		if ((*i).empty()) continue;
		std::string dir;
#ifndef PLATFORM_WINDOWS
		if ((*i).substr(0,1) == "~") {
			dir = Glib::build_filename(Glib::get_home_dir(), (*i).substr(1));
		}
		else
#endif
		{
			dir = *i;
		}
		if (!check_if_exists || Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
			pathlist.push_back(dir);
		}
	}
  return pathlist;
}
