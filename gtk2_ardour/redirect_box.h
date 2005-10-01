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

    $Id$
*/

#ifndef __ardour_gtk_redirect_box__
#define __ardour_gtk_redirect_box__

#include <vector>

#include <cmath>
#include <gtkmm.h>
#include <gtkmm2ext/auto_spin.h>
#include <gtkmm2ext/click_box.h>

#include <ardour/types.h>
#include <ardour/ardour.h>
#include <ardour/io.h>
#include <ardour/insert.h>
#include <ardour/stateful.h>
#include <ardour/redirect.h>

#include <pbd/fastlog.h>

#include "route_ui.h"
#include "io_selector.h"
#include "enums.h"

class MotionController;
class PluginSelector;
class RouteRedirectSelection;

namespace ARDOUR {
	class Route;
	class Send;
	class Insert;
	class Session;
	class PortInsert;
	class Connection;
	class Plugin;
}


class RedirectBox : public Gtk::HBox
{
  public:
	RedirectBox (ARDOUR::Placement, ARDOUR::Session&, ARDOUR::Route &, PluginSelector &, RouteRedirectSelection &, bool owner_is_mixer = false);
	~RedirectBox ();

	void set_width (Width);

	void set_title (const std::string & title);
	void set_title_shown (bool flag);
	
	void update();

	void select_all_redirects ();
	void deselect_all_redirects ();
	void select_all_plugins ();
	void select_all_inserts ();
	void select_all_sends ();
	
	sigc::signal<void,ARDOUR::Redirect *> RedirectSelected;
	sigc::signal<void,ARDOUR::Redirect *> RedirectUnselected;
	
  protected:
	void set_stuff_from_route ();

  private:
	ARDOUR::Route &     _route;
	ARDOUR::Session &   _session;
	bool                _owner_is_mixer;

	ARDOUR::Placement   _placement;

	PluginSelector     & _plugin_selector;
	RouteRedirectSelection  & _rr_selection;
	
	
	Gtk::EventBox	    redirect_eventbox;
	Gtk::HBox           redirect_hpacker;
	Gtk::TreeView       redirect_display;
	Gtk::ScrolledWindow redirect_scroller;

	Width _width;
	
	sigc::connection newplug_connection;
	
	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();

	Gtk::Menu *redirect_menu;
	vector<Gtk::MenuItem*> selection_dependent_items;
	Gtk::MenuItem* redirect_paste_item;
	gint redirect_menu_map_handler (GdkEventAny *ev);
	Gtk::Menu * build_redirect_menu (Gtk::TreeView&);
	void build_redirect_tooltip (Gtk::TreeView&, Gtk::EventBox&, string);
	void show_redirect_menu (gint arg);

	void choose_send ();
	void send_io_finished (IOSelector::Result, ARDOUR::Redirect*, IOSelectorWindow*);
	void choose_insert ();
	void choose_plugin ();
	void insert_plugin_chosen (ARDOUR::Plugin *);

	gint redirect_button (GdkEventButton *);
	void redirects_changed (void *);
	void show_redirect_active (ARDOUR::Redirect *, void *);
	void show_redirect_name (void*, ARDOUR::Redirect *);
	void add_redirect_to_display (ARDOUR::Redirect *);

	void show_plugin_selector ();


	string redirect_name (ARDOUR::Redirect&);


	void remove_redirect_gui (ARDOUR::Redirect *);

	void disconnect_newplug();

	void redirects_reordered (gint, gint);
	gint compute_redirect_sort_keys ();
	vector<sigc::connection> redirect_active_connections;
	vector<sigc::connection> redirect_name_connections;
	
	bool redirect_drag_in_progress;
	void redirect_drag_begin (GdkDragContext*);
	void redirect_drag_end (GdkDragContext*);
	void all_redirects_active(bool state);

	void cut_redirects ();
	void copy_redirects ();
	void paste_redirects ();
	void clear_redirects ();
	void clone_redirects ();
	void rename_redirects ();

	void for_selected_redirects (void (RedirectBox::*pmf)(ARDOUR::Redirect*));
	void get_selected_redirects (vector<ARDOUR::Redirect*>&);

	
	void activate_redirect (ARDOUR::Redirect*);
	void deactivate_redirect (ARDOUR::Redirect*);
	void cut_redirect (ARDOUR::Redirect*);
	void copy_redirect (ARDOUR::Redirect*);
	void edit_redirect (ARDOUR::Redirect*);
	void hide_redirect_editor (ARDOUR::Redirect*);
	void rename_redirect (ARDOUR::Redirect*);

	gint idle_delete_redirect (ARDOUR::Redirect *);

	void wierd_plugin_dialog (ARDOUR::Plugin& p, uint32_t streams, ARDOUR::IO& io);

};

#endif /* __ardour_gtk_redirect_box__ */
