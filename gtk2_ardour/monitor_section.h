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

#include "axis_view.h"
#include "level_meter.h"
#include "route_ui.h"

namespace Gtkmm2ext {
        class TearOff;
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

  private:
        Gtk::VBox vpacker;
        Gtk::HBox hpacker;
        Gtk::Table main_table;
        Gtk::VBox upper_packer;
        Gtk::VBox lower_packer;
        Gtkmm2ext::TearOff* _tearoff;

        struct ChannelButtonSet { 
            BindableToggleButton cut;
            BindableToggleButton dim;
            BindableToggleButton solo;
            BindableToggleButton invert;

            ChannelButtonSet ();
        };

        typedef std::vector<ChannelButtonSet*> ChannelButtons;
        ChannelButtons _channel_buttons;

        Gtk::Adjustment   gain_adjustment;
        VolumeController* gain_control;
        Gtk::Adjustment   dim_adjustment;
        VolumeController* dim_control;
        Gtk::Adjustment   solo_boost_adjustment;
        VolumeController* solo_boost_control;
        Gtk::Adjustment   solo_cut_adjustment;
        VolumeController* solo_cut_control;

        void populate_buttons ();
	void set_button_names ();
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
        void toggle_mute_overrides_solo ();
        void dim_level_changed ();
        void solo_boost_changed ();
        void gain_value_changed ();

        bool nonlinear_gain_printer (Gtk::SpinButton*);
        bool linear_gain_printer (Gtk::SpinButton*);

        Gtk::RadioButtonGroup solo_model_group;
        Gtk::RadioButton solo_in_place_button;
        Gtk::RadioButton afl_button;
        Gtk::RadioButton pfl_button;
        Gtk::HBox        solo_model_box;

        void solo_use_in_place ();
        void solo_use_afl ();
        void solo_use_pfl ();

        BindableToggleButton cut_all_button;
        BindableToggleButton dim_all_button;
        BindableToggleButton mono_button;
        BindableToggleButton rude_solo_button;
        BindableToggleButton rude_audition_button;
        BindableToggleButton exclusive_solo_button;
        BindableToggleButton solo_mute_override_button;

        void do_blink (bool);
        void solo_blink (bool);
        void audition_blink (bool);
        bool cancel_solo (GdkEventButton*);
        bool cancel_audition (GdkEventButton*);
        void solo_cut_changed ();
        void update_solo_model ();
        void parameter_changed (std::string);
        
        PBD::ScopedConnection config_connection;
        PBD::ScopedConnectionList control_connections;
        
        void assign_controllables ();
};
