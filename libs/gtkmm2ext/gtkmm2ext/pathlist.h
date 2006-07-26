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

#ifndef __gtkmm2ext_pathlist_h__
#define __gtkmm2ext_pathlist_h__

#include <vector>
#include <string>

#include <gtkmm.h>

namespace Gtkmm2ext {

class PathList : public Gtk::VBox
{
  public:
	PathList ();
	~PathList () {};
		
	std::vector<std::string> get_paths ();
	void set_paths (std::vector<std::string> paths);
	
	sigc::signal<void> paths_updated;
	
  protected:
	Gtk::Button    add_btn;
	Gtk::Button    subtract_btn;

	void add_btn_clicked ();
	void subtract_btn_clicked ();

  private:
	struct PathColumns : public Gtk::TreeModel::ColumnRecord {
		PathColumns() { add (paths); }
		Gtk::TreeModelColumn<std::string> paths;
	};
	PathColumns path_columns;
	
	Glib::RefPtr<Gtk::ListStore> _store;
	Gtk::TreeView  _view;
	
	void selection_changed ();
};

} // namespace Gtkmm2ext

#endif // __gtkmm2ext_pathlist_h__
