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
#include <sstream>
#include <climits>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include <pbd/controllable.h>
#include <pbd/locale_guard.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/barcontroller.h"

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
	logarithmic = false;

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
	spinner.set_digits (9);
	spinner.set_numeric (true);
	
	add (darea);

	show_all ();
}

BarController::~BarController ()
{
//	delete pattern;
//	delete shine_pattern;
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

		if ((ev->state & (Keyboard::TertiaryModifier|Keyboard::PrimaryModifier)) == Keyboard::TertiaryModifier) {
			adjustment.set_value (initial_value);
		} else {
			double scale;

			if ((ev->state & (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) == (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) {
				scale = 0.01;
			} else if (ev->state & Keyboard::PrimaryModifier) {
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

	if ((ev->state & (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) == (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) {
		scale = 0.01;
	} else if (ev->state & Keyboard::PrimaryModifier) {
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

	if ((ev->state & (Keyboard::TertiaryModifier|Keyboard::PrimaryModifier)) == Keyboard::TertiaryModifier) {
		return TRUE;
	}

	if ((ev->state & (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) == (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) {
		scale = 0.01;
	} else if (ev->state & Keyboard::PrimaryModifier) {
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
	case Blob:
	case LeftToRight:
        case CenterOut:
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

void
BarController::create_patterns ()
{
	Glib::RefPtr<Gdk::Window> win (darea.get_window());
    Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();

	Gdk::Color c = get_style()->get_fg (get_state());
    float r, g, b;
	r = c.get_red_p ();
	g = c.get_green_p ();
	b = c.get_blue_p ();

	float rheight = darea.get_height()-2;

 	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, rheight);
	cairo_pattern_add_color_stop_rgba (pat, 0, r*0.8,g*0.8,b*0.8, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 1, r*0.6,g*0.6,b*0.6, 1.0);
	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));
	pattern = p;
	cairo_pattern_destroy(pat);

	pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, rheight);
	cairo_pattern_add_color_stop_rgba (pat, 0, 1,1,1,0.0);
	cairo_pattern_add_color_stop_rgba (pat, 0.2, 1,1,1,0.3);
	cairo_pattern_add_color_stop_rgba (pat, 0.5, 1,1,1,0.0);
	cairo_pattern_add_color_stop_rgba (pat, 1, 1,1,1,0.0);
	Cairo::RefPtr<Cairo::Pattern> p2 (new Cairo::Pattern (pat, false));
	shine_pattern = p2;
	cairo_pattern_destroy(pat);

}

bool
BarController::expose (GdkEventExpose* /*event*/)
{
	Glib::RefPtr<Gdk::Window> win (darea.get_window());
	Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();

	if( !pattern )
		create_patterns();

	Gdk::Color c;
	Widget* parent;
	gint x1=0, x2=0, y2=0;
	gint w, h;
	double fract, radius;
    float r, g, b;

	fract = ((adjustment.get_value() - adjustment.get_lower()) /
		 (adjustment.get_upper() - adjustment.get_lower()));
	
	switch (_style) {
	case Line:
		w = darea.get_width() - 1;
		h = darea.get_height();
		x1 = (gint) floor (w * fract);
		x2 = x1;
		y2 = h - 1;

		if (use_parent) {
			parent = get_parent();
                        
			if (parent) {
                                c = parent->get_style()->get_fg (parent->get_state());
                                r = c.get_red_p ();
                                g = c.get_green_p ();
                                b = c.get_blue_p ();
                                context->set_source_rgb (r, g, b);
                                context->rectangle (0, 0, darea.get_width(), darea.get_height());
                                context->fill ();
			}

		} else {

                        c = get_style()->get_bg (get_state());
                        r = c.get_red_p ();
                        g = c.get_green_p ();
                        b = c.get_blue_p ();
                        context->set_source_rgb (r, g, b);
                        context->rectangle (0, 0, darea.get_width() - ((darea.get_width()+1) % 2), darea.get_height());
                        context->fill ();
		}
                
                c = get_style()->get_fg (get_state());
                r = c.get_red_p ();
                g = c.get_green_p ();
                b = c.get_blue_p ();
                context->set_source_rgb (r, g, b);
                context->move_to (x1, 0);
                context->line_to (x1, h);
                context->stroke ();
		break;

        case Blob:
		w = darea.get_width() - 1;
		h = darea.get_height();
		x1 = (gint) floor (w * fract);
		x2 = min (w-2,h-2);

		if (use_parent) {
			parent = get_parent();
			
			if (parent) {
                                c = parent->get_style()->get_fg (parent->get_state());
                                r = c.get_red_p ();
                                g = c.get_green_p ();
                                b = c.get_blue_p ();
                                context->set_source_rgb (r, g, b);
                                context->rectangle (0, 0, darea.get_width(), darea.get_height());
                                context->fill ();
			}

		} else {

                        c = get_style()->get_bg (get_state());
                        r = c.get_red_p ();
                        g = c.get_green_p ();
                        b = c.get_blue_p ();
                        context->set_source_rgb (r, g, b);
                        context->rectangle (0, 0, darea.get_width() - ((darea.get_width()+1) % 2), darea.get_height());
                        context->fill ();
		}
		
                c = get_style()->get_fg (get_state());
                r = c.get_red_p ();
                g = c.get_green_p ();
                b = c.get_blue_p ();
                context->arc (x1, ((h-2)/2)-1, x2, 0, 2*M_PI);
		break;

	case CenterOut:
		w = darea.get_width();
		h = darea.get_height()-2;
                if (use_parent) {
                        parent = get_parent();
                        if (parent) {
                                c = parent->get_style()->get_fg (parent->get_state());
                                r = c.get_red_p ();
                                g = c.get_green_p ();
                                b = c.get_blue_p ();
                                context->set_source_rgb (r, g, b);
                                context->rectangle (0, 0, darea.get_width(), darea.get_height());
                                context->fill ();
                        }
                } else {
                        c = get_style()->get_bg (get_state());
                        r = c.get_red_p ();
                        g = c.get_green_p ();
                        b = c.get_blue_p ();
                        context->set_source_rgb (r, g, b);
                        context->rectangle (0, 0, darea.get_width(), darea.get_height());
                        context->fill ();
                }
                c = get_style()->get_fg (get_state());
                r = c.get_red_p ();
                g = c.get_green_p ();
                b = c.get_blue_p ();
                x1 = (w/2) - ((w*fract)/2); // center, back up half the bar width
                context->set_source_rgb (r, g, b);
                context->rectangle (x1, 1, w*fract, h);
                context->fill ();
		break;

	case LeftToRight:

		w = darea.get_width() - 2;
		h = darea.get_height() - 2;

		x2 = (gint) floor (w * fract);
		y2 = h;
		radius = 4;
		if (x2 < 8) x2 = 8;

		/* border */

		context->set_source_rgb (0,0,0);
		cairo_rectangle (context->cobj(), 0, 0, darea.get_width(), darea.get_height());
		context->fill ();

		/* draw active box */

		context->set_source (pattern);
		rounded_rectangle (context, 1, 1, x2, y2, radius-1.5);
		context->fill ();

//		context->set_source (shine_pattern);
//		rounded_rectangle (context, 2, 3, x2-2, y2-8, radius-2);
//		context->fill ();
		break;

	case RightToLeft:
		break;
	case TopToBottom:
		break;
	case BottomToTop:
		break;
	}

	if (!darea.get_sensitive()) {
		rounded_rectangle (context, 0, 0, darea.get_width(), darea.get_height(), 3);
		context->set_source_rgba (0.505, 0.517, 0.525, 0.6);
		context->fill ();
	}

	/* draw label */

	double xpos = -1;
	std::string const label = get_label (xpos);

	if (!label.empty()) {
		
		layout->set_text (label);
		
		int width, height, x;
		layout->get_pixel_size (width, height);

		if (xpos == -1) {
			x = max (3, 1 + (x2 - (width/2)));
			x = min (darea.get_width() - width - 3, (int) lrint (xpos));
		} else {
                        x = lrint (darea.get_width() * xpos);
                }

                c = get_style()->get_text (get_state());
                r = c.get_red_p ();
                g = c.get_green_p ();
                b = c.get_blue_p ();
                context->set_source_rgb (r, g, b);
                context->move_to (x, (darea.get_height()/2) - (height/2));
                layout->show_in_cairo_context (context);
	}
	
	return true;
}

void
BarController::set_style (barStyle s)
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

	SpinnerActive (false); /* EMIT SIGNAL */
	
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

	SpinnerActive (true); /* EMIT SIGNAL */

	return FALSE;
}

void
BarController::entry_activated ()
{
	switch_to_bar ();
}

bool
BarController::entry_focus_out (GdkEventFocus* /*ev*/)
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

/* 
    This is called when we need to update the adjustment with the value
    from the spinner's text entry.
    
    We need to use Gtk::Entry::get_text to avoid recursive nastiness :)
    
    If we're not in logarithmic mode we can return false to use the 
    default conversion.
    
    In theory we should check for conversion errors but set numeric
    mode to true on the spinner prevents invalid input.
*/
int
BarController::entry_input (double* new_value)
{
	if (!logarithmic) {
		return false;
	}

	// extract a double from the string and take its log
	Entry *entry = dynamic_cast<Entry *>(&spinner);
	double value;

	{
		// Switch to user's preferred locale so that
		// if they use different LC_NUMERIC conventions,
		// we will honor them.

		PBD::LocaleGuard lg ("");
		sscanf (entry->get_text().c_str(), "%lf", &value);
	}

	*new_value = log(value);

	return true;
}

/* 
    This is called when we need to update the spinner's text entry 
    with the value of the adjustment.
    
    We need to use Gtk::Entry::set_text to avoid recursive nastiness :)
    
    If we're not in logarithmic mode we can return false to use the 
    default conversion.
*/
bool
BarController::entry_output ()
{
	if (!logarithmic) {
		return false;
	}

	// generate the exponential and turn it into a string
	// convert to correct locale. 
	
	stringstream stream;
	string str;

	char buf[128];

	{
		// Switch to user's preferred locale so that
		// if they use different LC_NUMERIC conventions,
		// we will honor them.
		
		PBD::LocaleGuard lg ("");
		snprintf (buf, sizeof (buf), "%g", exp (spinner.get_adjustment()->get_value()));
	}

	Entry *entry = dynamic_cast<Entry *>(&spinner);
	entry->set_text(buf);
	
	return true;
}


	
