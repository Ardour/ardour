/*
    Copyright (C) 2009 Paul Davis

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

#include "ardour/session.h"

#include "canvas/debug.h"

#include "time_axis_view.h"
#include "streamview.h"
#include "editor_summary.h"
#include "gui_thread.h"
#include "editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "keyboard.h"
#include "editor_routes.h"
#include "editor_cursors.h"
#include "mouse_cursors.h"
#include "route_time_axis.h"

using namespace std;
using namespace ARDOUR;
using Gtkmm2ext::Keyboard;

/** Construct an EditorSummary.
 *  @param e Editor to represent.
 */
EditorSummary::EditorSummary (Editor* e)
	: EditorComponent (e),
	  _start (0),
	  _end (1),
	  _overhang_fraction (0.1),
	  _x_scale (1),
	  _track_height (16),
	  _last_playhead (-1),
	  _move_dragging (false),
	  _moved (false),
	  _view_rectangle_x (0, 0),
	  _view_rectangle_y (0, 0),
	  _zoom_dragging (false),
	  _old_follow_playhead (false)
{
	Region::RegionPropertyChanged.connect (region_property_connection, invalidator (*this), boost::bind (&CairoWidget::set_dirty, this), gui_context());
	Route::RemoteControlIDChange.connect (route_ctrl_id_connection, invalidator (*this), boost::bind (&CairoWidget::set_dirty, this), gui_context());
	_editor->playhead_cursor->PositionChanged.connect (position_connection, invalidator (*this), boost::bind (&EditorSummary::playhead_position_changed, this, _1), gui_context());

	add_events (Gdk::POINTER_MOTION_MASK|Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);
	set_flags (get_flags() | Gtk::CAN_FOCUS);
}

/** Connect to a session.
 *  @param s Session.
 */
void
EditorSummary::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	set_dirty ();

	/* Note: the EditorSummary already finds out about new regions from Editor::region_view_added
	 * (which attaches to StreamView::RegionViewAdded), and cut regions by the RegionPropertyChanged
	 * emitted when a cut region is added to the `cutlist' playlist.
	 */

	if (_session) {
		_session->StartTimeChanged.connect (_session_connections, invalidator (*this), boost::bind (&CairoWidget::set_dirty, this), gui_context());
		_session->EndTimeChanged.connect (_session_connections, invalidator (*this), boost::bind (&CairoWidget::set_dirty, this), gui_context());
	}
}

/** Render the required regions to a cairo context.
 *  @param cr Context.
 */
void
EditorSummary::render (cairo_t* cr)
{
	/* background (really just the dividing lines between tracks */

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, get_width(), get_height());
	cairo_fill (cr);

	if (_session == 0) {
		return;
	}

	/* compute start and end points for the summary */

	framecnt_t const session_length = _session->current_end_frame() - _session->current_start_frame ();
	double const theoretical_start = _session->current_start_frame() - session_length * _overhang_fraction;
	_start = theoretical_start > 0 ? theoretical_start : 0;
	_end = _session->current_end_frame() + session_length * _overhang_fraction;

	/* compute track height */
	int N = 0;
	for (TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		if (!(*i)->hidden()) {
			++N;
		}
	}

	if (N == 0) {
		_track_height = 16;
	} else {
		_track_height = (double) get_height() / N;
	}

	/* calculate x scale */
	if (_end != _start) {
		_x_scale = static_cast<double> (get_width()) / (_end - _start);
 	} else {
		_x_scale = 1;
	}

	/* render tracks and regions */

	double y = 0;
	for (TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->hidden()) {
			continue;
		}

		/* paint a non-bg colored strip to represent the track itself */

		cairo_set_source_rgb (cr, 0.2, 0.2, 0.2);
		cairo_set_line_width (cr, _track_height - 1);
		cairo_move_to (cr, 0, y + _track_height / 2);
		cairo_line_to (cr, get_width(), y + _track_height / 2);
		cairo_stroke (cr);

		StreamView* s = (*i)->view ();

		if (s) {
			cairo_set_line_width (cr, _track_height * 0.6);

			s->foreach_regionview (sigc::bind (
						       sigc::mem_fun (*this, &EditorSummary::render_region),
						       cr,
						       y + _track_height / 2
						       ));
		}

		y += _track_height;
	}

	/* start and end markers */

	cairo_set_line_width (cr, 1);
	cairo_set_source_rgb (cr, 1, 1, 0);

	const double p = (_session->current_start_frame() - _start) * _x_scale;
	cairo_move_to (cr, p, 0);
	cairo_line_to (cr, p, get_height());
	cairo_stroke (cr);

	double const q = (_session->current_end_frame() - _start) * _x_scale;
	cairo_move_to (cr, q, 0);
	cairo_line_to (cr, q, get_height());
	cairo_stroke (cr);

	/* Render the view rectangle.  If there is an editor visual pending, don't update
	   the view rectangle now --- wait until the expose event that we'll get after
	   the visual change.  This prevents a flicker.
	*/

	if (_editor->pending_visual_change.idle_handler_id < 0) {
		get_editor (&_view_rectangle_x, &_view_rectangle_y);
	}

	cairo_move_to (cr, _view_rectangle_x.first, _view_rectangle_y.first);
	cairo_line_to (cr, _view_rectangle_x.second, _view_rectangle_y.first);
	cairo_line_to (cr, _view_rectangle_x.second, _view_rectangle_y.second);
	cairo_line_to (cr, _view_rectangle_x.first, _view_rectangle_y.second);
	cairo_line_to (cr, _view_rectangle_x.first, _view_rectangle_y.first);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.25);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, 1);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
	cairo_stroke (cr);

	/* Playhead */

	cairo_set_line_width (cr, 1);
	/* XXX: colour should be set from configuration file */
	cairo_set_source_rgba (cr, 1, 0, 0, 1);

	const double ph= playhead_frame_to_position (_editor->playhead_cursor->current_frame());
	cairo_move_to (cr, ph, 0);
	cairo_line_to (cr, ph, get_height());
	cairo_stroke (cr);
	_last_playhead = ph;

}

/** Render a region for the summary.
 *  @param r Region view.
 *  @param cr Cairo context.
 *  @param y y coordinate to render at.
 */
void
EditorSummary::render_region (RegionView* r, cairo_t* cr, double y) const
{
	uint32_t const c = r->get_fill_color ();
	cairo_set_source_rgb (cr, UINT_RGBA_R (c) / 255.0, UINT_RGBA_G (c) / 255.0, UINT_RGBA_B (c) / 255.0);

	if (r->region()->position() > _start) {
		cairo_move_to (cr, (r->region()->position() - _start) * _x_scale, y);
	} else {
		cairo_move_to (cr, 0, y);
	}

	if ((r->region()->position() + r->region()->length()) > _start) {
		cairo_line_to (cr, ((r->region()->position() - _start + r->region()->length())) * _x_scale, y);
	} else {
		cairo_line_to (cr, 0, y);
	}

	cairo_stroke (cr);
}

/** Set the summary so that just the overlays (viewbox, playhead etc.) will be re-rendered */
void
EditorSummary::set_overlays_dirty ()
{
	ENSURE_GUI_THREAD (*this, &EditorSummary::set_overlays_dirty);
	queue_draw ();
}

/** Set the summary so that just the overlays (viewbox, playhead etc.) in a given area will be re-rendered */
void
EditorSummary::set_overlays_dirty (int x, int y, int w, int h)
{
	ENSURE_GUI_THREAD (*this, &EditorSummary::set_overlays_dirty);
	queue_draw_area (x, y, w, h);
}


/** Handle a size request.
 *  @param req GTK requisition
 */
void
EditorSummary::on_size_request (Gtk::Requisition *req)
{
	/* Use a dummy, small width and the actual height that we want */
	req->width = 64;
	req->height = 32;
}


void
EditorSummary::centre_on_click (GdkEventButton* ev)
{
	pair<double, double> xr;
	pair<double, double> yr;
	get_editor (&xr, &yr);

	double const w = xr.second - xr.first;
	double ex = ev->x - w / 2;
	if (ex < 0) {
		ex = 0;
	} else if ((ex + w) > get_width()) {
		ex = get_width() - w;
	}

	double const h = yr.second - yr.first;
	double ey = ev->y - h / 2;
	if (ey < 0) {
		ey = 0;
	} else if ((ey + h) > get_height()) {
		ey = get_height() - h;
	}

	set_editor (ex, ey);
}

bool 
EditorSummary::on_enter_notify_event (GdkEventCrossing*)
{
	grab_focus ();
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool 
EditorSummary::on_leave_notify_event (GdkEventCrossing*)
{
	/* there are no inferior/child windows, so any leave event means that
	   we're gone.
	*/
	Keyboard::magic_widget_drop_focus ();
	return false;
}

bool
EditorSummary::on_key_press_event (GdkEventKey* key)
{
	gint x, y;
	GtkAccelKey set_playhead_accel;
	if (gtk_accel_map_lookup_entry ("<Actions>/Editor/set-playhead", &set_playhead_accel)) {
		if (key->keyval == set_playhead_accel.accel_key && (int) key->state == set_playhead_accel.accel_mods) {
			if (_session) {
				get_pointer (x, y);
				_session->request_locate (_start + (framepos_t) x / _x_scale, _session->transport_rolling());
				return true;
			}
		}
	}

	return false;
}

bool
EditorSummary::on_key_release_event (GdkEventKey* key)
{

	GtkAccelKey set_playhead_accel;
	if (gtk_accel_map_lookup_entry ("<Actions>/Editor/set-playhead", &set_playhead_accel)) {
		if (key->keyval == set_playhead_accel.accel_key && (int) key->state == set_playhead_accel.accel_mods) {
			return true;
		}
	}
	return false;
}

/** Handle a button press.
 *  @param ev GTK event.
 */
bool
EditorSummary::on_button_press_event (GdkEventButton* ev)
{
	_old_follow_playhead = _editor->follow_playhead ();
	
	if (ev->button == 1) {

		pair<double, double> xr;
		pair<double, double> yr;
		get_editor (&xr, &yr);

		_start_editor_x = xr;
		_start_editor_y = yr;
		_start_mouse_x = ev->x;
		_start_mouse_y = ev->y;
		_start_position = get_position (ev->x, ev->y);

		if (_start_position != INSIDE && _start_position != BELOW_OR_ABOVE &&
		    _start_position != TO_LEFT_OR_RIGHT && _start_position != OTHERWISE_OUTSIDE
			) {

			/* start a zoom drag */

			_zoom_position = get_position (ev->x, ev->y);
			_zoom_dragging = true;
			_editor->_dragging_playhead = true;
			_editor->set_follow_playhead (false);

			if (suspending_editor_updates ()) {
				get_editor (&_pending_editor_x, &_pending_editor_y);
				_pending_editor_changed = false;
			}
			
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {

			/* secondary-modifier-click: locate playhead */
			if (_session) {
				_session->request_locate (ev->x / _x_scale + _start);
			}

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {

			centre_on_click (ev);

		} else {

			/* start a move drag */

			/* get the editor's state in case we are suspending updates */
			get_editor (&_pending_editor_x, &_pending_editor_y);
			_pending_editor_changed = false;

			_move_dragging = true;
			_moved = false;
			_editor->_dragging_playhead = true;
			_editor->set_follow_playhead (false);

			ArdourCanvas::checkpoint ("sum", "------------------ summary move drag starts.\n");
		}
	}

	return true;
}

/** @return true if we are currently suspending updates to the editor's viewport,
 *  which we do if configured to do so, and if in a drag of some kind.
 */
bool
EditorSummary::suspending_editor_updates () const
{
	return (!Config->get_update_editor_during_summary_drag () && (_zoom_dragging || _move_dragging));
}

/** Fill in x and y with the editor's current viewable area in summary coordinates */
void
EditorSummary::get_editor (pair<double, double>* x, pair<double, double>* y) const
{
	assert (x);
	assert (y);

	if (suspending_editor_updates ()) {

		/* We are dragging, and configured not to update the editor window during drags,
		   so just return where the editor will be when the drag finishes.
		*/
		   
		*x = _pending_editor_x;
		*y = _pending_editor_y;

	} else {

		/* Otherwise query the editor for its actual position */

		x->first = (_editor->leftmost_position () - _start) * _x_scale;
		x->second = x->first + _editor->current_page_frames() * _x_scale;
		
		y->first = editor_y_to_summary (_editor->vertical_adjustment.get_value ());
		y->second = editor_y_to_summary (_editor->vertical_adjustment.get_value () + _editor->visible_canvas_height());
	}
}

/** Get an expression of the position of a point with respect to the view rectangle */
EditorSummary::Position
EditorSummary::get_position (double x, double y) const
{
	/* how close the mouse has to be to the edge of the view rectangle to be considered `on it',
	   in pixels */

	int x_edge_size = (_view_rectangle_x.second - _view_rectangle_x.first) / 4;
	x_edge_size = min (x_edge_size, 8);
	x_edge_size = max (x_edge_size, 1);

	int y_edge_size = (_view_rectangle_y.second - _view_rectangle_y.first) / 4;
	y_edge_size = min (y_edge_size, 8);
	y_edge_size = max (y_edge_size, 1);

	bool const near_left = (std::abs (x - _view_rectangle_x.first) < x_edge_size);
	bool const near_right = (std::abs (x - _view_rectangle_x.second) < x_edge_size);
 	bool const near_top = (std::abs (y - _view_rectangle_y.first) < y_edge_size);
 	bool const near_bottom = (std::abs (y - _view_rectangle_y.second) < y_edge_size);
	bool const within_x = _view_rectangle_x.first < x && x < _view_rectangle_x.second;
	bool const within_y = _view_rectangle_y.first < y && y < _view_rectangle_y.second;

	if (near_left && near_top) {
		return LEFT_TOP;
	} else if (near_left && near_bottom) {
		return LEFT_BOTTOM;
	} else if (near_right && near_top) {
		return RIGHT_TOP;
	} else if (near_right && near_bottom) {
		return RIGHT_BOTTOM;
	} else if (near_left && within_y) {
		return LEFT;
	} else if (near_right && within_y) {
		return RIGHT;
	} else if (near_top && within_x) {
		return TOP;
	} else if (near_bottom && within_x) {
		return BOTTOM;
	} else if (within_x && within_y) {
		return INSIDE;
	} else if (within_x) {
		return BELOW_OR_ABOVE;
	} else if (within_y) {
		return TO_LEFT_OR_RIGHT;
	} else {
		return OTHERWISE_OUTSIDE;
	}
}

void
EditorSummary::set_cursor (Position p)
{
	switch (p) {
	case LEFT:
		get_window()->set_cursor (*_editor->_cursors->resize_left);
		break;
	case LEFT_TOP:
		get_window()->set_cursor (*_editor->_cursors->resize_top_left);
		break;
	case TOP:
		get_window()->set_cursor (*_editor->_cursors->resize_top);
		break;
	case RIGHT_TOP:
		get_window()->set_cursor (*_editor->_cursors->resize_top_right);
		break;
	case RIGHT:
		get_window()->set_cursor (*_editor->_cursors->resize_right);
		break;
	case RIGHT_BOTTOM:
		get_window()->set_cursor (*_editor->_cursors->resize_bottom_right);
		break;
	case BOTTOM:
		get_window()->set_cursor (*_editor->_cursors->resize_bottom);
		break;
	case LEFT_BOTTOM:
		get_window()->set_cursor (*_editor->_cursors->resize_bottom_left);
		break;
	case INSIDE:
		get_window()->set_cursor (*_editor->_cursors->move);
		break;
	case TO_LEFT_OR_RIGHT:
		get_window()->set_cursor (*_editor->_cursors->expand_left_right);
		break;
	case BELOW_OR_ABOVE:
		get_window()->set_cursor (*_editor->_cursors->expand_up_down);
		break;
	default:
		get_window()->set_cursor ();
		break;
	}
}

bool
EditorSummary::on_motion_notify_event (GdkEventMotion* ev)
{
	pair<double, double> xr = _start_editor_x;
	pair<double, double> yr = _start_editor_y;
	double x = _start_editor_x.first;
	double y = _start_editor_y.first;

	if (_move_dragging) {

		_moved = true;

		/* don't alter x if we clicked outside and above or below the viewbox */
		if (_start_position == INSIDE || _start_position == TO_LEFT_OR_RIGHT || _start_position == OTHERWISE_OUTSIDE) {
			x += ev->x - _start_mouse_x;
		}

		/* don't alter y if we clicked outside and to the left or right of the viewbox */
		if (_start_position == INSIDE || _start_position == BELOW_OR_ABOVE) {
			y += ev->y - _start_mouse_y;
		}

		if (x < 0) {
			x = 0;
		}

		if (y < 0) {
			y = 0;
		}

		set_editor (x, y);
		// set_cursor (_start_position);

	} else if (_zoom_dragging) {

		double const dx = ev->x - _start_mouse_x;
		double const dy = ev->y - _start_mouse_y;

		if (_zoom_position == LEFT || _zoom_position == LEFT_TOP || _zoom_position == LEFT_BOTTOM) {
			xr.first += dx;
		} else if (_zoom_position == RIGHT || _zoom_position == RIGHT_TOP || _zoom_position == RIGHT_BOTTOM) {
			xr.second += dx;
		}

		if (_zoom_position == TOP || _zoom_position == LEFT_TOP || _zoom_position == RIGHT_TOP) {
			yr.first += dy;
		} else if (_zoom_position == BOTTOM || _zoom_position == LEFT_BOTTOM || _zoom_position == RIGHT_BOTTOM) {
			yr.second += dy;
		}

		set_overlays_dirty ();
		set_cursor (_zoom_position);
		set_editor (xr, yr);

	} else {

		set_cursor (get_position (ev->x, ev->y));

	}

	return true;
}

bool
EditorSummary::on_button_release_event (GdkEventButton*)
{
	bool const was_suspended = suspending_editor_updates ();
	
	_move_dragging = false;
	_zoom_dragging = false;
	_editor->_dragging_playhead = false;
	_editor->set_follow_playhead (_old_follow_playhead, false);

	if (was_suspended && _pending_editor_changed) {
		set_editor (_pending_editor_x, _pending_editor_y);
	}
		
	return true;
}

bool
EditorSummary::on_scroll_event (GdkEventScroll* ev)
{
	/* mouse wheel */

	pair<double, double> xr;
	pair<double, double> yr;
	get_editor (&xr, &yr);
	double x = xr.first;
	double y = yr.first;

	double amount = 8;

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
		amount = 64;
	} else if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		amount = 1;
	}

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {

		/* secondary-wheel == left-right scrolling */

		if (ev->direction == GDK_SCROLL_UP) {
			x -= amount;
		} else if (ev->direction == GDK_SCROLL_DOWN) {
			x += amount;
		}

	} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

		/* primary-wheel == zoom */
		
		if (ev->direction == GDK_SCROLL_UP) {
			_editor->temporal_zoom_step (false);
		} else {
			_editor->temporal_zoom_step (true);
		}

	} else {

		if (ev->direction == GDK_SCROLL_DOWN) {
			y += amount;
		} else if (ev->direction == GDK_SCROLL_UP) {
			y -= amount;
		} else if (ev->direction == GDK_SCROLL_LEFT) {
			x -= amount;
		} else if (ev->direction == GDK_SCROLL_RIGHT) {
			x += amount;
		}
	}

	set_editor (x, y);
	return true;
}

/** Set the editor to display a x range with the left at a given position
 *  and a y range with the top at a given position.
 *  x and y parameters are specified in summary coordinates.
 *  Zoom is not changed in either direction.
 */
void
EditorSummary::set_editor (double const x, double const y)
{
	if (_editor->pending_visual_change.idle_handler_id >= 0 && _editor->pending_visual_change.being_handled == true) {

		/* As a side-effect, the Editor's visual change idle handler processes
		   pending GTK events.  Hence this motion notify handler can be called
		   in the middle of a visual change idle handler, and if this happens,
		   the queue_visual_change calls below modify the variables that the
		   idle handler is working with.  This causes problems.  Hence this
		   check.  It ensures that we won't modify the pending visual change
		   while a visual change idle handler is in progress.  It's not perfect,
		   as it also means that we won't change these variables if an idle handler
		   is merely pending but not executing.  But c'est la vie.
		*/
		
		return;
	}

	set_editor_x (x);
	set_editor_y (y);
}

/** Set the editor to display a given x range and a y range with the top at a given position.
 *  The editor's x zoom is adjusted if necessary, but the y zoom is not changed.
 *  x and y parameters are specified in summary coordinates.
 */
void
EditorSummary::set_editor (pair<double,double> const x, double const y)
{
	if (_editor->pending_visual_change.idle_handler_id >= 0) {
		/* see comment in other set_editor () */
		return;
	}

	set_editor_x (x);
	set_editor_y (y);
}

/** Set the editor to display given x and y ranges.  x zoom and track heights are
 *  adjusted if necessary.
 *  x and y parameters are specified in summary coordinates.
 */
void
EditorSummary::set_editor (pair<double,double> const x, pair<double, double> const y)
{
	if (_editor->pending_visual_change.idle_handler_id >= 0) {
		/* see comment in other set_editor () */
		return;
	}

	set_editor_x (x);
	set_editor_y (y);
}

/** Set the left of the x range visible in the editor.
 *  Caller should have checked that Editor::pending_visual_change.idle_handler_id is < 0
 *  @param x new x left position in summary coordinates.
 */
void
EditorSummary::set_editor_x (double x)
{
	if (x < 0) {
		x = 0;
	}

	if (suspending_editor_updates ()) {
		double const w = _pending_editor_x.second - _pending_editor_x.first;
		_pending_editor_x.first = x;
		_pending_editor_x.second = x + w;
		_pending_editor_changed = true;
		set_dirty ();
	} else {
		_editor->reset_x_origin (x / _x_scale + _start);
	}
}

/** Set the x range visible in the editor.
 *  Caller should have checked that Editor::pending_visual_change.idle_handler_id is < 0
 *  @param x new x range in summary coordinates.
 */
void
EditorSummary::set_editor_x (pair<double, double> x)
{
	if (x.first < 0) {
		x.first = 0;
	}

	if (x.second < 0) {
		x.second = x.first + 1;
	}

	if (suspending_editor_updates ()) {
		_pending_editor_x = x;
		_pending_editor_changed = true;
		set_dirty ();
	} else {
		_editor->reset_x_origin (x.first / _x_scale + _start);
		
		double const nx = (
			((x.second - x.first) / _x_scale) /
			_editor->sample_to_pixel (_editor->current_page_frames())
			);
		
		if (nx != _editor->get_current_zoom ()) {
			_editor->reset_zoom (nx);
		}
	}
}

/** Set the top of the y range visible in the editor.
 *  Caller should have checked that Editor::pending_visual_change.idle_handler_id is < 0
 *  @param y new editor top in summary coodinates.
 */
void
EditorSummary::set_editor_y (double const y)
{
	double y1 = summary_y_to_editor (y);
	double const eh = _editor->visible_canvas_height();
	double y2 = y1 + eh;

	double const full_editor_height = _editor->_full_canvas_height;

	if (y2 > full_editor_height) {
		y1 -= y2 - full_editor_height;
	}

	if (y1 < 0) {
		y1 = 0;
	}

	if (suspending_editor_updates ()) {
		double const h = _pending_editor_y.second - _pending_editor_y.first;
		_pending_editor_y.first = y;
		_pending_editor_y.second = y + h;
		_pending_editor_changed = true;
		set_dirty ();
	} else {
		_editor->reset_y_origin (y1);
	}
}

/** Set the y range visible in the editor.  This is achieved by scaling track heights,
 *  if necessary.
 *  Caller should have checked that Editor::pending_visual_change.idle_handler_id is < 0
 *  @param y new editor range in summary coodinates.
 */
void
EditorSummary::set_editor_y (pair<double, double> const y)
{
	if (suspending_editor_updates ()) {
		_pending_editor_y = y;
		_pending_editor_changed = true;
		set_dirty ();
		return;
	}
	
	/* Compute current height of tracks between y.first and y.second.  We add up
	   the total height into `total_height' and the height of complete tracks into
	   `scale height'.
	*/

	/* Copy of target range for use below */
	pair<double, double> yc = y;
	/* Total height of all tracks */
	double total_height = 0;
	/* Height of any parts of tracks that aren't fully in the desired range */
	double partial_height = 0;
	/* Height of any tracks that are fully in the desired range */
	double scale_height = 0;

	_editor->_routes->suspend_redisplay ();

	for (TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->hidden()) {
			continue;
		}

		double const h = (*i)->effective_height ();
		total_height += h;

		if (yc.first > 0 && yc.first < _track_height) {
			partial_height += (_track_height - yc.first) * h / _track_height;
		} else if (yc.first <= 0 && yc.second >= _track_height) {
			scale_height += h;
		} else if (yc.second > 0 && yc.second < _track_height) {
			partial_height += yc.second * h / _track_height;
			break;
		}

		yc.first -= _track_height;
		yc.second -= _track_height;
	}

	/* Height that we will use for scaling; use the whole editor height unless there are not
	   enough tracks to fill it.
	*/
	double const ch = min (total_height, _editor->visible_canvas_height());

	/* hence required scale factor of the complete tracks to fit the required y range;
	   the amount of space they should take up divided by the amount they currently take up.
	*/
	double const scale = (ch - partial_height) / scale_height;

	yc = y;

	/* Scale complete tracks within the range to make it fit */

	for (TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->hidden()) {
			continue;
		}

		if (yc.first <= 0 && yc.second >= _track_height) {
			(*i)->set_height (max (TimeAxisView::preset_height (HeightSmall), (uint32_t) ((*i)->effective_height() * scale)));
		}

		yc.first -= _track_height;
		yc.second -= _track_height;
	}

	_editor->_routes->resume_redisplay ();

        set_editor_y (y.first);
}

void
EditorSummary::playhead_position_changed (framepos_t p)
{
	int const o = int (_last_playhead);
	int const n = int (playhead_frame_to_position (p));
	if (_session && o != n) {
		int a = max(2, min (o, n));
		int b = max (o, n);
		set_overlays_dirty (a - 2, 0, b + 2, get_height ());
	}
}

double
EditorSummary::summary_y_to_editor (double y) const
{
	double ey = 0;
	for (TrackViewList::const_iterator i = _editor->track_views.begin (); i != _editor->track_views.end(); ++i) {

		if ((*i)->hidden()) {
			continue;
		}

		double const h = (*i)->effective_height ();
		if (y < _track_height) {
			/* in this track */
			return ey + y * h / _track_height;
		}

		ey += h;
		y -= _track_height;
	}

	return ey;
}

double
EditorSummary::editor_y_to_summary (double y) const
{
	double sy = 0;
	for (TrackViewList::const_iterator i = _editor->track_views.begin (); i != _editor->track_views.end(); ++i) {

		if ((*i)->hidden()) {
			continue;
		}

		double const h = (*i)->effective_height ();
		if (y < h) {
			/* in this track */
			return sy + y * _track_height / h;
		}

		sy += _track_height;
		y -= h;
	}

	return sy;
}

void
EditorSummary::routes_added (list<RouteTimeAxisView*> const & r)
{
	for (list<RouteTimeAxisView*>::const_iterator i = r.begin(); i != r.end(); ++i) {
		/* Connect to gui_changed() on the route so that we know when their colour has changed */
		(*i)->route()->gui_changed.connect (*this, invalidator (*this), boost::bind (&EditorSummary::route_gui_changed, this, _1), gui_context ());
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> ((*i)->route ());
		if (tr) {
			tr->PlaylistChanged.connect (*this, invalidator (*this), boost::bind (&CairoWidget::set_dirty, this), gui_context ());
		}
	}

	set_dirty ();
}

void
EditorSummary::route_gui_changed (string c)
{
	if (c == "color") {
		set_dirty ();
	}
}

double
EditorSummary::playhead_frame_to_position (framepos_t t) const
{
	return (t - _start) * _x_scale;
}

framepos_t
EditorSummary::position_to_playhead_frame_to_position (double pos) const
{
	return _start  + (pos * _x_scale);
}
