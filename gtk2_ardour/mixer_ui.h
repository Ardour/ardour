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
#include <gtkmm/liststore.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "enums.h"
#include "mixer_actor.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
};

class MixerStrip;
class PluginSelector;
class MixerGroupTabs;
class MonitorSection;

class Mixer_UI : public Gtk::Window, public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr, public MixerActor
{
  public:
	static Mixer_UI* instance();
	~Mixer_UI();

	void set_session (ARDOUR::Session *);
	void track_editor_selection ();

	PluginSelector* plugin_selector();

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

        MonitorSection* monitor_section() const { return _monitor_section; }

  protected:
	void set_route_targets_for_operation ();

  private:
	Mixer_UI ();
	static Mixer_UI* _instance;

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

	MixerGroupTabs* _group_tabs;

	// for restoring window geometry.
	int m_root_x, m_root_y, m_width, m_height;

	void set_window_pos_and_size ();
	void get_window_pos_and_size ();

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
	bool on_scroll_event (GdkEventScroll*);

	void pane_allocation_handler (Gtk::Allocation&, Gtk::Paned*);

	std::list<MixerStrip *> strips;

	bool strip_scroller_button_release (GdkEventButton*);
	void scroll_left ();
	void scroll_right ();

	void add_strip (ARDOUR::RouteList&);
	void remove_strip (MixerStrip *);

	MixerStrip* strip_by_route (boost::shared_ptr<ARDOUR::Route>);

	void hide_all_strips (bool with_select);
	void unselect_all_strips();
	void select_all_strips ();
	void unselect_all_audiotrack_strips ();
	void select_all_audiotrack_strips ();
	void unselect_all_audiobus_strips ();
	void select_all_audiobus_strips ();

	void strip_select_op (bool audiotrack, bool select);
	void select_strip_op (MixerStrip*, bool select);

	gint start_updating ();
	gint stop_updating ();

	void session_going_away ();

	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void track_name_changed (MixerStrip *);

	void redisplay_track_list ();
	bool no_track_list_redisplay;
	bool track_display_button_press (GdkEventButton*);
	void strip_width_changed ();

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

	bool in_group_row_change;

	void group_selected (gint row, gint col, GdkEvent *ev);
	void group_unselected (gint row, gint col, GdkEvent *ev);
	void group_display_active_clicked();
	void new_route_group ();
	void remove_selected_route_group ();
	void activate_all_route_groups ();
	void disable_all_route_groups ();
	void add_route_group (ARDOUR::RouteGroup *);
	void route_groups_changed ();
	void route_group_name_edit (const std::string&, const std::string&);
	void route_group_row_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter);
	void route_group_row_deleted (Gtk::TreeModel::Path const &);

	Gtk::Menu *track_menu;
	void track_column_click (gint);
	void build_track_menu ();

        MonitorSection* _monitor_section;
	PluginSelector    *_plugin_selector;

	void strip_property_changed (const PBD::PropertyChange&, MixerStrip *);
	void route_group_property_changed (ARDOUR::RouteGroup *, const PBD::PropertyChange &);

	/* various treeviews */

	struct TrackDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    TrackDisplayModelColumns () {
		    add (text);
		    add (visible);
		    add (route);
		    add (strip);
	    }
	    Gtk::TreeModelColumn<bool>           visible;
	    Gtk::TreeModelColumn<std::string>  text;
	    Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Route> > route;
	    Gtk::TreeModelColumn<MixerStrip*>    strip;
	};

	struct GroupDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    GroupDisplayModelColumns() {
		    add (visible);
		    add (text);
		    add (group);
	    }
	    Gtk::TreeModelColumn<bool>					visible;
	    Gtk::TreeModelColumn<std::string>			text;
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

	Width _strip_width;

	void sync_order_keys (std::string const &);
	bool strip_redisplay_does_not_reset_order_keys;
	bool strip_redisplay_does_not_sync_order_keys;
	bool ignore_sync;

	void parameter_changed (std::string const &);
	void set_route_group_activation (ARDOUR::RouteGroup *, bool);

	void setup_track_display ();
	void new_track_or_bus ();

	static const int32_t default_width = 478;
	static const int32_t default_height = 765;

	/** true if we are rebuilding the route group list, or clearing
	    it during a session teardown.
	*/
	bool _in_group_rebuild_or_clear;

	void update_title ();
	MixerStrip* strip_by_x (int x);

	friend class MixerGroupTabs;

	void follow_editor_selection ();
	bool _following_editor_selection;

	void monitor_section_going_away ();
};

#endif /* __ardour_mixer_ui_h__ */


