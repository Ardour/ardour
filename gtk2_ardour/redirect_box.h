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
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm2ext/dndtreeview.h>
#include <gtkmm2ext/auto_spin.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/dndtreeview.h>

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
	
	struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
		ModelColumns () {
			add (text);
		    add (redirect);
		}
	    Gtk::TreeModelColumn<std::string> text;
	    Gtk::TreeModelColumn<ARDOUR::Redirect*>   redirect;
	};

	ModelColumns columns;
	Glib::RefPtr<Gtk::ListStore> model;
	
	void selection_changed ();
	
	Gtk::EventBox	       redirect_eventbox;
	Gtk::HBox              redirect_hpacker;
	Gtkmm2ext::DnDTreeView redirect_display;
	Gtk::ScrolledWindow    redirect_scroller;

	void object_drop (std::string type, uint32_t cnt, void**);

	Width _width;
	
	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();

	Glib::RefPtr<Gtk::UIManager> popup_ui_mgr;
	Glib::RefPtr<Gtk::ActionGroup> popup_act_grp;
	Gtk::Menu *redirect_menu;
	gint redirect_menu_map_handler (GdkEventAny *ev);
	Gtk::Menu * build_redirect_menu ();
	void build_redirect_tooltip (Gtk::EventBox&, string);
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

	string redirect_name (ARDOUR::Redirect&);

	void remove_redirect_gui (ARDOUR::Redirect *);

	void redirects_reordered (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator&, int*);
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
