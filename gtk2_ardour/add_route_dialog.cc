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
#include <gtkmm/stock.h>
#include <pbd/error.h>
#include <pbd/convert.h>
#include <gtkmm2ext/utils.h>

#include "utils.h"
#include "add_route_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;
using namespace std;

static const char* channel_setup_names[] = {
	"Mono",
	"Stereo",
	"3 Channels",
	"4 Channels",
	"6 Channels",
	"8 Channels",
	"Manual Setup",
	0
};

static const char* track_mode_names[] = {
	"Normal",
	"Tape",
	0
};

static vector<string> channel_combo_strings;
static vector<string> track_mode_strings;


AddRouteDialog::AddRouteDialog ()
	: Dialog (_("ardour: add track/bus")),
	  track_button (_("Tracks")),
	  bus_button (_("Busses")),
	  routes_adjustment (1, 1, 128, 1, 4),
	  routes_spinner (routes_adjustment)
{
	if (channel_combo_strings.empty()) {
		channel_combo_strings = PBD::internationalize (channel_setup_names);
	}

	if (track_mode_strings.empty()) {
		track_mode_strings = PBD::internationalize (track_mode_names);
	}

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
	hbrb->pack_start (routes_spinner, true, false, 5);
	hbrb->pack_start (track_button, true, false, 5);
	hbrb->pack_start (bus_button, true, false, 5);

	aframe.set_label (_("Add"));
	aframe.set_shadow_type (SHADOW_IN);
	aframe.add (*hbrb);

	set_popdown_strings (channel_combo, channel_combo_strings);
	set_popdown_strings (track_mode_combo, track_mode_strings);
	channel_combo.set_active_text (channel_combo_strings.front());
	channel_combo.set_name (X_("ChannelCountSelector"));

	track_button.signal_clicked().connect (mem_fun (*this, &AddRouteDialog::track_type_chosen));
	bus_button.signal_clicked().connect (mem_fun (*this, &AddRouteDialog::track_type_chosen));

	track_mode_combo.set_active_text (track_mode_strings.front());
	track_mode_combo.set_name (X_("ChannelCountSelector"));
	
#if NOT_USEFUL_YET
	HBox *hbnt = manage (new HBox);

	hbnt->pack_start (*(manage (new Label (_("Name (template)")))), false, false);
	hbnt->pack_start (name_template_entry, true, true);
#endif
	VBox *dvbox = manage (new VBox);
	HBox *dhbox = manage (new HBox);

        ccframe.set_label (_("Channel Configuration"));
	ccframe.set_shadow_type (SHADOW_IN);

	dvbox->pack_start (channel_combo, true, false, 5);
	dvbox->pack_start (track_mode_combo, true, false, 5);
	dhbox->pack_start (*dvbox, true, false, 5);

	ccframe.add (*dhbox);

	get_vbox()->pack_start (aframe, true, false, 10);
	get_vbox()->pack_start (ccframe, true, false);
#if NOT_USEFUL_YET
	get_vbox()->pack_start (*hbnt, false, false);
#endif

	get_vbox()->show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);
}

AddRouteDialog::~AddRouteDialog ()
{
}

void
AddRouteDialog::track_type_chosen ()
{
	if (track_button.get_active()) {
		track_mode_combo.set_sensitive (true);
	} else {
		track_mode_combo.set_sensitive (true);
	}
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

ARDOUR::TrackMode
AddRouteDialog::mode ()
{
	Glib::ustring str = track_mode_combo.get_active_text();
	if (str == _("Normal")) {
		return ARDOUR::Normal;
	} else if (str == _("Tape")) {
		return ARDOUR::Destructive;
	} else {
		fatal << string_compose (X_("programming error: unknown track mode in add route dialog combo = %1"), str)
		      << endmsg;
		/*NOTREACHED*/
	}
	/* keep gcc happy */
	return ARDOUR::Normal;
}

int
AddRouteDialog::channels ()
{
	string str = channel_combo.get_active_text();
	int chns;

	if (str == _("Mono")) {
		return 1;
	} else if (str == _("Stereo")) {
		return 2;
	} else if ((chns = PBD::atoi (str)) != 0) {
		return chns;
	} 

	return 0;
}

