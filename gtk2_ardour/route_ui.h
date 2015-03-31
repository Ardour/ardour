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

#include <gtkmm/textview.h>

#include "gtkmm2ext/widget_state.h"

#include "ardour/ardour.h"
#include "ardour/mute_master.h"
#include "ardour/session_event.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/track.h"

#include "axis_view.h"
#include "selectable.h"

namespace ARDOUR {
	class AudioTrack;
	class MidiTrack;
}

namespace Gtk {
	class Menu;
	class CheckMenuItem;
	class Widget;
}

class ArdourButton;
class ArdourWindow;
class IOSelectorWindow;

class RouteUI : public virtual AxisView
{
  public:
	RouteUI(ARDOUR::Session*);
	RouteUI(boost::shared_ptr<ARDOUR::Route>, ARDOUR::Session*);

	virtual ~RouteUI();

	Gdk::Color color () const;

	virtual void set_route (boost::shared_ptr<ARDOUR::Route>);
	virtual void set_button_names () = 0;

	bool is_track() const;
	bool is_audio_track() const;
	bool is_midi_track() const;
	bool has_audio_outputs () const;

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }
	ARDOUR::RouteGroup* route_group() const;

	boost::shared_ptr<ARDOUR::Track>      track() const;
	boost::shared_ptr<ARDOUR::AudioTrack> audio_track() const;
	boost::shared_ptr<ARDOUR::MidiTrack>  midi_track() const;

	std::string name() const;

	// protected: XXX sigh this should be here

	boost::shared_ptr<ARDOUR::Route> _route;

	void request_redraw ();

	virtual void set_color (const Gdk::Color & c);
	void choose_color ();

	bool ignore_toggle;
	bool wait_for_release;
	bool multiple_mute_change;
	bool multiple_solo_change;

	Gtk::HBox _invert_button_box;
	ArdourButton* mute_button;
	ArdourButton* solo_button;
	ArdourButton* rec_enable_button; /* audio tracks */
	ArdourButton* show_sends_button; /* busses */
	ArdourButton* monitor_input_button;
	ArdourButton* monitor_disk_button;

	Glib::RefPtr<Gdk::Pixbuf> solo_safe_pixbuf;

        ArdourButton* solo_safe_led;
        ArdourButton* solo_isolated_led;

	Gtk::Label monitor_input_button_label;
	Gtk::Label monitor_disk_button_label;

	void send_blink (bool);
	sigc::connection send_blink_connection;

	sigc::connection rec_blink_connection;

	Gtk::Menu* mute_menu;
	Gtk::Menu* solo_menu;
	Gtk::Menu* sends_menu;

	boost::shared_ptr<ARDOUR::Delivery> _current_delivery;

	bool mute_press(GdkEventButton*);
	bool mute_release(GdkEventButton*);
	bool solo_press(GdkEventButton*);
	bool solo_release(GdkEventButton*);
	bool rec_enable_press(GdkEventButton*);
	bool rec_enable_release(GdkEventButton*);
	bool show_sends_press(GdkEventButton*);
	bool show_sends_release(GdkEventButton*);

	bool monitor_release(GdkEventButton*, ARDOUR::MonitorChoice);
	bool monitor_input_press(GdkEventButton*);
	bool monitor_input_release(GdkEventButton*);
	bool monitor_disk_press(GdkEventButton*);
	bool monitor_disk_release(GdkEventButton*);
	void monitoring_changed ();
	void update_monitoring_display ();

	void edit_input_configuration ();
	void edit_output_configuration ();

	void step_gain_up ();
	void step_gain_down ();
	void page_gain_up ();
	void page_gain_down ();

	void build_sends_menu ();
	void set_sends_gain_from_track ();
	void set_sends_gain_to_zero ();
	void set_sends_gain_to_unity ();
	void create_sends (ARDOUR::Placement, bool);
	void create_selected_sends (ARDOUR::Placement, bool);

	void solo_changed(bool, void*);
	void solo_changed_so_update_mute ();
	void listen_changed(void*);
	virtual void processors_changed (ARDOUR::RouteProcessorChange) {}
	void route_rec_enable_changed();
	void session_rec_enable_changed();

	void build_solo_menu ();

	void solo_isolated_toggle (void*, Gtk::CheckMenuItem*);
	void toggle_solo_isolated (Gtk::CheckMenuItem*);

        bool solo_isolate_button_release (GdkEventButton*);
        bool solo_safe_button_release (GdkEventButton*);

	void solo_safe_toggle (void*, Gtk::CheckMenuItem*);
	void toggle_solo_safe (Gtk::CheckMenuItem*);

	Gtk::CheckMenuItem* pre_fader_mute_check;
	Gtk::CheckMenuItem* post_fader_mute_check;
	Gtk::CheckMenuItem* listen_mute_check;
	Gtk::CheckMenuItem* main_mute_check;
	Gtk::CheckMenuItem* solo_safe_check;
	Gtk::CheckMenuItem* solo_isolated_check;

	void toggle_mute_menu(ARDOUR::MuteMaster::MutePoint, Gtk::CheckMenuItem*);
	void muting_change ();
	void build_mute_menu(void);
	void init_mute_menu(ARDOUR::MuteMaster::MutePoint, Gtk::CheckMenuItem*);

	int  set_color_from_route ();

	void remove_this_route (bool apply_to_selection = false);
	static gint idle_remove_this_route (RouteUI *);

	void route_rename();

	virtual void property_changed (const PBD::PropertyChange&);
	void route_removed ();

	virtual void route_active_changed () {}
	void set_route_active (bool, bool);

        Gtk::Menu* record_menu;
        void build_record_menu ();

	Gtk::CheckMenuItem *step_edit_item;
	void toggle_step_edit ();
	virtual void step_edit_changed (bool);

	virtual void polarity_changed ();

	Gtk::CheckMenuItem *denormal_menu_item;
	void toggle_denormal_protection();
	virtual void denormal_protection_changed ();

	void disconnect_input ();
	void disconnect_output ();

	virtual void blink_rec_display (bool onoff);
	void update_mute_display ();

	void update_solo_display ();

	virtual void map_frozen ();

	void adjust_latency ();
	void save_as_template ();
	void open_remote_control_id_dialog ();

	static Gtkmm2ext::ActiveState solo_active_state (boost::shared_ptr<ARDOUR::Route>);
	static Gtkmm2ext::ActiveState solo_isolate_active_state (boost::shared_ptr<ARDOUR::Route>);
	static Gtkmm2ext::ActiveState solo_safe_active_state (boost::shared_ptr<ARDOUR::Route>);
	static Gtkmm2ext::ActiveState mute_active_state (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);

	/** Emitted when a bus has been set or unset from `display sends to this bus' mode
	 *  by a click on the `Sends' button.  The parameter is the route that the sends are
	 *  to, or 0 if no route is now in this mode.
	 */
	static PBD::Signal1<void, boost::shared_ptr<ARDOUR::Route> > BusSendDisplayChanged;

	void comment_editor_done_editing ();
	void setup_comment_editor ();
	void open_comment_editor ();
	void toggle_comment_editor ();

	gint comment_key_release_handler (GdkEventKey*);
	void comment_changed (void *src);
	void comment_edited ();
	bool ignore_comment_edit;

   protected:

	ArdourWindow*  comment_window;
	Gtk::TextView* comment_area;
	IOSelectorWindow *input_selector;
	IOSelectorWindow *output_selector;

	PBD::ScopedConnectionList route_connections;
	bool self_destruct;

 	void init ();
 	void reset ();

	void self_delete ();
        virtual void start_step_editing () {}
        virtual void stop_step_editing() {}

        void set_invert_sensitive (bool);
	bool verify_new_route_name (const std::string& name);

	void route_gui_changed (std::string);
	virtual void route_color_changed () {}

	virtual void bus_send_display_changed (boost::shared_ptr<ARDOUR::Route>);

  private:
	void check_rec_enable_sensitivity ();
	void parameter_changed (std::string const &);
	void relabel_solo_button ();
	void track_mode_changed ();

	std::string route_state_id () const;

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

	void setup_invert_buttons ();
	void set_invert_button_state ();
	void invert_menu_toggled (uint32_t);
	bool invert_press (GdkEventButton *);
	bool invert_release (GdkEventButton *, uint32_t i);

	int _i_am_the_modifier;
	std::vector<ArdourButton*> _invert_buttons;
	Gtk::Menu* _invert_menu;

	static void set_showing_sends_to (boost::shared_ptr<ARDOUR::Route>);
	static boost::weak_ptr<ARDOUR::Route> _showing_sends_to;
	
	static uint32_t _max_invert_buttons;
};

#endif /* __ardour_route_ui__ */
