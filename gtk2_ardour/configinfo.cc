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

#include <gtkmm.h>

#include "ardour/ardour.h"

#include "configinfo.h"
#include "i18n.h"

ConfigInfoDialog::ConfigInfoDialog ()
	: ArdourDialog (_("Build Configuration"))
{
	set_border_width (12);
	text.get_buffer()->set_text (std::string (ARDOUR::ardour_config_info));
	text.set_wrap_mode (Gtk::WRAP_WORD);
	text.show ();

	scroller.set_shadow_type(Gtk::SHADOW_NONE);
	scroller.set_border_width(0);
	scroller.add (text);
	scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	scroller.show();

	get_vbox()->pack_start (scroller, true, true);
	set_size_request (400, 600);

	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_ACCEPT);
}
