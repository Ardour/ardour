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
#include <cassert>

#include "editor.h"
#include "keyboard.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "gui_thread.h"
#include "actions.h"

#include <pbd/unknown_type.h>

#include <ardour/route.h>

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;


void
Editor::handle_new_route (Session::RouteList& routes)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_new_route), routes));
	
	TimeAxisView *tv;
	RouteTimeAxisView *rtv;
	TreeModel::Row parent;
	TreeModel::Row row;

	ignore_route_list_reorder = true;
	ignore_route_order_sync = true;
	no_route_list_redisplay = true;

	for (Session::RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);

		if (route->is_hidden()) {
			continue;
		}
		
		if (route->default_type() == ARDOUR::DataType::AUDIO)
			tv = new AudioTimeAxisView (*this, *session, route, *track_canvas);
		else if (route->default_type() == ARDOUR::DataType::MIDI)
			tv = new MidiTimeAxisView (*this, *session, route, *track_canvas);
		else
			throw unknown_type();

		//cerr << "Editor::handle_new_route() called on " << route->name() << endl;//DEBUG
#if 0
		if (route_display_model->children().size() == 0) {
			
			/* set up basic entries */
			
			TreeModel::Row row;
			
			row = *(route_display_model->append());  // path = "0"
			row[route_display_columns.text] = _("Busses");
			row[route_display_columns.tv] = 0;
			row = *(route_display_model->append());  // path = "1"
			row[route_display_columns.text] = _("Tracks");
			row[route_display_columns.tv] = 0;
			
		}
		
		if (dynamic_cast<AudioTrack*>(route.get()) != 0) {
			TreeModel::iterator iter = route_display_model->get_iter ("1");  // audio tracks 
			parent = *iter;
		} else {
			TreeModel::iterator iter = route_display_model->get_iter ("0");  // busses
			parent = *iter;
		}
		
		
		row = *(route_display_model->append (parent.children()));
#else 
		row = *(route_display_model->append ());
#endif

		// cerr << route->name() << " marked for display ? " << tv->marked_for_display() << endl;
		
		row[route_display_columns.text] = route->name();
		row[route_display_columns.visible] = tv->marked_for_display();
		row[route_display_columns.tv] = tv;
		row[route_display_columns.route] = route;

		track_views.push_back (tv);
		
		ignore_route_list_reorder = true;
		
		if ((rtv = dynamic_cast<RouteTimeAxisView*> (tv)) != 0) {
			/* added a new fresh one at the end */
			if (rtv->route()->order_key(N_("editor")) == -1) {
				rtv->route()->set_order_key (N_("editor"), route_display_model->children().size()-1);
			}
			rtv->effective_gain_display ();
		}
		
		ignore_route_list_reorder = false;

		tv->set_old_order_key (route_display_model->children().size() - 1);
		route->gui_changed.connect (mem_fun(*this, &Editor::handle_gui_changes));
		
		tv->GoingAway.connect (bind (mem_fun(*this, &Editor::remove_route), tv));
	}

	ignore_route_list_reorder = false;
	ignore_route_order_sync = false;
	no_route_list_redisplay = false;

	redisplay_route_list ();

	if (show_editor_mixer_when_tracks_arrive) {
		show_editor_mixer (true);
	}
	editor_list_button.set_sensitive(true);
}

void
Editor::handle_gui_changes (const string & what, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_gui_changes), what, src));
	
	if (what == "track_height") {
		/* Optional :make tracks change height while it happens, instead 
		   of on first-idle
		*/
		//track_canvas->update_now ();
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

	if (tv == entered_track) {
		entered_track = 0;
	}

	/* Decrement old order keys for tracks `above' the one that is being removed */
	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		TimeAxisView* v = (*ri)[route_display_columns.tv];
		if (v->old_order_key() > tv->old_order_key()) {
			v->set_old_order_key (v->old_order_key() - 1);
		}
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[route_display_columns.tv] == tv) {
			route_display_model->erase (ri);
			break;
		}
	}

	if ((i = find (track_views.begin(), track_views.end(), tv)) != track_views.end()) {
		track_views.erase (i);
	}

	/* since the editor mixer goes away when you remove a route, set the
	 * button to inactive and untick the menu option
	 */

	ActionManager::uncheck_toggleaction ("<Actions>/Editor/show-editor-mixer");
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
		cerr << "marked " << tv->name() << " for display = " << tv->marked_for_display() << endl;
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
			// if (temponly) {
			tv.set_marked_for_display (false);
			// }
			break;
		}
	}

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&tv);

	if (rtv && current_mixer_strip && (rtv->route() == current_mixer_strip->route())) {
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
			tv.set_marked_for_display (true);
			break;
		}
	}
}

void
Editor::route_list_reordered (const TreeModel::Path& path,const TreeModel::iterator& iter,int* what)
{
	redisplay_route_list ();
}


void
Editor::sync_order_keys ()
{
	vector<int> neworder;
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator ri;

	if (ignore_route_order_sync || !session || (session->state_of_the_state() & Session::Loading) || rows.empty()) {
		return;
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		neworder.push_back (0);
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		TimeAxisView* tv = (*ri)[route_display_columns.tv];
		boost::shared_ptr<Route> route = (*ri)[route_display_columns.route];
		neworder[route->order_key (X_("editor"))] = tv->old_order_key ();
	}

	ignore_route_list_reorder = true;
	route_display_model->reorder (neworder);
	ignore_route_list_reorder = false;
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

	for (n = 0, order = 0, position = 0, i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[route_display_columns.tv];
		RouteTimeAxisView* rt; 

		if (tv == 0) {
			// just a "title" row
			continue;
		}

		if (!ignore_route_list_reorder) {
			
			/* this reorder is caused by user action, so reassign sort order keys
			   to tracks.
			*/
			
			if ((rt = dynamic_cast<RouteTimeAxisView*> (tv)) != 0) {
				rt->route()->set_order_key (N_("editor"), order);
				++order;
			}
		}

		bool visible = (*i)[route_display_columns.visible];

		if (visible) {
			tv->set_marked_for_display (true);
			position += tv->show_at (position, n, &edit_controls_vbox);
		} else {
			tv->hide ();
		}
		
		n++;
		
	}

	full_canvas_height = position;

	vertical_adjustment.set_upper (position + canvas_timebars_vsize);
	if ((vertical_adjustment.get_value() + canvas_height) > vertical_adjustment.get_upper()) {
		/* 
		   We're increasing the size of the canvas while the bottom is visible.
		   We scroll down to keep in step with the controls layout.
		*/
		vertical_adjustment.set_value (position + canvas_timebars_vsize - canvas_height);
	} 

	if (Config->get_sync_all_route_ordering() && !ignore_route_list_reorder) {
		ignore_route_order_sync = true;
		Route::SyncOrderKeys (); // EMIT SIGNAL
		ignore_route_order_sync = false;
	}

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
	    return a->order_key ("editor") < b->order_key ("editor");
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
	session->set_remote_control_ids();
	redisplay_route_list ();
}

void
Editor::route_list_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
	session->set_remote_control_ids();
	redisplay_route_list ();
}

void
Editor::route_list_delete (const Gtk::TreeModel::Path& path)
{
	session->set_remote_control_ids();
	ignore_route_list_reorder = true;
	redisplay_route_list ();
	ignore_route_list_reorder = false;
}

void  
Editor::route_list_display_drag_data_received (const RefPtr<Gdk::DragContext>& context,
						int x, int y, 
						const SelectionData& data,
						guint info, guint time)
{
	cerr << "RouteLD::dddr target = " << data.get_target() << endl;
	
	if (data.get_target() == "GTK_TREE_MODEL_ROW") {
		cerr << "Delete drag data drop to treeview\n";
		route_list_display.on_drag_data_received (context, x, y, data, info, time);
		return;
	}
	cerr << "some other kind of drag\n";
	context->drag_finish (true, false, time);
}

RouteTimeAxisView*
Editor::get_route_view_by_id (PBD::ID& id)
{
	RouteTimeAxisView* v;

	for(TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if((v = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
			if(v->route()->id() == id) {
				return v;
			}
		}
	}

	return 0;
}

void
Editor::foreach_time_axis_view (sigc::slot<void,TimeAxisView&> theslot)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		theslot (**i);
	}
}
