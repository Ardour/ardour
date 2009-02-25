/*
    Copyright (C) 2000 Paul Davis 

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

#ifndef __ardour_mixer_ui_h__
#define __ardour_mixer_ui_h__

#include <list>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/frame.h>
#include <gtkmm/paned.h>
#include <gtkmm/menu.h>
#include <gtkmm/treeview.h>

#include "pbd/stateful.h"

#include "ardour/ardour.h"

#include "route_processor_selection.h"
#include "enums.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
	class Session;
	class AudioDiskstream;
};

class MixerStrip;
class PluginSelector;

class Mixer_UI : public Gtk::Window
{
  public:
	Mixer_UI ();
	~Mixer_UI();

	void connect_to_session (ARDOUR::Session *);
	
	PluginSelector&  plugin_selector() { return *_plugin_selector; }

	void  set_strip_width (Width);
	Width get_strip_width () const { return _strip_width; }

	void unselect_strip_in_display (MixerStrip*);
	void select_strip_in_display (MixerStrip*);

	XMLNode& get_state (void);
	int set_state (const XMLNode& );

	void show_window ();
	bool hide_window (GdkEventAny *ev);
	void show_strip (MixerStrip *);
	void hide_strip (MixerStrip *);

	void ensure_float (Gtk::Window&);
	void toggle_auto_rebinding ();
	void set_auto_rebinding(bool);

	RouteRedirectSelection& selection() { return _selection; }

	static const char* get_order_key();
	
  private:
	ARDOUR::Session         *session;

	bool					_visible;
	
	Gtk::HBox				global_hpacker;
	Gtk::VBox				global_vpacker;
	Gtk::ScrolledWindow		scroller;
	Gtk::EventBox			scroller_base;
	Gtk::HBox				scroller_hpacker;
	Gtk::VBox				mixer_scroller_vpacker;
	Gtk::VBox				list_vpacker;
	Gtk::Label				group_display_button_label;
	Gtk::Button				group_display_button;
	Gtk::ScrolledWindow		track_display_scroller;
	Gtk::ScrolledWindow		group_display_scroller;
	Gtk::VBox				group_display_vbox;
	Gtk::Frame				track_display_frame;
	Gtk::Frame				group_display_frame;
	Gtk::VPaned				rhs_pane1;
	Gtk::HBox				strip_packer;
	Gtk::HBox				out_packer;
	Gtk::HPaned				list_hpane;

	// for restoring window geometry.
	int m_root_x, m_root_y, m_width, m_height;
	
	void set_window_pos_and_size ();
	void get_window_pos_and_size ();

	bool on_key_press_event (GdkEventKey*);

	void pane_allocation_handler (Gtk::Allocation&, Gtk::Paned*);
	
	list<MixerStrip *> strips;

	bool strip_scroller_button_release (GdkEventButton*);

	void add_strip (ARDOUR::RouteList&);
	void remove_strip (MixerStrip *);

	void hide_all_strips (bool with_select);
	void unselect_all_strips();
	void select_all_strips ();
	void unselect_all_audiotrack_strips ();
	void select_all_audiotrack_strips ();
	void unselect_all_audiobus_strips ();
	void select_all_audiobus_strips ();

	void auto_rebind_midi_controls ();
	bool auto_rebinding;

	void strip_select_op (bool audiotrack, bool select);
	void select_strip_op (MixerStrip*, bool select);

	void follow_strip_selection ();

	gint start_updating ();
	gint stop_updating ();

	void disconnect_from_session ();
	
	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void track_name_changed (MixerStrip *);

	void redisplay_track_list ();
	bool no_track_list_redisplay;
	bool track_display_button_press (GdkEventButton*);
#ifdef GTKOSX
	void queue_draw_all_strips ();
#endif
	
	void track_list_change (const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
	void track_list_delete (const Gtk::TreeModel::Path&);
	void track_list_reorder (const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, int* new_order);

	void initial_track_display ();
	void show_track_list_menu ();

	void set_all_strips_visibility (bool yn);
	void set_all_audio_visibility (int tracks, bool yn);
	
	void hide_all_routes ();
	void show_all_routes ();
	void show_all_audiobus ();
	void hide_all_audiobus ();
	void show_all_audiotracks();
	void hide_all_audiotracks ();

	Gtk::Menu* mix_group_context_menu;
	bool in_group_row_change;

	void group_selected (gint row, gint col, GdkEvent *ev);
	void group_unselected (gint row, gint col, GdkEvent *ev);
	void group_display_active_clicked();
	void new_mix_group ();
	void remove_selected_mix_group ();
	void build_mix_group_context_menu ();
	void activate_all_mix_groups ();
	void disable_all_mix_groups ();
	void add_mix_group (ARDOUR::RouteGroup *);
	void mix_groups_changed ();
	void mix_group_name_edit (const Glib::ustring&, const Glib::ustring&);
	void mix_group_row_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter);

	Gtk::Menu *track_menu;
	void track_column_click (gint);
	void build_track_menu ();

	PluginSelector    *_plugin_selector;

	void strip_name_changed (MixerStrip *);

	void group_flags_changed (void *src, ARDOUR::RouteGroup *);

	/* various treeviews */
	
	struct TrackDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    TrackDisplayModelColumns () {
		    add (text);
		    add (visible);
		    add (route);
		    add (strip);
	    }
	    Gtk::TreeModelColumn<bool>           visible;
	    Gtk::TreeModelColumn<Glib::ustring>  text;
	    Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Route> > route;
	    Gtk::TreeModelColumn<MixerStrip*>    strip;
	};

	struct GroupDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    GroupDisplayModelColumns() { 
		    add (active);
		    add (visible);
		    add (text);
		    add (group);
	    }
	    Gtk::TreeModelColumn<bool>					active;
	    Gtk::TreeModelColumn<bool>					visible;
	    Gtk::TreeModelColumn<Glib::ustring>			text;
	    Gtk::TreeModelColumn<ARDOUR::RouteGroup*>	group;
	};

	TrackDisplayModelColumns    track_columns;
	GroupDisplayModelColumns    group_columns;

	Gtk::TreeView track_display;
	Gtk::TreeView group_display;

	Glib::RefPtr<Gtk::ListStore> track_model;
	Glib::RefPtr<Gtk::ListStore> group_model;

	bool group_display_button_press (GdkEventButton*);
	void group_display_selection_changed ();

	bool strip_button_release_event (GdkEventButton*, MixerStrip*);

	RouteRedirectSelection _selection;

	Width _strip_width;

	void sync_order_keys (const char *src);
	bool strip_redisplay_does_not_reset_order_keys;
	bool strip_redisplay_does_not_sync_order_keys;
	bool ignore_sync;

	static const int32_t default_width = 478;
	static const int32_t default_height = 765;
};

#endif /* __ardour_mixer_ui_h__ */


