/*
    Copyright (C) 2010 Paul Davis

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

#include <iostream>
#include <cmath>
#include <algorithm>

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "ardour_knob.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "canvas/colors.h"
#include "canvas/utils.h"

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;

ArdourKnob::Element ArdourKnob::default_elements = ArdourKnob::Element (ArdourKnob::Arc);

ArdourKnob::ArdourKnob (Element e)
	: _elements (e)
	, _hovering (false)
{
	ARDOUR_UI_UTILS::ColorsChanged.connect (sigc::mem_fun (*this, &ArdourKnob::color_handler));
}

ArdourKnob::~ArdourKnob()
{
}

void
ArdourKnob::render (cairo_t* cr, cairo_rectangle_t *)
{
	cairo_pattern_t* shade_pattern;
	
	float width = get_width();
	float height = get_height();
	
	const float scale = min(width, height);
	const float pointer_thickness = 3.0 * (scale/80);  //(if the knob is 80 pixels wide, we want a 3-pix line on it)
	
	float start_angle = ((180 - 65) * G_PI) / 180;
	float end_angle = ((360 + 65) * G_PI) / 180;
	float value_angle = start_angle + (_val * (end_angle - start_angle));
	
	float value_x = cos (value_angle);
	float value_y = sin (value_angle);

	float xc =  0.5 + width/ 2.0;
	float yc = 0.5 + height/ 2.0;
	
	cairo_translate (cr, xc, yc);  //after this, everything is based on the center of the knob

	//get the knob color from the theme
	ArdourCanvas::Color knob_color = ARDOUR_UI::config()->color (string_compose ("%1", get_name()));

	float center_radius = 0.48*scale;
	float border_width = 0.8;
	
	bool arc = (_elements & Arc)==Arc;
	bool bevel = (_elements & Bevel)==Bevel;
	bool flat = _flat_buttons;
	
	if ( arc ) {
		center_radius = scale*0.30;

		float inner_progress_radius = scale*0.30;
		float outer_progress_radius = scale*0.48;
		float progress_width = (outer_progress_radius-inner_progress_radius);
		float progress_radius = inner_progress_radius + progress_width/2.0;
		
		float start_angle_x = cos (start_angle);
		float start_angle_y = sin (start_angle);
		float end_angle_x = cos (end_angle);
		float end_angle_y = sin (end_angle);

		//dark arc background
		cairo_set_source_rgb (cr, 0.3, 0.3, 0.3 );
		cairo_set_line_width (cr, progress_width);
		cairo_arc (cr, 0, 0, progress_radius, start_angle, end_angle);
		cairo_stroke (cr);

		//look up the arc colors from the config
		double red_start, green_start, blue_start, unused;
		ArdourCanvas::Color arc_start_color = ARDOUR_UI::config()->color ( string_compose ("%1: arc start", get_name()));
		ArdourCanvas::color_to_rgba( arc_start_color, red_start, green_start, blue_start, unused );
		double red_end, green_end, blue_end;
		ArdourCanvas::Color arc_end_color = ARDOUR_UI::config()->color ( string_compose ("%1: arc end", get_name()) );
		ArdourCanvas::color_to_rgba( arc_end_color, red_end, green_end, blue_end, unused );

		//vary the arc color over the travel of the knob
		float r = (1.0-_val) * red_end + _val * red_start;
		float g = (1.0-_val) * green_end + _val * green_start;
		float b = (1.0-_val) * blue_end + _val * blue_start;

		//draw the arc
		cairo_set_source_rgb (cr, r,g,b);
		cairo_set_line_width (cr, progress_width);
		cairo_arc (cr, 0, 0, progress_radius, start_angle, value_angle);
		cairo_stroke (cr);

		//shade the arc
		if (!flat) {
			shade_pattern = cairo_pattern_create_linear (0.0, -yc, 0.0,  yc);  //note we have to offset the pattern from our centerpoint
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1,1,1, 0.15);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.5, 1,1,1, 0.0);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 1,1,1, 0.0);
			cairo_set_source (cr, shade_pattern);
			cairo_arc (cr, 0, 0, outer_progress_radius-1, 0, 2.0*G_PI);
			cairo_fill (cr);
			cairo_pattern_destroy (shade_pattern);
		}
		
		//black border
		cairo_set_source_rgb (cr, 0, 0, 0 );
		cairo_set_line_width (cr, border_width);
		cairo_move_to (cr, (outer_progress_radius * start_angle_x), (outer_progress_radius * start_angle_y));
		cairo_line_to (cr, (inner_progress_radius * start_angle_x), (inner_progress_radius * start_angle_y));
		cairo_stroke (cr);
		cairo_move_to (cr, (outer_progress_radius * end_angle_x), (outer_progress_radius * end_angle_y));
		cairo_line_to (cr, (inner_progress_radius * end_angle_x), (inner_progress_radius * end_angle_y));
		cairo_stroke (cr);
		cairo_arc (cr, 0, 0, outer_progress_radius, start_angle, end_angle);
		cairo_stroke (cr);
	}
	
	if (!flat) {
		//knob shadow
		cairo_save(cr);
		cairo_translate(cr, pointer_thickness+1, pointer_thickness+1 );
		cairo_set_source_rgba (cr, 0, 0, 0, 0.1 );
		cairo_arc (cr, 0, 0, center_radius-1, 0, 2.0*G_PI);
		cairo_fill (cr);
		cairo_restore(cr);

		//inner circle
		ArdourCanvas::set_source_rgba(cr, knob_color);
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
		cairo_fill (cr);
		
		//gradient	
		if (bevel) {
			//knob gradient
			shade_pattern = cairo_pattern_create_linear (0.0, -yc, 0.0,  yc);  //note we have to offset the gradient from our centerpoint
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1,1,1, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.2, 1,1,1, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.8, 0,0,0, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 0,0,0, 0.2);
			cairo_set_source (cr, shade_pattern);
			cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
			cairo_fill (cr);
			cairo_pattern_destroy (shade_pattern);

			//flat top over beveled edge
			ArdourCanvas::set_source_rgb_a (cr, knob_color, 0.5 );
			cairo_arc (cr, 0, 0, center_radius-pointer_thickness, 0, 2.0*G_PI);
			cairo_fill (cr);
		} else {	
			//radial gradient
			shade_pattern = cairo_pattern_create_radial ( -center_radius, -center_radius, 1, -center_radius, -center_radius, center_radius*2.5  );  //note we have to offset the gradient from our centerpoint
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1,1,1, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 0,0,0, 0.3);
			cairo_set_source (cr, shade_pattern);
			cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
			cairo_fill (cr);
			cairo_pattern_destroy (shade_pattern);
		}
		
	} else {
		//inner circle
		ArdourCanvas::set_source_rgba(cr, knob_color);
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
		cairo_fill (cr);
	}
	

	//black knob border
	cairo_set_line_width (cr, border_width);
	cairo_set_source_rgba (cr, 0,0,0, 1 );
	cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
	cairo_stroke (cr);
			
	//line shadow
	if (!flat) {
		cairo_save(cr);
		cairo_translate(cr, 1, 1 );
		cairo_set_source_rgba (cr, 0,0,0,0.3 );
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
		cairo_set_line_width (cr, pointer_thickness);
		cairo_move_to (cr, (center_radius * value_x), (center_radius * value_y));
		cairo_line_to (cr, ((center_radius*0.4) * value_x), ((center_radius*0.4) * value_y));
		cairo_stroke (cr);
		cairo_restore(cr);
	}
	
	//line
	cairo_set_source_rgba (cr, 1,1,1, 1 );
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_width (cr, pointer_thickness);
	cairo_move_to (cr, (center_radius * value_x), (center_radius * value_y));
	cairo_line_to (cr, ((center_radius*0.4) * value_x), ((center_radius*0.4) * value_y));
	cairo_stroke (cr);

	//highlight if grabbed or if mouse is hovering over me
	if ( _grabbed || (_hovering && ARDOUR::Config->get_widget_prelight() ) ) {
		cairo_set_source_rgba (cr, 1,1,1, 0.12 );
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
		cairo_fill (cr);
	}
	
	cairo_identity_matrix(cr);	
}

void
ArdourKnob::on_size_request (Gtk::Requisition* req)
{
	CairoWidget::on_size_request (req);
	
	//perhaps render the knob base into a cached image here?
}

bool
ArdourKnob::on_scroll_event (GdkEventScroll* ev)
{
	/* mouse wheel */

	float scale = 0.05;  //by default, we step in 1/20ths of the knob travel
	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale *= 0.01;
		} else {
			scale *= 0.10;
		}
	}

	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
	if (c) {
		float val = c->get_interface();
	
		if ( ev->direction == GDK_SCROLL_UP )
			val += scale;  
		else
			val -= scale;			

		c->set_interface(val);
	}

	return true;
}

bool
ArdourKnob::on_motion_notify_event (GdkEventMotion *ev) 
{
	//scale the adjustment based on keyboard modifiers
	float scale = 0.0025;
	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale *= 0.01;
		} else {
			scale *= 0.10;
		}
	}

	//calculate the travel of the mouse
	int y_delta = 0;
	if (ev->state & Gdk::BUTTON1_MASK) {
		y_delta = _grabbed_y - ev->y;
		_grabbed_y = ev->y;
		if (y_delta == 0) return TRUE;
	}

	//step the value of the controllable
	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
	if (c) {
		float val = c->get_interface();
		val += y_delta * scale;
		c->set_interface(val);
	}

	return true;
}

bool
ArdourKnob::on_button_press_event (GdkEventButton *ev)
{
	_grabbed_y = ev->y;
	_grabbed = true;
	
	set_active_state (Gtkmm2ext::ExplicitActive);

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	return false;
}

bool
ArdourKnob::on_button_release_event (GdkEventButton *ev)
{
	if ( (_grabbed_y == ev->y) && Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {  //no move, shift-click sets to default
		boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
		if (!c) return false;
		c->set_value (c->normal());
        return true;
	}

	_grabbed = false;
	unset_active_state ();

	return false;
}

void
ArdourKnob::color_handler ()
{
	set_dirty ();
}

void
ArdourKnob::on_size_allocate (Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
}

void
ArdourKnob::set_controllable (boost::shared_ptr<Controllable> c)
{
    watch_connection.disconnect ();  //stop watching the old controllable

	if (!c) return;

	binding_proxy.set_controllable (c);

	c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourKnob::controllable_changed, this), gui_context());

	controllable_changed();
}

void
ArdourKnob::controllable_changed ()
{
	_val = binding_proxy.get_controllable()->get_interface();  //% of knob travel

	_val = min( max(0.0f, _val), 1.0f);  //range check

	set_dirty();
}

void
ArdourKnob::on_style_changed (const RefPtr<Gtk::Style>&)
{
	set_dirty ();
}

void
ArdourKnob::on_name_changed ()
{
	set_dirty ();
}


void
ArdourKnob::set_active_state (Gtkmm2ext::ActiveState s)
{
	if (_active_state != s)
		CairoWidget::set_active_state (s);
}
	
void
ArdourKnob::set_visual_state (Gtkmm2ext::VisualState s)
{
	if (_visual_state != s)
		CairoWidget::set_visual_state (s);
}
	

bool
ArdourKnob::on_focus_in_event (GdkEventFocus* ev)
{
	set_dirty ();
	return CairoWidget::on_focus_in_event (ev);
}

bool
ArdourKnob::on_focus_out_event (GdkEventFocus* ev)
{
	set_dirty ();
	return CairoWidget::on_focus_out_event (ev);
}

bool
ArdourKnob::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = true;

	set_dirty ();

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ArdourKnob::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	set_dirty ();

	return CairoWidget::on_leave_notify_event (ev);
}

void
ArdourKnob::set_elements (Element e)
{
	_elements = e;
}

void
ArdourKnob::add_elements (Element e)
{
	_elements = (ArdourKnob::Element) (_elements | e);
}
