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

#include <gtkmm/radiobutton.h>
#include "ardour_dialog.h"

class AmbiguousFileDialog : public ArdourDialog
{
public:
	AmbiguousFileDialog (const std::string &, const std::vector<std::string> &);

	int get_which () const;

private:
	Gtk::RadioButtonGroup _group;
	std::vector<Gtk::RadioButton*> _radio_buttons;
        bool rb_button_press (GdkEventButton*);
};


