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
using namespace sigc;
using namespace ARDOUR;

/** Construct an EditorSummary.
 *  @param e Editor to represent.
 */
EditorSummary::EditorSummary (Editor* e)
	: _editor (e),
	  _session (0),
	  _x_scale (1),
	  _y_scale (1),
	  _last_playhead (-1),
	  _move_dragging (false),
	  _moved (false),
	  _zoom_dragging (false)
	  
{
	
}

/** Set the session.
 *  @param s Session.
 */
void
EditorSummary::set_session (Session* s)
{
	_session = s;

	Region::RegionPropertyChanged.connect (sigc::hide (mem_fun (*this, &EditorSummary::set_dirty)));

	_session->RegionRemoved.connect (sigc::hide (mem_fun (*this, &EditorSummary::set_dirty)));
	_session->EndTimeChanged.connect (mem_fun (*this, &EditorSummary::set_dirty));
	_session->StartTimeChanged.connect (mem_fun (*this, &EditorSummary::set_dirty));
	_editor->playhead_cursor->PositionChanged.connect (mem_fun (*this, &EditorSummary::playhead_position_changed));

	set_dirty ();
}

/** Handle an expose event.
 *  @param event Event from GTK.
 */
bool
EditorSummary::on_expose_event (GdkEventExpose* event)
{
	CairoWidget::on_expose_event (event);

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

	double const p = _editor->playhead_cursor->current_frame * _x_scale;
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
	if (_session == 0) {
		return;
	}

	/* background */
	
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* compute total height of all tracks */
	
	int h = 0;
	int max_height = 0;
	for (PublicEditor::TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		int const t = (*i)->effective_height ();
		h += t;
		max_height = max (max_height, t);
	}

	nframes_t const start = _session->current_start_frame ();
	_x_scale = static_cast<double> (_width) / (_session->current_end_frame() - start);
	_y_scale = static_cast<double> (_height) / h;

	/* tallest a region should ever be in the summary, in pixels */
	int const tallest_region_pixels = 4;

	if (max_height * _y_scale > tallest_region_pixels) {
		_y_scale = static_cast<double> (tallest_region_pixels) / max_height;

	}

	/* render regions */

	double y = 0;
	for (PublicEditor::TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		StreamView* s = (*i)->view ();

		if (s) {
			double const h = (*i)->effective_height () * _y_scale;
			cairo_set_line_width (cr, h);

			s->foreach_regionview (bind (
						       mem_fun (*this, &EditorSummary::render_region),
						       cr,
						       start,
						       y + h / 2
						       ));
			y += h;
		}
	}

}

/** Render a region for the summary.
 *  @param r Region view.
 *  @param cr Cairo context.
 *  @param start Frame offset that the summary starts at.
 *  @param y y coordinate to render at.
 */
void
EditorSummary::render_region (RegionView* r, cairo_t* cr, nframes_t start, double y) const
{
	uint32_t const c = r->get_fill_color ();
	cairo_set_source_rgb (cr, UINT_RGBA_R (c) / 255.0, UINT_RGBA_G (c) / 255.0, UINT_RGBA_B (c) / 255.0);
			
	cairo_move_to (cr, (r->region()->position() - start) * _x_scale, y);
	cairo_line_to (cr, ((r->region()->position() - start + r->region()->length())) * _x_scale, y);
	cairo_stroke (cr);
}

/** Set the summary so that just the overlays (viewbox, playhead etc.) will be re-rendered */
void
EditorSummary::set_overlays_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_overlays_dirty));
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

		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {

			/* secondary-modifier-click: locate playhead */
			if (_session) {
				_session->request_locate (ev->x / _x_scale + _session->current_start_frame());
			}

		} else {
		
			pair<double, double> xr;
			pair<double, double> yr;
			get_editor (&xr, &yr);
			
			if (xr.first <= ev->x && ev->x <= xr.second && yr.first <= ev->y && ev->y <= yr.second) {
				
				_start_editor_x = xr;
				_start_editor_y = yr;
				_start_mouse_x = ev->x;
				_start_mouse_y = ev->y;
				
				if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
					
					/* modifier-click inside the view rectangle: start a zoom drag */
					
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
					
				} else {
					
					/* ordinary click inside the view rectangle: start a move drag */
					
					_move_dragging = true;
					_moved = false;
					_editor->_dragging_playhead = true;
				}
				
			} else {
			
				/* click outside the view rectangle: centre the view around the mouse click */
				centre_on_click (ev);
			}
		}
	}
	
	return true;
}

void
EditorSummary::get_editor (pair<double, double>* x, pair<double, double>* y) const
{
	x->first = (_editor->leftmost_position () - _session->current_start_frame ()) * _x_scale;
	x->second = x->first + _editor->current_page_frames() * _x_scale;

	y->first = _editor->vertical_adjustment.get_value() * _y_scale;
	y->second = y->first + _editor->canvas_height () * _y_scale;
}

bool
EditorSummary::on_motion_notify_event (GdkEventMotion* ev)
{
	pair<double, double> xr = _start_editor_x;
	pair<double, double> yr = _start_editor_y;
	
	if (_move_dragging) {

		_moved = true;

		xr.first += ev->x - _start_mouse_x;
		xr.second += ev->x - _start_mouse_x;
		yr.first += ev->y - _start_mouse_y;
		yr.second += ev->y - _start_mouse_y;
		
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
EditorSummary::on_button_release_event (GdkEventButton* ev)
{
	if (_move_dragging && !_moved) {
		centre_on_click (ev);
	}

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

	double const amount = 8;
		
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		
		if (ev->direction == GDK_SCROLL_UP) {
			xr.first += amount;
			xr.second += amount;
		} else {
			xr.first -= amount;
			xr.second -= amount;
		}

	} else {
		
		if (ev->direction == GDK_SCROLL_DOWN) {
			yr.first += amount;
			yr.second += amount;
		} else {
			yr.first -= amount;
			yr.second -= amount;
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

		_editor->reset_x_origin (x.first / _x_scale);
		_editor->reset_y_origin (y.first / _y_scale);

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
	if (int (p * _x_scale) != int (_last_playhead)) {
		set_overlays_dirty ();
	}
}

	
