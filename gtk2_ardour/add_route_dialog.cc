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

#include "ardour/plugin_manager.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "utils.h"
#include "add_route_dialog.h"
#include "route_group_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

std::vector<std::string> AddRouteDialog::channel_combo_strings;

AddRouteDialog::AddRouteDialog (Session* s)
	: ArdourDialog (_("Add Track or Bus"))
	, routes_adjustment (1, 1, 128, 1, 4)
	, routes_spinner (routes_adjustment)
	, configuration_label (_("Configuration:"))
	, mode_label (_("Track mode:"))
	, instrument_label (_("Instrument:"))
{
	set_session (s);

	set_name ("AddRouteDialog");
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	name_template_entry.set_name (X_("AddRouteDialogNameTemplateEntry"));
	// routes_spinner.set_name (X_("AddRouteDialogSpinner"));
	channel_combo.set_name (X_("ChannelCountSelector"));
	mode_combo.set_name (X_("ChannelCountSelector"));

	refill_channel_setups ();
	refill_route_groups ();
	refill_track_modes ();

	channel_combo.set_active_text (channel_combo_strings.front());

	track_bus_combo.append_text (_("Audio Tracks"));
	track_bus_combo.append_text (_("MIDI Tracks"));
	track_bus_combo.append_text (_("Audio+MIDI Tracks"));
	track_bus_combo.append_text (_("Busses"));
	track_bus_combo.set_active (0);

	build_instrument_list ();
	instrument_combo.set_model (instrument_list);
	instrument_combo.pack_start (instrument_list_columns.name);
	instrument_combo.set_active (0);
	instrument_combo.set_button_sensitivity (Gtk::SENSITIVITY_AUTO);

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

	if (!ARDOUR::Profile->get_sae ()) {

		/* Track mode */

		mode_label.set_alignment (Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
		table2->attach (mode_label, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
		table2->attach (mode_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
		++n;

	}

	instrument_label.set_alignment (Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	table2->attach (instrument_label, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (instrument_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

	/* Group choice */

	l = manage (new Label (_("Group:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	table2->attach (*l, 1, 2, n, n + 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
	table2->attach (route_group_combo, 2, 3, n, n + 1, Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 0, 0);
	++n;

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

	track_type_chosen ();
}

AddRouteDialog::~AddRouteDialog ()
{
}

void
AddRouteDialog::channel_combo_changed ()
{
	maybe_update_name_template_entry ();
	refill_track_modes ();
}

AddRouteDialog::TypeWanted
AddRouteDialog::type_wanted() const
{
	switch (track_bus_combo.get_active_row_number ()) {
	case 0:
		return AudioTrack;
	case 1:
		return MidiTrack;
	case 2:
		return MixedTrack;
	default:
		break;
	}

	return AudioBus;
}

void
AddRouteDialog::maybe_update_name_template_entry ()
{
	if (
		name_template_entry.get_text() != "" &&
		name_template_entry.get_text() != _("Audio") &&
		name_template_entry.get_text() != _("MIDI")  &&
		name_template_entry.get_text() != _("Audio+MIDI")  &&
		name_template_entry.get_text() != _("Bus")) {
		return;
	}

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
		name_template_entry.set_text (_("Bus"));
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
		break;
	case MidiTrack:
		channel_combo.set_sensitive (false);
		mode_combo.set_sensitive (false);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (false);
		instrument_label.set_sensitive (true);
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
		break;
	case AudioBus:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (true);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (false);
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
	    n == _("Bus")) {
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

	if (!ARDOUR::Profile->get_sae ()) {
		s.push_back (_("Non Layered"));
		s.push_back (_("Tape"));
	}

	set_popdown_strings (mode_combo, s);
	mode_combo.set_active_text (s.front());
}

ARDOUR::TrackMode
AddRouteDialog::mode ()
{
	if (ARDOUR::Profile->get_sae()) {
		return ARDOUR::Normal;
	}

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
		/*NOTREACHED*/
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
	}

	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		channel_combo_strings.push_back ((*i).name);
	}

	set_popdown_strings (channel_combo, channel_combo_strings);
	channel_combo.set_active_text (channel_combo_strings.front());
}

void
AddRouteDialog::add_route_group (RouteGroup* g)
{
	route_group_combo.insert_text (3, g->name ());
}

RouteGroup*
AddRouteDialog::route_group ()
{
	if (route_group_combo.get_active_row_number () == 2) {
		return 0;
	}

	return _session->route_group_by_name (route_group_combo.get_active_text());
}

void
AddRouteDialog::refill_route_groups ()
{
	route_group_combo.clear ();
	route_group_combo.append_text (_("New Group..."));

	route_group_combo.append_text ("separator");

	route_group_combo.append_text (_("No Group"));

	_session->foreach_route_group (sigc::mem_fun (*this, &AddRouteDialog::add_route_group));

	route_group_combo.set_active (2);
}

void
AddRouteDialog::group_changed ()
{
	if (_session && route_group_combo.get_active_text () == _("New Group...")) {
		RouteGroup* g = new RouteGroup (*_session, "");

		PropertyList plist;
		plist.add (Properties::active, true);
		g->apply_changes (plist);

		RouteGroupDialog d (g, true);

		if (d.do_run ()) {
			delete g;
			route_group_combo.set_active (2);
		} else {
			_session->add_route_group (g);
			add_route_group (g);
			route_group_combo.set_active (3);
		}
	}
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

void
AddRouteDialog::build_instrument_list ()
{
	PluginInfoList all_plugs;
	PluginManager& manager (PluginManager::instance());
	TreeModel::Row row;

	all_plugs.insert (all_plugs.end(), manager.ladspa_plugin_info().begin(), manager.ladspa_plugin_info().end());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.windows_vst_plugin_info().begin(), manager.windows_vst_plugin_info().end());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.lxvst_plugin_info().begin(), manager.lxvst_plugin_info().end());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.au_plugin_info().begin(), manager.au_plugin_info().end());
#endif
#ifdef LV2_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.lv2_plugin_info().begin(), manager.lv2_plugin_info().end());
#endif


	instrument_list = ListStore::create (instrument_list_columns);

	row = *(instrument_list->append());
	row[instrument_list_columns.info_ptr] = PluginInfoPtr ();
	row[instrument_list_columns.name] = _("-none-");

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {

		if (manager.get_status (*i) == PluginManager::Hidden) continue;

		if ((*i)->is_instrument()) {
			row = *(instrument_list->append());
			row[instrument_list_columns.name] = (*i)->name;
			row[instrument_list_columns.info_ptr] = *i;
		}
	}
}

PluginInfoPtr
AddRouteDialog::requested_instrument ()
{
	TreeModel::iterator iter = instrument_combo.get_active ();
	TreeModel::Row row;
	
	if (iter) {
		row = (*iter);
		return row[instrument_list_columns.info_ptr];
	}

	return PluginInfoPtr();
}
