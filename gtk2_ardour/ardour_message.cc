/*
    Copyright (C) 2004 Paul Davis

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

    $Id$

*/

#include <gtkmm/stock.h>

#include "ardour_message.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;

ArdourMessage::ArdourMessage (Gtk::Window* parent, 
			      string name, string msg, 
			      bool grab_focus, bool auto_run)
	: ArdourDialog (name)
{
	label.set_text (msg);
	label.set_alignment (0.5, 0.5);
	label.set_name (X_("PrompterLabel"));

	get_vbox()->pack_start (label);

	Button* ok_button = add_button (Stock::OK, RESPONSE_ACCEPT);
	
	set_name (X_("Prompter"));
	set_position (Gtk::WIN_POS_MOUSE);
	set_modal (true);
	set_type_hint (Gdk::WINDOW_TYPE_HINT_MENU);

	if (grab_focus) {
		ok_button->grab_focus ();
	}

	if (parent) {
		set_transient_for (*parent);
	}

	if (auto_run) {
		run ();
	}
}

ArdourMessage::~ArdourMessage()
{
}
