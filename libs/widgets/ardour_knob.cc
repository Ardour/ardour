/*
 * Copyright (C) 2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <cmath>
#include <algorithm>

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/controllable.h"
#include "pbd/error.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_knob.h"
#include "widgets/ui_config.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;

ArdourKnob::Element ArdourKnob::default_elements = ArdourKnob::Element (ArdourKnob::Arc);

ArdourKnob::ArdourKnob (Element e, Flags flags)
	: _elements (e)
	, _hovering (false)
	, _grabbed_x (0)
	, _grabbed_y (0)
	, _val (0)
	, _normal (0)
	, _dead_zone_delta (0)
	, _flags (flags)
	, _tooltip (this)
{
	UIConfigurationBase::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ArdourKnob::color_handler));

	// watch automation :(
	// TODO only use for GainAutomation
	//Timers::rapid_connect (sigc::bind (sigc::mem_fun (*this, &ArdourKnob::controllable_changed), false));
}

ArdourKnob::~ArdourKnob()
{
}

void
ArdourKnob::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj();
	cairo_pattern_t* shade_pattern;

	float width = get_width();
	float height = get_height();

	const float scale = min(width, height);
	const float pointer_thickness = 3.0 * (scale/80);  //(if the knob is 80 pixels wide, we want a 3-pix line on it)

	const float start_angle = ((180 - 65) * G_PI) / 180;
	const float end_angle = ((360 + 65) * G_PI) / 180;

	float zero = 0;
	if (_flags & ArcToZero) {
		zero = _normal;
	}

	const float value_angle = start_angle + (_val * (end_angle - start_angle));
	const float zero_angle = start_angle + (zero * (end_angle - start_angle));

	float value_x = cos (value_angle);
	float value_y = sin (value_angle);

	float xc =  0.5 + width/ 2.0;
	float yc = 0.5 + height/ 2.0;

	cairo_translate (cr, xc, yc);  //after this, everything is based on the center of the knob

	//get the knob color from the theme
	Gtkmm2ext::Color knob_color = UIConfigurationBase::instance().color (string_compose ("%1", get_name()));

	float center_radius = 0.48*scale;
	float border_width = 0.8;

	bool arc = (_elements & Arc)==Arc;
	bool bevel = (_elements & Bevel)==Bevel;
	bool flat = flat_buttons ();

	if ( arc ) {
		center_radius = scale*0.33;

		float inner_progress_radius = scale*0.38;
		float outer_progress_radius = scale*0.48;
		float progress_width = (outer_progress_radius-inner_progress_radius);
		float progress_radius = inner_progress_radius + progress_width/2.0;

		//dark arc background
		cairo_set_source_rgb (cr, 0.3, 0.3, 0.3 );
		cairo_set_line_width (cr, progress_width);
		cairo_arc (cr, 0, 0, progress_radius, start_angle, end_angle);
		cairo_stroke (cr);

		//look up the arc colors from the config
		double red_start, green_start, blue_start, unused;
		Gtkmm2ext::Color arc_start_color = UIConfigurationBase::instance().color ( string_compose ("%1: arc start", get_name()));
		Gtkmm2ext::color_to_rgba( arc_start_color, red_start, green_start, blue_start, unused );
		double red_end, green_end, blue_end;
		Gtkmm2ext::Color arc_end_color = UIConfigurationBase::instance().color ( string_compose ("%1: arc end", get_name()) );
		Gtkmm2ext::color_to_rgba( arc_end_color, red_end, green_end, blue_end, unused );

		//vary the arc color over the travel of the knob
		float intensity = fabsf (_val - zero) / std::max(zero, (1.f - zero));
		const float intensity_inv = 1.0 - intensity;
		float r = intensity_inv * red_end   + intensity * red_start;
		float g = intensity_inv * green_end + intensity * green_start;
		float b = intensity_inv * blue_end  + intensity * blue_start;

		//draw the arc
		cairo_set_source_rgb (cr, r,g,b);
		cairo_set_line_width (cr, progress_width);
		if (zero_angle > value_angle) {
			cairo_arc (cr, 0, 0, progress_radius, value_angle, zero_angle);
		} else {
			cairo_arc (cr, 0, 0, progress_radius, zero_angle, value_angle);
		}
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

#if 0 //black border
		const float start_angle_x = cos (start_angle);
		const float start_angle_y = sin (start_angle);
		const float end_angle_x = cos (end_angle);
		const float end_angle_y = sin (end_angle);

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
#endif
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
		Gtkmm2ext::set_source_rgba(cr, knob_color);
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
			Gtkmm2ext::set_source_rgb_a (cr, knob_color, 0.5 );
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
		Gtkmm2ext::set_source_rgba(cr, knob_color);
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

	// a transparent overlay to indicate insensitivity
	if (!sensitive ()) {
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
		uint32_t ins_color = UIConfigurationBase::instance().color ("gtk_background");
		Gtkmm2ext::set_source_rgb_a (cr, ins_color, 0.6);
		cairo_fill (cr);
	}

	//highlight if grabbed or if mouse is hovering over me
	if (_tooltip.dragging() || (_hovering && UIConfigurationBase::instance().get_widget_prelight() ) ) {
		cairo_set_source_rgba (cr, 1,1,1, 0.12);
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0*G_PI);
		cairo_fill (cr);
	}

	cairo_identity_matrix(cr);
}

void
ArdourKnob::on_size_request (Gtk::Requisition* req)
{
	// see ardour-button VectorIcon size, use font scaling as default
	CairoWidget::on_size_request (req); // allow to override

	// we're square
	if (req->width < req->height) {
		req->width = req->height;
	}
	if (req->height < req->width) {
		req->height = req->width;
	}
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
		float val = c->get_interface (true);

		if ( ev->direction == GDK_SCROLL_UP )
			val += scale;
		else
			val -= scale;

		c->set_interface (val, true);
	}

	return true;
}

bool
ArdourKnob::on_motion_notify_event (GdkEventMotion *ev)
{
	if (!(ev->state & Gdk::BUTTON1_MASK)) {
		return true;
	}

	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
	if (!c) {
		return true;
	}


	//scale the adjustment based on keyboard modifiers & GUI size
	const float ui_scale = max (1.f, UIConfigurationBase::instance().get_ui_scale());
	float scale = 0.0025 / ui_scale;

	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale *= 0.01;
		} else {
			scale *= 0.10;
		}
	}

	//calculate the travel of the mouse
	int delta = (_grabbed_y - ev->y) - (_grabbed_x - ev->x);
	if (delta == 0) {
		return true;
	}

	_grabbed_x = ev->x;
	_grabbed_y = ev->y;
	float val = c->get_interface (true);

	if (_flags & Detent) {
		const float px_deadzone = 42.f * ui_scale;

		if ((val - _normal) * (val - _normal + delta * scale) < 0) {
			/* detent */
			const int tozero = (_normal - val) * scale;
			int remain = delta - tozero;
			if (abs (remain) > px_deadzone) {
				/* slow down passing the default value */
				remain += (remain > 0) ? px_deadzone * -.5 : px_deadzone * .5;
				delta = tozero + remain;
				_dead_zone_delta = 0;
			} else {
				c->set_value (c->normal(), Controllable::NoGroup);
				_dead_zone_delta = remain / px_deadzone;
				return true;
			}
		}

		if (fabsf (rintf((val - _normal) / scale) + _dead_zone_delta) < 1) {
			c->set_value (c->normal(), Controllable::NoGroup);
			_dead_zone_delta += delta / px_deadzone;
			return true;
		}

		_dead_zone_delta = 0;
	}

	val += delta * scale;
	c->set_interface (val, true);

	return true;
}

bool
ArdourKnob::on_button_press_event (GdkEventButton *ev)
{
	_grabbed_x = ev->x;
	_grabbed_y = ev->y;
	_dead_zone_delta = 0;

	if (ev->type != GDK_BUTTON_PRESS) {
		if (_grabbed) {
			remove_modal_grab();
			_grabbed = false;
			StopGesture ();
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
		}
		return true;
	}

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	if (ev->button != 1 && ev->button != 2) {
		return false;
	}

	set_active_state (Gtkmm2ext::ExplicitActive);
	_tooltip.start_drag();
	add_modal_grab();
	_grabbed = true;
	StartGesture ();
	gdk_pointer_grab(ev->window,false,
			GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
			NULL,NULL,ev->time);
	return true;
}

bool
ArdourKnob::on_button_release_event (GdkEventButton *ev)
{
	_tooltip.stop_drag();
	_grabbed = false;
	StopGesture ();
	remove_modal_grab();
	gdk_pointer_ungrab (GDK_CURRENT_TIME);

	if ( (_grabbed_y == ev->y && _grabbed_x == ev->x) && Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {  //no move, shift-click sets to default
		boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
		if (!c) return false;
		c->set_value (c->normal(), Controllable::NoGroup);
		return true;
	}

	unset_active_state ();

	return true;
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

	c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourKnob::controllable_changed, this, false), gui_context());

	_normal = c->internal_to_interface(c->normal());

	controllable_changed();
}

void
ArdourKnob::controllable_changed (bool force_update)
{
	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
	if (!c) return;

	float val = c->get_interface (true);
	val = min( max(0.0f, val), 1.0f); // clamp

	if (val == _val && !force_update) {
		return;
	}

	_val = val;
	if (!_tooltip_prefix.empty()) {
		_tooltip.set_tip (_tooltip_prefix + c->get_user_string());
	}
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

	boost::shared_ptr<PBD::Controllable> c (binding_proxy.get_controllable ());
	if (c) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> (c));
	}

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ArdourKnob::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	set_dirty ();

	if (binding_proxy.get_controllable()) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> ());
	}

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


KnobPersistentTooltip::KnobPersistentTooltip (Gtk::Widget* w)
	: PersistentTooltip (w, true, 3)
	, _dragging (false)
{
}

void
KnobPersistentTooltip::start_drag ()
{
	_dragging = true;
}

void
KnobPersistentTooltip::stop_drag ()
{
	_dragging = false;
}

bool
KnobPersistentTooltip::dragging () const
{
	return _dragging;
}
