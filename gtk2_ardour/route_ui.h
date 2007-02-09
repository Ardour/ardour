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

    $Id$
*/

#ifndef __ardour_route_ui__
#define __ardour_route_ui__

#include <list>

#include <pbd/xml++.h>
#include <ardour/ardour.h>
#include <ardour/route.h>
#include <ardour/track.h>

#include "axis_view.h"

namespace ARDOUR {
	class AudioTrack;
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
	RouteUI(boost::shared_ptr<ARDOUR::Route>, ARDOUR::Session&, const char*, const char*, const char*);
	virtual ~RouteUI();

	bool is_track() const;
	bool is_audio_track() const;

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }
	
	// FIXME: make these return shared_ptr
	ARDOUR::Track*      track() const;
	ARDOUR::AudioTrack* audio_track() const;
	
	boost::shared_ptr<ARDOUR::Diskstream> get_diskstream() const;

	string name() const;

	// protected: XXX sigh this should be here

	boost::shared_ptr<ARDOUR::Route> _route;
	
	void set_color (const Gdk::Color & c);
	bool choose_color ();

	bool ignore_toggle;
	bool wait_for_release;

	BindableToggleButton* mute_button;
	BindableToggleButton* solo_button;
	BindableToggleButton* rec_enable_button;
	
	virtual string solo_button_name () const { return "SoloButton"; }
	virtual string safe_solo_button_name () const { return "SafeSoloButton"; }
	
	Gtk::Menu* mute_menu;
	Gtk::Menu* solo_menu;
	Gtk::Menu* remote_control_menu;

	XMLNode *xml_node;
	void ensure_xml_node ();

	XMLNode* get_child_xml_node (const string & childname);
	
	bool mute_press(GdkEventButton*);
	bool mute_release(GdkEventButton*);
	bool solo_press(GdkEventButton*);
	bool solo_release(GdkEventButton*);
	bool rec_enable_press(GdkEventButton*);

	void solo_changed(void*);
	void mute_changed(void*);
	virtual void redirects_changed (void *) {}
	void route_rec_enable_changed();
	void session_rec_enable_changed();

	void build_solo_menu (void);
	void build_remote_control_menu (void);
	void refresh_remote_control_menu ();

	void solo_safe_toggle (void*, Gtk::CheckMenuItem*);
	void toggle_solo_safe (Gtk::CheckMenuItem*);

	void toggle_mute_menu(ARDOUR::mute_type, Gtk::CheckMenuItem*);
	void pre_fader_toggle(void*, Gtk::CheckMenuItem*);
	void post_fader_toggle(void*, Gtk::CheckMenuItem*);
	void control_outs_toggle(void*, Gtk::CheckMenuItem*);
	void main_outs_toggle(void*, Gtk::CheckMenuItem*);

	void build_mute_menu(void);
	void init_mute_menu(ARDOUR::mute_type, Gtk::CheckMenuItem*);
	
	void set_mix_group_solo(boost::shared_ptr<ARDOUR::Route>, bool);
	void set_mix_group_mute(boost::shared_ptr<ARDOUR::Route>, bool);
	void set_mix_group_rec_enable(boost::shared_ptr<ARDOUR::Route>, bool);

	int  set_color_from_route ();

	sigc::connection blink_connection;

	void rec_enable_button_blink (bool onoff, ARDOUR::AudioDiskstream *, Gtk::Widget *w);
	
	void remove_this_route ();
	static gint idle_remove_this_route (RouteUI *);

	void route_rename();
	
	virtual void name_changed (void *src);
	void route_removed ();

	Gtk::CheckMenuItem *route_active_menu_item;
	void toggle_route_active ();
	virtual void route_active_changed ();

	Gtk::CheckMenuItem *polarity_menu_item;
	void toggle_polarity ();
	virtual void polarity_changed ();

	void disconnect_input ();
	void disconnect_output ();

	void update_rec_display ();
	void update_mute_display ();

	bool was_solo_safe;
	void update_solo_display ();

	virtual void map_frozen ();

	void set_remote_control_id (uint32_t id, Gtk::CheckMenuItem* item);

	void reversibly_apply_route_boolean (string name, void (ARDOUR::Route::*func)(bool, void*), bool, void *);
	void reversibly_apply_audio_track_boolean (string name, void (ARDOUR::AudioTrack::*func)(bool, void*), bool, void *);
};

#endif /* __ardour_route_ui__ */
