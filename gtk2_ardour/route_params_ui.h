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

#ifndef __ardour_route_params_ui_h__
#define __ardour_route_params_ui_h__

#include <list>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/treeview.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/ardour.h"

#include "ardour_window.h"
#include "processor_box.h"
#include "route_processor_selection.h"
#include "latency_gui.h"

namespace ARDOUR {
	class Route;
	class Send;
	class Processor;
	class Session;
	class PortInsert;
	class Connection;
	class Plugin;
}

class PluginSelector;
class IOSelector;

class RouteParams_UI : public ArdourWindow, public PBD::ScopedConnectionList
{
  public:
	RouteParams_UI ();
	~RouteParams_UI();

	void set_session (ARDOUR::Session*);
	void session_going_away ();
	PluginSelector* plugin_selector() { return _plugin_selector; }

  private:
	Gtk::HBox                global_hpacker;
	Gtk::VBox                global_vpacker;
	Gtk::ScrolledWindow      scroller;
	Gtk::EventBox            scroller_base;
	Gtk::HBox                scroller_hpacker;
	Gtk::VBox                mixer_scroller_vpacker;

	Gtk::VBox                list_vpacker;
	Gtk::Label               route_list_button_label;
	Gtk::Button              route_list_button;
	Gtk::ScrolledWindow      route_select_scroller;

	Gtk::Notebook            notebook;
	Gtk::Frame		 input_frame;
	Gtk::Frame		 output_frame;
	Gtk::HPaned		 redir_hpane;

	Gtk::Frame		 route_select_frame;

	Gtk::HBox                route_hpacker;
	Gtk::VBox                route_vpacker;

	ProcessorBox*            insert_box;

	Gtk::HPaned		 list_hpane;

	Gtk::HPaned		 right_hpane;

	Gtk::Frame		 route_choice_frame;

	Gtk::Frame		 route_param_frame;

	Gtk::VBox                choice_vpacker;

	Gtk::Frame               latency_frame;
	Gtk::VBox                latency_packer;
	Gtk::HButtonBox          latency_button_box;
	Gtk::Button              latency_apply_button;
	LatencyGUI*              latency_widget;
	Gtk::Label               delay_label;

	PBD::ScopedConnectionList latency_connections;
	sigc::connection          latency_click_connection;

	void refresh_latency ();

	Gtk::ToggleButton input_button;
	Gtk::ToggleButton output_button;
	Gtk::Label  track_input_label;

	Gtk::Label  title_label;

	Gtk::Container * _active_view;
	IOSelector     * _input_iosel;
	IOSelector     * _output_iosel;

	PluginSelector    *_plugin_selector;
	RouteProcessorSelection  _rr_selection;

	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnection _route_processors_connection;

	boost::shared_ptr<ARDOUR::Processor> _processor;
	PBD::ScopedConnection _processor_going_away_connection;


	enum ConfigView {
		NO_CONFIG_VIEW = 0,
		INPUT_CONFIG_VIEW,
		OUTPUT_CONFIG_VIEW,
		PLUGIN_CONFIG_VIEW,
		PORTINSERT_CONFIG_VIEW,
		SEND_CONFIG_VIEW
	};

	ConfigView _current_view;


	/* treeview */
	struct RouteDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
		RouteDisplayModelColumns() {
			add(text);
			add(route);
		}
		Gtk::TreeModelColumn<std::string> text;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Route> > route;
	};

	RouteDisplayModelColumns route_display_columns ;
	Gtk::TreeView route_display;
	Glib::RefPtr<Gtk::ListStore> route_display_model;


	void add_routes (ARDOUR::RouteList&);

	void route_property_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Route> route);
	void route_removed (boost::weak_ptr<ARDOUR::Route> route);


	void route_selected();
	//void route_unselected (gint row, gint col, GdkEvent *ev);

	void setup_io_frames();
	void cleanup_io_frames();
	void cleanup_view(bool stopupdate = true);
	void cleanup_latency_frame ();
	void setup_latency_frame ();

	void processors_changed (ARDOUR::RouteProcessorChange);

	void setup_processor_boxes();
	void cleanup_processor_boxes();

	void redirect_selected (boost::shared_ptr<ARDOUR::Processor>);

	void plugin_going_away (ARDOUR::Placement);
	void processor_going_away (boost::weak_ptr<ARDOUR::Processor>);

	gint edit_input_configuration (GdkEventButton *ev);
	gint edit_output_configuration (GdkEventButton *ev);

	void update_routeinfo (ARDOUR::Route * route);

	Gtk::Menu *track_menu;
	void show_track_menu();

	void update_title ();
	//void unselect_all_redirects ();

	sigc::connection update_connection;
	void update_views ();

	void start_updating ();
	void stop_updating ();
};


#endif /* __ardour_route_params_ui_h__ */
