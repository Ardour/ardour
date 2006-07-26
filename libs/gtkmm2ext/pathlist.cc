/*
    Copyright (C) 2006 Paul Davis 

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

#include <gtkmm2ext/pathlist.h>

#include "i18n.h"

using namespace std;
using namespace Gtkmm2ext;

PathList::PathList ()
	:
	add_btn(_("+")),
	subtract_btn(_("-")),
	path_columns(),
	_store(Gtk::ListStore::create(path_columns)),
	_view(_store)
{
	_view.append_column(_("Paths"), path_columns.paths);
	_view.set_size_request(-1, 100);
	_view.set_headers_visible (false);
	
	Gtk::ScrolledWindow* scroll = manage(new Gtk::ScrolledWindow);
	scroll->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroll->add(_view);
	
	add (*scroll);
	
	Gtk::HBox* btn_box = manage(new Gtk::HBox);
	btn_box->add(add_btn);
	btn_box->add(subtract_btn);

	add (*btn_box);
	
	add_btn.signal_clicked().connect (mem_fun(*this, &PathList::add_btn_clicked));
	subtract_btn.signal_clicked().connect (mem_fun(*this, &PathList::subtract_btn_clicked));
	_view.get_selection()->signal_changed().connect (mem_fun(*this, &PathList::selection_changed));
}

vector<string>
PathList::get_paths ()
{
	vector<string> paths;
	
	Gtk::TreeModel::Children children(_store->children());
	
	for (Gtk::TreeIter iter = children.begin(); iter != children.end(); ++iter) {
		Gtk::ListStore::Row row = *iter;
		
		paths.push_back(row[path_columns.paths]);
	}
	
	return paths;
}

void
PathList::set_paths (vector<string> paths)
{
	_store.clear();
	
	for (vector<string>::iterator i = paths.begin(); i != paths.end(); ++i) {
		Gtk::ListStore::iterator iter = _store->append();
		Gtk::ListStore::Row row = *iter;
		row[path_columns.paths] = *i;
	}
}

void
PathList::add_btn_clicked ()
{
	Gtk::FileChooserDialog path_chooser (_("Path Chooser"), Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	
	path_chooser.add_button (Gtk::Stock::ADD, Gtk::RESPONSE_OK);
	path_chooser.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	
	int result = path_chooser.run ();
	
	if (result == Gtk::RESPONSE_OK) {
		string pathname = path_chooser.get_filename();
		
		if (pathname.length ()) {
			Gtk::ListStore::iterator iter = _store->append ();
			Gtk::ListStore::Row row = *iter;
			row[path_columns.paths] = pathname;
			
			paths_updated (); // EMIT_SIGNAL
		}
	}
}

void
PathList::subtract_btn_clicked ()
{
	Gtk::ListStore::iterator iter = _view.get_selection()->get_selected();
	_store->erase (iter);
	
	paths_updated (); // EMIT_SIGNAL
}

void
PathList::selection_changed ()
{
	if (_view.get_selection()->count_selected_rows ()) {
		subtract_btn.set_sensitive (true);
	} else {
		subtract_btn.set_sensitive (false);
	}
}
