/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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
#ifndef _WIDGETS_PATHS_DIALOG_H_
#define _WIDGETS_PATHS_DIALOG_H_

#include <string>
#include <vector>

#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <gtkmm/listviewtext.h>

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API PathsDialog : public Gtk::Dialog
{
public:
	PathsDialog (Gtk::Window& parent, std::string, std::string current_paths = "", std::string default_paths = "");
	~PathsDialog ();

	std::string get_serialized_paths ();

private:
	void on_show ();

	Gtk::ListViewText  paths_list_view;

	Gtk::Button    add_path_button;
	Gtk::Button    remove_path_button;
	Gtk::Button    set_default_button;

	void selection_changed();
	void add_path();
	void remove_path();
	void set_default();

	std::string _default_paths;
};

} /* namespace */

#endif
