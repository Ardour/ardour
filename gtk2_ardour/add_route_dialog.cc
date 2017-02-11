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
#include <gtkmm/messagedialog.h>
#include <gtkmm/separator.h>
#include <gtkmm/table.h>

#include "pbd/error.h"
#include "pbd/convert.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/doi.h"

#include "ardour/plugin_manager.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/vca.h"

#include "utils.h"
#include "add_route_dialog.h"
#include "route_group_dialog.h"
#include "tooltips.h"
#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

std::vector<std::string> AddRouteDialog::channel_combo_strings;

AddRouteDialog::AddRouteDialog ()
	: ArdourDialog (_("Add Track/Bus/VCA"))
	, routes_adjustment (1, 1, 128, 1, 4)
	, routes_spinner (routes_adjustment)
	, configuration_label (_("Configuration:"))
	, mode_label (_("Record Mode:"))
	, instrument_label (_("Instrument:"))
{
	set_name ("AddRouteDialog");
	set_skip_taskbar_hint (true);
	set_resizable (false);
	set_position (WIN_POS_MOUSE);

	name_template_entry.set_name (X_("AddRouteDialogNameTemplateEntry"));
	// routes_spinner.set_name (X_("AddRouteDialogSpinner"));
	channel_combo.set_name (X_("ChannelCountSelector"));
	mode_combo.set_name (X_("ChannelCountSelector"));

	refill_track_modes ();

	track_bus_combo.append_text (_("Audio Tracks"));
	track_bus_combo.append_text (_("MIDI Tracks"));
	track_bus_combo.append_text (_("Audio+MIDI Tracks"));
	track_bus_combo.append_text (_("Audio Busses"));
	track_bus_combo.append_text (_("MIDI Busses"));
	track_bus_combo.append_text (_("VCA Masters"));
	track_bus_combo.set_active (0);

	insert_at_combo.append_text (_("First"));
	insert_at_combo.append_text (_("Before Selection"));
	insert_at_combo.append_text (_("After Selection"));
	insert_at_combo.append_text (_("Last"));
	insert_at_combo.set_active (3);

	strict_io_combo.append_text (_("Flexible-I/O"));
	strict_io_combo.append_text (_("Strict-I/O"));
	strict_io_combo.set_active (Config->get_strict_io () ? 1 : 0);

	VBox* vbox = manage (new VBox);
	Gtk::Label* l;

	get_vbox()->set_spacing (4);

	vbox->set_spacing (18);
	vbox->set_border_width (5);

	HBox *type_hbox = manage (new HBox);
	type_hbox->set_spacing (6);

	/* track/bus choice */

	type_hbox->pack_start (*manage (new Label (_("Add:"))));
	type_hbox->pack_start (routes_spinner);
	type_hbox->pack_start (track_bus_combo);

	vbox->pack_start (*type_hbox, false, true);

	VBox* options_box = manage (new VBox);
	Table *table2 = manage (new Table (3, 3, false));

	options_box->set_spacing (6);
	table2->set_row_spacings (6);
	table2->set_col_spacing	(1, 6);

	l = manage (new Label (_("<b>Options</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true);

	l = manage (new Label ("", Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_padding (8, 0);
	table2->attach (*l, 0, 1, 0, 3, Gtk::FILL, Gtk::FILL, 0, 0);

	int n = 0;

	l = manage (new Label (_("Name:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	table2->attach (*l, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (name_template_entry, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	/* Route configuration */

	configuration_label.set_alignment (Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	table2->attach (configuration_label, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (channel_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	mode_label.set_alignment (Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	table2->attach (mode_label, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (mode_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	instrument_label.set_alignment (Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	table2->attach (instrument_label, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (instrument_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	/* Group choice */

	l = manage (new Label (_("Group:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	table2->attach (*l, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (route_group_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	/* New route will be inserted at.. */
	l = manage (new Label (_("Insert:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	table2->attach (*l, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (insert_at_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	/* New Route's Routing is.. */

	if (Profile->get_mixbus ()) {
		strict_io_combo.set_active (1);
	} else {
		l = manage (new Label (_("Output Ports:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		table2->attach (*l, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
		table2->attach (strict_io_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);

		ARDOUR_UI_UTILS::set_tooltip (strict_io_combo,
				_("With strict-i/o enabled, Effect Processors will not modify the number of channels on a track. The number of output channels will always match the number of input channels."));
		++n;
	}

	options_box->pack_start (*table2, false, true);
	vbox->pack_start (*options_box, false, true);

	get_vbox()->pack_start (*vbox, false, false);

	track_bus_combo.signal_changed().connect (sigc::mem_fun (*this, &AddRouteDialog::track_type_chosen));
	channel_combo.signal_changed().connect (sigc::mem_fun (*this, &AddRouteDialog::channel_combo_changed));
	channel_combo.set_row_separator_func (sigc::mem_fun (*this, &AddRouteDialog::channel_separator));
	route_group_combo.set_row_separator_func (sigc::mem_fun (*this, &AddRouteDialog::route_separator));
	route_group_combo.signal_changed ().connect (sigc::mem_fun (*this, &AddRouteDialog::group_changed));

	show_all_children ();

	/* track template info will be managed whenever
	   this dialog is shown, via ::on_show()
	*/

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);
	set_response_sensitive (RESPONSE_ACCEPT, true);
	set_default_response (RESPONSE_ACCEPT);

	track_type_chosen ();
}

AddRouteDialog::~AddRouteDialog ()
{
}

void
AddRouteDialog::channel_combo_changed ()
{
	refill_track_modes ();
}

AddRouteDialog::TypeWanted
AddRouteDialog::type_wanted() const
{
	std::string str = track_bus_combo.get_active_text();
	if (str == _("Audio Busses")) {
		return AudioBus;
	} else if (str == _("MIDI Busses")){
		return MidiBus;
	} else if (str == _("MIDI Tracks")){
		return MidiTrack;
	} else if (str == _("Audio+MIDI Tracks")) {
		return MixedTrack;
	} else if (str == _("Audio Tracks")) {
		return AudioTrack;
	} else {
		return VCAMaster;
	}
}

void
AddRouteDialog::maybe_update_name_template_entry ()
{
	switch (type_wanted()) {
	case AudioTrack:
		name_template_entry.set_text (_("Audio"));
		break;
	case MidiTrack:
		name_template_entry.set_text (_("MIDI"));
		break;
	case MixedTrack:
		name_template_entry.set_text (_("Audio+MIDI"));
		break;
	case AudioBus:
	case MidiBus:
		name_template_entry.set_text (_("Bus"));
		break;
	case VCAMaster:
		name_template_entry.set_text (VCA::default_name_template());
		break;
	}
}

void
AddRouteDialog::track_type_chosen ()
{
	switch (type_wanted()) {
	case AudioTrack:
		mode_combo.set_sensitive (true);
		channel_combo.set_sensitive (true);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (false);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case MidiTrack:
		channel_combo.set_sensitive (false);
		mode_combo.set_sensitive (false);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (false);
		instrument_label.set_sensitive (true);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case MixedTrack:
		{
			MessageDialog msg (_("Audio+MIDI tracks are intended for use <b>ONLY</b> with plugins that use both audio and MIDI input data\n\n"
					     "If you do not plan to use such a plugin, then use a normal audio or MIDI track instead."),
					   true, MESSAGE_INFO, BUTTONS_OK, true);
			msg.set_position (WIN_POS_MOUSE);
			msg.run ();
	        }
		channel_combo.set_sensitive (true);
		mode_combo.set_sensitive (true);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (true);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case AudioBus:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (true);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (false);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case VCAMaster:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (false);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (false);
		instrument_label.set_sensitive (false);
		route_group_combo.set_sensitive (false);
		strict_io_combo.set_sensitive (false);
		insert_at_combo.set_sensitive (false);
		break;
	case MidiBus:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (false);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	}

	maybe_update_name_template_entry ();
}


string
AddRouteDialog::name_template () const
{
	return name_template_entry.get_text ();
}

bool
AddRouteDialog::name_template_is_default() const
{
	string n = name_template();

	if (n == _("Audio") ||
	    n == _("MIDI") ||
	    n == _("Audio+MIDI") ||
	    n == _("Bus") ||
	    n == VCA::default_name_template()) {
		return true;
	}

	return false;
}

int
AddRouteDialog::count ()
{
	return (int) floor (routes_adjustment.get_value ());
}

void
AddRouteDialog::refill_track_modes ()
{
	vector<string> s;

	s.push_back (_("Normal"));
#ifdef XXX_OLD_DESTRUCTIVE_API_XXX
	s.push_back (_("Non Layered"));
#endif
	s.push_back (_("Tape"));
	if (!ARDOUR::Profile->get_mixbus ()) {
		s.push_back (_("Tape"));
	}

	set_popdown_strings (mode_combo, s);
	mode_combo.set_active_text (s.front());
}

ARDOUR::TrackMode
AddRouteDialog::mode ()
{
	std::string str = mode_combo.get_active_text();
	if (str == _("Normal")) {
		return ARDOUR::Normal;
	} else if (str == _("Non Layered")){
		return ARDOUR::NonLayered;
	} else if (str == _("Tape")) {
		return ARDOUR::Destructive;
	} else {
		fatal << string_compose (X_("programming error: unknown track mode in add route dialog combo = %1"), str)
		      << endmsg;
		abort(); /*NOTREACHED*/
	}
	/* keep gcc happy */
	return ARDOUR::Normal;
}

ChanCount
AddRouteDialog::channels ()
{
	ChanCount ret;
	string str;
	switch (type_wanted()) {
	case AudioTrack:
	case AudioBus:
		str = channel_combo.get_active_text();
		for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
			if (str == (*i).name) {
				ret.set (DataType::AUDIO, (*i).channels);
				break;
			}
		}
		ret.set (DataType::MIDI, 0);
		break;

	case MidiBus:
	case MidiTrack:
		ret.set (DataType::AUDIO, 0);
		ret.set (DataType::MIDI, 1);
		break;

	case MixedTrack:
		str = channel_combo.get_active_text();
		for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
			if (str == (*i).name) {
				ret.set (DataType::AUDIO, (*i).channels);
				break;
			}
		}
		ret.set (DataType::MIDI, 1);
		break;
	default:
		break;
	}

	return ret;
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
	refill_route_groups ();

	Dialog::on_show ();
}

void
AddRouteDialog::refill_channel_setups ()
{
	ChannelSetup chn;

	route_templates.clear ();

	string channel_current_choice = channel_combo.get_active_text();

	channel_combo_strings.clear ();
	channel_setups.clear ();

	chn.name = _("Mono");
	chn.channels = 1;
	channel_setups.push_back (chn);

	chn.name = _("Stereo");
	chn.channels = 2;
	channel_setups.push_back (chn);

	chn.name = "separator";
	channel_setups.push_back (chn);

	ARDOUR::find_route_templates (route_templates);

	if (!route_templates.empty()) {
		vector<string> v;
		for (vector<TemplateInfo>::iterator x = route_templates.begin(); x != route_templates.end(); ++x) {
			chn.name = x->name;
			chn.channels = 0;
			chn.template_path = x->path;
			channel_setups.push_back (chn);
		}
	}

	/* clear template path for the rest */

	chn.template_path = "";

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

	chn.name = _("Custom");
	chn.channels = 0;
	channel_setups.push_back (chn);

	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		channel_combo_strings.push_back ((*i).name);
	}

	set_popdown_strings (channel_combo, channel_combo_strings);

	if (!channel_current_choice.empty()) {
		channel_combo.set_active_text (channel_current_choice);
	} else {
		channel_combo.set_active_text (channel_combo_strings.front());
	}
}

void
AddRouteDialog::add_route_group (RouteGroup* g)
{
	route_group_combo.insert_text (3, g->name ());
}

RouteGroup*
AddRouteDialog::route_group ()
{
	if (!_session || route_group_combo.get_active_row_number () == 2) {
		return 0;
	}

	return _session->route_group_by_name (route_group_combo.get_active_text());
}

bool
AddRouteDialog::use_strict_io() {
	return strict_io_combo.get_active_row_number () == 1;
}

void
AddRouteDialog::refill_route_groups ()
{
	route_group_combo.clear ();
	route_group_combo.append_text (_("New Group..."));

	route_group_combo.append_text ("separator");

	route_group_combo.append_text (_("No Group"));

	if (_session) {
		_session->foreach_route_group (sigc::mem_fun (*this, &AddRouteDialog::add_route_group));
	}

	route_group_combo.set_active (2);
}

void
AddRouteDialog::group_changed ()
{
	if (_session && route_group_combo.get_active_text () == _("New Group...")) {
		RouteGroup* g = new RouteGroup (*_session, "");
		RouteGroupDialog* d = new RouteGroupDialog (g, true);

		d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &AddRouteDialog::new_group_dialog_finished), d));
		d->present();
	}
}

void
AddRouteDialog::new_group_dialog_finished (int r, RouteGroupDialog* d)
{
	if (r == RESPONSE_OK) {

		if (!d->name_check()) {
			return;
		}

		if (_session) {
			_session->add_route_group (d->group());
		}

		add_route_group (d->group());
		route_group_combo.set_active (3);
	} else {
		delete d->group ();
		route_group_combo.set_active (2);
	}

	delete_when_idle (d);
}

RouteDialogs::InsertAt
AddRouteDialog::insert_at ()
{
	using namespace RouteDialogs;

	std::string str = insert_at_combo.get_active_text();

	if (str == _("First")) {
		return First;
	} else if (str == _("After Selection")) {
		return AfterSelection;
	} else if (str == _("Before Selection")){
		return BeforeSelection;
	}
	return Last;
}

bool
AddRouteDialog::channel_separator (const Glib::RefPtr<Gtk::TreeModel> &, const Gtk::TreeModel::iterator &i)
{
	channel_combo.set_active (i);

	return channel_combo.get_active_text () == "separator";
}

bool
AddRouteDialog::route_separator (const Glib::RefPtr<Gtk::TreeModel> &, const Gtk::TreeModel::iterator &i)
{
	route_group_combo.set_active (i);

	return route_group_combo.get_active_text () == "separator";
}

PluginInfoPtr
AddRouteDialog::requested_instrument ()
{
	return instrument_combo.selected_instrument();
}
