/*
    Copyright (C) 2004 Paul Davis
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

#include <string>
#include <climits>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include <pbd/controllable.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/barcontroller.h>

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

BarController::BarController (Gtk::Adjustment& adj,
			      boost::shared_ptr<PBD::Controllable> mc)

	: adjustment (adj),
	  binding_proxy (mc),
	  spinner (adjustment)

{			  
	_style = LeftToRight;
	grabbed = false;
	switching = false;
	switch_on_release = false;
	use_parent = false;

	layout = darea.create_pango_layout("");

	set_shadow_type (SHADOW_NONE);

	initial_value = adjustment.get_value ();

	adjustment.signal_value_changed().connect (mem_fun (*this, &Gtk::Widget::queue_draw));
	adjustment.signal_changed().connect (mem_fun (*this, &Gtk::Widget::queue_draw));

	darea.add_events (Gdk::BUTTON_RELEASE_MASK|
			  Gdk::BUTTON_PRESS_MASK|
			  Gdk::POINTER_MOTION_MASK|
			  Gdk::ENTER_NOTIFY_MASK|
			  Gdk::LEAVE_NOTIFY_MASK|
			  Gdk::SCROLL_MASK);

	darea.signal_expose_event().connect (mem_fun (*this, &BarController::expose));
	darea.signal_motion_notify_event().connect (mem_fun (*this, &BarController::motion));
	darea.signal_button_press_event().connect (mem_fun (*this, &BarController::button_press), false);
	darea.signal_button_release_event().connect (mem_fun (*this, &BarController::button_release), false);
	darea.signal_scroll_event().connect (mem_fun (*this, &BarController::scroll));

	spinner.signal_activate().connect (mem_fun (*this, &BarController::entry_activated));
	spinner.signal_focus_out_event().connect (mem_fun (*this, &BarController::entry_focus_out));
	spinner.signal_input().connect (mem_fun (*this, &BarController::entry_input));
	spinner.signal_output().connect (mem_fun (*this, &BarController::entry_output));
	spinner.set_digits (3);

	add (darea);
	show_all ();
}

void
BarController::drop_grab ()
{
	if (grabbed) {
		grabbed = false;
		darea.remove_modal_grab();
		StopGesture ();
	}
}

bool
BarController::button_press (GdkEventButton* ev)
{
	double fract;

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	switch (ev->button) {
	case 1:
		if (ev->type == GDK_2BUTTON_PRESS) {
			switch_on_release = true;
			drop_grab ();
		} else {
			switch_on_release = false;
			darea.add_modal_grab();
			grabbed = true;
			grab_x = ev->x;
			grab_window = ev->window;
			StartGesture ();
		}
		return true;
		break;

	case 2:
		fract = ev->x / (darea.get_width() - 2.0);
		adjustment.set_value (adjustment.get_lower() + fract * (adjustment.get_upper() - adjustment.get_lower()));

	case 3:
		break;

	case 4:
	case 5:
		break;
	}

	return false;
}

bool
BarController::button_release (GdkEventButton* ev)
{
	drop_grab ();
	
	switch (ev->button) {
	case 1:
		if (switch_on_release) {
			Glib::signal_idle().connect (mem_fun (*this, &BarController::switch_to_spinner));
			return true;
		}

		if ((ev->state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) == GDK_SHIFT_MASK) {
			adjustment.set_value (initial_value);
		} else {
			double scale;

			if ((ev->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) {
				scale = 0.01;
			} else if (ev->state & GDK_CONTROL_MASK) {
				scale = 0.1;
			} else {
				scale = 1.0;
			}

			mouse_control (ev->x, ev->window, scale);
		}
		break;

	case 2:
		break;
		
	case 3:
		return false;
		
	default:
		break;
	}

	return true;
}

bool
BarController::scroll (GdkEventScroll* ev)
{
	double scale;

	if ((ev->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) {
		scale = 0.01;
	} else if (ev->state & GDK_CONTROL_MASK) {
		scale = 0.1;
	} else {
		scale = 1.0;
	}

	switch (ev->direction) {
	case GDK_SCROLL_UP:
	case GDK_SCROLL_RIGHT:
		adjustment.set_value (adjustment.get_value() + (scale * adjustment.get_step_increment()));
		break;

	case GDK_SCROLL_DOWN:
	case GDK_SCROLL_LEFT:
		adjustment.set_value (adjustment.get_value() - (scale * adjustment.get_step_increment()));
		break;
	}

	return true;
}

bool
BarController::motion (GdkEventMotion* ev)
{
	double scale;
	
	if (!grabbed) {
		return true;
	}

	if ((ev->state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) == GDK_SHIFT_MASK) {
		return TRUE;
	}

	if ((ev->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) {
		scale = 0.01;
	} else if (ev->state & GDK_CONTROL_MASK) {
		scale = 0.1;
	} else {
		scale = 1.0;
	}

	return mouse_control (ev->x, ev->window, scale);
}

gint
BarController::mouse_control (double x, GdkWindow* window, double scaling)
{
	double fract = 0.0;
	double delta;

	if (window != grab_window) {
		grab_x = x;
		grab_window = window;
		return TRUE;
	}

	delta = x - grab_x;
	grab_x = x;

	switch (_style) {
	case Line:
	case LeftToRight:
		fract = scaling * (delta / (darea.get_width() - 2));
		fract = min (1.0, fract);
		fract = max (-1.0, fract);
		adjustment.set_value (adjustment.get_value() + fract * (adjustment.get_upper() - adjustment.get_lower()));
		break;

	default:
		fract = 0.0;
	}
	
	
	return TRUE;
}

bool
BarController::expose (GdkEventExpose* event)
{
	Glib::RefPtr<Gdk::Window> win (darea.get_window());
	Widget* parent;
	gint x1=0, x2=0, y1=0, y2=0;
	gint w, h;
	double fract;

	fract = ((adjustment.get_value() - adjustment.get_lower()) /
		 (adjustment.get_upper() - adjustment.get_lower()));
	
	switch (_style) {
	case Line:
		w = darea.get_width() - 1;
		h = darea.get_height();
		x1 = (gint) floor (w * fract);
		x2 = x1;
		y1 = 0;
		y2 = h - 1;

		if (use_parent) {
			parent = get_parent();
			
			if (parent) {
				win->draw_rectangle (parent->get_style()->get_fg_gc (parent->get_state()),
						     true,
						     0, 0, darea.get_width(), darea.get_height());
			}

		} else {

			win->draw_rectangle (get_style()->get_bg_gc (get_state()),
					     true,
					     0, 0, darea.get_width() - ((darea.get_width()+1) % 2), darea.get_height());
		}
		
		win->draw_line (get_style()->get_fg_gc (get_state()), x1, 0, x1, h);
		break;

	case CenterOut:
		break;

	case LeftToRight:

		w = darea.get_width() - 2;
		h = darea.get_height() - 2;

		x1 = 0;
		x2 = (gint) floor (w * fract);
		y1 = 0;
		y2 = h - 1;

		win->draw_rectangle (get_style()->get_bg_gc (get_state()),
				    false,
				    0, 0, darea.get_width() - 1, darea.get_height() - 1);

		/* draw active box */

		win->draw_rectangle (get_style()->get_fg_gc (get_state()),
				    true,
				    1 + x1,
				    1 + y1,
				    x2,
				    1 + y2);
		
		/* draw inactive box */

		win->draw_rectangle (get_style()->get_fg_gc (STATE_INSENSITIVE),
				    true,
				    1 + x2,
				    1 + y1,
				    w - x2,
				    1 + y2);

		break;

	case RightToLeft:
		break;
	case TopToBottom:
		break;
	case BottomToTop:
		break;
	}

	/* draw label */

	int xpos = -1;
	std::string const label = get_label (xpos);

	if (!label.empty()) {
		
		layout->set_text (label);
		
		int width, height;
		layout->get_pixel_size (width, height);

		if (xpos == -1) {
			xpos = max (3, 1 + (x2 - (width/2)));
			xpos = min (darea.get_width() - width - 3, xpos);
		}
		
		win->draw_layout (get_style()->get_text_gc (get_state()),
				  xpos,
				  (darea.get_height()/2) - (height/2),
				  layout);
	}
	
	return true;
}

void
BarController::set_style (Style s)
{
	_style = s;
	darea.queue_draw ();
}

gint
BarController::switch_to_bar ()
{
	if (switching) {
		return FALSE;
	}

	switching = true;

	if (get_child() == &darea) {
		return FALSE;
	}

	remove ();
	add (darea);
	darea.show ();

	switching = false;
	return FALSE;
}

gint
BarController::switch_to_spinner ()
{
	if (switching) {
		return FALSE;
	}

	switching = true;

	if (get_child() == &spinner) {
		return FALSE;
	}

	remove ();
	add (spinner);
	spinner.show ();
	spinner.select_region (0, spinner.get_text_length());
	spinner.grab_focus ();

	switching = false;
	return FALSE;
}

void
BarController::entry_activated ()
{
	string text = spinner.get_text ();
	float val;

	if (sscanf (text.c_str(), "%f", &val) == 1) {
		adjustment.set_value (val);
	}
	
	switch_to_bar ();
}

bool
BarController::entry_focus_out (GdkEventFocus* ev)
{
	entry_activated ();
	return true;
}

void
BarController::set_use_parent (bool yn)
{
	use_parent = yn;
	queue_draw ();
}

void
BarController::set_sensitive (bool yn)
{
	Frame::set_sensitive (yn);
	darea.set_sensitive (yn);
}

bool
BarController::entry_input (double* v)
{
	return false;
}

bool
BarController::entry_output ()
{
	return false;
}

	
