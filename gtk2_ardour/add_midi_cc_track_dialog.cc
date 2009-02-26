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
#include <cassert>

#include <sigc++/bind.h>
#include <gtkmm/stock.h>
#include "pbd/error.h"
#include "pbd/convert.h"

#include "utils.h"
#include "add_midi_cc_track_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace sigc;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

AddMidiCCTrackDialog::AddMidiCCTrackDialog ()
	: Dialog (_("ardour: add midi controller track"))
	, _chan_adjustment (1, 1, 16, 1, 8, 8)
	, _chan_spinner (_chan_adjustment)
	, _cc_num_adjustment (1, 1, 128, 1, 10, 10)
	, _cc_num_spinner (_cc_num_adjustment)
{
	set_name ("AddMidiCCTrackDialog");
	set_wmclass (X_("ardour_add_track_bus"), "Ardour");
	set_position (Gtk::WIN_POS_MOUSE);
	set_resizable (false);

	_chan_spinner.set_name ("AddMidiCCTrackDialogSpinner");
	_cc_num_spinner.set_name ("AddMidiCCTrackDialogSpinner");
	
	HBox *chan_box = manage (new HBox());
	Label *chan_label = manage(new Label("Channel: "));
	chan_box->pack_start(*chan_label, true, true, 4);
	chan_box->pack_start(_chan_spinner, false, false, 4);
	get_vbox()->pack_start(*chan_box, true, true, 4);

	HBox* num_box = manage (new HBox());
	Label *num_label = manage(new Label("Controller: "));
	num_box->pack_start(*num_label, true, true, 4);
	num_box->pack_start(_cc_num_spinner, false, false, 4);
	get_vbox()->pack_start(*num_box, true, true, 4);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);
	
	_chan_spinner.show();
	chan_box->show();
	chan_label->show();
	_cc_num_spinner.show();
	num_box->show();
	num_label->show();
}


Evoral::Parameter
AddMidiCCTrackDialog::parameter ()
{
	int chan   = _chan_spinner.get_value_as_int() - 1;
	int cc_num = _cc_num_spinner.get_value_as_int() - 1;

	return Evoral::Parameter(MidiCCAutomation, chan, cc_num);
}

