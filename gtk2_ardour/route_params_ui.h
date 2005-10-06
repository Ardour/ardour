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

#ifndef __ardour_route_params_ui_h__
#define __ardour_route_params_ui_h__

#include <list>

#include <gtkmm.h>

#include <ardour/ardour.h>
#include <ardour/stateful.h>
#include <ardour/io.h>
#include <ardour/redirect.h>

#include "io_selector.h"
#include "ardour_dialog.h"
#include "keyboard_target.h"
#include "redirect_box.h"
#include "route_redirect_selection.h"

namespace ARDOUR {
	class Route;
	class Send;
	class Insert;
	class Session;
	class PortInsert;
	class Connection;
	class Plugin;
}

class PluginSelector;

class RouteParams_UI : public ArdourDialog
{
  public:
	RouteParams_UI (ARDOUR::AudioEngine&);
	~RouteParams_UI();

	void set_session (ARDOUR::Session *);
	void session_gone ();
	PluginSelector&  plugin_selector() { return *_plugin_selector; }

  private:
	ARDOUR::AudioEngine&     engine;

	Gtk::HBox                global_hpacker;
	Gtk::VBox                global_vpacker;
	Gtk::ScrolledWindow      scroller;
	Gtk::EventBox            scroller_base;
	Gtk::HBox                scroller_hpacker;
	Gtk::VBox                mixer_scroller_vpacker;

	Gtk::VBox                list_vpacker;
	Gtk::TreeView            route_select_list;
	Gtk::Label               route_list_button_label;
	Gtk::Button              route_list_button;
	Gtk::ScrolledWindow      route_select_scroller;

	Gtk::Notebook            notebook;
	Gtk::Frame 		 input_frame;
	Gtk::Frame 		 output_frame;
	Gtk::HPaned		 pre_redir_hpane;
	Gtk::HPaned		 post_redir_hpane;
	
	Gtk::Frame 		 route_select_frame;

	Gtk::HBox                route_hpacker;
	Gtk::VBox                route_vpacker;

	RedirectBox              * pre_redirect_box;
	RedirectBox              * post_redirect_box;
	
	Gtk::HPaned		 list_hpane;

	Gtk::HPaned		 right_hpane;
	
	Gtk::Frame 		 route_choice_frame;

	Gtk::Frame 		 route_param_frame;

	Gtk::VBox                choice_vpacker;
	

	Gtk::ToggleButton input_button;
	Gtk::ToggleButton output_button;
	Gtk::Label  track_input_label;
	
	Gtk::Label  title_label;
	
	Gtk::Container * _active_pre_view;
	Gtk::Container * _active_post_view;
	IOSelector     * _input_iosel;
	IOSelector     * _output_iosel;
	
	PluginSelector    *_plugin_selector;
	RouteRedirectSelection  _rr_selection;

	ARDOUR::Route           *_route;
	sigc::connection            _route_conn;
	sigc::connection            _route_ds_conn;

	ARDOUR::Redirect       * _pre_redirect;
	sigc::connection            _pre_plugin_conn;

	ARDOUR::Redirect       * _post_redirect;
	sigc::connection            _post_plugin_conn;
	
	
	enum ConfigView {
		NO_CONFIG_VIEW = 0,
		INPUT_CONFIG_VIEW,
		OUTPUT_CONFIG_VIEW,
		PLUGIN_CONFIG_VIEW,		
		PORTINSERT_CONFIG_VIEW,
		SEND_CONFIG_VIEW
	};
	
	ConfigView _current_view;
	
	void add_route (ARDOUR::Route*);

	void route_name_changed (void *src, ARDOUR::Route *route);
	void route_removed (ARDOUR::Route *route);


	void route_selected (gint row, gint col, GdkEvent *ev);
	void route_unselected (gint row, gint col, GdkEvent *ev);

	void setup_io_frames();
	void cleanup_io_frames();
	void cleanup_pre_view(bool stopupdate = true);
	void cleanup_post_view(bool stopupdate = true);

	
	
	void redirects_changed (void *src);
	
	void setup_redirect_boxes();
	void cleanup_redirect_boxes();

	void redirect_selected (ARDOUR::Redirect *, ARDOUR::Placement);
	void redirect_unselected (ARDOUR::Redirect *);
	
	void plugin_going_away (ARDOUR::Plugin *foo, ARDOUR::Placement);
	void redirect_going_away (ARDOUR::Redirect *foo);

	gint edit_input_configuration (GdkEventButton *ev);
	gint edit_output_configuration (GdkEventButton *ev);
	
	void update_routeinfo (ARDOUR::Route * route);
	
	Gtk::Menu *track_menu;
	void show_track_menu(gint arg);
	
	void update_title ();
	//void unselect_all_redirects ();

	sigc::connection update_connection;
	void update_views ();

	void start_updating ();
	void stop_updating ();
};


#endif /* __ardour_route_params_ui_h__ */
