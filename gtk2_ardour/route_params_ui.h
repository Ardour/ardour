/*
 * Copyright (C) 2005-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_route_params_ui_h__
#define __ardour_route_params_ui_h__

#include <list>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/treeview.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/ardour.h"

#include <widgets/pane.h>

#include "ardour_window.h"
#include "processor_box.h"
#include "processor_selection.h"

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
	PluginSelector* plugin_selector();

private:
	Gtk::VBox                list_vpacker;
	Gtk::ScrolledWindow      route_select_scroller;

	Gtk::Notebook            notebook;
	Gtk::Frame               input_frame;
	Gtk::Frame               output_frame;
	ArdourWidgets::HPane     redir_hpane;

	Gtk::Frame               route_select_frame;

	Gtk::HBox                route_hpacker;
	Gtk::VBox                route_vpacker;

	ProcessorBox*            insert_box;

	ArdourWidgets::HPane     list_hpane;

	ArdourWidgets::HPane     right_hpane;

	Gtk::Frame               route_param_frame;

	Gtk::VBox                choice_vpacker;


	Gtk::ToggleButton input_button;
	Gtk::ToggleButton output_button;
	Gtk::Label  track_input_label;

	Gtk::Label  title_label;

	Gtk::Container * _active_view;
	IOSelector     * _input_iosel;
	IOSelector     * _output_iosel;

	ProcessorSelection  _p_selection;

	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnection _route_processors_connection;
	PBD::ScopedConnectionList route_connections;

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
	void map_frozen ();


	void route_selected();
	//void route_unselected (gint row, gint col, GdkEvent *ev);

	void setup_io_selector();
	void cleanup_io_selector();
	void cleanup_view(bool stopupdate = true);

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
