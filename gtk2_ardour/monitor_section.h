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
#include "axis_view.h"
#include "level_meter.h"
#include "route_ui.h"

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
        static void setup_knob_images ();

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

        VolumeController* gain_control;
        VolumeController* dim_control;
        VolumeController* solo_boost_control;
        VolumeController* solo_cut_control;

        void populate_buttons ();
        void map_state ();

        boost::shared_ptr<ARDOUR::MonitorProcessor> _monitor;
        boost::shared_ptr<ARDOUR::Route> _route;

	static Glib::RefPtr<Gtk::ActionGroup> monitor_actions;
        void register_actions ();

        static Glib::RefPtr<Gdk::Pixbuf> big_knob_pixbuf;
        static Glib::RefPtr<Gdk::Pixbuf> little_knob_pixbuf;

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

	bool _inhibit_solo_model_update;
	
        void assign_controllables ();
};
