/*
    Copyright (C) 2010 Paul Davis

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

#include <gtkmm/box.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/bindable_button.h"

#include "ardour_button.h"
#include "ardour_knob.h"
#include "ardour_display.h"
#include "axis_view.h"
#include "level_meter.h"
#include "route_ui.h"
#include "monitor_selector.h"

namespace Gtkmm2ext {
	class TearOff;
	class MotionFeedback;
}

class VolumeController;

class MonitorSection : public RouteUI
{
	public:
	MonitorSection (ARDOUR::Session*);
	~MonitorSection ();

	void set_session (ARDOUR::Session*);

	Gtkmm2ext::TearOff& tearoff() const { return *_tearoff; }

	std::string state_id() const;

	private:
	Gtk::VBox vpacker;
	Gtk::HBox hpacker;
	Gtk::VBox upper_packer;
	Gtk::VBox lower_packer;
	Gtkmm2ext::TearOff* _tearoff;

	Gtk::HBox  channel_table_packer;
	Gtk::HBox  table_hpacker;
	Gtk::Table channel_table;
	Gtk::Table channel_table_header;
	Gtk::ScrolledWindow channel_table_scroller;
	Gtk::Viewport channel_table_viewport;
	Glib::RefPtr<Gtk::SizeGroup> channel_size_group;

	struct ChannelButtonSet {
		ArdourButton cut;
		ArdourButton dim;
		ArdourButton solo;
		ArdourButton invert;

		ChannelButtonSet ();
	};

	typedef std::vector<ChannelButtonSet*> ChannelButtons;
	ChannelButtons _channel_buttons;

	ArdourKnob* gain_control;
	ArdourKnob* dim_control;
	ArdourKnob* solo_boost_control;
	ArdourKnob* solo_cut_control;

	ArdourDisplay*  gain_display;
	ArdourDisplay*  dim_display;
	ArdourDisplay*  solo_boost_display;
	ArdourDisplay*  solo_cut_display;

	std::list<boost::shared_ptr<ARDOUR::Bundle> > output_menu_bundles;
	Gtk::Menu output_menu;
	MonitorSelectorWindow *_output_selector;
	ArdourButton* output_button;

	void maybe_add_bundle_to_output_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const &);
	void bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle>);
	void output_button_resized (Gtk::Allocation&);
	void update_output_display ();
	void disconnect_output ();
	void edit_output_configuration ();

	void populate_buttons ();
	void map_state ();

	boost::shared_ptr<ARDOUR::MonitorProcessor> _monitor;
	boost::shared_ptr<ARDOUR::Route> _route;

	static Glib::RefPtr<Gtk::ActionGroup> monitor_actions;
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

	ArdourButton solo_in_place_button;
	ArdourButton afl_button;
	ArdourButton pfl_button;
	Gtk::HBox        solo_model_box;

	void solo_use_in_place ();
	void solo_use_afl ();
	void solo_use_pfl ();

	ArdourButton cut_all_button;
	ArdourButton dim_all_button;
	ArdourButton mono_button;
	ArdourButton rude_solo_button;
	ArdourButton rude_iso_button;
	ArdourButton rude_audition_button;
	ArdourButton exclusive_solo_button;
	ArdourButton solo_mute_override_button;

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
	PBD::ScopedConnectionList control_connections;
	PBD::ScopedConnection _output_changed_connection;

	bool _inhibit_solo_model_update;

	void assign_controllables ();
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
};
