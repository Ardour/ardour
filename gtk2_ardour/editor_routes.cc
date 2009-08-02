/*
    Copyright (C) 2000-2009 Paul Davis 

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

#include <list>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cassert>

#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"

#include "ardour/diskstream.h"

#include "editor.h"
#include "keyboard.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "gui_thread.h"
#include "actions.h"
#include "utils.h"
#include "editor_group_tabs.h"
#include "editor_routes.h"

#include "pbd/unknown_type.h"

#include "ardour/route.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;

EditorRoutes::EditorRoutes (Editor* e)
	: EditorComponent (e),
	  _ignore_reorder (false),
	  _no_redisplay (false),
	  _redisplay_does_not_sync_order_keys (false),
	  _redisplay_does_not_reset_order_keys (false),
	  _menu (0)
{
	_scroller.add (_display);
	_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	_model = ListStore::create (_columns);
	_display.set_model (_model);

	CellRendererPixbufToggle* rec_col_renderer = manage (new CellRendererPixbufToggle());

	rec_col_renderer->set_active_pixbuf (::get_icon("record_normal_red"));
	rec_col_renderer->set_inactive_pixbuf (::get_icon("record_disabled_grey"));

	rec_col_renderer->signal_toggled().connect (mem_fun (*this, &EditorRoutes::on_tv_rec_enable_toggled));

	Gtk::TreeViewColumn* rec_state_column = manage (new TreeViewColumn("Rec", *rec_col_renderer));
	rec_state_column->add_attribute(rec_col_renderer->property_active(), _columns.rec_enabled);
	rec_state_column->add_attribute(rec_col_renderer->property_visible(), _columns.is_track);

	_display.append_column (*rec_state_column);
	_display.append_column (_("Show"), _columns.visible);
	_display.append_column (_("Name"), _columns.text);
	
	_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	_display.get_column (2)->set_data (X_("colnum"), GUINT_TO_POINTER(2));
	
	_display.set_headers_visible (true);
	_display.set_name ("TrackListDisplay");
	_display.get_selection()->set_mode (SELECTION_NONE);
	_display.set_reorderable (true);
	_display.set_rules_hint (true);
	_display.set_size_request (100, -1);
	_display.add_object_drag (_columns.route.index(), "routes");

	CellRendererToggle* visible_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (1));
	
	visible_cell->property_activatable() = true;
	visible_cell->property_radio() = false;

	_model->signal_row_deleted().connect (mem_fun (*this, &EditorRoutes::route_deleted));
	_model->signal_row_changed().connect (mem_fun (*this, &EditorRoutes::changed));
	_model->signal_rows_reordered().connect (mem_fun (*this, &EditorRoutes::reordered));
	_display.signal_button_press_event().connect (mem_fun (*this, &EditorRoutes::button_press), false);

	Route::SyncOrderKeys.connect (mem_fun (*this, &EditorRoutes::sync_order_keys));
}

void
EditorRoutes::connect_to_session (Session* s)
{
	EditorComponent::connect_to_session (s);

	initial_display ();
}

void 
EditorRoutes::on_tv_rec_enable_toggled (Glib::ustring const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	AudioTimeAxisView *atv = dynamic_cast<AudioTimeAxisView*> (tv);

	if (atv != 0 && atv->is_audio_track()){
		atv->reversibly_apply_track_boolean ("rec-enable change", &Track::set_record_enable, !atv->track()->record_enabled(), this);
	}
}

void
EditorRoutes::build_menu ()
{
	using namespace Menu_Helpers;
	using namespace Gtk;

	_menu = new Menu;
	
	MenuList& items = _menu->items();
	_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All"), mem_fun (*this, &EditorRoutes::show_all_routes)));
	items.push_back (MenuElem (_("Hide All"), mem_fun (*this, &EditorRoutes::hide_all_routes)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), mem_fun (*this, &EditorRoutes::show_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), mem_fun (*this, &EditorRoutes::hide_all_audiotracks)));
	items.push_back (MenuElem (_("Show All Audio Busses"), mem_fun (*this, &EditorRoutes::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Audio Busses"), mem_fun (*this, &EditorRoutes::hide_all_audiobus)));

}

void
EditorRoutes::show_menu ()
{
	if (_menu == 0) {
		build_menu ();
	}

	_menu->popup (1, gtk_get_current_event_time());
}

void
EditorRoutes::redisplay ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	uint32_t position;
	int n;

	if (_no_redisplay) {
		return;
	}

	for (n = 0, position = 0, i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[_columns.tv];
		boost::shared_ptr<Route> route = (*i)[_columns.route];

		if (tv == 0) {
			// just a "title" row
			continue;
		}

		if (!_redisplay_does_not_reset_order_keys) {
			
			/* this reorder is caused by user action, so reassign sort order keys
			   to tracks.
			*/
			
			route->set_order_key (N_ ("editor"), n);
		}

		bool visible = (*i)[_columns.visible];

		/* show or hide the TimeAxisView */
		if (visible) {
			tv->set_marked_for_display (true);
			position += tv->show_at (position, n, &_editor->edit_controls_vbox);
			tv->clip_to_viewport ();
		} else {
			tv->set_marked_for_display (false);
			tv->hide ();
		}
		
		n++;
	}

	/* whenever we go idle, update the track view list to reflect the new order.
	   we can't do this here, because we could mess up something that is traversing
	   the track order and has caused a redisplay of the list.
	*/

	Glib::signal_idle().connect (mem_fun (*_editor, &Editor::sync_track_view_list_and_routes));
	
	_editor->full_canvas_height = position + _editor->canvas_timebars_vsize;
	_editor->vertical_adjustment.set_upper (_editor->full_canvas_height);

	if ((_editor->vertical_adjustment.get_value() + _editor->_canvas_height) > _editor->vertical_adjustment.get_upper()) {
		/* 
		   We're increasing the size of the canvas while the bottom is visible.
		   We scroll down to keep in step with the controls layout.
		*/
		_editor->vertical_adjustment.set_value (_editor->full_canvas_height - _editor->_canvas_height);
	}

	if (!_redisplay_does_not_reset_order_keys && !_redisplay_does_not_sync_order_keys) {
		_session->sync_order_keys (N_ ("editor"));
	}
}

void
EditorRoutes::route_deleted (Gtk::TreeModel::Path const &)
{
	/* this could require an order reset & sync */
	_session->set_remote_control_ids();
	_ignore_reorder = true;
	redisplay ();
	_ignore_reorder = false;
}


void
EditorRoutes::changed (Gtk::TreeModel::Path const &, Gtk::TreeModel::iterator const &)
{
	/* never reset order keys because of a property change */
	_redisplay_does_not_reset_order_keys = true;
	_session->set_remote_control_ids();
	redisplay ();
	_redisplay_does_not_reset_order_keys = false;
}

void
EditorRoutes::routes_added (list<RouteTimeAxisView*> routes)
{
	TreeModel::Row row;

	_redisplay_does_not_sync_order_keys = true;
	suspend_redisplay ();

	for (list<RouteTimeAxisView*>::iterator x = routes.begin(); x != routes.end(); ++x) {

		row = *(_model->append ());

		row[_columns.text] = (*x)->route()->name();
		row[_columns.visible] = (*x)->marked_for_display();
		row[_columns.tv] = *x;
		row[_columns.route] = (*x)->route ();
		row[_columns.is_track] = (boost::dynamic_pointer_cast<Track> ((*x)->route()) != 0);

		_ignore_reorder = true;
		
		/* added a new fresh one at the end */
		if ((*x)->route()->order_key (N_ ("editor")) == -1) {
			(*x)->route()->set_order_key (N_ ("editor"), _model->children().size()-1);
		}
		
		_ignore_reorder = false;

		boost::weak_ptr<Route> wr ((*x)->route());
		(*x)->route()->gui_changed.connect (mem_fun (*this, &EditorRoutes::handle_gui_changes));
		(*x)->route()->NameChanged.connect (bind (mem_fun (*this, &EditorRoutes::route_name_changed), wr));
		(*x)->GoingAway.connect (bind (mem_fun (*this, &EditorRoutes::route_removed), *x));

		if ((*x)->is_track()) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> ((*x)->route());
			t->diskstream()->RecordEnableChanged.connect (mem_fun (*this, &EditorRoutes::update_rec_display));
		}
	}

	resume_redisplay ();
	_redisplay_does_not_sync_order_keys = false;
}

void
EditorRoutes::handle_gui_changes (string const & what, void *src)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &EditorRoutes::handle_gui_changes), what, src));

	if (what == "track_height") {
		/* Optional :make tracks change height while it happens, instead 
		   of on first-idle
		*/
		//update_canvas_now ();
		redisplay ();
	}

	if (what == "visible_tracks") {
		redisplay ();
	}
}

void
EditorRoutes::route_removed (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &EditorRoutes::route_removed), tv));

	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;

	/* the core model has changed, there is no need to sync 
	   view orders.
	*/

	_redisplay_does_not_sync_order_keys = true;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[_columns.tv] == tv) {
			_model->erase (ri);
			break;
		}
	}

	_redisplay_does_not_sync_order_keys = false;
}

void
EditorRoutes::route_name_changed (boost::weak_ptr<Route> r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &EditorRoutes::route_name_changed), r));

	boost::shared_ptr<Route> route = r.lock ();
	if (!route) {
		return;
	}
	
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> t = (*i)[_columns.route];
		if (t == route) {
			(*i)[_columns.text] = route->name();
			break;
		}
	} 
}

void
EditorRoutes::update_visibility ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

	for (i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[_columns.tv];
		(*i)[_columns.visible] = tv->marked_for_display ();
		cerr << "marked " << tv->name() << " for display = " << tv->marked_for_display() << endl;
	}

	resume_redisplay ();
}

void
EditorRoutes::hide_track_in_display (TimeAxisView& tv)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[_columns.tv] == &tv) { 
			(*i)[_columns.visible] = false;
			break;
		}
	}
}

void
EditorRoutes::show_track_in_display (TimeAxisView& tv)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[_columns.tv] == &tv) { 
			(*i)[_columns.visible] = true;
			break;
		}
	}
}

void
EditorRoutes::reordered (TreeModel::Path const &, TreeModel::iterator const &, int* /*what*/)
{
	redisplay ();
}

/** If src == "editor", take editor order keys from each route and use them to rearrange the
 *  route list so that the visual arrangement of routes matches the order keys from the routes.
 */
void
EditorRoutes::sync_order_keys (string const & src)
{
	vector<int> neworder;
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;

	if (src == N_ ("editor") || !_session || (_session->state_of_the_state() & Session::Loading) || rows.empty()) {
		return;
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		neworder.push_back (0);
	}

	bool changed = false;
	int order;

	for (order = 0, ri = rows.begin(); ri != rows.end(); ++ri, ++order) {
		boost::shared_ptr<Route> route = (*ri)[_columns.route];

		int old_key = order;
		int new_key = route->order_key (N_ ("editor"));

		neworder[new_key] = old_key;

		if (new_key != old_key) {
			changed = true;
		}
	}

	if (changed) {
		_redisplay_does_not_reset_order_keys = true;
		_model->reorder (neworder);
		_redisplay_does_not_reset_order_keys = false;
	}
}


void
EditorRoutes::hide_all_tracks (bool /*with_select*/)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

	for (i = rows.begin(); i != rows.end(); ++i) {
		
		TreeModel::Row row = (*i);
		TimeAxisView *tv = row[_columns.tv];

		if (tv == 0) {
			continue;
		}
		
		row[_columns.visible] = false;
	}

	resume_redisplay ();

	/* XXX this seems like a hack and half, but its not clear where to put this
	   otherwise.
	*/

	//reset_scrolling_region ();
}

void
EditorRoutes::set_all_tracks_visibility (bool yn)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		TimeAxisView* tv = row[_columns.tv];

		if (tv == 0) {
			continue;
		}
		
		(*i)[_columns.visible] = yn;
	}

	resume_redisplay ();
}

void
EditorRoutes::set_all_audio_visibility (int tracks, bool yn) 
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

	for (i = rows.begin(); i != rows.end(); ++i) {
		TreeModel::Row row = (*i);
		TimeAxisView* tv = row[_columns.tv];
		AudioTimeAxisView* atv;

		if (tv == 0) {
			continue;
		}

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			switch (tracks) {
			case 0:
				(*i)[_columns.visible] = yn;
				break;

			case 1:
				if (atv->is_audio_track()) {
					(*i)[_columns.visible] = yn;
				}
				break;
				
			case 2:
				if (!atv->is_audio_track()) {
					(*i)[_columns.visible] = yn;
				}
				break;
			}
		}
	}

	resume_redisplay ();
}

void
EditorRoutes::hide_all_routes ()
{
	set_all_tracks_visibility (false);
}

void
EditorRoutes::show_all_routes ()
{
	set_all_tracks_visibility (true);
}

void
EditorRoutes::show_all_audiobus ()
{
	set_all_audio_visibility (2, true);
}
void
EditorRoutes::hide_all_audiobus ()
{
	set_all_audio_visibility (2, false);
}

void
EditorRoutes::show_all_audiotracks()
{
	set_all_audio_visibility (1, true);
}
void
EditorRoutes::hide_all_audiotracks ()
{
	set_all_audio_visibility (1, false);
}

bool
EditorRoutes::button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		show_menu ();
		return true;
	}

	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	
	if (!_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {

	case 0:
		/* allow normal processing to occur */
		return false;
	case 1:
		if ((iter = _model->get_iter (path))) {
			TimeAxisView* tv = (*iter)[_columns.tv];
			if (tv) {
				bool visible = (*iter)[_columns.visible];
				(*iter)[_columns.visible] = !visible;
			}
		}
		return true;

	case 2:
		/* allow normal processing to occur */
		return false;

	default:
		break;
	}

	return false;
}

bool
EditorRoutes::selection_filter (Glib::RefPtr<TreeModel> const &, TreeModel::Path const &, bool)
{
	return true;
}

struct EditorOrderRouteSorter {
    bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
	    /* use of ">" forces the correct sort order */
	    return a->order_key (N_ ("editor")) < b->order_key (N_ ("editor"));
    }
};

void
EditorRoutes::initial_display ()
{
	boost::shared_ptr<RouteList> routes = _session->get_routes();
	RouteList r (*routes);
	EditorOrderRouteSorter sorter;

	r.sort (sorter);

	suspend_redisplay ();

	_model->clear ();
	_editor->handle_new_route (r);

	/* don't show master bus in a new session */

	if (ARDOUR_UI::instance()->session_is_new ()) {

		TreeModel::Children rows = _model->children();
		TreeModel::Children::iterator i;
	
		_no_redisplay = true;
		
		for (i = rows.begin(); i != rows.end(); ++i) {
			TimeAxisView *tv =  (*i)[_columns.tv];
			RouteTimeAxisView *rtv;
			
			if ((rtv = dynamic_cast<RouteTimeAxisView*>(tv)) != 0) {
				if (rtv->route()->is_master()) {
					_display.get_selection()->unselect (i);
				}
			}
		}
		
		_no_redisplay = false;
		redisplay ();
	}	

	resume_redisplay ();
}

void
EditorRoutes::track_list_reorder (Gtk::TreeModel::Path const &, Gtk::TreeModel::iterator const &, int* /*new_order*/)
{
	_redisplay_does_not_sync_order_keys = true;
	_session->set_remote_control_ids();
	redisplay ();
	_redisplay_does_not_sync_order_keys = false;
}

void  
EditorRoutes::display_drag_data_received (const RefPtr<Gdk::DragContext>& context,
					     int x, int y, 
					     const SelectionData& data,
					     guint info, guint time)
{
	if (data.get_target() == "GTK_TREE_MODEL_ROW") {
		_display.on_drag_data_received (context, x, y, data, info, time);
		return;
	}
	
	context->drag_finish (true, false, time);
}

void
EditorRoutes::move_selected_tracks (bool up)
{
	if (_editor->selection->tracks.empty()) {
		return;
	}

	typedef std::pair<TimeAxisView*,boost::shared_ptr<Route> > ViewRoute;
	std::list<ViewRoute> view_routes;
	std::vector<int> neworder;
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		TimeAxisView* tv = (*ri)[_columns.tv];
		boost::shared_ptr<Route> route = (*ri)[_columns.route];

		view_routes.push_back (ViewRoute (tv, route));
	}

	list<ViewRoute>::iterator trailing;
	list<ViewRoute>::iterator leading;
	
	if (up) {
		
		trailing = view_routes.begin();
		leading = view_routes.begin();
		
		++leading;
		
		while (leading != view_routes.end()) {
			if (_editor->selection->selected (leading->first)) {
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

			if (_editor->selection->selected (leading->first)) {
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
		neworder.push_back (leading->second->order_key (N_ ("editor")));
	}

	_model->reorder (neworder);

	_session->sync_order_keys (N_ ("editor"));
}

void
EditorRoutes::update_rec_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];

		if (boost::dynamic_pointer_cast<Track>(route)) {

			if (route->record_enabled()){
				(*i)[_columns.rec_enabled] = true;
			} else {
				(*i)[_columns.rec_enabled] = false;
			}
		} 
	}
}

list<TimeAxisView*>
EditorRoutes::views () const
{
	list<TimeAxisView*> v;
	for (TreeModel::Children::iterator i = _model->children().begin(); i != _model->children().end(); ++i) {
		v.push_back ((*i)[_columns.tv]);
	}

	return v;
}

void
EditorRoutes::clear ()
{
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);
}
