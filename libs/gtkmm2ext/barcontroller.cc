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
#include "gtkmm2ext/cairo_widget.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

BarController::BarController (Gtk::Adjustment& adj,
			      boost::shared_ptr<PBD::Controllable> mc)

	: adjustment (adj)
	, binding_proxy (mc)
	, spinner (adjustment)
	, _hovering (false)
{			  
	_style = LeftToRight;
	grabbed = false;
	switching = false;
	switch_on_release = false;
	use_parent = false;
	logarithmic = false;

	layout = darea.create_pango_layout("");

	set (.5, .5, 1.0, 1.0);
	set_border_width(0);

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
	darea.signal_enter_notify_event().connect (mem_fun (*this, &BarController::on_enter_notify_event));
	darea.signal_leave_notify_event().connect (mem_fun (*this, &BarController::on_leave_notify_event));

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
}

bool
BarController::on_enter_notify_event (GdkEventCrossing*)
{
	_hovering = true;
	Keyboard::magic_widget_grab_focus ();
	queue_draw ();
	return false;
}

bool
BarController::on_leave_notify_event (GdkEventCrossing*)
{
	_hovering = false;
	Keyboard::magic_widget_drop_focus();
	queue_draw ();
	return false;
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

Gdk::Color
BarController::get_parent_bg ()
{
        Widget* parent;

	parent = get_parent ();

        while (parent) {
		static const char* has_cairo_widget_background_info = "has_cairo_widget_background_info";
		void* p = g_object_get_data (G_OBJECT(parent->gobj()), has_cairo_widget_background_info);

		if (p) {
			Glib::RefPtr<Gtk::Style> style = parent->get_style();
			return style->get_bg (get_state());
		}
		
		if (!parent->get_has_window()) {
			parent = parent->get_parent();
		} else {
			break;
		}
        }

        if (parent && parent->get_has_window()) {
		return parent->get_style ()->get_bg (parent->get_state());
        } 

	return get_style ()->get_bg (get_state());
}

bool
BarController::expose (GdkEventExpose* /*event*/)
{
	Glib::RefPtr<Gdk::Window> win (darea.get_window());
	Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();
	cairo_t* cr = context->cobj();

	Gdk::Color fg_col = get_style()->get_fg (get_state());

	double fract = ((adjustment.get_value() - adjustment.get_lower()) /
		 (adjustment.get_upper() - adjustment.get_lower()));
	
	gint w = darea.get_width() ;
	gint h = darea.get_height();
	gint bar_start, bar_width;
	double radius = 4;

	switch (_style) {
	case Line:
		bar_start = (gint) floor ((w-1) * fract);
		bar_width = 1;
		break;

	case Blob:
		// ????
		break;

	case CenterOut:
        bar_width = (w*fract);
        bar_start = (w/2) - bar_width/2; // center, back up half the bar width
 		break;

	case LeftToRight:
		bar_start = 1;
		bar_width = floor((w-2)*fract);

		break;

	case RightToLeft:
		break;
	case TopToBottom:
		break;
	case BottomToTop:
		break;
	}

	//fill in the bg rect ... 
	Gdk::Color c = get_parent_bg(); //get_style()->get_bg (Gtk::STATE_PRELIGHT);  //why prelight?  Shouldn't we be using the parent's color?  maybe   get_parent_bg  ?
	CairoWidget::set_source_rgb_a (cr, c);
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_fill(cr);

	//"slot"
	cairo_set_source_rgba (cr, 0.17, 0.17, 0.17, 1.0);
	Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, radius-0.5);
	cairo_fill(cr);

	//mask off the corners
	Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, radius-0.5);
	cairo_clip(cr);
	
		//background gradient
		if ( !CairoWidget::flat_buttons() ) {
			cairo_pattern_t *bg_gradient = cairo_pattern_create_linear (0.0, 0.0, 0, h);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 0, 0, 0, 0, 0.4);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 0.2, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 1, 0, 0, 0, 0.0);
			cairo_set_source (cr, bg_gradient);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, radius-1.5);
			cairo_fill (cr);
			cairo_pattern_destroy(bg_gradient);
		}
		
		//fg color
		CairoWidget::set_source_rgb_a (cr, fg_col, 1.0);
		Gtkmm2ext::rounded_rectangle (cr, bar_start, 1, bar_width, h-2, radius - 1.5);
		cairo_fill(cr);

		//fg gradient
		if (!CairoWidget::flat_buttons() ) {
			cairo_pattern_t * fg_gradient = cairo_pattern_create_linear (0.0, 0.0, 0, h);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 0, 0, 0, 0, 0.0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 0.1, 0, 0, 0, 0.0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 1, 0, 0, 0, 0.3);
			cairo_set_source (cr, fg_gradient);
			Gtkmm2ext::rounded_rectangle (cr, bar_start, 1, bar_width, h-2, radius - 1.5);
			cairo_fill (cr);
			cairo_pattern_destroy(fg_gradient);
		}
		
	cairo_reset_clip(cr);

	//black border
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	Gtkmm2ext::rounded_rectangle (cr, 0.5, 0.5, w-1, h-1, radius);
	cairo_stroke(cr);

	/* draw the unity-position line if it's not at either end*/
/*	if (unity_loc > 0) {
		context->set_line_width (1);
		cairo_set_source_rgba (cr, 1,1,1, 1.0);
		if ( _orien == VERT) {
			if (unity_loc < h ) {
				context->move_to (2.5, unity_loc + radius + .5);
				context->line_to (girth-2.5, unity_loc + radius + .5);
				context->stroke ();
			}
		} else {
			if ( unity_loc < w ){
				context->move_to (unity_loc - radius + .5, 3.5);
				context->line_to (unity_loc - radius + .5, girth-3.5);
				context->stroke ();
			}
		}
	}*/
	
	if (!darea.get_sensitive()) {
		rounded_rectangle (context, 0, 0, darea.get_width(), darea.get_height(), 3);
		context->set_source_rgba (0.505, 0.517, 0.525, 0.6);
		context->fill ();
	} else if (_hovering) {
		Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, radius);
		cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.1);
		cairo_fill (cr);
	}

	/* draw label */

	double xpos = -1;
	std::string const label = get_label (xpos);
	if (!label.empty()) {
		
		int twidth, theight;
		layout->set_text (label);
		layout->get_pixel_size (twidth, theight);

		if (fract > 0.5) {
			cairo_set_source_rgba (cr, 0.17, 0.17, 0.17, 1.0);
			context->move_to ( 5, (darea.get_height()/2) - (theight/2));
		} else {
			c = get_style()->get_text (get_state());
			CairoWidget::set_source_rgb_a (cr, c, 0.7);
			context->move_to ( w-twidth-5, (darea.get_height()/2) - (theight/2));
		}
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
	Alignment::set_sensitive (yn);
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


	
