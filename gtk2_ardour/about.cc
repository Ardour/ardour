/*
    Copyright (C) 2003 Paul Davis

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

#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"
#include "about.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

About::About ()
	: ArdourDialog (_("About Tracks"))
{
	set_modal (true);
	set_resizable (false);
    
	get_vbox()->set_spacing (0);

	string image_path;
    
	if (find_file_in_search_path (ardour_data_search_path(), "splash.png", image_path)) {
		Gtk::Image* image;
		if ((image = manage (new Gtk::Image (image_path))) != 0) {
			get_vbox()->pack_start (*image, false, false);
		}
	}
    
    add_button("CLOSE", RESPONSE_OK);
    set_default_response(RESPONSE_OK);
	show_all ();
}
void
About::on_response (int response_id)
{
    ArdourDialog::on_response (response_id);
    switch (response_id) {
        case RESPONSE_OK:
            hide ();
            break;
    }
}

bool About::close_button_pressed (GdkEventButton*)
{
    hide();
}

About::~About ()
{
}
