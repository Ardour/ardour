/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __ardour_gtk_redirect_box__
#define __ardour_gtk_redirect_box__

#include <vector>

#include <cmath>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm2ext/dndtreeview.h>
#include <gtkmm2ext/auto_spin.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/dndtreeview.h>

#include <pbd/stateful.h>

#include <ardour/types.h>
#include <ardour/ardour.h>
#include <ardour/io.h>
#include <ardour/insert.h>
#include <ardour/redirect.h>

#include <pbd/fastlog.h>

#include "route_ui.h"
#include "io_selector.h"
#include "enums.h"

class MotionController;
class PluginSelector;
class PluginUIWindow;
class RouteRedirectSelection;

namespace ARDOUR {
	class Bundle;
	class Insert;
	class Plugin;
	class PluginInsert;
	class PortInsert;
	class Route;
	class Send;
	class Session;
}


// FIXME: change name to InsertBox
class RedirectBox : public Gtk::HBox
{
  public:
	RedirectBox (ARDOUR::Placement, ARDOUR::Session&, 
		     boost::shared_ptr<ARDOUR::Route>, PluginSelector &, RouteRedirectSelection &, bool owner_is_mixer = false);
	~RedirectBox ();

	void set_width (Width);

	void update();

	void select_all_inserts ();
	void deselect_all_inserts ();
	void select_all_plugins ();
	void select_all_sends ();
	
	sigc::signal<void,boost::shared_ptr<ARDOUR::Insert> > InsertSelected;
	sigc::signal<void,boost::shared_ptr<ARDOUR::Insert> > InsertUnselected;
	
	static void register_actions();

  protected:
	void set_stuff_from_route ();

  private:
	boost::shared_ptr<ARDOUR::Route>  _route;
	ARDOUR::Session &   _session;
	bool                _owner_is_mixer;
	bool                 ab_direction;

	ARDOUR::Placement   _placement;

	PluginSelector     & _plugin_selector;
	RouteRedirectSelection  & _rr_selection;

	void route_going_away ();

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
	    ModelColumns () {
		    add (text);
		    add (insert);
		    add (color);
	    }
	    Gtk::TreeModelColumn<std::string>       text;
	    Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Insert> > insert;
	    Gtk::TreeModelColumn<Gdk::Color>        color;
	};

	ModelColumns columns;
	Glib::RefPtr<Gtk::ListStore> model;
	
	void selection_changed ();

	static bool get_colors;
	static Gdk::Color* active_insert_color;
	static Gdk::Color* inactive_insert_color;
	
	Gtk::EventBox	       insert_eventbox;
	Gtk::HBox              insert_hpacker;
	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Insert> > insert_display;
	Gtk::ScrolledWindow    insert_scroller;

	void object_drop (std::string type, uint32_t cnt, const boost::shared_ptr<ARDOUR::Insert>*);

	Width _width;
	
	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();

	Gtk::Menu *insert_menu;
	gint insert_menu_map_handler (GdkEventAny *ev);
	Gtk::Menu * build_insert_menu ();
	void build_insert_tooltip (Gtk::EventBox&, string);
	void show_insert_menu (gint arg);

	void choose_send ();
	void send_io_finished (IOSelector::Result, boost::shared_ptr<ARDOUR::Send>, IOSelectorWindow*);
	void choose_insert ();
	void choose_plugin ();
	void insert_plugin_chosen (boost::shared_ptr<ARDOUR::Plugin>);

	bool no_insert_redisplay;
	bool ignore_delete;

	bool insert_button_press_event (GdkEventButton *);
	bool insert_button_release_event (GdkEventButton *);
	void redisplay_inserts ();
	void add_insert_to_display (boost::shared_ptr<ARDOUR::Insert>);
	void row_deleted (const Gtk::TreeModel::Path& path);
	void show_insert_active (boost::weak_ptr<ARDOUR::Insert>);
	void show_insert_name (boost::weak_ptr<ARDOUR::Insert>);
	string insert_name (boost::weak_ptr<ARDOUR::Insert>);

	void remove_insert_gui (boost::shared_ptr<ARDOUR::Insert>);

	void inserts_reordered (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator&, int*);
	void compute_insert_sort_keys ();
	vector<sigc::connection> insert_active_connections;
	vector<sigc::connection> insert_name_connections;
	
	bool insert_drag_in_progress;
	void insert_drag_begin (GdkDragContext*);
	void insert_drag_end (GdkDragContext*);
	void all_inserts_active(bool state);
	void all_plugins_active(bool state);
	void ab_plugins ();

	void cut_inserts ();
	void copy_inserts ();
	void paste_inserts ();
	void delete_inserts ();
	void clear_inserts ();
	void clone_inserts ();
	void rename_inserts ();

	void for_selected_inserts (void (RedirectBox::*pmf)(boost::shared_ptr<ARDOUR::Insert>));
	void get_selected_inserts (vector<boost::shared_ptr<ARDOUR::Insert> >&);

	static Glib::RefPtr<Gtk::Action> paste_action;
	void paste_insert_list (std::list<boost::shared_ptr<ARDOUR::Insert> >& inserts);
	
	void activate_insert (boost::shared_ptr<ARDOUR::Insert>);
	void deactivate_insert (boost::shared_ptr<ARDOUR::Insert>);
	void cut_insert (boost::shared_ptr<ARDOUR::Insert>);
	void copy_insert (boost::shared_ptr<ARDOUR::Insert>);
	void edit_insert (boost::shared_ptr<ARDOUR::Insert>);
	void hide_insert_editor (boost::shared_ptr<ARDOUR::Insert>);
	void rename_insert (boost::shared_ptr<ARDOUR::Insert>);

	gint idle_delete_insert (boost::weak_ptr<ARDOUR::Insert>);

	void weird_plugin_dialog (ARDOUR::Plugin& p, ARDOUR::Route::InsertStreams streams, boost::shared_ptr<ARDOUR::IO> io);

	static RedirectBox* _current_insert_box;
	static bool enter_box (GdkEventCrossing*, RedirectBox*);
	static bool leave_box (GdkEventCrossing*, RedirectBox*);

	static void rb_choose_plugin ();
	static void rb_choose_insert ();
	static void rb_choose_send ();
	static void rb_clear ();
	static void rb_cut ();
	static void rb_copy ();
	static void rb_paste ();
	static void rb_delete ();
	static void rb_rename ();
	static void rb_select_all ();
	static void rb_deselect_all ();
	static void rb_activate ();
	static void rb_deactivate ();
	static void rb_activate_all ();
	static void rb_deactivate_all ();
	static void rb_edit ();
	static void rb_ab_plugins ();
	static void rb_deactivate_plugins ();
	
	void route_name_changed (PluginUIWindow* plugin_ui, boost::weak_ptr<ARDOUR::PluginInsert> pi);
	std::string generate_insert_title (boost::shared_ptr<ARDOUR::PluginInsert> pi);
};

#endif /* __ardour_gtk_redirect_box__ */
