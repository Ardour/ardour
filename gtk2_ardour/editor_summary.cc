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
	  _pixmap (0),
	  _regions_dirty (true),
	  _width (512),
	  _height (64),
	  _x_scale (1),
	  _y_scale (1),
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

	set_dirty ();
}

/** Destroy */
EditorSummary::~EditorSummary ()
{
	if (_pixmap) {
		gdk_pixmap_unref (_pixmap);
	}
}

/** Handle an expose event.
 *  @param event Event from GTK.
 */
bool
EditorSummary::on_expose_event (GdkEventExpose* event)
{
	/* Render the regions pixmap */
	
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	Gdk::Rectangle r = exposure;
	Gdk::Rectangle content (0, 0, _width, _height);
	bool intersects;
	r.intersect (content, intersects);
	
	if (intersects) {

		GdkPixmap* p = get_pixmap (get_window()->gobj ());

		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			p,
 			r.get_x(),
 			r.get_y(),
 			r.get_x(),
 			r.get_y(),
 			r.get_width(),
 			r.get_height()
			);
	}

	/* Render the view rectangle */
	
	pair<double, double> x;
	pair<double, double> y;
	editor_view (&x, &y);
	
	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

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

	cairo_destroy (cr);
	
	return true;
}

/** @param drawable GDK drawable.
 *  @return pixmap for the regions.
 */
GdkPixmap *
EditorSummary::get_pixmap (GdkDrawable* drawable)
{
	if (_regions_dirty) {

		if (_pixmap) {
			gdk_pixmap_unref (_pixmap);
		}
		_pixmap = gdk_pixmap_new (drawable, _width, _height, -1);

		cairo_t* cr = gdk_cairo_create (_pixmap);
		render (cr);
		cairo_destroy (cr);

		_regions_dirty = false;
	}

	return _pixmap;
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
	int const tallest_region_pixels = 12;

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

/** Set the summary so that the whole thing will be re-rendered next time it is required */
void
EditorSummary::set_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_dirty));

	_regions_dirty = true;
	queue_draw ();
}

/** Set the summary so that just the view boundary markers will be re-rendered */
void
EditorSummary::set_bounds_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_bounds_dirty));
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
	req->height = 64;
}

/** Handle a size allocation.
 *  @param alloc GTK allocation.
 */
void
EditorSummary::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_width = alloc.get_width ();
	_height = alloc.get_height ();

	set_dirty ();
}

void
EditorSummary::centre_on_click (GdkEventButton* ev)
{
	nframes_t x = (ev->x / _x_scale) + _session->current_start_frame();
	nframes_t const xh = _editor->current_page_frames () / 2;
	if (x > xh) {
		x -= xh;
	} else {
		x = 0;
	}
	
	_editor->reset_x_origin (x);
	
	double y = ev->y / _y_scale;
	double const yh = _editor->canvas_height () / 2;
	if (y > yh) {
		y -= yh;
	} else {
		y = 0;
	}
	
	_editor->reset_y_origin (y);
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
		editor_view (&xr, &yr);
		
		if (xr.first <= ev->x && ev->x <= xr.second && yr.first <= ev->y && ev->y <= yr.second) {

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

				/* modifier-click inside the view rectangle: start a zoom drag */
				_zoom_position = NONE;

				double const x1 = xr.first + (xr.second - xr.first) * 0.33;
				double const x2 = xr.first + (xr.second - xr.first) * 0.67;

				if (ev->x < x1) {
					_zoom_position = LEFT;
				} else if (ev->x > x2) {
					_zoom_position = RIGHT;
				} else {
					_zoom_position = NONE;
				}
						
				if (_zoom_position != NONE) {
					_zoom_dragging = true;
					_mouse_x_start = ev->x;
					_width_start = xr.second - xr.first;
					_zoom_start = _editor->get_current_zoom ();
					_frames_start = _editor->leftmost_position ();
					_editor->_dragging_playhead = true;
				}
					
			} else {

				/* ordinary click inside the view rectangle: start a move drag */
				
				_move_dragging = true;
				_moved = false;
				_x_offset = ev->x - xr.first;
				_y_offset = ev->y - yr.first;
				_editor->_dragging_playhead = true;
			}
			
		} else {
			
			/* click outside the view rectangle: centre the view around the mouse click */
			centre_on_click (ev);
		}
	}

	return true;
}

void
EditorSummary::editor_view (pair<double, double>* x, pair<double, double>* y) const
{
	x->first = (_editor->leftmost_position () - _session->current_start_frame ()) * _x_scale;
	x->second = x->first + _editor->current_page_frames() * _x_scale;

	y->first = _editor->get_trackview_group_vertical_offset () * _y_scale;
	y->second = y->first + _editor->canvas_height () * _y_scale;
}

bool
EditorSummary::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_move_dragging) {

		_moved = true;
		_editor->reset_x_origin (((ev->x - _x_offset) / _x_scale) + _session->current_start_frame ());
		_editor->reset_y_origin ((ev->y - _y_offset) / _y_scale);
		return true;

	} else if (_zoom_dragging) {

		double const dx = ev->x - _mouse_x_start;

		nframes64_t rx = _frames_start;
		double f = 1;
		
		switch (_zoom_position) {
		case LEFT:
			f = 1 - (dx / _width_start);
			rx += (dx / _x_scale);
			break;
		case RIGHT:
			f = 1 + (dx / _width_start);
			break;
		case NONE:
			break;
		}

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
			   
			_editor->queue_visual_change (rx);
			_editor->queue_visual_change (_zoom_start * f);
		}
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
	editor_view (&xr, &yr);
		
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		
		double x = xr.first;
		
		if (ev->direction == GDK_SCROLL_UP) {
			x += 16;
		} else {
			x -= 16;
		}
		
		_editor->reset_x_origin (x / _x_scale);
		
	} else {
		
		double y = yr.first;
		
		if (ev->direction == GDK_SCROLL_DOWN) {
			y += 16;
		} else {
			y -= 16;
		}
		
		
		_editor->reset_y_origin (y / _y_scale);
	}
	
	return true;
}
