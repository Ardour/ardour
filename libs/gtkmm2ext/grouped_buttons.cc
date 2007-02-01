/*
    Copyright (C) 2001 Paul Davis 

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

#include <gtkmm.h>

#include <gtkmm2ext/grouped_buttons.h>

using namespace std;

GroupedButtons::GroupedButtons (vector<Gtk::ToggleButton *>& buttonset)
{
	uint32_t n = 0;

	buttons = buttonset;

	for (vector<Gtk::ToggleButton *>::iterator i = buttons.begin(); i != buttons.end(); ++i, ++n) {
		if ((*i)->get_active()) {
			current_active = n;
		}
		(*i)->signal_clicked().connect (sigc::bind (mem_fun (*this, &GroupedButtons::one_clicked), n));
	}
}

GroupedButtons::GroupedButtons (uint32_t nbuttons, uint32_t first_active)
{
	buttons.reserve (nbuttons);
	current_active = first_active;

	for (uint32_t n = 0; n < nbuttons; ++n) {

		Gtk::ToggleButton *button;
		
		button = manage (new (Gtk::ToggleButton));
		
		if (n == current_active) {
			button->set_active (true);
		} 

		button->signal_clicked().connect (sigc::bind (mem_fun (*this, &GroupedButtons::one_clicked), n));
		buttons.push_back (button);
	}
}

static gint
reactivate_button (void *arg)
{
	Gtk::ToggleButton *b = (Gtk::ToggleButton *) arg;
	b->set_active (true);
	return FALSE;
}

void
GroupedButtons::one_clicked (uint32_t which)
{
	if (buttons[which]->get_active()) {

		if (which != current_active) {
			uint32_t old = current_active;
			current_active = which;
			buttons[old]->set_active (false);
		}

	} else if (which == current_active) {

		/* Someobody tried to unset the current active
		   button by clicking on it. This caused
		   set_active (false) to be called. We don't
		   allow that, so just reactivate it.

		   Don't try this right here, because of some
		   design glitches with GTK+ toggle buttons.
		   Setting the button back to active from
		   within the signal emission that marked
		   it as inactive causes a segfault ...
		*/

		gtk_idle_add (reactivate_button, buttons[which]);
	}
}
