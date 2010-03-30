/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __ardour_route_ui__
#define __ardour_route_ui__

#include <list>

#include "pbd/xml++.h"
#include "pbd/signals.h"

#include "ardour/ardour.h"
#include "ardour/mute_master.h"
#include "ardour/session_event.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/track.h"

#include "axis_view.h"

namespace ARDOUR {
	class AudioTrack;
	class MidiTrack;
}

namespace Gtk {
	class Menu;
	class CheckMenuItem;
	class Widget;
}

class BindableToggleButton;

class RouteUI : public virtual AxisView
{
  public:
	RouteUI(ARDOUR::Session*);
	RouteUI(boost::shared_ptr<ARDOUR::Route>, ARDOUR::Session*);

	virtual ~RouteUI();

	virtual void set_route (boost::shared_ptr<ARDOUR::Route>);
	virtual void set_button_names () = 0;

	bool is_track() const;
	bool is_audio_track() const;
	bool is_midi_track() const;

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }

	boost::shared_ptr<ARDOUR::Track>      track() const;
	boost::shared_ptr<ARDOUR::AudioTrack> audio_track() const;
	boost::shared_ptr<ARDOUR::MidiTrack>  midi_track() const;

	boost::shared_ptr<ARDOUR::Diskstream> get_diskstream() const;

	std::string name() const;

	// protected: XXX sigh this should be here

	boost::shared_ptr<ARDOUR::Route> _route;

	void set_color (const Gdk::Color & c);
	bool choose_color ();

	bool ignore_toggle;
	bool wait_for_release;
	bool multiple_mute_change;
	bool multiple_solo_change;

	BindableToggleButton* invert_button;
	BindableToggleButton* mute_button;
	BindableToggleButton* solo_button;
	BindableToggleButton* rec_enable_button; /* audio tracks */
	BindableToggleButton* show_sends_button; /* busses */

	Gtk::Label solo_button_label;
	Gtk::Label mute_button_label;
	Gtk::Label invert_button_label;
	Gtk::Label rec_enable_button_label;

	void send_blink (bool);
	sigc::connection send_blink_connection;

	virtual std::string solo_button_name () const { return "SoloButton"; }
	virtual std::string safe_solo_button_name () const { return "SafeSoloButton"; }

	Gtk::Menu* mute_menu;
	Gtk::Menu* solo_menu;
	Gtk::Menu* sends_menu;

	XMLNode *xml_node;
	void ensure_xml_node ();

	virtual XMLNode* get_automation_child_xml_node (Evoral::Parameter param);

	bool invert_press(GdkEventButton*);
	bool invert_release(GdkEventButton*);
	bool mute_press(GdkEventButton*);
	bool mute_release(GdkEventButton*);
	bool solo_press(GdkEventButton*);
	bool solo_release(GdkEventButton*);
	bool rec_enable_press(GdkEventButton*);
	bool rec_enable_release(GdkEventButton*);
	bool show_sends_press(GdkEventButton*);
	bool show_sends_release(GdkEventButton*);

	void step_gain_up ();
	void step_gain_down ();
	void page_gain_up ();
	void page_gain_down ();

	void build_sends_menu ();
	void set_sends_gain_from_track ();
	void set_sends_gain_to_zero ();
	void set_sends_gain_to_unity ();
	void create_sends (ARDOUR::Placement);
	void create_selected_sends (ARDOUR::Placement);

	void solo_changed(void*);
	void solo_changed_so_update_mute ();
	void mute_changed(void*);
	void listen_changed(void*);
	virtual void processors_changed (ARDOUR::RouteProcessorChange) {}
	void route_rec_enable_changed();
	void session_rec_enable_changed();

	void build_solo_menu ();

	void solo_isolated_toggle (void*, Gtk::CheckMenuItem*);
	void toggle_solo_isolated (Gtk::CheckMenuItem*);

	void solo_safe_toggle (void*, Gtk::CheckMenuItem*);
	void toggle_solo_safe (Gtk::CheckMenuItem*);

	Gtk::CheckMenuItem* pre_fader_mute_check;
	Gtk::CheckMenuItem* post_fader_mute_check;
	Gtk::CheckMenuItem* listen_mute_check;
	Gtk::CheckMenuItem* main_mute_check;

	void toggle_mute_menu(ARDOUR::MuteMaster::MutePoint, Gtk::CheckMenuItem*);
	void muting_change ();
	void build_mute_menu(void);
	void init_mute_menu(ARDOUR::MuteMaster::MutePoint, Gtk::CheckMenuItem*);

	int  set_color_from_route ();

	void remove_this_route ();
	static gint idle_remove_this_route (RouteUI *);

	void route_rename();

	virtual void property_changed (const PBD::PropertyChange&);
	void route_removed ();

	Gtk::CheckMenuItem *route_active_menu_item;
	void toggle_route_active ();
	virtual void route_active_changed ();

	Gtk::CheckMenuItem *polarity_menu_item;
	void toggle_polarity ();
	virtual void polarity_changed ();

	Gtk::CheckMenuItem *denormal_menu_item;
	void toggle_denormal_protection();
	virtual void denormal_protection_changed ();

	void disconnect_input ();
	void disconnect_output ();

	virtual void update_rec_display ();
	void update_mute_display ();

	void update_solo_display ();

	virtual void map_frozen ();

	void adjust_latency ();
	void save_as_template ();
	void open_remote_control_id_dialog ();

	static int solo_visual_state (boost::shared_ptr<ARDOUR::Route>);
	static int solo_visual_state_with_isolate (boost::shared_ptr<ARDOUR::Route>);
	static int solo_isolate_visual_state (boost::shared_ptr<ARDOUR::Route>);
	static int mute_visual_state (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);

   protected:
	PBD::ScopedConnectionList route_connections;
	bool self_destruct;

 	void init ();
 	void reset ();

	void self_delete ();

  private:
	void check_rec_enable_sensitivity ();
	void parameter_changed (std::string const &);
	void relabel_solo_button ();

	struct SoloMuteRelease {
	    SoloMuteRelease (bool was_active) 
	    : active (was_active)
	    , exclusive (false) {}
	    
	    boost::shared_ptr<ARDOUR::RouteList> routes;
	    boost::shared_ptr<ARDOUR::RouteList> routes_on;
	    boost::shared_ptr<ARDOUR::RouteList> routes_off;
	    boost::shared_ptr<ARDOUR::Route> route;
	    bool active;
	    bool exclusive;
	};

	SoloMuteRelease* _solo_release;
	SoloMuteRelease* _mute_release;

};

#endif /* __ardour_route_ui__ */
