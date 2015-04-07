/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include <iostream>
#include <string>

#include <gtkmm/filechooserdialog.h>

#include "open_file_dialog.h"
#include "i18n.h"

using namespace std;

string 
ARDOUR::save_file_dialog (string /*initial_path*/, string title)
{
        Gtk::FileChooserDialog dialog (title, Gtk::FILE_CHOOSER_ACTION_SAVE);
        
	dialog.add_button (_("CANCEL"), Gtk::RESPONSE_CANCEL);
	dialog.add_button (_("OK"), Gtk::RESPONSE_OK);
        
        if (dialog.run() == Gtk::RESPONSE_OK) {
                return dialog.get_filename ();
	}

        return string();
}

std::string
ARDOUR::save_file_dialog (std::vector<std::string> /* extensions */, std::string /* initial_path */, std::string /* title */)
{
	return string();
}

string 
ARDOUR::open_file_dialog (string /*initial_path*/, string /*title*/)
{
        return string();
}

vector<string>
ARDOUR::open_file_dialog (vector<string> extensions, bool /* multiple_selection_allowed */, string /*initial_path*/, string /*title*/)
{
	vector<string> ret;

	return ret;
}

string 
ARDOUR::choose_folder_dialog (string /*initial_path*/, string /*title*/)
{
        return string();
}

