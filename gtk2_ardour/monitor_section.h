/*
 * Copyright (C) 2010-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk2_ardour_monitor_section_h__
#define __gtk2_ardour_monitor_section_h__

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/table.h>
#include <gtkmm/viewport.h>

#include "gtkmm2ext/bindings.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"
#include "widgets/ardour_display.h"

#include "level_meter.h"
#include "route_ui.h"
#include "monitor_selector.h"

#include "processor_box.h"
#include "processor_selection.h"

namespace ArdourWidgets {
	class TearOff;
}

class PluginSelector;

class MonitorSection : public RouteUI, public Gtk::EventBox
{
public:
	MonitorSection ();
	~MonitorSection ();

	void set_session (ARDOUR::Session*);

	ArdourWidgets::TearOff& tearoff() const { return *_tearoff; }

	std::string state_id() const;

	PluginSelector* plugin_selector();

	void use_others_actions ();

private:
	Gtk::HBox hpacker;
	Gtk::VBox vpacker;
	ArdourWidgets::TearOff* _tearoff;

	Gtk::HBox  channel_table_packer;
	Gtk::HBox  table_hpacker;
	Gtk::HBox  master_packer;
	Gtk::Table channel_table_header;
	Gtk::Table *channel_table;
	Gtk::ScrolledWindow channel_table_scroller;
	Gtk::Viewport channel_table_viewport;
	Glib::RefPtr<Gtk::SizeGroup> channel_size_group;

	struct ChannelButtonSet {
		ArdourWidgets::ArdourButton cut;
		ArdourWidgets::ArdourButton dim;
		ArdourWidgets::ArdourButton solo;
		ArdourWidgets::ArdourButton invert;

		ChannelButtonSet ();
	};

	typedef std::vector<ChannelButtonSet*> ChannelButtons;
	ChannelButtons _channel_buttons;

	ArdourWidgets::ArdourKnob* gain_control;
	ArdourWidgets::ArdourKnob* dim_control;
	ArdourWidgets::ArdourKnob* solo_boost_control;
	ArdourWidgets::ArdourKnob* solo_cut_control;

	ArdourWidgets::ArdourDisplay*  gain_display;
	ArdourWidgets::ArdourDisplay*  dim_display;
	ArdourWidgets::ArdourDisplay*  solo_boost_display;
	ArdourWidgets::ArdourDisplay*  solo_cut_display;

	std::list<boost::shared_ptr<ARDOUR::Bundle> > output_menu_bundles;
	Gtk::Menu output_menu;
	MonitorSelectorWindow *_output_selector;
	ArdourWidgets::ArdourButton* output_button;

	void maybe_add_bundle_to_output_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const &);
	void bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle>);
	void update_output_display ();
	void disconnect_output ();
	void edit_output_configuration ();

	void populate_buttons ();
	void map_state ();

	boost::shared_ptr<ARDOUR::MonitorProcessor> _monitor;

	Glib::RefPtr<Gtk::ActionGroup> monitor_actions;
	Glib::RefPtr<Gtk::ActionGroup> solo_actions;
	void register_actions ();

	void cut_channel (uint32_t);
	void dim_channel (uint32_t);
	void solo_channel (uint32_t);
	void invert_channel (uint32_t);
	void dim_all ();
	void cut_all ();
	void mono ();
	void toggle_exclusive_solo ();
	void set_button_names () {}
	void toggle_mute_overrides_solo ();
	void dim_level_changed ();
	void solo_boost_changed ();
	void gain_value_changed ();
	gint output_press (GdkEventButton *);
	gint output_release (GdkEventButton *);

	ArdourWidgets::ArdourButton solo_in_place_button;
	ArdourWidgets::ArdourButton afl_button;
	ArdourWidgets::ArdourButton pfl_button;
	Gtk::VBox    solo_model_box;

	void solo_use_in_place ();
	void solo_use_afl ();
	void solo_use_pfl ();

	ArdourWidgets::ArdourButton cut_all_button;
	ArdourWidgets::ArdourButton dim_all_button;
	ArdourWidgets::ArdourButton mono_button;
	ArdourWidgets::ArdourButton rude_solo_button;
	ArdourWidgets::ArdourButton rude_iso_button;
	ArdourWidgets::ArdourButton rude_audition_button;
	ArdourWidgets::ArdourButton exclusive_solo_button;
	ArdourWidgets::ArdourButton solo_mute_override_button;
	ArdourWidgets::ArdourButton toggle_processorbox_button;

	void do_blink (bool);
	void solo_blink (bool);
	void audition_blink (bool);
	bool cancel_solo (GdkEventButton*);
	bool cancel_isolate (GdkEventButton*);
	bool cancel_audition (GdkEventButton*);
	void update_solo_model ();
	void parameter_changed (std::string);
	void isolated_changed ();

	PBD::ScopedConnection config_connection;
	PBD::ScopedConnectionList connections;
	PBD::ScopedConnectionList route_connections;

	bool _inhibit_solo_model_update;

	void assign_controllables ();
	void unassign_controllables ();

	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
	void port_pretty_name_changed (std::string);

	void update_processor_box ();

	void route_property_changed (const PBD::PropertyChange&) {}

	ProcessorBox* insert_box;
	ProcessorSelection _rr_selection;
	void help_count_processors (boost::weak_ptr<ARDOUR::Processor> p, uint32_t* cnt) const;
	uint32_t count_processors ();

	void processors_changed (ARDOUR::RouteProcessorChange);
	Glib::RefPtr<Gtk::ToggleAction> proctoggle;
	bool _ui_initialized;

	Gtkmm2ext::Bindings* bindings;

	void load_bindings ();
	bool enter_handler (GdkEventCrossing*);
	bool leave_handler (GdkEventCrossing*);

	void toggle_use_monitor_section ();
	void drop_route ();
};

#endif /* __gtk2_ardour_monitor_section_h__ */
