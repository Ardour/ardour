/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

#include <gtkmm/label.h>
#include <gtkmm2ext/choice.h>

using namespace std;
using namespace Gtkmm2ext;
using namespace sigc;
using namespace Gtk;

Choice::Choice (string prompt, vector<string> choices, bool center)
{
	int n;
	vector<string>::iterator i;
	
	if (center) {
		set_position (Gtk::WIN_POS_CENTER);
	} else {
		set_position (Gtk::WIN_POS_MOUSE);
	}

	set_name ("ChoiceWindow");

	Label* label = manage (new Label (prompt));
	label->show ();

	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (*label);
	
	set_has_separator (false);

	for (n = 0, i = choices.begin(); i != choices.end(); ++i, ++n) {
		add_button (*i, n);
	}
}

void
Choice::on_realize ()
{
	Gtk::Window::on_realize();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH));
}

Choice::~Choice ()
{
}
