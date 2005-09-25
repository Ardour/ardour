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

#include "ardour_message.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;


ArdourMessage::ArdourMessage (Gtk::Window* parent, 
			      string name, string msg, 
			      bool grab_focus, bool auto_run)
	: ArdourDialog (name),
	  ok_button (_("OK"))
{
	set_keyboard_input (true);

	label.set_text (msg);
	label.set_alignment (0.5, 0.5);
	label.set_name (X_("PrompterLabel"));
	
	ok_button.set_name ("EditorGTKButton");	
	ok_button.clicked.connect (bind (slot (*this, &ArdourDialog::stop), 1));
	
	packer.set_spacing (10);
	packer.set_border_width (10);
	packer.pack_start (label);
	packer.pack_start (ok_button);
	
	set_name (X_("Prompter"));
	set_position (GTK_WIN_POS_MOUSE);
	set_modal (true);
	add (packer);
	show_all ();
	
	realize();
	get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	if (grab_focus) {
		ok_button.grab_focus ();
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
