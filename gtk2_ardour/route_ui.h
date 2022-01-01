/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_route_ui__
#define __ardour_route_ui__

#include <list>

#include "pbd/signals.h"
#include "pbd/xml++.h"

#include <gtkmm/colorselection.h>
#include <gtkmm/textview.h>

#include "gtkmm2ext/widget_state.h"

#include "ardour/ardour.h"
#include "ardour/mute_master.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/session_handle.h"
#include "ardour/track.h"

#include "axis_view.h"
#include "selectable.h"
#include "stripable_colorpicker.h"
#include "window_manager.h"

namespace ARDOUR {
	class AudioTrack;
	class MidiTrack;
	class SoloMuteRelease;
}

namespace Gtk {
	class Menu;
	class CheckMenuItem;
	class Widget;
}

namespace ArdourWidgets {
	class ArdourButton;
	class Prompter;
}

class ArdourWindow;
class IOSelectorWindow;
class PatchChangeGridDialog;
class PlaylistSelector;
class SaveTemplateDialog;

class RoutePinWindowProxy : public WM::ProxyBase
{
public:
	RoutePinWindowProxy (std::string const&, boost::shared_ptr<ARDOUR::Route>);
	~RoutePinWindowProxy ();

	Gtk::Window*              get (bool create = false);
	ARDOUR::SessionHandlePtr* session_handle ();

private:
	boost::weak_ptr<ARDOUR::Route> _route;

	void                  route_going_away ();
	PBD::ScopedConnection going_away_connection;
};

class RouteUI : public virtual Selectable, public virtual ARDOUR::SessionHandlePtr, public virtual PBD::ScopedConnectionList, public virtual sigc::trackable
{
public:
	RouteUI (ARDOUR::Session*);

	virtual ~RouteUI ();

	boost::shared_ptr<ARDOUR::Stripable> stripable () const;

	virtual void set_session (ARDOUR::Session*);
	virtual void set_route (boost::shared_ptr<ARDOUR::Route>);
	virtual void set_button_names () = 0;

	bool is_track () const;
	bool is_master () const;
	bool is_foldbackbus () const;
	bool is_audio_track () const;
	bool is_midi_track () const;
	bool has_audio_outputs () const;

	boost::shared_ptr<ARDOUR::Route> route () const
	{
		return _route;
	}
	ARDOUR::RouteGroup* route_group () const;

	boost::shared_ptr<ARDOUR::Track>      track () const;
	boost::shared_ptr<ARDOUR::AudioTrack> audio_track () const;
	boost::shared_ptr<ARDOUR::MidiTrack>  midi_track () const;

	Gdk::Color route_color () const;

	// protected: XXX sigh this should be here
	// callbacks used by dervice classes via &RouteUI::*

	void edit_input_configuration ();
	void edit_output_configuration ();
	void select_midi_patch ();
	void choose_color ();
	void route_rename ();
	void manage_pins ();
	void duplicate_selected_routes ();
	void toggle_step_edit ();
	void toggle_denormal_protection ();
	void save_as_template ();

	bool mute_press (GdkEventButton*);
	bool mute_release (GdkEventButton*);
	bool solo_press (GdkEventButton*);
	bool solo_release (GdkEventButton*);
	bool rec_enable_press (GdkEventButton*);
	bool rec_enable_release (GdkEventButton*);
	bool show_sends_press (GdkEventButton*);
	bool show_sends_release (GdkEventButton*);
	bool solo_isolate_button_release (GdkEventButton*);
	bool solo_safe_button_release (GdkEventButton*);

	bool monitor_release (GdkEventButton*, ARDOUR::MonitorChoice);
	bool monitor_input_press (GdkEventButton*);
	bool monitor_input_release (GdkEventButton*);
	bool monitor_disk_press (GdkEventButton*);
	bool monitor_disk_release (GdkEventButton*);
	void update_monitoring_display ();
	void open_comment_editor ();
	void toggle_comment_editor ();
	void comment_changed ();
	void set_route_active (bool, bool);
	void set_disk_io_point (ARDOUR::DiskIOPoint);
	void fan_out (bool to_busses = true, bool group = true);

	/* The editor calls these when mapping an operation across multiple tracks */
	void use_new_playlist (std::string name, std::string group_id, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const&, bool copy);
	void clear_playlist ();

	void        use_playlist (Gtk::RadioMenuItem* item, boost::weak_ptr<ARDOUR::Playlist> wpl);
	void        select_playlist_matching (boost::weak_ptr<ARDOUR::Playlist> wpl);
	void        show_playlist_selector ();

	/* used by EditorRoutes */
	static Gtkmm2ext::ActiveState solo_active_state (boost::shared_ptr<ARDOUR::Stripable>);
	static Gtkmm2ext::ActiveState solo_isolate_active_state (boost::shared_ptr<ARDOUR::Stripable>);
	static Gtkmm2ext::ActiveState solo_safe_active_state (boost::shared_ptr<ARDOUR::Stripable>);
	static Gtkmm2ext::ActiveState mute_active_state (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Stripable>);

protected:
	virtual void set_color (uint32_t c);
	virtual void processors_changed (ARDOUR::RouteProcessorChange) {}

	virtual void route_property_changed (const PBD::PropertyChange&) = 0;
	virtual void route_active_changed () {}

	void disconnect_input ();
	void disconnect_output ();

	Gtk::HBox invert_button_box;

	ArdourWidgets::ArdourButton* mute_button;
	ArdourWidgets::ArdourButton* solo_button;
	ArdourWidgets::ArdourButton* rec_enable_button; /* audio tracks */
	ArdourWidgets::ArdourButton* show_sends_button; /* busses */
	ArdourWidgets::ArdourButton* monitor_input_button;
	ArdourWidgets::ArdourButton* monitor_disk_button;

	ArdourWidgets::ArdourButton* solo_safe_led;
	ArdourWidgets::ArdourButton* solo_isolated_led;

	Gtk::Menu* mute_menu;
	Gtk::Menu* solo_menu;
	Gtk::Menu* sends_menu;

	boost::shared_ptr<ARDOUR::Route>    _route;
	boost::shared_ptr<ARDOUR::Delivery> _current_delivery;

	Gtk::CheckMenuItem* pre_fader_mute_check;
	Gtk::CheckMenuItem* post_fader_mute_check;
	Gtk::CheckMenuItem* listen_mute_check;
	Gtk::CheckMenuItem* main_mute_check;
	Gtk::CheckMenuItem* solo_safe_check;
	Gtk::CheckMenuItem* solo_isolated_check;
	int                 set_color_from_route ();

protected:
	typedef std::map<PBD::ID, IOSelectorWindow*> IOSelectorMap;

	static IOSelectorMap input_selectors;
	static IOSelectorMap output_selectors;

	static void delete_ioselector (IOSelectorMap&, boost::shared_ptr<ARDOUR::Route>);

	static void help_count_plugins (boost::weak_ptr<ARDOUR::Processor> p, uint32_t*);

	PBD::ScopedConnectionList route_connections;
	bool                      self_destruct;

	void init ();
	void reset ();

	virtual void self_delete ();
	virtual void blink_rec_display (bool onoff);
	virtual void map_frozen ();
	virtual void route_rec_enable_changed ();
	virtual void route_color_changed () {}
	virtual void start_step_editing () {}
	virtual void stop_step_editing () {}
	virtual void create_sends (ARDOUR::Placement, bool);
	virtual void create_selected_sends (ARDOUR::Placement, bool);
	virtual void bus_send_display_changed (boost::shared_ptr<ARDOUR::Route>);

	bool mark_hidden (bool yn);
	void set_invert_sensitive (bool);
	bool verify_new_route_name (const std::string& name);
	void check_rec_enable_sensitivity ();
	void route_gui_changed (PBD::PropertyChange const&);

	PatchChangeGridDialog* patch_change_dialog () const;

	std::string playlist_tip () const;
	void        build_playlist_menu ();
	Gtk::Menu*  playlist_action_menu;

	void         show_playlist_copy_selector ();
	void         show_playlist_share_selector ();
	void         show_playlist_steal_selector ();

	Gtk::CheckMenuItem* denormal_menu_item;

	static void        set_showing_sends_to (boost::shared_ptr<ARDOUR::Route>);
	static std::string program_port_prefix;

	ARDOUR::SoloMuteRelease* _solo_release;
	ARDOUR::SoloMuteRelease* _mute_release;

private:
	void setup_invert_buttons ();
	void invert_menu_toggled (uint32_t);
	bool invert_press (GdkEventButton*);
	bool invert_release (GdkEventButton*, uint32_t i);

	void toggle_solo_safe (Gtk::CheckMenuItem*);
	void toggle_mute_menu (ARDOUR::MuteMaster::MutePoint, Gtk::CheckMenuItem*);
	void toggle_solo_isolated (Gtk::CheckMenuItem*);

	void update_solo_display ();
	void update_mute_display ();
	void update_polarity_display ();
	void update_solo_button ();
	void solo_changed_so_update_mute ();
	void session_rec_enable_changed ();
	void denormal_protection_changed ();
	void muting_change ();

	void step_edit_changed (bool);
	void toggle_rec_safe ();

	void setup_comment_editor ();
	void comment_editor_done_editing ();

	void init_mute_menu (ARDOUR::MuteMaster::MutePoint, Gtk::CheckMenuItem*);
	void build_mute_menu ();
	void build_solo_menu ();
	void build_record_menu ();

	void build_sends_menu ();
	void set_sends_gain_from_track ();
	void set_sends_gain_to_zero ();
	void set_sends_gain_to_unity ();

	void rename_current_playlist ();

	void parameter_changed (std::string const&);
	void relabel_solo_button ();
	void track_mode_changed ();
	void send_blink (bool);

	void delete_patch_change_dialog ();
	void maybe_add_route_print_mgr ();

	std::string route_state_id () const;

	void save_as_template_dialog_response (int response, SaveTemplateDialog* d);

	std::string resolve_new_group_playlist_name (std::string const&, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const&);

	PlaylistSelector*  _playlist_selector;

	Gtk::Menu*     _record_menu;
	ArdourWindow*  _comment_window;
	Gtk::TextView* _comment_area;

	Gtk::CheckMenuItem* _step_edit_item;
	Gtk::CheckMenuItem* _rec_safe_item;

	bool       _ignore_comment_edit;
	int        _i_am_the_modifier;
	Gtk::Menu* _invert_menu;
	uint32_t   _n_polarity_invert;

	std::vector<ArdourWidgets::ArdourButton*> _invert_buttons;

	StripableColorDialog _color_picker;

	sigc::connection send_blink_connection;
	sigc::connection rec_blink_connection;

	/** Emitted when a bus has been set or unset from `display sends to this bus' mode
	 *  by a click on the `Sends' button.  The parameter is the route that the sends are
	 *  to, or 0 if no route is now in this mode.
	 */
	static PBD::Signal1<void, boost::shared_ptr<ARDOUR::Route> > BusSendDisplayChanged;

	static boost::weak_ptr<ARDOUR::Route> _showing_sends_to;

	static uint32_t _max_invert_buttons;
};

#endif /* __ardour_route_ui__ */
