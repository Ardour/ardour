/*
    Copyright (C) 2010 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include "ardour_dialog.h"

namespace ARDOUR {
	class Plugin;
}

class EditPluginPresetsDialog : public ArdourDialog
{
public:
	EditPluginPresetsDialog (boost::shared_ptr<ARDOUR::Plugin>);

private:
	void setup_list ();
	void delete_presets ();
	void update_sensitivity ();

	boost::shared_ptr<ARDOUR::Plugin> _plugin;
	Gtk::ListViewText _list;
	Gtk::Button _delete;

	PBD::ScopedConnection _preset_added_connection;
	PBD::ScopedConnection _preset_removed_connection;
};
