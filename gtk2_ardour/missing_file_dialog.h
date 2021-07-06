/*
 * Copyright (C) 2010 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_missing_file_dialog_h__
#define __gtk_ardour_missing_file_dialog_h__

#include <string>
#include <gtkmm/label.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/radiobutton.h>

#include "ardour/types.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class Session;
}

class MissingFileDialog : public ArdourDialog
{
public:
	MissingFileDialog (Gtk::Window&, ARDOUR::Session*, const std::string& path, ARDOUR::DataType type);

	int get_action();

private:
	ARDOUR::DataType filetype;
	bool is_absolute_path;

	Gtk::FileChooserButton chooser;
	Gtk::RadioButton use_chosen;
	Gtk::RadioButton::Group choice_group;
	Gtk::RadioButton use_chosen_and_no_more_questions;
	Gtk::RadioButton stop_loading_button;
	Gtk::RadioButton all_missing_ok;
	Gtk::RadioButton this_missing_ok;
	Gtk::Label msg;

	void add_chosen ();
	void set_absolute ();
};

#endif /* __gtk_ardour_missing_file_dialog_h__ */
