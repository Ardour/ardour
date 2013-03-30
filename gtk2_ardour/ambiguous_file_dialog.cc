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

#include <gtkmm/label.h>
#include "pbd/compose.h"
#include "ambiguous_file_dialog.h"
#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Gtk;

AmbiguousFileDialog::AmbiguousFileDialog (const string& file, const vector<string>& paths)
	: ArdourDialog (_("Ambiguous File"), true, false)
{
	get_vbox()->set_spacing (6);

	Label* l = manage (new Label);
	l->set_markup (string_compose (_("%1 has found the file <i>%2</i> in the following places:\n\n"), PROGRAM_NAME, file));
	get_vbox()->pack_start (*l);

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		_radio_buttons.push_back (manage (new RadioButton (_group, *i)));
		get_vbox()->pack_start (*_radio_buttons.back ());
		_radio_buttons.back()->signal_button_press_event().connect (sigc::mem_fun (*this, &AmbiguousFileDialog::rb_button_press), false);
	}

	get_vbox()->pack_start (*manage (new Label (_("\n\nPlease select the path that you want to get the file from."))));

	add_button (_("Done"), RESPONSE_OK);
	set_default_response (RESPONSE_OK);

	show_all ();
}

bool
AmbiguousFileDialog::rb_button_press (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS) {
		response (RESPONSE_OK);
	}
	return false;
}

int
AmbiguousFileDialog::get_which () const
{
	int i = 0;
	vector<RadioButton*>::const_iterator j = _radio_buttons.begin ();
	while (j != _radio_buttons.end() && !(*j)->get_active ()) {
		++i;
		++j;
	}

	if (j == _radio_buttons.end()) {
		return 0;
	}

	return i;
}
