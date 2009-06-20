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
#include <gtkmm/table.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/utils.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/route_group.h"
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

AddRouteDialog::AddRouteDialog (Session & s)
	: Dialog (_("ardour: add track/bus")),
	  _session (s),
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

	refill_channel_setups ();
	set_popdown_strings (track_mode_combo, track_mode_strings, true);

	edit_group_combo.append_text (_("No group"));
	_session.foreach_edit_group (mem_fun (*this, &AddRouteDialog::add_edit_group));
	
	channel_combo.set_active_text (channel_combo_strings.front());
	track_mode_combo.set_active_text (track_mode_strings.front());
	edit_group_combo.set_active (0);

	RadioButton::Group g = track_button.get_group();
	bus_button.set_group (g);
	track_button.set_active (true);

	Table* table = manage (new Table (5, 2));
	table->set_spacings (4);

	/* add */

	Label* l = manage (new Label (_("Add this many:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 0, 1);
	table->attach (routes_spinner, 1, 2, 0, 1, FILL | EXPAND);

	/* track/bus choice & modes */

	HBox* hbox = manage (new HBox);
	hbox->set_spacing (6);
	hbox->pack_start (track_button, PACK_EXPAND_PADDING);
	hbox->pack_start (bus_button, PACK_EXPAND_PADDING);
	table->attach (*hbox, 0, 2, 1, 2);

	channel_combo.set_name (X_("ChannelCountSelector"));
	track_mode_combo.set_name (X_("ChannelCountSelector"));

	track_button.signal_clicked().connect (mem_fun (*this, &AddRouteDialog::track_type_chosen));
	bus_button.signal_clicked().connect (mem_fun (*this, &AddRouteDialog::track_type_chosen));

	l = manage (new Label (_("Channel configuration:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 2, 3);
	table->attach (channel_combo, 1, 2, 2, 3, FILL | EXPAND);

	if (!ARDOUR::Profile->get_sae ()) {
		l = manage (new Label (_("Track mode:")));
		l->set_alignment (1, 0.5);
		table->attach (*l, 0, 1, 3, 4);
		table->attach (track_mode_combo, 1, 2, 3, 4, FILL | EXPAND);
	}
	
	l = manage (new Label (_("Add to edit group:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 4, 5);
	table->attach (edit_group_combo, 1, 2, 4, 5, FILL | EXPAND);
	get_vbox()->pack_start (*table);

	get_vbox()->set_spacing (6);
	get_vbox()->set_border_width (6);
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

void
AddRouteDialog::add_edit_group (RouteGroup* g)
{
	edit_group_combo.append_text (g->name ());
}

RouteGroup*
AddRouteDialog::edit_group ()
{
	if (edit_group_combo.get_active_row_number () == 0) {
		return 0;
	}

	return _session.edit_group_by_name (edit_group_combo.get_active_text());
}
