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

*/

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>
#include <gtkmm/stock.h>
#include <gtkmm/separator.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/utils.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"

#include "utils.h"
#include "add_route_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

static const char* track_mode_names[] = {
	N_("Normal"),
	N_("Non Layered"),
	N_("Tape"),
	0
};

AddRouteDialog::AddRouteDialog ()
	: Dialog (_("ardour: add track/bus")),
	  track_button (_("Tracks")),
	  bus_button (_("Busses")),
	  routes_adjustment (1, 1, 128, 1, 4),
	  routes_spinner (routes_adjustment)
{
	if (track_mode_strings.empty()) {
		track_mode_strings = I18N (track_mode_names);

		if (ARDOUR::Profile->get_sae()) {
			/* remove all but the first track mode (Normal) */

			while (track_mode_strings.size() > 1) {
				track_mode_strings.pop_back();
			}
		}
	}
	
	set_name ("AddRouteDialog");
	set_wmclass (X_("ardour_add_track_bus"), "Ardour");
	set_position (Gtk::WIN_POS_MOUSE);
	set_resizable (false);

	name_template_entry.set_name ("AddRouteDialogNameTemplateEntry");
	track_button.set_name ("AddRouteDialogRadioButton");
	bus_button.set_name ("AddRouteDialogRadioButton");
	routes_spinner.set_name ("AddRouteDialogSpinner");
	
	RadioButton::Group g = track_button.get_group();
	bus_button.set_group (g);
	track_button.set_active (true);

	/* add */

	HBox* hbox1 = manage (new HBox);
	hbox1->set_spacing (6);
	Label* label1 = manage (new Label (_("Add this many:")));
	hbox1->pack_start (*label1, PACK_SHRINK);
	hbox1->pack_start (routes_spinner, PACK_SHRINK);

	HBox* hbox2 = manage (new HBox);
	hbox2->set_spacing (6);
	hbox2->set_border_width (6);
	hbox2->pack_start (*hbox1, PACK_EXPAND_WIDGET);

	/* track/bus choice & modes */

	HBox* hbox5 = manage (new HBox);
	hbox5->set_spacing (6);
	hbox5->pack_start (track_button, PACK_EXPAND_PADDING);
	hbox5->pack_start (bus_button, PACK_EXPAND_PADDING);

	channel_combo.set_name (X_("ChannelCountSelector"));
	track_mode_combo.set_name (X_("ChannelCountSelector"));

	refill_channel_setups ();
	set_popdown_strings (track_mode_combo, track_mode_strings, true);

	channel_combo.set_active_text (channel_combo_strings.front());
	track_mode_combo.set_active_text (track_mode_strings.front());

	track_button.signal_clicked().connect (mem_fun (*this, &AddRouteDialog::track_type_chosen));
	bus_button.signal_clicked().connect (mem_fun (*this, &AddRouteDialog::track_type_chosen));
	
	VBox* vbox1 = manage (new VBox);
	vbox1->set_spacing (6);
	vbox1->set_border_width (6);

	Frame* frame1 = manage (new Frame (_("Channel Configuration")));
	frame1->add (channel_combo);
	Frame* frame2 = manage (new Frame (_("Track Mode")));
	frame2->add (track_mode_combo);

	vbox1->pack_start (*hbox5, PACK_SHRINK);
	vbox1->pack_start (*frame1, PACK_SHRINK);

	if (!ARDOUR::Profile->get_sae()) {
		vbox1->pack_start (*frame2, PACK_SHRINK);
	}

	get_vbox()->set_spacing (6);
	get_vbox()->set_border_width (6);

	get_vbox()->pack_start (*hbox2, PACK_SHRINK);
	get_vbox()->pack_start (*vbox1, PACK_SHRINK);

	get_vbox()->show_all ();

	/* track template info will be managed whenever
	   this dialog is shown, via ::on_show()
	*/

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);

	track_type_chosen ();
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
		track_mode_combo.set_sensitive (false);
	}
}

bool
AddRouteDialog::track ()
{
	return track_button.get_active ();
}

ARDOUR::DataType
AddRouteDialog::type ()
{
	// FIXME: ew
	
	const string str = channel_combo.get_active_text();
	if (str == _("MIDI"))
		return ARDOUR::DataType::MIDI;
	else
		return ARDOUR::DataType::AUDIO;
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
	if (ARDOUR::Profile->get_sae()) {
		return ARDOUR::Normal;
	}

	Glib::ustring str = track_mode_combo.get_active_text();
	if (str == _("Normal")) {
		return ARDOUR::Normal;
	} else if (str == _("Non Layered")){
		return ARDOUR::NonLayered;
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
	
	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		if (str == (*i).name) {
			return (*i).channels;
		}
	}

	return 0;
}

string
AddRouteDialog::track_template ()
{
	string str = channel_combo.get_active_text();
	
	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		if (str == (*i).name) {
			return (*i).template_path;
		}
	}

	return string();
}

void
AddRouteDialog::on_show ()
{
	refill_channel_setups ();
	Dialog::on_show ();
}

void
AddRouteDialog::refill_channel_setups ()
{
	ChannelSetup chn;
	
	route_templates.clear ();
	channel_combo_strings.clear ();
	channel_setups.clear ();

	chn.name = X_("MIDI");
	chn.channels = 0;
	channel_setups.push_back (chn);

	chn.name = _("Mono");
	chn.channels = 1;
	channel_setups.push_back (chn);

	chn.name = _("Stereo");
	chn.channels = 2;
	channel_setups.push_back (chn);

	ARDOUR::find_route_templates (route_templates);

	if (!ARDOUR::Profile->get_sae()) {
		if (!route_templates.empty()) {
			vector<string> v;
			for (vector<TemplateInfo>::iterator x = route_templates.begin(); x != route_templates.end(); ++x) {
				chn.name = x->name;
				chn.channels = 0;
				chn.template_path = x->path;
				channel_setups.push_back (chn);
			}
		} 

		chn.name = _("3 Channel");
		chn.channels = 3;
		channel_setups.push_back (chn);

		chn.name = _("4 Channel");
		chn.channels = 4;
		channel_setups.push_back (chn);

		chn.name = _("5 Channel");
		chn.channels = 5;
		channel_setups.push_back (chn);

		chn.name = _("6 Channel");
		chn.channels = 6;
		channel_setups.push_back (chn);

		chn.name = _("8 Channel");
		chn.channels = 8;
		channel_setups.push_back (chn);

		chn.name = _("12 Channel");
		chn.channels = 12;
		channel_setups.push_back (chn);

		chn.name = X_("Custom");
		chn.channels = 0;
		channel_setups.push_back (chn);
	}

	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		channel_combo_strings.push_back ((*i).name);
	}

	set_popdown_strings (channel_combo, channel_combo_strings, true);
	channel_combo.set_active_text (channel_combo_strings.front());
}
