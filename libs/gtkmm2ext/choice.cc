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

#include <gtkmm2ext/choice.h>

using namespace std;
using namespace Gtkmm2ext;
using namespace sigc;
using namespace Gtk;

Choice::Choice (string prompt,
		vector<string> choices)
	: Gtk::Window (WINDOW_TOPLEVEL),
	  prompt_label (prompt)
{
	int n;
	vector<string>::iterator i;

	set_position (WIN_POS_MOUSE);
	set_name ("ChoiceWindow");
	add (packer);
	
	packer.set_spacing (10);
	packer.set_border_width (10);
	packer.pack_start (prompt_label);
	packer.pack_start (button_packer);
	prompt_label.set_name ("ChoicePrompt");
	
	for (n = 0, i = choices.begin(); i != choices.end(); ++i, ++n) {
		Button *button = manage (new Gtk::Button (*i));
		button->set_name ("ChoiceButton");

		button_packer.set_spacing (5);
		button_packer.set_homogeneous (true);
		button_packer.pack_start (*button, false, true);

		button->signal_clicked().connect (bind (mem_fun (*this, &Choice::_choice_made), n));
		buttons.push_back (button);
	}

	signal_delete_event().connect(mem_fun(*this, &Choice::closed));

	packer.show_all ();
	which_choice = -1;
}

void
Choice::on_realize ()
{
	Gtk::Window::on_realize();
	Glib::RefPtr<Gdk::Window> win (get_window());
	win->set_decorations (Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH));
}

Choice::~Choice ()
{
}

void
Choice::_choice_made (int nbutton)
{
	which_choice = nbutton;
	choice_made (which_choice);
	chosen ();
}

gint
Choice::closed (GdkEventAny *ev)
{
	which_choice = -1;
	choice_made (which_choice);
	chosen ();
	return TRUE;
}

int
Choice::get_choice ()
{
	return which_choice;
}
