/*
    Copyright (C) 2003 Paul Davis 

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

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>

#include <gtkmm2ext/utils.h>

#include "utils.h"
#include "add_route_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

extern std::vector<string> channel_combo_strings;

AddRouteDialog::AddRouteDialog ()
	: ArdourDialog ("add route dialog"),
	  ok_button (_("OK")),
	  cancel_button (_("Cancel")),
	  track_button (_("Tracks")),
	  bus_button (_("Busses")),
	  routes_adjustment (1, 1, 32, 1, 4),
	  routes_spinner (routes_adjustment)
{
	set_name ("AddRouteDialog");
	set_title (_("ardour: add track/bus"));
	set_wmclass (X_("ardour_add_track_bus"), "Ardour");
	set_position (Gtk::WIN_POS_MOUSE);
	set_keyboard_input (true);

	name_template_entry.set_name ("AddRouteDialogNameTemplateEntry");
	track_button.set_name ("AddRouteDialogRadioButton");
	bus_button.set_name ("AddRouteDialogRadioButton");
	ok_button.set_name ("AddRouteDialogButton");
	cancel_button.set_name ("AddRouteDialogButton");
	routes_spinner.set_name ("AddRouteDialogSpinner");
	
	bus_button.set_group (track_button.get_group());
	track_button.set_active (true);

	HBox *hbrb = manage (new HBox);

	hbrb->set_spacing (6);
	hbrb->pack_start (*(manage (new Label (_("Add")))), false, false);
	hbrb->pack_start (routes_spinner, false, false);
	hbrb->pack_start (track_button, false, false);
	hbrb->pack_start (bus_button, false, false);

	set_popdown_strings (channel_combo, channel_combo_strings);
	channel_combo.set_name (X_("ChannelCountSelector"));
	
	VBox *vbcc = manage (new VBox);

	vbcc->set_spacing (6);
	vbcc->pack_start (*(manage (new Label ("Channel configuration"))), false, false);
	vbcc->pack_start (channel_combo, false, false);

#if NOT_USEFUL_YET
	HBox *hbnt = manage (new HBox);

	hbnt->pack_start (*(manage (new Label (_("Name (template)")))), false, false);
	hbnt->pack_start (name_template_entry, true, true);
#endif

	HBox* hbbut = manage (new HBox);

	set_size_request_to_display_given_text (ok_button, _("Cancel"), 20, 15); // this is cancel on purpose
	set_size_request_to_display_given_text (cancel_button, _("Cancel"), 20, 15);
	
	hbbut->set_homogeneous (true);
	hbbut->set_spacing (6);
	hbbut->pack_end (cancel_button, false, false);	
 	hbbut->pack_end (ok_button, false, false);

	HBox* hbbutouter = manage (new HBox);
	hbbutouter->set_border_width (12);
	hbbutouter->pack_end (*hbbut, false, false);

	VBox* vb2 = manage (new VBox);

	vb2->set_border_width (12);
	vb2->set_spacing (6);
	vb2->pack_start (*hbrb, false, false);
	vb2->pack_start (*vbcc, false, false);
#if NOT_USEFUL_YET
	vb2->pack_start (*hbnt, false, false);
#endif
	vb2->pack_start (*hbbutouter, false, false);

	add (*vb2);

	// delete_event.connect (mem_fun(*this, &ArdourDialog::wm_close_event));
	ok_button.signal_clicked().connect (bind (mem_fun(*this,  &ArdourDialog::stop), 0));
	cancel_button.signal_clicked().connect (bind (mem_fun(*this, &ArdourDialog::stop), 1));
}

AddRouteDialog::~AddRouteDialog ()
{
}

bool
AddRouteDialog::track ()
{
	return track_button.get_active ();
}

string
AddRouteDialog::name_template ()
{
	return name_template_entry.get_text ();
}

int
AddRouteDialog::count ()
{
	return (int) floor (routes_adjustment.get_value ());
}

int
AddRouteDialog::channels ()
{
	return channel_combo_get_channel_count (channel_combo);
}
