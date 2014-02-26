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
#ifndef __gtk_ardour_paths_dialog_h__
#define __gtk_ardour_paths_dialog_h__

#include <string>
#include <vector>
#include <gtkmm.h>

#include "ardour_dialog.h"

class PathsDialog : public ArdourDialog
{
  public:
	PathsDialog (ARDOUR::Session*, std::string, std::string);
	~PathsDialog ();

	std::string get_serialized_paths (bool include_fixed = false);

  private:
	void on_show ();

	Gtk::ListViewText  paths_list_view;

	Gtk::Button    add_path_button;
	Gtk::Button    remove_path_button;

	void selection_changed();
	void add_path();
	void remove_path();

	// TODO move to PBD ?
	const std::vector <std::string> parse_path(std::string path, bool check_if_exists = false) const;

};

#endif /* __gtk_ardour_paths_dialog_h__ */
