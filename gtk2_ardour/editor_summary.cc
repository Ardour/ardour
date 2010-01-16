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
#include "time_axis_view.h"
#include "streamview.h"
#include "editor_summary.h"
#include "gui_thread.h"
#include "editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "keyboard.h"

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
	  _y_scale (1),
	  _last_playhead (-1),
	  _move_dragging (false),
	  _moved (false),
	  _zoom_dragging (false)

{
	Region::RegionPropertyChanged.connect (region_property_connection, boost::bind (&CairoWidget::set_dirty, this), gui_context());
	_editor->playhead_cursor->PositionChanged.connect (position_connection, ui_bind (&EditorSummary::playhead_position_changed, this, _1), gui_context());
}

/** Connect to a session.
 *  @param s Session.
 */
void
EditorSummary::set_session (Session* s)
{
	EditorComponent::set_session (s);

	set_dirty ();

	if (_session) {
		_session->RegionRemoved.connect (_session_connections, boost::bind (&EditorSummary::set_dirty, this), gui_context());
		_session->StartTimeChanged.connect (_session_connections, boost::bind (&EditorSummary::set_dirty, this), gui_context());
		_session->EndTimeChanged.connect (_session_connections, boost::bind (&EditorSummary::set_dirty, this), gui_context());
	}
}

/** Handle an expose event.
 *  @param event Event from GTK.
 */
bool
EditorSummary::on_expose_event (GdkEventExpose* event)
{
	CairoWidget::on_expose_event (event);

	if (_session == 0) {
		return false;
	}

	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	/* Render the view rectangle */

	pair<double, double> x;
	pair<double, double> y;
	get_editor (&x, &y);

	cairo_move_to (cr, x.first, y.first);
	cairo_line_to (cr, x.second, y.first);
	cairo_line_to (cr, x.second, y.second);
	cairo_line_to (cr, x.first, y.second);
	cairo_line_to (cr, x.first, y.first);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.25);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, 1);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
	cairo_stroke (cr);

	/* Playhead */

	cairo_set_line_width (cr, 1);
	/* XXX: colour should be set from configuration file */
	cairo_set_source_rgba (cr, 1, 0, 0, 1);

	double const p = (_editor->playhead_cursor->current_frame - _start) * _x_scale;
	cairo_move_to (cr, p, 0);
	cairo_line_to (cr, p, _height);
	cairo_stroke (cr);
	_last_playhead = p;

	cairo_destroy (cr);

	return true;
}

/** Render the required regions to a cairo context.
 *  @param cr Context.
 */
void
EditorSummary::render (cairo_t* cr)
{
	/* background */

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	if (_session == 0) {
		return;
	}

	/* compute start and end points for the summary */
	
	nframes_t const session_length = _session->current_end_frame() - _session->current_start_frame ();
	double const theoretical_start = _session->current_start_frame() - session_length * _overhang_fraction;
	_start = theoretical_start > 0 ? theoretical_start : 0;
	_end = _session->current_end_frame() + session_length * _overhang_fraction;

	/* compute total height of all tracks */

	int h = 0;
	int max_height = 0;
	for (TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		int const t = (*i)->effective_height ();
		h += t;
		max_height = max (max_height, t);
	}

	_x_scale = static_cast<double> (_width) / (_end - _start);
	_y_scale = static_cast<double> (_height) / h;

	/* tallest a region should ever be in the summary, in pixels */
	int const tallest_region_pixels = _height / 16;

	if (max_height * _y_scale > tallest_region_pixels) {
		_y_scale = static_cast<double> (tallest_region_pixels) / max_height;
	}

	/* render regions */

	double y = 0;
	for (TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		StreamView* s = (*i)->view ();

		if (s) {
			double const h = (*i)->effective_height () * _y_scale;
			cairo_set_line_width (cr, h);

			s->foreach_regionview (sigc::bind (
						       sigc::mem_fun (*this, &EditorSummary::render_region),
						       cr,
						       y + h / 2
						       ));
			y += h;
		}
	}

	/* start and end markers */

	cairo_set_line_width (cr, 1);
	cairo_set_source_rgb (cr, 1, 1, 0);

	double const p = (_session->current_start_frame() - _start) * _x_scale;
	cairo_move_to (cr, p, 0);
	cairo_line_to (cr, p, _height);
	cairo_stroke (cr);

	double const q = (_session->current_end_frame() - _start) * _x_scale;
	cairo_move_to (cr, q, 0);
	cairo_line_to (cr, q, _height);
	cairo_stroke (cr);
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
	ENSURE_GUI_THREAD (*this, &EditorSummary::set_overlays_dirty)
	queue_draw ();
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
	double const h = yr.second - yr.first;

	xr.first = ev->x - w / 2;
	xr.second = ev->x + w / 2;
	yr.first = ev->y - h / 2;
	yr.second = ev->y + h / 2;

	if (xr.first < 0) {
		xr.first = 0;
		xr.second = w;
	} else if (xr.second > _width) {
		xr.second = _width;
		xr.first = _width - w;
	}

	if (yr.first < 0) {
		yr.first = 0;
		yr.second = h;
	} else if (yr.second > _height) {
		yr.second = _height;
		yr.first = _height - h;
	}

	set_editor (xr, yr);
}

/** Handle a button press.
 *  @param ev GTK event.
 */
bool
EditorSummary::on_button_press_event (GdkEventButton* ev)
{
	if (ev->button == 1) {

		pair<double, double> xr;
		pair<double, double> yr;
		get_editor (&xr, &yr);

		_start_editor_x = xr;
		_start_editor_y = yr;
		_start_mouse_x = ev->x;
		_start_mouse_y = ev->y;

		if (
			_start_editor_x.first <= _start_mouse_x && _start_mouse_x <= _start_editor_x.second &&
			_start_editor_y.first <= _start_mouse_y && _start_mouse_y <= _start_editor_y.second
			) {

			_start_position = IN_VIEWBOX;

		} else if (_start_editor_x.first <= _start_mouse_x && _start_mouse_x <= _start_editor_x.second) {

			_start_position = BELOW_OR_ABOVE_VIEWBOX;

		} else {

			_start_position = TO_LEFT_OR_RIGHT_OF_VIEWBOX;
		}

		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			/* primary-modifier-click: start a zoom drag */

			double const hx = (xr.first + xr.second) * 0.5;
			_zoom_left = ev->x < hx;
			_zoom_dragging = true;
			_editor->_dragging_playhead = true;


			/* In theory, we could support vertical dragging, which logically
			   might scale track heights in order to make the editor reflect
			   the dragged viewbox.  However, having tried this:
			   a) it's hard to do
			   b) it's quite slow
			   c) it doesn't seem particularly useful, especially with the
			   limited height of the summary

			   So at the moment we don't support that...
			*/


		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {

			/* secondary-modifier-click: locate playhead */
			if (_session) {
				_session->request_locate (ev->x / _x_scale + _start);
			}

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {

			centre_on_click (ev);

		} else {

			/* ordinary click: start a move drag */

			_move_dragging = true;
			_moved = false;
			_editor->_dragging_playhead = true;
		}
	}

	return true;
}

void
EditorSummary::get_editor (pair<double, double>* x, pair<double, double>* y) const
{
	x->first = (_editor->leftmost_position () - _start) * _x_scale;
	x->second = x->first + _editor->current_page_frames() * _x_scale;

	y->first = _editor->vertical_adjustment.get_value() * _y_scale;
	y->second = y->first + (_editor->canvas_height () - _editor->get_canvas_timebars_vsize()) * _y_scale;
}

bool
EditorSummary::on_motion_notify_event (GdkEventMotion* ev)
{
	pair<double, double> xr = _start_editor_x;
	pair<double, double> yr = _start_editor_y;

	if (_move_dragging) {

		_moved = true;

		/* don't alter x if we clicked outside and above or below the viewbox */
		if (_start_position == IN_VIEWBOX || _start_position == TO_LEFT_OR_RIGHT_OF_VIEWBOX) {
			xr.first += ev->x - _start_mouse_x;
			xr.second += ev->x - _start_mouse_x;
		}

		/* don't alter y if we clicked outside and to the left or right of the viewbox */
		if (_start_position == IN_VIEWBOX || _start_position == BELOW_OR_ABOVE_VIEWBOX) {
			yr.first += ev->y - _start_mouse_y;
			yr.second += ev->y - _start_mouse_y;
		}

		if (xr.first < 0) {
			xr.second -= xr.first;
			xr.first = 0;
		}

		if (yr.first < 0) {
			yr.second -= yr.first;
			yr.first = 0;
		}

		set_editor (xr, yr);

	} else if (_zoom_dragging) {

		double const dx = ev->x - _start_mouse_x;

		if (_zoom_left) {
			xr.first += dx;
		} else {
			xr.second += dx;
		}

		set_editor (xr, yr);
	}

	return true;
}

bool
EditorSummary::on_button_release_event (GdkEventButton*)
{
	_move_dragging = false;
	_zoom_dragging = false;
	_editor->_dragging_playhead = false;
	return true;
}

bool
EditorSummary::on_scroll_event (GdkEventScroll* ev)
{
	/* mouse wheel */

	pair<double, double> xr;
	pair<double, double> yr;
	get_editor (&xr, &yr);

	double amount = 8;

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
		amount = 64;
	} else if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		amount = 1;
	}

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

		/* primary-wheel == left-right scrolling */

		if (ev->direction == GDK_SCROLL_UP) {
			xr.first += amount;
			xr.second += amount;
		} else if (ev->direction == GDK_SCROLL_DOWN) {
			xr.first -= amount;
			xr.second -= amount;
		}

	} else {

		if (ev->direction == GDK_SCROLL_DOWN) {
			yr.first += amount;
			yr.second += amount;
		} else if (ev->direction == GDK_SCROLL_UP) {
			yr.first -= amount;
			yr.second -= amount;
		} else if (ev->direction == GDK_SCROLL_LEFT) {
			xr.first -= amount;
			xr.second -= amount;
		} else if (ev->direction == GDK_SCROLL_RIGHT) {
			xr.first += amount;
			xr.second += amount;
		}
	}

	set_editor (xr, yr);
	return true;
}

void
EditorSummary::set_editor (pair<double,double> const & x, pair<double, double> const & y)
{
	if (_editor->pending_visual_change.idle_handler_id < 0) {

		/* As a side-effect, the Editor's visual change idle handler processes
		   pending GTK events.  Hence this motion notify handler can be called
		   in the middle of a visual change idle handler, and if this happens,
		   the queue_visual_change calls below modify the variables that the
		   idle handler is working with.  This causes problems.  Hence the
		   check above.  It ensures that we won't modify the pending visual change
		   while a visual change idle handler is in progress.  It's not perfect,
		   as it also means that we won't change these variables if an idle handler
		   is merely pending but not executing.  But c'est la vie.
		*/

		/* proposed bottom of the editor with the requested position */
		double const pb = y.second / _y_scale;

		/* bottom of the canvas */
		double const ch = _editor->full_canvas_height - _editor->canvas_timebars_vsize;

		/* requested y position */
		double ly = y.first / _y_scale;

		/* clamp y position so as not to go off the bottom */
		if (pb > ch) {
			ly -= (pb - ch);
		}

		if (ly < 0) {
			ly = 0;
		}

		_editor->reset_x_origin (x.first / _x_scale + _start);
		_editor->reset_y_origin (ly);

		double const nx = (
			((x.second - x.first) / _x_scale) /
			_editor->frame_to_unit (_editor->current_page_frames())
			);

		if (nx != _editor->get_current_zoom ()) {
			_editor->reset_zoom (nx);
		}
	}
}

void
EditorSummary::playhead_position_changed (nframes64_t p)
{
	if (_session && int (p * _x_scale) != int (_last_playhead)) {
		set_overlays_dirty ();
	}
}


