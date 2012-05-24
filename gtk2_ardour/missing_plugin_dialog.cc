/*
    Copyright (C) 2010 Paul Davis

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

#include <gtkmm/label.h>
#include "missing_plugin_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

MissingPluginDialog::MissingPluginDialog (Session * s, list<string> const & plugins)
        : ArdourDialog (_("Missing Plugins"), true, false)
{
        set_session (s);

        add_button (_("OK"), RESPONSE_OK);
        set_default_response (RESPONSE_OK);

	Label* m = manage (new Label);

	stringstream t;
	t << "This session contains the following plugins that cannot be found on this system:\n\n";

	for (list<string>::const_iterator i = plugins.begin(); i != plugins.end(); ++i) {
		t << *i << "\n";
	}

	t << "\nThose plugins and any following them on a track or buss have been disabled, and will be hidden.\n";
	t << "It is recommended that you install the missing plugins and re-load the session.\n";

        m->set_markup (t.str ());
        get_vbox()->pack_start (*m, false, false);

	show_all ();
}
