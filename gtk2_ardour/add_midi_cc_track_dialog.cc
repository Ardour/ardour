/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>
#include <gtkmm/stock.h>
#include <pbd/error.h>
#include <pbd/convert.h>

#include "utils.h"
#include "add_midi_cc_track_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace sigc;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

AddMidiCCTrackDialog::AddMidiCCTrackDialog ()
	: Dialog (_("ardour: add midi controller track")),
	  _cc_num_adjustment (1, 0, 127, 1, 10),
	  _cc_num_spinner (_cc_num_adjustment)
{
	set_name ("AddMidiCCTrackDialog");
	set_wmclass (X_("ardour_add_track_bus"), "Ardour");
	set_position (Gtk::WIN_POS_MOUSE);
	set_resizable (false);


	_cc_num_spinner.set_name ("AddMidiCCTrackDialogSpinner");
	
	HBox *hbox = manage (new HBox());
	Label *label = manage(new Label("Controller Number: "));

	hbox->pack_start(*label, true, true, 4);
	hbox->pack_start(_cc_num_spinner, false, false, 4);

	get_vbox()->pack_start(*hbox, true, true, 4);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);

	_cc_num_spinner.show();
	hbox->show();
	label->show();
}


ARDOUR::Parameter
AddMidiCCTrackDialog::parameter ()
{
	int cc_num = _cc_num_spinner.get_value_as_int();

	return Parameter(MidiCCAutomation, cc_num);
}

