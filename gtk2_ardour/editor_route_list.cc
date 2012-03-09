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
#include <cstdlib>
#include <cmath>

#include "editor.h"
#include "keyboard.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "mixer_strip.h"
#include "gui_thread.h"
#include "actions.h"

#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/route_group.h>

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;

const char* _order_key = N_("editor");

void
Editor::handle_new_route (Session::RouteList& routes)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_new_route), routes));
	
	TimeAxisView *tv;
	AudioTimeAxisView *atv;
	TreeModel::Row parent;
	TreeModel::Row row;

	route_redisplay_does_not_sync_order_keys = true;
	no_route_list_redisplay = true;

	for (Session::RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);

		if (route->hidden()) {
			continue;
		}
		
		tv = new AudioTimeAxisView (*this, *session, route, *track_canvas);
		//cerr << "Editor::handle_new_route() called on " << route->name() << endl;//DEBUG
		row = *(route_display_model->append ());
		
		row[route_display_columns.route] = route;
		row[route_display_columns.text] = route->name();
		row[route_display_columns.visible] = tv->marked_for_display();
		row[route_display_columns.tv] = tv;

		RouteGroup *group = route->edit_group();
		if (group) {
			if (tv->marked_for_display()) {
				group->set_hidden(false, this);
				group_flags_changed(this, group);
			}
		}

		track_views.push_back (tv);
		
		if ((atv = dynamic_cast<AudioTimeAxisView*> (tv)) != 0) {
			/* added a new fresh one at the end */
			if (atv->route()->order_key(_order_key) == -1) {
				atv->route()->set_order_key (_order_key, route_display_model->children().size()-1);
			}
			atv->effective_gain_display ();
		}

		route->gui_changed.connect (mem_fun(*this, &Editor::handle_gui_changes));
		tv->GoingAway.connect (bind (mem_fun(*this, &Editor::remove_route), tv));
	}

	no_route_list_redisplay = false;

	redisplay_route_list ();

	if (show_editor_mixer_when_tracks_arrive) {
		show_editor_mixer (true);
	}
	
	route_redisplay_does_not_sync_order_keys = false;
}

void
Editor::handle_gui_changes (const string & what, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_gui_changes), what, src));

	if (what == "track_height") {
		/* Optional :make tracks change height while it happens, instead 
		   of on first-idle
		*/
		redisplay_route_list ();
	}

	if (what == "visible_tracks") {
		redisplay_route_list ();
	}
}

void
Editor::remove_route (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::remove_route), tv));

	TrackViewList::iterator i;
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator ri;
	boost::shared_ptr<Route> route;
	TimeAxisView* next_tv = 0;

	if (entered_track) {
		if (tv == entered_track || tv == entered_track->get_parent()) {
			entered_track = 0;
		}
	}

	/* the core model has changed, there is no need to sync 
	   view orders.
	*/

	route_redisplay_does_not_sync_order_keys = true;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[route_display_columns.tv] == tv) {
			route = (*ri)[route_display_columns.route];
			route_display_model->erase (ri);
			break;
		}
	}

	route_redisplay_does_not_sync_order_keys = false;

	if ((i = find (track_views.begin(), track_views.end(), tv)) != track_views.end()) {
		i = track_views.erase (i);
		
		if (track_views.empty()) {
			next_tv = 0;
		} else if (i == track_views.end()) {
			next_tv = track_views.front();
		} else {
			next_tv = (*i);
		}
	}

	if (current_mixer_strip && (current_mixer_strip->route() == route)) {

		if (next_tv) {
			set_selected_mixer_strip (*next_tv);
		} else {
			/* make the editor mixer strip go away by setting the
			 * button to inactive (which also unticks the menu option)
			 */
			
			ActionManager::uncheck_toggleaction ("<Actions>/Editor/show-editor-mixer");
		}
	}
}

void
Editor::route_name_changed (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::route_name_changed), tv));
	
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[route_display_columns.tv] == tv) {
			(*i)[route_display_columns.text] = tv->name();
			break;
		}
	} 
}

void
Editor::update_route_visibility ()
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	
	no_route_list_redisplay = true;

	for (i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[route_display_columns.tv];
		(*i)[route_display_columns.visible] = tv->marked_for_display ();
	}

	no_route_list_redisplay = false;
	redisplay_route_list ();
}

void
Editor::hide_track_in_display (TimeAxisView& tv, bool temponly)
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[route_display_columns.tv] == &tv) { 
			(*i)[route_display_columns.visible] = false;
			break;
		}
	}

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (&tv);

	if (atv && current_mixer_strip && (atv->route() == current_mixer_strip->route())) {
		// this will hide the mixer strip
		set_selected_mixer_strip (tv);
	}
}

void
Editor::show_track_in_display (TimeAxisView& tv)
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[route_display_columns.tv] == &tv) { 
			(*i)[route_display_columns.visible] = true;
			break;
		}
	}
}

void
Editor::sync_order_keys (const char *src)
{
	vector<int> neworder;
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator ri;

	if ((strcmp (src, _order_key) == 0) || !session || (session->state_of_the_state() & Session::Loading) || rows.empty()) {
		return;
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		neworder.push_back (0);
	}

	bool changed = false;
	int order;

	for (order = 0, ri = rows.begin(); ri != rows.end(); ++ri, ++order) {
		boost::shared_ptr<Route> route = (*ri)[route_display_columns.route];

		int old_key = order;
		int new_key = route->order_key (_order_key);

		neworder[new_key] = old_key;

		if (new_key != old_key) {
			changed = true;
		}
	}

	if (changed) {
		route_redisplay_does_not_reset_order_keys = true;
		route_display_model->reorder (neworder);
		route_redisplay_does_not_reset_order_keys = false;
	}
}

void
Editor::redisplay_route_list ()
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	uint32_t position;
	uint32_t order;
	int n;

	if (no_route_list_redisplay) {
		return;
	}

	if (session && (rows.size() > session->nroutes())) {
		/* temporary condition during a drag-n-drop */
		return;
	}

        if (session && session->deletion_in_progress()) {
                return;
        }

	for (n = 0, order = 0, position = 0, i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[route_display_columns.tv];
		boost::shared_ptr<Route> route = (*i)[route_display_columns.route];

		if (tv == 0) {
			// just a "title" row
			continue;
		}

		if (!route_redisplay_does_not_reset_order_keys) {
			
			/* this reorder is caused by user action, so reassign sort order keys
			   to tracks.
			*/
			
			route->set_order_key (_order_key, order);
		}

		bool visible = (*i)[route_display_columns.visible];
		
		if (visible) {
			tv->set_marked_for_display (true);
			position += tv->show_at (position, n, &edit_controls_vbox);
			tv->clip_to_viewport ();
		} else {
			tv->set_marked_for_display (false);
			tv->hide ();
		}
		
		++order;
		++n;
	}

	/* whenever we go idle, update the track view list to reflect the new order.
	   we can't do this here, because we could mess up something that is traversing
	   the track order and has caused a redisplay of the list.
	*/

	Glib::signal_idle().connect (mem_fun (*this, &Editor::sync_track_view_list_and_route_list));

	full_canvas_height = position + canvas_timebars_vsize;
	vertical_adjustment.set_upper (full_canvas_height);
	if ((vertical_adjustment.get_value() + canvas_height) > vertical_adjustment.get_upper()) {
		/* 
		   We're increasing the size of the canvas while the bottom is visible.
		   We scroll down to keep in step with the controls layout.
		*/
		vertical_adjustment.set_value (full_canvas_height - canvas_height);
	} 

	if (!route_redisplay_does_not_reset_order_keys && !route_redisplay_does_not_sync_order_keys) {
		session->sync_order_keys (_order_key);
	}
}

bool
Editor::sync_track_view_list_and_route_list ()
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	track_views.clear ();

	for (i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[route_display_columns.tv];
		track_views.push_back (tv);
	}
	
	return false; // do not call again (until needed)
}

void
Editor::hide_all_tracks (bool with_select)
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	no_route_list_redisplay = true;

	for (i = rows.begin(); i != rows.end(); ++i) {
		
		TreeModel::Row row = (*i);
		TimeAxisView *tv = row[route_display_columns.tv];

		if (tv == 0) {
			continue;
		}
		
		row[route_display_columns.visible] = false;
	}

	no_route_list_redisplay = false;
	redisplay_route_list ();

	/* XXX this seems like a hack and half, but its not clear where to put this
	   otherwise.
	*/

	//reset_scrolling_region ();
}

void
Editor::build_route_list_menu ()
{
        using namespace Menu_Helpers;
	using namespace Gtk;

	route_list_menu = new Menu;
	
	MenuList& items = route_list_menu->items();
	route_list_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All"), mem_fun(*this, &Editor::show_all_routes)));
	items.push_back (MenuElem (_("Hide All"), mem_fun(*this, &Editor::hide_all_routes)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), mem_fun(*this, &Editor::show_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), mem_fun(*this, &Editor::hide_all_audiotracks)));
	items.push_back (MenuElem (_("Show All Audio Busses"), mem_fun(*this, &Editor::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Audio Busses"), mem_fun(*this, &Editor::hide_all_audiobus)));
	items.push_back (MenuElem (_("Show Tracks With Regions Under Playhead"), mem_fun (*this, &Editor::show_tracks_with_regions_at_playhead)));
}

void
Editor::set_all_tracks_visibility (bool yn)
{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	no_route_list_redisplay = true;

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		TimeAxisView* tv = row[route_display_columns.tv];

		if (tv == 0) {
			continue;
		}
		
		(*i)[route_display_columns.visible] = yn;
	}

	no_route_list_redisplay = false;
	redisplay_route_list ();
}

void
Editor::set_all_audio_visibility (int tracks, bool yn) 
{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	no_route_list_redisplay = true;

	for (i = rows.begin(); i != rows.end(); ++i) {
		TreeModel::Row row = (*i);
		TimeAxisView* tv = row[route_display_columns.tv];
		AudioTimeAxisView* atv;

		if (tv == 0) {
			continue;
		}

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			switch (tracks) {
			case 0:
				(*i)[route_display_columns.visible] = yn;
				break;

			case 1:
				if (atv->is_audio_track()) {
					(*i)[route_display_columns.visible] = yn;
				}
				break;
				
			case 2:
				if (!atv->is_audio_track()) {
					(*i)[route_display_columns.visible] = yn;
				}
				break;
			}
		}
	}

	no_route_list_redisplay = false;
	redisplay_route_list ();
}

void
Editor::hide_all_routes ()
{
	set_all_tracks_visibility (false);
}

void
Editor::show_all_routes ()
{
	set_all_tracks_visibility (true);
}

void
Editor::show_all_audiobus ()
{
	set_all_audio_visibility (2, true);
}
void
Editor::hide_all_audiobus ()
{
	set_all_audio_visibility (2, false);
}

void
Editor::show_all_audiotracks()
{
	set_all_audio_visibility (1, true);
}
void
Editor::hide_all_audiotracks ()
{
	set_all_audio_visibility (1, false);
}

void
Editor::show_tracks_with_regions_at_playhead ()
{
	boost::shared_ptr<Session::RouteList> const regions = session->get_routes_with_regions_at (session->transport_frame ());

	//suspend_redisplay ();
	
	TreeModel::Children rows = route_display_model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[route_display_columns.route];

		bool found = false;
		for (Session::RouteList::iterator x = (*regions).begin(); x != (*regions).end(); ++x) {
			if ((*x) == route)
				found = true;
		}
				
		(*i)[route_display_columns.visible] = found;
	}

	no_route_list_redisplay = false;
	redisplay_route_list ();

	//resume_redisplay ();
}

bool
Editor::route_list_display_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		show_route_list_menu ();
		return true;
	}

	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	
	if (!route_list_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		if ((iter = route_display_model->get_iter (path))) {
			TimeAxisView* tv = (*iter)[route_display_columns.tv];
			if (tv) {
				bool visible = (*iter)[route_display_columns.visible];
				(*iter)[route_display_columns.visible] = !visible;
			}
		}
		return true;

	case 1:
		/* allow normal processing to occur */
		return false;

	default:
		break;
	}

	return false;
}

void
Editor::show_route_list_menu()
{
	if (route_list_menu == 0) {
		build_route_list_menu ();
	}

	route_list_menu->popup (1, gtk_get_current_event_time());
}

bool
Editor::route_list_selection_filter (const Glib::RefPtr<TreeModel>& model, const TreeModel::Path& path, bool yn)
{
	return true;
}

struct EditorOrderRouteSorter {
    bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
	    /* use of ">" forces the correct sort order */
	    return a->order_key (_order_key) < b->order_key (_order_key);
    }
};

void
Editor::initial_route_list_display ()
{
	boost::shared_ptr<Session::RouteList> routes = session->get_routes();
	Session::RouteList r (*routes);
	EditorOrderRouteSorter sorter;

	r.sort (sorter);
	
	no_route_list_redisplay = true;

	route_display_model->clear ();

	handle_new_route (r);

	no_route_list_redisplay = false;

	redisplay_route_list ();
}

void
Editor::track_list_reorder (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter, int* new_order)
{
	route_redisplay_does_not_sync_order_keys = true;
	session->set_remote_control_ids();
	redisplay_route_list ();
	route_redisplay_does_not_sync_order_keys = false;
}

void
Editor::route_list_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
	/* never reset order keys because of a property change */
	route_redisplay_does_not_reset_order_keys = true;
	session->set_remote_control_ids();
	redisplay_route_list ();
	route_redisplay_does_not_reset_order_keys = false;
}

void
Editor::route_list_delete (const Gtk::TreeModel::Path& path)
{
	/* this could require an order reset & sync */
	session->set_remote_control_ids();
	redisplay_route_list ();
}

void  
Editor::route_list_display_drag_data_received (const RefPtr<Gdk::DragContext>& context,
						int x, int y, 
						const SelectionData& data,
						guint info, guint time)
{
	if (data.get_target() == "GTK_TREE_MODEL_ROW") {
		route_list_display.on_drag_data_received (context, x, y, data, info, time);
		return;
	}
	context->drag_finish (true, false, time);
}

void
Editor::foreach_time_axis_view (sigc::slot<void,TimeAxisView&> theslot)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		theslot (**i);
	}
}

void
Editor::move_selected_tracks (bool up)
{
	if (selection->tracks.empty()) {
		return;
	}

	typedef pair<TimeAxisView*,boost::shared_ptr<Route> > ViewRoute;
	list<ViewRoute> view_routes;
	vector<int> neworder;
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		TimeAxisView* tv = (*ri)[route_display_columns.tv];
		boost::shared_ptr<Route> route = (*ri)[route_display_columns.route];

		view_routes.push_back (ViewRoute (tv, route));
	}

	list<ViewRoute>::iterator trailing;
	list<ViewRoute>::iterator leading;
	
	if (up) {
		
		trailing = view_routes.begin();
		leading = view_routes.begin();
		
		++leading;
		
		while (leading != view_routes.end()) {
			if (selection->selected (leading->first)) {
				view_routes.insert (trailing, ViewRoute (leading->first, leading->second));
				leading = view_routes.erase (leading);
			} else {
				++leading;
				++trailing;
			}
		}

	} else {

		/* if we could use reverse_iterator in list::insert, this code
		   would be a beautiful reflection of the code above. but we can't
		   and so it looks like a bit of a mess.
		*/

		trailing = view_routes.end();
		leading = view_routes.end();

		--leading; if (leading == view_routes.begin()) { return; }
		--leading;
		--trailing;

		while (1) {

			if (selection->selected (leading->first)) {
				list<ViewRoute>::iterator tmp;

				/* need to insert *after* trailing, not *before* it,
				   which is what insert (iter, val) normally does.
				*/

				tmp = trailing;
				tmp++;

				view_routes.insert (tmp, ViewRoute (leading->first, leading->second));
					
				/* can't use iter = cont.erase (iter); form here, because
				   we need iter to move backwards.
				*/

				tmp = leading;
				--tmp;

				bool done = false;

				if (leading == view_routes.begin()) {
					/* the one we've just inserted somewhere else
					   was the first in the list. erase this copy,
					   and then break, because we're done.
					*/
					done = true;
				}

				view_routes.erase (leading);
				
				if (done) {
					break;
				}

				leading = tmp;

			} else {
				if (leading == view_routes.begin()) {
					break;
				}
				--leading;
				--trailing;
			}
		};
	}

	for (leading = view_routes.begin(); leading != view_routes.end(); ++leading) {
		neworder.push_back (leading->second->order_key (_order_key));
	}

	route_display_model->reorder (neworder);

	session->sync_order_keys (_order_key);
}
