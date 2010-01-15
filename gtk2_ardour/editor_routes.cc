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

#include "ardour/diskstream.h"
#include "ardour/session.h"

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

#include "gtkmm2ext/cell_renderer_pixbuf_multi.h"
#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using Gtkmm2ext::Keyboard;

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

	// Record enable toggle
	CellRendererPixbufToggle* rec_col_renderer = manage (new CellRendererPixbufToggle());

	rec_col_renderer->set_active_pixbuf (::get_icon("rec-enabled"));
	rec_col_renderer->set_inactive_pixbuf (::get_icon("act-disabled"));
	rec_col_renderer->signal_toggled().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_rec_enable_toggled));

	TreeViewColumn* rec_state_column = manage (new TreeViewColumn("R", *rec_col_renderer));

	rec_state_column->add_attribute(rec_col_renderer->property_active(), _columns.rec_enabled);
	rec_state_column->add_attribute(rec_col_renderer->property_visible(), _columns.is_track);

	// Mute enable toggle
	CellRendererPixbufMulti* mute_col_renderer = manage (new CellRendererPixbufMulti());

	mute_col_renderer->set_pixbuf (0, ::get_icon("act-disabled"));
	mute_col_renderer->set_pixbuf (1, ::get_icon("mute-enabled"));
	mute_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_mute_enable_toggled));

	TreeViewColumn* mute_state_column = manage (new TreeViewColumn("M", *mute_col_renderer));

	mute_state_column->add_attribute(mute_col_renderer->property_state(), _columns.mute_state);

	// Solo enable toggle
	CellRendererPixbufMulti* solo_col_renderer = manage (new CellRendererPixbufMulti());

	solo_col_renderer->set_pixbuf (0, ::get_icon("act-disabled"));
	solo_col_renderer->set_pixbuf (1, ::get_icon("solo-enabled"));
	solo_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_solo_enable_toggled));

	TreeViewColumn* solo_state_column = manage (new TreeViewColumn("S", *solo_col_renderer));

	solo_state_column->add_attribute(solo_col_renderer->property_state(), _columns.solo_state);

	// Solo isolate toggle
	CellRendererPixbufMulti* solo_iso_renderer = manage (new CellRendererPixbufMulti());

	solo_iso_renderer->set_pixbuf (0, ::get_icon("act-disabled"));
	solo_iso_renderer->set_pixbuf (1, ::get_icon("solo-isolated"));
	solo_iso_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_solo_isolate_toggled));

	TreeViewColumn* solo_isolate_state_column = manage (new TreeViewColumn("I", *solo_iso_renderer));

	solo_isolate_state_column->add_attribute(solo_iso_renderer->property_state(), _columns.solo_isolate_state);

	_display.append_column (*rec_state_column);
	_display.append_column (*mute_state_column);
	_display.append_column (*solo_state_column);
	_display.append_column (*solo_isolate_state_column);
	_display.append_column (_("Show"), _columns.visible);
	_display.append_column (_("Name"), _columns.text);

	_display.set_headers_visible (true);
	_display.set_name ("TrackListDisplay");
	_display.get_selection()->set_mode (SELECTION_SINGLE);
	_display.set_reorderable (true);
	_display.set_rules_hint (true);
	_display.set_size_request (100, -1);
	_display.add_object_drag (_columns.route.index(), "routes");

	CellRendererText* name_cell = dynamic_cast<CellRendererText*> (_display.get_column_cell_renderer (5));
	assert (name_cell);

	TreeViewColumn* name_column = _display.get_column (5);
	assert (name_column);

	name_column->add_attribute (name_cell->property_editable(), _columns.name_editable);
	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (sigc::mem_fun (*this, &EditorRoutes::name_edit));

	CellRendererToggle* visible_cell = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (4));

	visible_cell->property_activatable() = true;
	visible_cell->property_radio() = false;
	visible_cell->signal_toggled().connect (sigc::mem_fun (*this, &EditorRoutes::visible_changed));

	_model->signal_row_deleted().connect (sigc::mem_fun (*this, &EditorRoutes::route_deleted));
	_model->signal_rows_reordered().connect (sigc::mem_fun (*this, &EditorRoutes::reordered));
	_display.signal_button_press_event().connect (sigc::mem_fun (*this, &EditorRoutes::button_press), false);

	Route::SyncOrderKeys.connect (*this, ui_bind (&EditorRoutes::sync_order_keys, this, _1), gui_context());
}

void
EditorRoutes::set_session (Session* s)
{
	EditorComponent::set_session (s);

	initial_display ();

	if (_session) {
		_session->SoloChanged.connect (*this, boost::bind (&EditorRoutes::solo_changed_so_update_mute, this), gui_context());
	}
}

void
EditorRoutes::on_tv_rec_enable_toggled (Glib::ustring const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	row[_columns.name_editable] = !row[_columns.rec_enabled];
	
	TimeAxisView *tv = row[_columns.tv];
	AudioTimeAxisView *atv = dynamic_cast<AudioTimeAxisView*> (tv);

	if (atv != 0 && atv->is_audio_track()){
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (atv->route());
		_session->set_record_enable (rl, !atv->track()->record_enabled(), Session::rt_cleanup);
	}
}

void
EditorRoutes::on_tv_mute_enable_toggled (Glib::ustring const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	AudioTimeAxisView *atv = dynamic_cast<AudioTimeAxisView*> (tv);

	if (atv != 0) {
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (atv->route());
		_session->set_mute (rl, !atv->route()->muted(), Session::rt_cleanup);
	}
}

void
EditorRoutes::on_tv_solo_enable_toggled (Glib::ustring const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	AudioTimeAxisView *atv = dynamic_cast<AudioTimeAxisView*> (tv);

	if (atv != 0) {
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (atv->route());
		_session->set_solo (rl, !atv->route()->soloed(), Session::rt_cleanup);
	}
}

void
EditorRoutes::on_tv_solo_isolate_toggled (Glib::ustring const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	AudioTimeAxisView *atv = dynamic_cast<AudioTimeAxisView*> (tv);

	if (atv != 0) {
		atv->route()->set_solo_isolated (!atv->route()->solo_isolated(), this);
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

	items.push_back (MenuElem (_("Show All"), sigc::mem_fun (*this, &EditorRoutes::show_all_routes)));
	items.push_back (MenuElem (_("Hide All"), sigc::mem_fun (*this, &EditorRoutes::hide_all_routes)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), sigc::mem_fun (*this, &EditorRoutes::show_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), sigc::mem_fun (*this, &EditorRoutes::hide_all_audiotracks)));
	items.push_back (MenuElem (_("Show All Audio Busses"), sigc::mem_fun (*this, &EditorRoutes::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Audio Busses"), sigc::mem_fun (*this, &EditorRoutes::hide_all_audiobus)));
	items.push_back (MenuElem (_("Show Tracks With Regions Under Playhead"), sigc::mem_fun (*this, &EditorRoutes::show_tracks_with_regions_at_playhead)));
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
	if (_no_redisplay || !_session) {
		return;
	}

	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	uint32_t position;
	int n;

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
	Glib::signal_idle().connect (sigc::mem_fun (*_editor, &Editor::sync_track_view_list_and_routes));

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
	if (!_session || _session->deletion_in_progress()) {
		return;
	}
		
        /* this could require an order reset & sync */
	_session->set_remote_control_ids();
	_ignore_reorder = true;
	redisplay ();
	_ignore_reorder = false;
}

void
EditorRoutes::visible_changed (Glib::ustring const & path)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	TreeIter iter;

	if ((iter = _model->get_iter (path))) {
		TimeAxisView* tv = (*iter)[_columns.tv];
		if (tv) {
			bool visible = (*iter)[_columns.visible];
			(*iter)[_columns.visible] = !visible;
		}
	}

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

		(*x)->route()->gui_changed.connect (*this, ui_bind (&EditorRoutes::handle_gui_changes, this, _1, _2), gui_context());
		(*x)->route()->NameChanged.connect (*this, boost::bind (&EditorRoutes::route_name_changed, this, wr), gui_context());

		if ((*x)->is_track()) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> ((*x)->route());
			t->diskstream()->RecordEnableChanged.connect (*this, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());
		}

		(*x)->route()->mute_changed.connect (*this, boost::bind (&EditorRoutes::update_mute_display, this), gui_context());
		(*x)->route()->solo_changed.connect (*this, boost::bind (&EditorRoutes::update_solo_display, this), gui_context());
		(*x)->route()->solo_isolated_changed.connect (*this, boost::bind (&EditorRoutes::update_solo_isolate_display, this), gui_context());
	}

	update_rec_display ();
	update_mute_display ();
	update_solo_display ();
	update_solo_isolate_display ();
	resume_redisplay ();
	_redisplay_does_not_sync_order_keys = false;
}

void
EditorRoutes::handle_gui_changes (string const & what, void*)
{
	ENSURE_GUI_THREAD (*this, &EditorRoutes::handle_gui_changes, what, src)

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
	ENSURE_GUI_THREAD (*this, &EditorRoutes::route_removed, tv)

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
	ENSURE_GUI_THREAD (*this, &EditorRoutes::route_name_changed, r)

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

/** If src != "editor", take editor order keys from each route and use them to rearrange the
 *  route list so that the visual arrangement of routes matches the order keys from the routes.
 */
void
EditorRoutes::sync_order_keys (string const & src)
{
	vector<int> neworder;
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;

	if (src == N_ ("editor") || !_session || (_session->state_of_the_state() & (Session::Loading|Session::Deletion)) || rows.empty()) {
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
	suspend_redisplay ();
	_model->clear ();

	if (!_session) {
		resume_redisplay ();
		return;
	}

	boost::shared_ptr<RouteList> routes = _session->get_routes();
	RouteList r (*routes);
	EditorOrderRouteSorter sorter;

	r.sort (sorter);
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
			(*i)[_columns.rec_enabled] = route->record_enabled ();
			(*i)[_columns.name_editable] = !route->record_enabled ();
		}
	}
}

void
EditorRoutes::update_mute_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.mute_state] = RouteUI::mute_visual_state (_session, route) > 0 ? 1 : 0;
	}
}

void
EditorRoutes::update_solo_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.solo_state] = RouteUI::solo_visual_state (route) > 0 ? 1 : 0;
	}
}

void
EditorRoutes::update_solo_isolate_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.solo_isolate_state] = RouteUI::solo_isolate_visual_state (route) > 0 ? 1 : 0;
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

void
EditorRoutes::name_edit (Glib::ustring const & path, Glib::ustring const & new_text)
{
	TreeIter iter = _model->get_iter (path);
	if (!iter) {
		return;
	}

	boost::shared_ptr<Route> route = (*iter)[_columns.route];

	if (route && route->name() != new_text) {
		route->set_name (new_text);
	}
}

void
EditorRoutes::solo_changed_so_update_mute ()
{
	ENSURE_GUI_THREAD (*this, &EditorRoutes::solo_changed_so_update_mute)
	update_mute_display ();
}

void
EditorRoutes::show_tracks_with_regions_at_playhead ()
{
	boost::shared_ptr<RouteList> const r = _session->get_routes_with_regions_at (_session->transport_frame ());

	set<TimeAxisView*> show;
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		TimeAxisView* tav = _editor->axis_view_from_route (*i);
		if (tav) {
			show.insert (tav);
		}
	}

	suspend_redisplay ();
	
	TreeModel::Children rows = _model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView* tv = (*i)[_columns.tv];
		(*i)[_columns.visible] = (show.find (tv) != show.end());
	}

	resume_redisplay ();
	
}
