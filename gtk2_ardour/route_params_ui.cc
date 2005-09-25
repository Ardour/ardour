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

#include <algorithm>

#include <pbd/lockmonitor.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>

#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/diskstream.h>
#include <ardour/plugin.h>
#include <ardour/plugin_manager.h>
#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/send.h>
#include <ardour/insert.h>
#include <ardour/connection.h>
#include <ardour/session_connection.h>

#include "route_params_ui.h"
#include "keyboard.h"
#include "mixer_strip.h"
#include "plugin_selector.h"
#include "ardour_ui.h"
#include "plugin_ui.h"
#include "io_selector.h"
#include "send_ui.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace sigc;

static const gchar *route_display_titles[] = { N_("Tracks/Buses"), 0 };
static const gchar *pre_display_titles[] = { N_("Pre Redirects"), 0 };
static const gchar *post_display_titles[] = { N_("Post Redirects"), 0 };

RouteParams_UI::RouteParams_UI (AudioEngine& eng)
	: ArdourDialog ("track/bus inspector"),
	  engine (eng),
	  route_select_list (internationalize(route_display_titles)),
	  _route(0), 
	  track_menu(0)
{
	pre_redirect_box = 0;
	post_redirect_box = 0;
	_route = 0;
	_pre_redirect = 0;
	_post_redirect = 0;
	_input_iosel = 0;
	_output_iosel = 0;
	_active_pre_view = 0;
	_active_post_view = 0;
	
	using namespace Notebook_Helpers;

	input_frame.set_shadow_type(GTK_SHADOW_NONE);
	output_frame.set_shadow_type(GTK_SHADOW_NONE);
	
	notebook.set_show_tabs (true);
	notebook.set_show_border (true);
	notebook.set_name ("RouteParamNotebook");
	
	route_select_list.column_titles_active();
	route_select_list.set_name ("RouteParamsListDisplay");
	route_select_list.set_shadow_type (Gtk::SHADOW_IN);
	route_select_list.set_selection_mode (GTK_SELECTION_SINGLE);
	route_select_list.set_reorderable (false);
	route_select_list.set_size_request (75, -1);
	route_select_scroller.add (route_select_list);
	route_select_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	route_select_frame.set_name("RouteSelectBaseFrame");
	route_select_frame.set_shadow_type (Gtk::SHADOW_IN);
	route_select_frame.add(route_select_scroller);

	list_vpacker.pack_start (route_select_frame, true, true);
	
	notebook.pages().push_back (TabElem (input_frame, _("Inputs")));
	notebook.pages().push_back (TabElem (output_frame, _("Outputs")));
	notebook.pages().push_back (TabElem (pre_redir_hpane, _("Pre-fader Redirects")));
	notebook.pages().push_back (TabElem (post_redir_hpane, _("Post-fader Redirects")));

	notebook.set_name ("InspectorNotebook");
	
	title_label.set_name ("RouteParamsTitleLabel");
	update_title();
	
	// changeable area
	route_param_frame.set_name("RouteParamsBaseFrame");
	route_param_frame.set_shadow_type (Gtk::SHADOW_IN);
	
	
	route_hpacker.pack_start (notebook, true, true);
	
	
	route_vpacker.pack_start (title_label, false, false);
	route_vpacker.pack_start (route_hpacker, true, true);

	
	list_hpane.add1 (list_vpacker);
	list_hpane.add2 (route_vpacker);

	list_hpane.set_position(110);

	pre_redir_hpane.set_position(110);
	post_redir_hpane.set_position(110);
	
	global_vpacker.pack_start (list_hpane, true, true);
	
	add (global_vpacker);
	set_name ("RouteParamsWindow");
	set_default_size (620,370);
	set_title (_("ardour: track/bus inspector"));
	set_wmclass (_("ardour_route_parameters"), "Ardour");

	// events
	route_select_list.select_row.connect (slot (*this, &RouteParams_UI::route_selected));
	route_select_list.unselect_row.connect (slot (*this, &RouteParams_UI::route_unselected));
	route_select_list.click_column.connect (slot (*this, &RouteParams_UI::show_track_menu));


	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_RELEASE_MASK);
	
	_plugin_selector = new PluginSelector (PluginManager::the_manager());
	_plugin_selector->delete_event.connect (bind (slot (just_hide_it), 
						     static_cast<Window *> (_plugin_selector)));


	delete_event.connect (bind (slot (just_hide_it), static_cast<Gtk::Window*> (this)));
}

RouteParams_UI::~RouteParams_UI ()
{
}

void
RouteParams_UI::add_route (Route* route)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &RouteParams_UI::add_route), route));
	
	if (route->hidden()) {
		return;
	}

	const gchar *rowdata[1];
	rowdata[0] = route->name().c_str();
	route_select_list.rows().push_back (rowdata);
	route_select_list.rows().back().set_data (route);
	//route_select_list.rows().back().select ();
	
	route->name_changed.connect (bind (slot (*this, &RouteParams_UI::route_name_changed), route));
	route->GoingAway.connect (bind (slot (*this, &RouteParams_UI::route_removed), route));
}


void
RouteParams_UI::route_name_changed (void *src, Route *route)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &RouteParams_UI::route_name_changed), src, route));
	
	CList_Helpers::RowList::iterator i;

	if ((i = route_select_list.rows().find_data (route)) == route_select_list.rows().end()) {
		error << _("route display list item for renamed route not found!") << endmsg;
		return;
	}

	route_select_list.cell ((*i)->get_row_num(), 0).set_text (route->name());

	if (route == _route) {
		track_input_label.set_text (route->name());
		update_title();
	}
}

void
RouteParams_UI::setup_redirect_boxes()
{
	if (session && _route) {

		// just in case... shouldn't need this
		cleanup_redirect_boxes();
		
		// construct new redirect boxes
		pre_redirect_box = new RedirectBox(PreFader, *session, *_route, *_plugin_selector, _rr_selection);
		post_redirect_box = new RedirectBox(PostFader, *session, *_route, *_plugin_selector, _rr_selection);

		pre_redirect_box->set_title (pre_display_titles[0]);
		pre_redirect_box->set_title_shown (true);
		post_redirect_box->set_title (post_display_titles[0]);
		post_redirect_box->set_title_shown (true);

	        pre_redir_hpane.add1 (*pre_redirect_box);
		post_redir_hpane.add1 (*post_redirect_box);

		pre_redirect_box->RedirectSelected.connect (bind (slot (*this, &RouteParams_UI::redirect_selected), PreFader));
		pre_redirect_box->RedirectUnselected.connect (bind (slot (*this, &RouteParams_UI::redirect_selected), PreFader));
		post_redirect_box->RedirectSelected.connect (bind (slot (*this, &RouteParams_UI::redirect_selected), PostFader));
		post_redirect_box->RedirectUnselected.connect (bind (slot (*this, &RouteParams_UI::redirect_selected), PostFader));
		
	}
	
}

void
RouteParams_UI::cleanup_redirect_boxes()
{
	if (pre_redirect_box) {
		pre_redir_hpane.remove(*pre_redirect_box);
		delete pre_redirect_box;
		pre_redirect_box = 0;
	}

	if (post_redirect_box) {
		post_redir_hpane.remove(*post_redirect_box);
		delete post_redirect_box;
		post_redirect_box = 0;
	}
}

void
RouteParams_UI::setup_io_frames()
{
	cleanup_io_frames();
	
	// input
	_input_iosel = new IOSelector (*session, *_route, true);
	_input_iosel->redisplay ();
        input_frame.add (*_input_iosel);
	input_frame.show_all();
	
	// output
	_output_iosel = new IOSelector (*session, *_route, false);
	_output_iosel->redisplay ();
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
RouteParams_UI::cleanup_pre_view (bool stopupdate)
{
	if (_active_pre_view) {
		PluginUI *   plugui = 0;
		
		if (stopupdate && (plugui = dynamic_cast<PluginUI*>(_active_pre_view)) != 0) {
			  plugui->stop_updating (0);
		}

		_pre_plugin_conn.disconnect();
 		pre_redir_hpane.remove(*_active_pre_view);
		delete _active_pre_view;
		_active_pre_view = 0;
	}
}

void
RouteParams_UI::cleanup_post_view (bool stopupdate)
{
	if (_active_post_view) {
		PluginUI *   plugui = 0;
		
		if (stopupdate && (plugui = dynamic_cast<PluginUI*>(_active_post_view)) != 0) {
			  plugui->stop_updating (0);
		}
		_post_plugin_conn.disconnect();
		post_redir_hpane.remove(*_active_post_view);
		delete _active_post_view;
		_active_post_view = 0;
	}
}


void
RouteParams_UI::route_removed (Route *route)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &RouteParams_UI::route_removed), route));
	/*
	route_select_list.freeze ();
	route_select_list.clear ();
	session->foreach_route (this, &RouteParams_UI::add_route);
	route_select_list.thaw ();
	*/
	
	CList_Helpers::RowList::iterator i;
	
	if ((i = route_select_list.rows().find_data (route)) == route_select_list.rows().end()) {
		// couldn't find route to be deleted
		return;
	}

	if (route == _route)
	{
		cleanup_io_frames();
		cleanup_pre_view();
		cleanup_post_view();
		cleanup_redirect_boxes();
		
		_route = 0;
		_pre_redirect = 0;
		_post_redirect = 0;
		update_title();
	}

	route_select_list.rows().erase(i);
	
}

void
RouteParams_UI::set_session (Session *sess)
{
	ArdourDialog::set_session (sess);

	route_select_list.freeze ();
	route_select_list.clear ();

	if (session) {
		session->foreach_route (this, &RouteParams_UI::add_route);
		session->going_away.connect (slot (*this, &ArdourDialog::session_gone));
		session->RouteAdded.connect (slot (*this, &RouteParams_UI::add_route));
		start_updating ();
	} else {
		stop_updating ();
	}

	route_select_list.thaw ();

	_plugin_selector->set_session (session);
}	


void
RouteParams_UI::session_gone ()
{

	route_select_list.clear ();

	cleanup_io_frames();
	cleanup_pre_view();
	cleanup_post_view();
	cleanup_redirect_boxes();

	_route = 0;
	_pre_redirect = 0;
	_post_redirect = 0;
	update_title();

	ArdourDialog::session_gone();

}

void
RouteParams_UI::route_selected (gint row, gint col, GdkEvent *ev)
{
	Route *route;

	if ((route = (Route *) route_select_list.get_row_data (row)) != 0) {

		if (_route == route) {
			// do nothing
			return;
		}
		
		// remove event binding from previously selected
		if (_route) {
			_route_conn.disconnect();
			_route_ds_conn.disconnect();
			cleanup_redirect_boxes();
			cleanup_pre_view();
			cleanup_post_view();
			cleanup_io_frames();
		}
	
		// update the other panes with the correct info
		_route = route;
		//update_routeinfo (route);

		setup_io_frames();
		setup_redirect_boxes();
		
		// bind to redirects changed event for this route
		_route_conn = route->redirects_changed.connect (slot (*this, &RouteParams_UI::redirects_changed));

		track_input_label.set_text (_route->name());
		
		update_title();
	}
}

void
RouteParams_UI::route_unselected (gint row, gint col, GdkEvent *ev)
{
	if (_route) {
		_route_conn.disconnect();

		// remove from view
		cleanup_io_frames();
		cleanup_pre_view();
		cleanup_post_view();
		cleanup_redirect_boxes();
		
		_route = 0;
		_pre_redirect = 0;
		_post_redirect = 0;
		track_input_label.set_text(_("NO TRACK"));
		update_title();
	}
}

void
RouteParams_UI::redirects_changed (void *src)

{
	ENSURE_GUI_THREAD(bind (slot (*this, &RouteParams_UI::redirects_changed), src));
	
// 	pre_redirect_list.freeze ();
// 	pre_redirect_list.clear ();
// 	post_redirect_list.freeze ();
// 	post_redirect_list.clear ();
// 	if (_route) {
// 		_route->foreach_redirect (this, &RouteParams_UI::add_redirect_to_display);
// 	}
// 	pre_redirect_list.thaw ();
// 	post_redirect_list.thaw ();

	cleanup_pre_view();
	cleanup_post_view();
	
	_pre_redirect = 0;
	_post_redirect = 0;
	//update_title();
}



void
RouteParams_UI::show_track_menu (gint arg)
{
	using namespace Menu_Helpers;
	
	if (track_menu == 0) {
		track_menu = new Menu;
		track_menu->set_name ("ArdourContextMenu");
		track_menu->items().push_back 
				(MenuElem (_("Add Track/Bus"), 
					   slot (*(ARDOUR_UI::instance()), &ARDOUR_UI::add_route)));
	}
	track_menu->popup (1, 0);
}



void
RouteParams_UI::redirect_selected (ARDOUR::Redirect *redirect, ARDOUR::Placement place)
{
	Insert *insert;

	if ((place == PreFader && _pre_redirect == redirect)
	    || (place == PostFader && _post_redirect == redirect)){
		return;
	}
	
	if ((insert = dynamic_cast<Insert *> (redirect)) == 0) {

		Send *send;

		if ((send = dynamic_cast<Send *> (redirect)) != 0) {

			/* its a send */

			SendUI *send_ui = new SendUI (*send, *session);

			if (place == PreFader) {
				cleanup_pre_view();
				_pre_plugin_conn = send->GoingAway.connect (slot (*this, &RouteParams_UI::redirect_going_away));
				_active_pre_view = send_ui;
				
				pre_redir_hpane.add2 (*_active_pre_view);
				pre_redir_hpane.show_all();
			}
			else {
				cleanup_post_view();
				_post_plugin_conn = send->GoingAway.connect (slot (*this, &RouteParams_UI::redirect_going_away));
				_active_post_view = send_ui;
				
				post_redir_hpane.add2 (*_active_post_view);
				post_redir_hpane.show_all();
			}
		}

	} else {
		/* its an insert, though we don't know what kind yet. */

		PluginInsert *plugin_insert;
		PortInsert *port_insert;
				
		if ((plugin_insert = dynamic_cast<PluginInsert *> (insert)) != 0) {				

			PluginUI *plugin_ui = new PluginUI (session->engine(), *plugin_insert, true);

			if (place == PreFader) {
				cleanup_pre_view();
				_pre_plugin_conn = plugin_insert->plugin().GoingAway.connect (bind (slot (*this, &RouteParams_UI::plugin_going_away), PreFader));
				plugin_ui->start_updating (0);
				_active_pre_view = plugin_ui;
				pre_redir_hpane.add2 (*_active_pre_view);
				pre_redir_hpane.show_all();
			}
			else {
				cleanup_post_view();
				_post_plugin_conn = plugin_insert->plugin().GoingAway.connect (bind (slot (*this, &RouteParams_UI::plugin_going_away), PostFader));
				plugin_ui->start_updating (0);
				_active_post_view = plugin_ui;
				post_redir_hpane.add2 (*_active_post_view);
				post_redir_hpane.show_all();
			}

		} else if ((port_insert = dynamic_cast<PortInsert *> (insert)) != 0) {

			PortInsertUI *portinsert_ui = new PortInsertUI (*session, *port_insert);
					
			if (place == PreFader) {
				cleanup_pre_view();
				_pre_plugin_conn = port_insert->GoingAway.connect (slot (*this, &RouteParams_UI::redirect_going_away));
				_active_pre_view = portinsert_ui;
				pre_redir_hpane.add2 (*_active_pre_view);
				portinsert_ui->redisplay();
				pre_redir_hpane.show_all();
			}
			else {
				cleanup_post_view();
				_post_plugin_conn = port_insert->GoingAway.connect (slot (*this, &RouteParams_UI::redirect_going_away));
				_active_post_view = portinsert_ui;
				post_redir_hpane.add2 (*_active_post_view);
				portinsert_ui->redisplay();
				post_redir_hpane.show_all();
			}
		}
				
	}

	if (place == PreFader) {
		_pre_redirect = redirect;
	}
	else {
		_post_redirect = redirect;
	}
	
	update_title();
		
}

void
RouteParams_UI::redirect_unselected (ARDOUR::Redirect *redirect)
{
	// not called anymore
	
	if (redirect == _pre_redirect) {
		cleanup_pre_view();
		_pre_redirect = 0;
	}
	else if (redirect == _post_redirect) {
		cleanup_post_view();
		_post_redirect = 0;
	}
}



void
RouteParams_UI::plugin_going_away (Plugin *plugin, Placement place)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &RouteParams_UI::plugin_going_away), plugin, place));
	
	// delete the current view without calling finish

	if (place == PreFader) {
		cleanup_pre_view (false);
		_pre_redirect = 0;
	}
	else {
		cleanup_post_view (false);
		_post_redirect = 0;
	}
}

void
RouteParams_UI::redirect_going_away (ARDOUR::Redirect *plugin)

{
	ENSURE_GUI_THREAD(bind (slot (*this, &RouteParams_UI::redirect_going_away), plugin));
	
	printf ("redirect going away\n");
	// delete the current view without calling finish
	if (plugin == _pre_redirect) {
		cleanup_pre_view (false);
		_pre_redirect = 0;
	}
	else if (plugin == _post_redirect) {
		cleanup_post_view (false);
		_post_redirect = 0;
	}
}


void
RouteParams_UI::update_title ()
{
     	if (_route) {
		string title;
		title += _route->name();
// 		title += ": ";

// 		if (_redirect && (_current_view == PLUGIN_CONFIG_VIEW || _current_view == SEND_CONFIG_VIEW)) {
// 			title += _redirect->name();
// 		}
// 		else if (_current_view == INPUT_CONFIG_VIEW) {
// 			title += _("INPUT");
// 		}
// 		else if (_current_view == OUTPUT_CONFIG_VIEW) {
// 			title += _("OUTPUT");
// 		}
		
		title_label.set_text(title);

		title = _("ardour: track/bus inspector: ") + title;
		set_title(title);
	}
	else {
		title_label.set_text(_("No Route Selected"));
		set_title(_("ardour: track/bus/inspector: no route selected"));
	}	
}


void
RouteParams_UI::start_updating ()
{
	update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect 
		(slot (*this, &RouteParams_UI::update_views));
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
	
	if ((sui = dynamic_cast<SendUI*> (_active_pre_view)) != 0) {
		sui->update ();
	}
	if ((sui = dynamic_cast<SendUI*> (_active_post_view)) != 0) {
		sui->update ();
	}

}
