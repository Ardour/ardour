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
using namespace std;

extern std::vector<string> channel_combo_strings;

AddRouteDialog::AddRouteDialog ()
	: Dialog (_("ardour: add track/bus")),
	  track_button (_("Tracks")),
	  bus_button (_("Busses")),
	  routes_adjustment (1, 1, 32, 1, 4),
	  routes_spinner (routes_adjustment)
{
	set_name ("AddRouteDialog");
	set_wmclass (X_("ardour_add_track_bus"), "Ardour");
	set_position (Gtk::WIN_POS_MOUSE);

	name_template_entry.set_name ("AddRouteDialogNameTemplateEntry");
	track_button.set_name ("AddRouteDialogRadioButton");
	bus_button.set_name ("AddRouteDialogRadioButton");
	routes_spinner.set_name ("AddRouteDialogSpinner");
	
	RadioButton::Group g = track_button.get_group();
	bus_button.set_group (g);
	track_button.set_active (true);

	HBox *hbrb = manage (new HBox);

	hbrb->set_spacing (6);
	hbrb->pack_start (*(manage (new Label (_("Add")))), false, false);
	hbrb->pack_start (routes_spinner, false, false);
	hbrb->pack_start (track_button, false, false);
	hbrb->pack_start (bus_button, false, false);

	set_popdown_strings (channel_combo, channel_combo_strings);
	channel_combo.set_active_text (channel_combo_strings.front());
	channel_combo.set_name (X_("ChannelCountSelector"));
	
#if NOT_USEFUL_YET
	HBox *hbnt = manage (new HBox);

	hbnt->pack_start (*(manage (new Label (_("Name (template)")))), false, false);
	hbnt->pack_start (name_template_entry, true, true);
#endif

	get_vbox()->pack_start (*hbrb, false, false);
	get_vbox()->pack_start (*(manage (new Label ("Channel configuration"))), false, false);
	get_vbox()->pack_start (channel_combo, false, false);
#if NOT_USEFUL_YET
	get_vbox()->pack_start (*hbnt, false, false);
#endif

	get_vbox()->show_all ();

	add_button (Stock::OK, RESPONSE_ACCEPT);
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
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
