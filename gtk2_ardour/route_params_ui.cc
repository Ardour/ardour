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

#include <algorithm>
#include <inttypes.h>

#include <glibmm/threads.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "ardour/audioengine.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/port_insert.h"
#include "ardour/return.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/internal_send.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "io_selector.h"
#include "keyboard.h"
#include "mixer_strip.h"
#include "port_insert_ui.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "return_ui.h"
#include "route_params_ui.h"
#include "send_ui.h"
#include "utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

RouteParams_UI::RouteParams_UI ()
	: ArdourWindow (_("Tracks and Busses")),
	  latency_apply_button (Stock::APPLY),
	  track_menu(0)
{
	insert_box = 0;
	_input_iosel = 0;
	_output_iosel = 0;
	_active_view = 0;
	latency_widget = 0;

	using namespace Notebook_Helpers;

	input_frame.set_shadow_type(Gtk::SHADOW_NONE);
	output_frame.set_shadow_type(Gtk::SHADOW_NONE);
	latency_frame.set_shadow_type (Gtk::SHADOW_NONE);

	notebook.set_show_tabs (true);
	notebook.set_show_border (true);
	notebook.set_name ("RouteParamNotebook");

	// create the tree model
	route_display_model = ListStore::create(route_display_columns);

	// setup the treeview
	route_display.set_model(route_display_model);
	route_display.append_column(_("Tracks/Busses"), route_display_columns.text);
	route_display.set_name(X_("RouteParamsListDisplay"));
	route_display.get_selection()->set_mode(Gtk::SELECTION_SINGLE); // default
	route_display.set_reorderable(false);
	route_display.set_size_request(75, -1);
	route_display.set_headers_visible(true);
	route_display.set_headers_clickable(true);

	dynamic_cast<Gtk::CellRendererText*>(route_display.get_column_cell_renderer(0))->property_ellipsize() = Pango::ELLIPSIZE_START;

	route_select_scroller.add(route_display);
	route_select_scroller.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	route_select_frame.set_name("RouteSelectBaseFrame");
	route_select_frame.set_shadow_type (Gtk::SHADOW_IN);
	route_select_frame.add(route_select_scroller);

	list_vpacker.pack_start (route_select_frame, true, true);

	notebook.pages().push_back (TabElem (input_frame, _("Inputs")));
	notebook.pages().push_back (TabElem (output_frame, _("Outputs")));
	notebook.pages().push_back (TabElem (redir_hpane, _("Plugins, Inserts & Sends")));
	notebook.pages().push_back (TabElem (latency_frame, _("Latency")));

	notebook.set_name ("InspectorNotebook");

	title_label.set_name ("RouteParamsTitleLabel");
	update_title();

	latency_packer.set_spacing (18);
	latency_button_box.pack_start (latency_apply_button);
	delay_label.set_alignment (0, 0.5);

	// changeable area
	route_param_frame.set_name("RouteParamsBaseFrame");
	route_param_frame.set_shadow_type (Gtk::SHADOW_IN);


	route_hpacker.pack_start (notebook, true, true);

	route_vpacker.pack_start (title_label, false, false);
	route_vpacker.pack_start (route_hpacker, true, true);


	list_hpane.pack1 (list_vpacker);
	list_hpane.add2 (route_vpacker);

	list_hpane.set_position(110);

	redir_hpane.set_position(110);

	//global_vpacker.pack_start (list_hpane, true, true);
	//get_vbox()->pack_start (global_vpacker);
	add (list_hpane);


	set_name ("RouteParamsWindow");
	set_default_size (620,370);
	set_wmclass (X_("ardour_route_parameters"), PROGRAM_NAME);

	// events
	route_display.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &RouteParams_UI::route_selected));
	route_display.get_column(0)->signal_clicked().connect(sigc::mem_fun(*this, &RouteParams_UI::show_track_menu));

	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_RELEASE_MASK);

	_plugin_selector = new PluginSelector (PluginManager::instance());
	show_all();
}

RouteParams_UI::~RouteParams_UI ()
{
}

void
RouteParams_UI::add_routes (RouteList& routes)
{
	ENSURE_GUI_THREAD (*this, &RouteParams_UI::add_routes, routes)

	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);

		if (route->is_auditioner()) {
			return;
		}

		TreeModel::Row row = *(route_display_model->append());
		row[route_display_columns.text] = route->name();
		row[route_display_columns.route] = route;

		//route_select_list.rows().back().select ();

		route->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&RouteParams_UI::route_property_changed, this, _1, boost::weak_ptr<Route>(route)), gui_context());
		route->DropReferences.connect (*this, invalidator (*this), boost::bind (&RouteParams_UI::route_removed, this, boost::weak_ptr<Route>(route)), gui_context());
	}
}


void
RouteParams_UI::route_property_changed (const PropertyChange& what_changed, boost::weak_ptr<Route> wr)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	boost::shared_ptr<Route> route (wr.lock());

	if (!route) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &RouteParams_UI::route_name_changed, wr)

	bool found = false ;
	TreeModel::Children rows = route_display_model->children();
	for(TreeModel::Children::iterator iter = rows.begin(); iter != rows.end(); ++iter) {
		boost::shared_ptr<Route> r =(*iter)[route_display_columns.route];
		if (r == route) {
			(*iter)[route_display_columns.text] = route->name() ;
			found = true ;
			break;
		}
	}

	if(!found) {
		error << _("route display list item for renamed route not found!") << endmsg;
	}

	if (route == _route) {
		track_input_label.set_text (route->name());
		update_title();
	}
}

void
RouteParams_UI::setup_processor_boxes()
{
	if (_session && _route) {

		// just in case... shouldn't need this
		cleanup_processor_boxes();

		// construct new redirect boxes
		insert_box = new ProcessorBox (_session, boost::bind (&RouteParams_UI::plugin_selector, this), _rr_selection, 0);
		insert_box->set_route (_route);

		redir_hpane.pack1 (*insert_box);

		insert_box->ProcessorSelected.connect (sigc::mem_fun(*this, &RouteParams_UI::redirect_selected));
		insert_box->ProcessorUnselected.connect (sigc::mem_fun(*this, &RouteParams_UI::redirect_selected));

		redir_hpane.show_all();
	}
}

void
RouteParams_UI::cleanup_processor_boxes()
{
	if (insert_box) {
		redir_hpane.remove(*insert_box);
		delete insert_box;
		insert_box = 0;
	}
}

void
RouteParams_UI::refresh_latency ()
{
	if (latency_widget) {
		latency_widget->refresh();

		char buf[128];
		snprintf (buf, sizeof (buf), _("Playback delay: %" PRId64 " samples"), _route->initial_delay());
		delay_label.set_text (buf);
	}
}

void
RouteParams_UI::cleanup_latency_frame ()
{
	if (latency_widget) {
		latency_frame.remove ();
		latency_packer.remove (*latency_widget);
		latency_packer.remove (latency_button_box);
		latency_packer.remove (delay_label);
		latency_connections.drop_connections ();
		latency_click_connection.disconnect ();

		delete latency_widget;
		latency_widget = 0;

	}
}

void
RouteParams_UI::setup_latency_frame ()
{
	latency_widget = new LatencyGUI (*(_route->output()), _session->frame_rate(), _session->engine().frames_per_cycle());

	char buf[128];
	snprintf (buf, sizeof (buf), _("Playback delay: %" PRId64 " samples"), _route->initial_delay());
	delay_label.set_text (buf);

	latency_packer.pack_start (*latency_widget, false, false);
	latency_packer.pack_start (latency_button_box, false, false);
	latency_packer.pack_start (delay_label);

	latency_click_connection = latency_apply_button.signal_clicked().connect (sigc::mem_fun (*latency_widget, &LatencyGUI::finish));
	_route->signal_latency_changed.connect (latency_connections, invalidator (*this), boost::bind (&RouteParams_UI::refresh_latency, this), gui_context());
	_route->initial_delay_changed.connect (latency_connections, invalidator (*this), boost::bind (&RouteParams_UI::refresh_latency, this), gui_context());

	latency_frame.add (latency_packer);
	latency_frame.show_all ();
}

void
RouteParams_UI::setup_io_frames()
{
	cleanup_io_frames();

	// input
	_input_iosel = new IOSelector (this, _session, _route->input());
	_input_iosel->setup ();
	input_frame.add (*_input_iosel);
	input_frame.show_all();

	// output
	_output_iosel = new IOSelector (this, _session, _route->output());
	_output_iosel->setup ();
	output_frame.add (*_output_iosel);
	output_frame.show_all();
}

void
RouteParams_UI::cleanup_io_frames()
{
	if (_input_iosel) {
		_input_iosel->Finished (IOSelector::Cancelled);
		input_frame.remove();
		delete _input_iosel;
		_input_iosel = 0;
	}

	if (_output_iosel) {
		_output_iosel->Finished (IOSelector::Cancelled);

		output_frame.remove();
		delete _output_iosel;
		_output_iosel = 0;
	}
}

void
RouteParams_UI::cleanup_view (bool stopupdate)
{
	if (_active_view) {
		GenericPluginUI *   plugui = 0;

		if (stopupdate && (plugui = dynamic_cast<GenericPluginUI*>(_active_view)) != 0) {
			  plugui->stop_updating (0);
		}

		_processor_going_away_connection.disconnect ();
 		redir_hpane.remove(*_active_view);
		delete _active_view;
		_active_view = 0;
	}
}

void
RouteParams_UI::route_removed (boost::weak_ptr<Route> wr)
{
	boost::shared_ptr<Route> route (wr.lock());

	if (!route) {
		return;
	}

	ENSURE_GUI_THREAD (*this, invalidator (*this), &RouteParams_UI::route_removed, wr)

	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator ri;

	for(TreeModel::Children::iterator iter = rows.begin(); iter != rows.end(); ++iter) {
		boost::shared_ptr<Route> r =(*iter)[route_display_columns.route];

		if (r == route) {
			route_display_model->erase(iter);
			break;
		}
	}

	if (route == _route) {
		cleanup_io_frames();
		cleanup_view();
		cleanup_processor_boxes();

		_route.reset ((Route*) 0);
		_processor.reset ((Processor*) 0);
		update_title();
	}
}

void
RouteParams_UI::set_session (Session *sess)
{
	ArdourWindow::set_session (sess);

	route_display_model->clear();
	_plugin_selector->set_session (_session);

	if (_session) {
		boost::shared_ptr<RouteList> r = _session->get_routes();
		add_routes (*r);
		_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&RouteParams_UI::add_routes, this, _1), gui_context());
		start_updating ();
	} else {
		stop_updating ();
	}
}


void
RouteParams_UI::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RouteParams_UI::session_going_away);

	SessionHandlePtr::session_going_away ();

	route_display_model->clear();

	cleanup_io_frames();
	cleanup_view();
	cleanup_processor_boxes();
	cleanup_latency_frame ();

	_route.reset ((Route*) 0);
	_processor.reset ((Processor*) 0);
	update_title();
}

void
RouteParams_UI::route_selected()
{
	Glib::RefPtr<TreeSelection> selection = route_display.get_selection();
	TreeModel::iterator iter = selection->get_selected(); // only used with Gtk::SELECTION_SINGLE

	if(iter) {
		//If anything is selected
		boost::shared_ptr<Route> route = (*iter)[route_display_columns.route] ;

		if (_route == route) {
			// do nothing
			return;
		}

		// remove event binding from previously selected
		if (_route) {
			_route_processors_connection.disconnect ();
			cleanup_processor_boxes();
			cleanup_view();
			cleanup_io_frames();
			cleanup_latency_frame ();
		}

		// update the other panes with the correct info
		_route = route;
		//update_routeinfo (route);

		setup_io_frames();
		setup_processor_boxes();
		setup_latency_frame ();

		route->processors_changed.connect (_route_processors_connection, invalidator (*this), boost::bind (&RouteParams_UI::processors_changed, this, _1), gui_context());

		track_input_label.set_text (_route->name());

		update_title();

	} else {
		// no selection
		if (_route) {
			_route_processors_connection.disconnect ();

			// remove from view
			cleanup_io_frames();
			cleanup_view();
			cleanup_processor_boxes();
			cleanup_latency_frame ();

			_route.reset ((Route*) 0);
			_processor.reset ((Processor*) 0);
			track_input_label.set_text(_("NO TRACK"));
			update_title();
		}
	}
}

void
RouteParams_UI::processors_changed (RouteProcessorChange)
{
	cleanup_view();

	_processor.reset ((Processor*) 0);

	//update_title();
}

void
RouteParams_UI::show_track_menu()
{
	using namespace Menu_Helpers;

	if (track_menu == 0) {
		track_menu = new Menu;
		track_menu->set_name ("ArdourContextMenu");
		track_menu->items().push_back
				(MenuElem (_("Add Track or Bus"),
					   sigc::bind (sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::add_route), (Gtk::Window*) 0)));
	}
	track_menu->popup (1, gtk_get_current_event_time());
}

void
RouteParams_UI::redirect_selected (boost::shared_ptr<ARDOUR::Processor> proc)
{
	boost::shared_ptr<Send> send;
	boost::shared_ptr<Return> retrn;
	boost::shared_ptr<PluginInsert> plugin_insert;
	boost::shared_ptr<PortInsert> port_insert;

	if ((boost::dynamic_pointer_cast<InternalSend> (proc)) != 0) {
		cleanup_view();
		_processor.reset ((Processor*) 0);
		update_title();
		return;
	} else if ((send = boost::dynamic_pointer_cast<Send> (proc)) != 0) {

		SendUI *send_ui = new SendUI (this, send, _session);

		cleanup_view();
		send->DropReferences.connect (_processor_going_away_connection, invalidator (*this), boost::bind (&RouteParams_UI::processor_going_away, this, boost::weak_ptr<Processor>(proc)), gui_context());
		_active_view = send_ui;

		redir_hpane.add2 (*_active_view);
		redir_hpane.show_all();

	} else if ((retrn = boost::dynamic_pointer_cast<Return> (proc)) != 0) {

		ReturnUI *return_ui = new ReturnUI (this, retrn, _session);

		cleanup_view();
		retrn->DropReferences.connect (_processor_going_away_connection, invalidator (*this), boost::bind (&RouteParams_UI::processor_going_away, this, boost::weak_ptr<Processor>(proc)), gui_context());
		_active_view = return_ui;

		redir_hpane.add2 (*_active_view);
		redir_hpane.show_all();

	} else if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (proc)) != 0) {

		GenericPluginUI *plugin_ui = new GenericPluginUI (plugin_insert, true);

		cleanup_view();
		plugin_insert->plugin()->DropReferences.connect (_processor_going_away_connection, invalidator (*this), boost::bind (&RouteParams_UI::plugin_going_away, this, PreFader), gui_context());
		plugin_ui->start_updating (0);
		_active_view = plugin_ui;

		redir_hpane.pack2 (*_active_view);
		redir_hpane.show_all();

	} else if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (proc)) != 0) {

		PortInsertUI *portinsert_ui = new PortInsertUI (this, _session, port_insert);

		cleanup_view();
		port_insert->DropReferences.connect (_processor_going_away_connection, invalidator (*this), boost::bind (&RouteParams_UI::processor_going_away, this, boost::weak_ptr<Processor> (proc)), gui_context());
		_active_view = portinsert_ui;

		redir_hpane.pack2 (*_active_view);
		portinsert_ui->redisplay();
		redir_hpane.show_all();
	}

	_processor = proc;
	update_title();

}

void
RouteParams_UI::plugin_going_away (Placement place)
{
	ENSURE_GUI_THREAD (*this, &RouteParams_UI::plugin_going_away, place)

	// delete the current view without calling finish

	if (place == PreFader) {
		cleanup_view (false);
		_processor.reset ((Processor*) 0);
	}
}

void
RouteParams_UI::processor_going_away (boost::weak_ptr<ARDOUR::Processor> wproc)
{
	boost::shared_ptr<Processor> proc = (wproc.lock());

	if (!proc) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &RouteParams_UI::processor_going_away, wproc)

	printf ("redirect going away\n");
	// delete the current view without calling finish
	if (proc == _processor) {
		cleanup_view (false);
		_processor.reset ((Processor*) 0);
	}
}

void
RouteParams_UI::update_title ()
{
	WindowTitle title (_("Tracks and Busses"));

	if (_route) {
		title_label.set_text(_route->name());
		title += _route->name();
		set_title(title.get_string());
	} else {
		title_label.set_text(_("No Track or Bus Selected"));
		title += _("No Track or Bus Selected");
		set_title(title.get_string());
	}
}

void
RouteParams_UI::start_updating ()
{
	update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect
		(sigc::mem_fun(*this, &RouteParams_UI::update_views));
}

void
RouteParams_UI::stop_updating ()
{
	update_connection.disconnect();
}

void
RouteParams_UI::update_views ()
{
	SendUI *sui;
	// TODO: only do it if correct tab is showing

	if ((sui = dynamic_cast<SendUI*> (_active_view)) != 0) {
		sui->update ();
	}
}
