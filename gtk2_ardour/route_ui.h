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

#include "axis_view.h"

namespace Gtkmm2ext {
	class BindableToggleButton;
}

namespace ARDOUR {
	class AudioTrack;
}

namespace Gtk {
	class Menu;
	class CheckMenuItem;
	class Widget;
}

class RouteUI : public virtual AxisView
{
  public:
	RouteUI(ARDOUR::Route&, ARDOUR::Session&, const char*, const char*, const char*);
	virtual ~RouteUI();

	bool is_audio_track() const;
	ARDOUR::DiskStream* get_diskstream() const;

	ARDOUR::Route& route() const { return _route; }
	ARDOUR::AudioTrack* audio_track() const;

	string name() const;
	
	ARDOUR::Route& _route;
	
	void set_color (const Gdk::Color & c);
	bool choose_color ();

	bool ignore_toggle;
	bool wait_for_release;

	Gtkmm2ext::BindableToggleButton * mute_button;
	Gtkmm2ext::BindableToggleButton * solo_button;
	Gtkmm2ext::BindableToggleButton * rec_enable_button;
	
	virtual string solo_button_name () const { return "SoloButton"; }
	virtual string safe_solo_button_name () const { return "SafeSoloButton"; }
	
	Gtk::Menu* mute_menu;
	Gtk::Menu* solo_menu;

	XMLNode *xml_node;
	void ensure_xml_node ();

	XMLNode* get_child_xml_node (ARDOUR::stringcr_t childname);
	
	gint mute_press(GdkEventButton*);
	gint mute_release(GdkEventButton*);
	gint solo_press(GdkEventButton*);
	gint solo_release(GdkEventButton*);
	gint rec_enable_press(GdkEventButton*);

	void solo_changed(void*);
	void mute_changed(void*);
	void route_rec_enable_changed(void*);
	void session_rec_enable_changed();

	void build_solo_menu (void);

	void solo_safe_toggle (void*, Gtk::CheckMenuItem*);
	void toggle_solo_safe (Gtk::CheckMenuItem*);

	void toggle_mute_menu(ARDOUR::mute_type, Gtk::CheckMenuItem*);
	void pre_fader_toggle(void*, Gtk::CheckMenuItem*);
	void post_fader_toggle(void*, Gtk::CheckMenuItem*);
	void control_outs_toggle(void*, Gtk::CheckMenuItem*);
	void main_outs_toggle(void*, Gtk::CheckMenuItem*);

	void build_mute_menu(void);
	void init_mute_menu(ARDOUR::mute_type, Gtk::CheckMenuItem*);
	
	void set_mix_group_solo(ARDOUR::Route&, bool);
	void set_mix_group_mute(ARDOUR::Route&, bool);
	void set_mix_group_rec_enable(ARDOUR::Route&, bool);

	int  set_color_from_route ();

	sigc::connection blink_connection;

	void rec_enable_button_blink (bool onoff, ARDOUR::DiskStream *, Gtk::Widget *w);
	
	void remove_this_route ();
	static gint idle_remove_this_route (RouteUI *);

	void route_rename();
	
	virtual void name_changed (void *src);
	void route_removed ();

	static gint okay_gplusplus_cannot_do_complex_templates (RouteUI *rui);

	Gtk::CheckMenuItem *route_active_menu_item;
	void toggle_route_active ();
	virtual void route_active_changed ();

	void disconnect_input ();
	void disconnect_output ();

	void update_rec_display ();
	void update_mute_display ();
	void update_solo_display ();
	virtual void map_frozen ();

	void reversibly_apply_route_boolean (string name, void (ARDOUR::Route::*func)(bool, void*), bool, void *);
	void reversibly_apply_audio_track_boolean (string name, void (ARDOUR::AudioTrack::*func)(bool, void*), bool, void *);
};

#endif /* __ardour_route_ui__ */
