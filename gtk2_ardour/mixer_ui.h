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

    $Id$
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

#include <ardour/ardour.h>
#include <ardour/stateful.h>
#include <ardour/io.h>

#include "keyboard_target.h"
#include "route_redirect_selection.h"
#include "enums.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
	class Session;
	class DiskStream;
	class AudioEngine;
};

class MixerStrip;
class PluginSelector;

class Mixer_UI : public Gtk::Window
{
  public:
	Mixer_UI (ARDOUR::AudioEngine&);
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

	void ensure_float (Gtk::Window&);

	RouteRedirectSelection& selection() { return _selection; }
	
  private:
	ARDOUR::AudioEngine&     engine;
	ARDOUR::Session         *session;
	
	Gtk::HBox                global_hpacker;
	Gtk::VBox                global_vpacker;
	Gtk::ScrolledWindow      scroller;
	Gtk::EventBox            scroller_base;
	Gtk::HBox                scroller_hpacker;
	Gtk::VBox                mixer_scroller_vpacker;
	Gtk::VBox                list_vpacker;
	Gtk::Label               group_display_button_label;
	Gtk::Button              group_display_button;
	Gtk::ScrolledWindow      track_display_scroller;
	Gtk::ScrolledWindow      group_display_scroller;
	Gtk::VBox		 group_display_vbox;
	Gtk::Frame 		 track_display_frame;
	Gtk::Frame		 group_display_frame;
	Gtk::VPaned		 rhs_pane1;
	Gtk::HBox                strip_packer;
	Gtk::HBox                out_packer;
	Gtk::HPaned		 list_hpane;

	void pane_allocation_handler (Gtk::Allocation&, Gtk::Paned*);
	
	list<MixerStrip *> strips;

	bool strip_scroller_button_release (GdkEventButton*);

	void add_strip (ARDOUR::Route*);
	void remove_strip (MixerStrip *);

	void show_strip (MixerStrip *);
	void hide_strip (MixerStrip *);

	void hide_all_strips (bool with_select);
	void unselect_all_strips();
	void select_all_strips ();
	void unselect_all_audiotrack_strips ();
	void select_all_audiotrack_strips ();
	void unselect_all_audiobus_strips ();
	void select_all_audiobus_strips ();

	void strip_select_op (bool audiotrack, bool select);
	void select_strip_op (MixerStrip*, bool select);

	void follow_strip_selection ();

	gint start_updating ();
	gint stop_updating ();

	void disconnect_from_session ();
	
	sigc::connection screen_update_connection;
	void update_strips ();
	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void track_display_selected (gint row, gint col, GdkEvent *ev);
	void track_display_unselected (gint row, gint col, GdkEvent *ev);
	void track_name_changed (MixerStrip *);

	void track_display_reordered_proxy (const Gtk::TreePath& path, const Gtk::TreeIter& i, int* n);
	void track_display_reordered ();
	sigc::connection reorder_connection;

	void group_selected (gint row, gint col, GdkEvent *ev);
	void group_unselected (gint row, gint col, GdkEvent *ev);
	void group_display_active_clicked();
	void new_mix_group ();
	void add_mix_group (ARDOUR::RouteGroup *);

	Gtk::Menu *track_menu;
	void track_column_click (gint);
	void build_track_menu ();

	PluginSelector    *_plugin_selector;

	void strip_name_changed (void *src, MixerStrip *);

	static GdkPixmap *check_pixmap;
	static GdkBitmap *check_mask;
	static GdkPixmap *empty_pixmap;
	static GdkBitmap *empty_mask;

	void group_flags_changed (void *src, ARDOUR::RouteGroup *);

	/* various treeviews */
	
	struct TrackDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    TrackDisplayModelColumns() { 
		    add (text);
		    add (route);
		    add (strip);
	    }
	    Gtk::TreeModelColumn<Glib::ustring>  text;
	    Gtk::TreeModelColumn<ARDOUR::Route*> route;
	    Gtk::TreeModelColumn<MixerStrip*>    strip;
	};

	struct GroupDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    GroupDisplayModelColumns() { 
		    add (active);
		    add (text);
		    add (group);
	    }
	    Gtk::TreeModelColumn<bool>                active;
	    Gtk::TreeModelColumn<Glib::ustring>       text;
	    Gtk::TreeModelColumn<ARDOUR::RouteGroup*> group;
	};

	TrackDisplayModelColumns    track_display_columns;
	GroupDisplayModelColumns    group_display_columns;

	Gtk::TreeView track_display;
	Gtk::TreeView group_display;

	Glib::RefPtr<Gtk::ListStore> track_display_model;
	Glib::RefPtr<Gtk::ListStore> group_display_model;

	bool track_display_button_press (GdkEventButton*);
	bool group_display_button_press (GdkEventButton*);

	void track_display_selection_changed ();
	void group_display_selection_changed ();

	bool strip_button_release_event (GdkEventButton*, MixerStrip*);

	RouteRedirectSelection _selection;

	Width _strip_width;

	static const int32_t default_width = -1;
	static const int32_t default_height = 765;
};

#endif /* __ardour_mixer_ui_h__ */


