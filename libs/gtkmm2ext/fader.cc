/*
    Copyright (C) 2014 Waves Audio Ltd.

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

    $Id: fastmeter.h 570 2006-06-07 21:21:21Z sampo $
*/


#include <iostream>

#include "pbd/stacktrace.h"

#include "gtkmm2ext/fader.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

#define CORNER_RADIUS 4
#define CORNER_SIZE   2
#define CORNER_OFFSET 1
#define FADER_RESERVE 5


static void get_closest_point_on_line(double xa, double ya, double xb, double yb, double xp, double yp, double& xl, double& yl )
{
	// Storing vector A->B
    double a_to_b_x = xb - xa;
	double a_to_b_y = yb - ya;
    
	// Storing vector A->P
    double a_to_p_x = xp - xa;
	double a_to_p_y = yp - ya;
    

    // Basically finding the squared magnitude
    // of a_to_b
    double atb2 = a_to_b_x * a_to_b_x + a_to_b_y * a_to_b_y;
 
    // The dot product of a_to_p and a_to_b
    double atp_dot_atb = a_to_p_x * a_to_b_x + a_to_p_y * a_to_b_y;
    
    // The normalized "distance" from a to
    // your closest point
    double t = atp_dot_atb / atb2;
    
    // The vector perpendicular to a_to_b;
    // This step can also be combined with the next
	double perpendicular_x = -a_to_b_y;
	double perpendicular_y = a_to_b_x;

    // Finding Q, the point "in the right direction"
    // If you want a mess, you can also combine this
    // with the next step.
	double xq = xp + perpendicular_x;
	double yq = yp + perpendicular_y;

    // Add the distance to A, moving
    // towards B
    double x = xa + a_to_b_x * t;
    double y = ya + a_to_b_y * t;

	if ((xa != xb)) {
		if ((x < xa) && (x < xb)) {
			if (xa < xb) {
				x = xa;
				y = ya;
			} else {
				x = xb;
				y = yb;
			}
		} else if ((x > xa) && (x > xb)) {
			if (xb > xa) {
				x = xb;
				y = yb;
			} else {
				x = xa;
				y = ya;
			}
		}
	} else {
		if ((y < ya) && (y < yb)) {
			if (ya < yb) {
				x = xa;
				y = ya;
			} else {
				x = xb;
				y = yb;
			}
		} else if ((y > ya) && (y > yb)) {
			if (yb > ya) {
				x = xb;
				y = yb;
			} else {
				x = xa;
				y = ya;
			}
		}
	}

	xl = x;
	yl = y;
}

Fader::Fader (Gtk::Adjustment& adj,
			  const Glib::RefPtr<Gdk::Pixbuf>& face_pixbuf,
			  const Glib::RefPtr<Gdk::Pixbuf>& active_face_pixbuf,
			  const Glib::RefPtr<Gdk::Pixbuf>& underlay_pixbuf,
			  const Glib::RefPtr<Gdk::Pixbuf>& handle_pixbuf,
			  const Glib::RefPtr<Gdk::Pixbuf>& active_handle_pixbuf,
			  int min_pos_x, 
			  int min_pos_y,
			  int max_pos_x,
			  int max_pos_y,
			  bool read_only)
	: adjustment (adj)
	, _face_pixbuf (face_pixbuf)
	, _active_face_pixbuf (active_face_pixbuf)
	, _underlay_pixbuf (underlay_pixbuf)
	, _handle_pixbuf (handle_pixbuf)
	, _active_handle_pixbuf (active_handle_pixbuf)
	, _min_pos_x (min_pos_x)
	, _min_pos_y (min_pos_y)
	, _max_pos_x (max_pos_x)
	, _max_pos_y (max_pos_y)
	, _default_value (adjustment.get_value())
	, _dragging (false)
	, _read_only (read_only)
	, _grab_window (0)
	, _touch_cursor (0)
{
	update_unity_position ();

	if (!_read_only) {
		add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);
	}

	adjustment.signal_value_changed().connect (mem_fun (*this, &Fader::adjustment_changed));
	adjustment.signal_changed().connect (mem_fun (*this, &Fader::adjustment_changed));
    CairoWidget::set_size_request(_face_pixbuf->get_width(), _face_pixbuf->get_height());
}

Fader::~Fader ()
{
	if (_touch_cursor) {
		delete _touch_cursor;
	}
}

void
Fader::set_touch_cursor (const Glib::RefPtr<Gdk::Pixbuf>& touch_cursor)
{
	_touch_cursor = new Gdk::Cursor (Gdk::Display::get_default(), touch_cursor, 12, 12);
}

void
Fader::render (cairo_t* cr, cairo_rectangle_t*)
{
	get_handle_position (_last_drawn_x, _last_drawn_y);

	if (_underlay_pixbuf != 0) {
	    cairo_rectangle (cr, 0, 0, get_width(), get_height());
		gdk_cairo_set_source_pixbuf (cr,
									 _underlay_pixbuf->gobj(),
									 _last_drawn_x - (int)(_underlay_pixbuf->get_width()/2.0 + 0.5),
									 _last_drawn_y - (int)(_underlay_pixbuf->get_height()/2.0 + 0.5));
	    cairo_fill (cr);
	}

	cairo_rectangle (cr, 0, 0, get_width(), get_height());
	gdk_cairo_set_source_pixbuf (cr, 
								 ((get_state () == Gtk::STATE_ACTIVE) && (_active_face_pixbuf != 0)) ? 
									_active_face_pixbuf->gobj() : 
									_face_pixbuf->gobj(),
								 0,
								 0);
	cairo_fill (cr);

    cairo_rectangle (cr, 0, 0, get_width(), get_height());
	if (_dragging) {
		gdk_cairo_set_source_pixbuf (cr,
									 _active_handle_pixbuf->gobj(),
									 _last_drawn_x - (int)(_active_handle_pixbuf->get_width()/2.0 + 0.5),
									 _last_drawn_y - (int)(_active_handle_pixbuf->get_height()/2.0 + 0.5));
	} else {
		gdk_cairo_set_source_pixbuf (cr,
									 _handle_pixbuf->gobj(),
									 _last_drawn_x - (int)(_handle_pixbuf->get_width()/2.0 + 0.5),
									 _last_drawn_y - (int)(_handle_pixbuf->get_height()/2.0 + 0.5));
	}
    cairo_fill (cr);
}

void
Fader::on_size_request (GtkRequisition* req)
{
	req->width = _face_pixbuf->get_width();
	req->height = _face_pixbuf->get_height();
}

void
Fader::on_size_allocate (Gtk::Allocation& alloc)
{
    CairoWidget::on_size_allocate(alloc);
	update_unity_position ();
}

bool
Fader::on_button_press_event (GdkEventButton* ev)
{
    focus_handler();
    
	if (_read_only) {
		return false;
	}
    
	if (ev->type != GDK_BUTTON_PRESS) {
        return false;
	}

	if (ev->button != 1 && ev->button != 2) {
		return false;
	}

	if (_touch_cursor) {
		get_window()->set_cursor (*_touch_cursor);
	}

	_grab_start_mouse_x = ev->x;
	_grab_start_mouse_y = ev->y;
	get_handle_position (_grab_start_handle_x, _grab_start_handle_y);

	double hw = _handle_pixbuf->get_width();
	double hh = _handle_pixbuf->get_height();

	if ((ev->x < (_grab_start_handle_x - hw/2)) || (ev->x > (_grab_start_handle_x + hw/2)) || (ev->y < (_grab_start_handle_y - hh/2)) || (ev->y > (_grab_start_handle_y + hh/2))) {
		return false;
	}
    
	double ev_pos_x;
	double ev_pos_y;
		
	get_closest_point_on_line(_min_pos_x, _min_pos_y,
							  _max_pos_x, _max_pos_y, 
							  ev->x, ev->y,
							  ev_pos_x, ev_pos_y );
	add_modal_grab ();
	
	_grab_window = ev->window;
	_dragging = true;
	
	gdk_pointer_grab(ev->window,false,
					 GdkEventMask (Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK),
					 NULL,
					 NULL,
					 ev->time);

	queue_draw();
	
	return true;
}

bool
Fader::on_button_release_event (GdkEventButton* ev)
{
	if (_read_only) {
		return false;
	}

	if (_touch_cursor) {
		get_window()->set_cursor ();
	}

	if (_dragging) { //temp
		remove_modal_grab();
		_dragging = false;
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		queue_draw ();
	}
	return false;
}

bool
Fader::on_scroll_event (GdkEventScroll* ev)
{
	if (_read_only) {
		return false;
	}

    int step_factor = 1;

	switch (ev->direction) {
	case GDK_SCROLL_RIGHT:
	case GDK_SCROLL_UP:
#ifdef __APPLE__
        if ( ev->state & GDK_SHIFT_MASK ) {
            step_factor = -1;
        } else {
            step_factor = 1;
        }
#else
        step_factor = 1;
#endif
		break;
	case GDK_SCROLL_LEFT:
	case GDK_SCROLL_DOWN:
#ifdef __APPLE__
        if ( ev->state & GDK_SHIFT_MASK ) {
            step_factor = 1;
        } else {
            step_factor = -1;
        }
#else
        step_factor = -1;
#endif
		break;
	default:
		return false;
	}
    adjustment.set_value (adjustment.get_value() + step_factor * (adjustment.get_step_increment() ));
	return true;
}

bool
Fader::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_read_only) {
		return false;
	}

	if (_dragging) {
		double ev_pos_x;
		double ev_pos_y;
		
		if (ev->window != _grab_window) {
			_grab_window = ev->window;
			return true;
		}

		get_closest_point_on_line(_min_pos_x, _min_pos_y,
								  _max_pos_x, _max_pos_y, 
								  _grab_start_handle_x + (ev->x - _grab_start_mouse_x), _grab_start_handle_y + (ev->y - _grab_start_mouse_y),
								  ev_pos_x, ev_pos_y );
		
		double const fract = sqrt((ev_pos_x - _min_pos_x) * (ev_pos_x - _min_pos_x) +
								  (ev_pos_y - _min_pos_y) * (ev_pos_y - _min_pos_y)) /
							 sqrt((_max_pos_x - _min_pos_x) * (_max_pos_x - _min_pos_x) +
								  (_max_pos_y - _min_pos_y) * (_max_pos_y - _min_pos_y));
		
		adjustment.set_value (adjustment.get_lower() + (adjustment.get_upper() - adjustment.get_lower()) * fract);
	}
	return true;
}

void
Fader::adjustment_changed ()
{
    double handle_x;
	double handle_y;
	get_handle_position (handle_x, handle_y);

	if ((handle_x != _last_drawn_x) || (handle_y != _last_drawn_y)) {
		queue_draw ();
	}
}

/** @return pixel offset of the current value from the right or bottom of the fader */
void
Fader::get_handle_position (double& x, double& y)
{
	double fract = (adjustment.get_value () - adjustment.get_lower()) / ((adjustment.get_upper() - adjustment.get_lower()));

	x = (int)(_min_pos_x + (_max_pos_x - _min_pos_x) * fract);
	y = (int)(_min_pos_y + (_max_pos_y - _min_pos_y) * fract);
}

bool
Fader::on_enter_notify_event (GdkEventCrossing*)
{
	_hovering = true;
	Keyboard::magic_widget_grab_focus ();
	queue_draw ();
	return false;
}

bool
Fader::on_leave_notify_event (GdkEventCrossing*)
{
	if (_read_only) {
		return false;
	}

	if (!_dragging) {
		_hovering = false;
		Keyboard::magic_widget_drop_focus();
		queue_draw ();
	}
	return false;
}

void
Fader::set_default_value (float d)
{
	_default_value = d;
	update_unity_position ();
}

void
Fader::update_unity_position ()
{
}
