/*
    Copyright (C) 1998-99 Paul Barton-Davis
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

    $Id: motionfeedback.cc,v 1.5 2004/03/01 03:44:19 pauld Exp $
*/

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <unistd.h>
#include <stdio.h> /* for snprintf, grrr */

#include <gdk/gdkkeysyms.h>
#include <gtkmm.h>

#include "gtkmm2ext/motionfeedback.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/prolooks-helpers.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

MotionFeedback::MotionFeedback (Glib::RefPtr<Gdk::Pixbuf> pix,
				Type t,
				const char *widget_name, 
				Adjustment *adj,
				bool with_numeric_display, 
                                int subw, 
                                int subh) 
	: type (t)
        , value_packer (0)
        , value (0)
	, pixbuf (pix)
        , subwidth (subw)
        , subheight (subh)
{
	char value_name[1024];

	if (adj == NULL) {
	    i_own_my_adjustment = true;
	    set_adjustment (new Adjustment (0, 0, 10000, 1, 10, 0));
	} else {
	    i_own_my_adjustment = false;
	    set_adjustment (adj);
	}

        default_value = adjustment->get_value();

        HBox* hpacker = manage (new HBox);
        hpacker->pack_start (pixwin, true, false);
        hpacker->show ();
	pack_start (*hpacker, false, false);
	pixwin.show ();

	if (with_numeric_display) {

                value_packer = new HBox;
		value = new SpinButton (*adjustment);
                value_packer->pack_start (*value, false, false);

		if (step_inc < 1) {
			value->set_digits (abs ((int) ceil (log10 (step_inc))));
		}
		
		pack_start (*value_packer, false, false);

		if (widget_name) {
			snprintf (value_name, sizeof(value_name), "%sValue", widget_name);
			value->set_name (value_name);
		}

		value->show ();
	}

	adjustment->signal_value_changed().connect (mem_fun (*this, &MotionFeedback::adjustment_changed));

	pixwin.set_events (Gdk::BUTTON_PRESS_MASK|
			   Gdk::BUTTON_RELEASE_MASK|
			   Gdk::POINTER_MOTION_MASK|
			   Gdk::ENTER_NOTIFY_MASK|
			   Gdk::LEAVE_NOTIFY_MASK|
			   Gdk::SCROLL_MASK|
			   Gdk::KEY_PRESS_MASK|
			   Gdk::KEY_RELEASE_MASK);

	pixwin.set_flags (CAN_FOCUS);

	/* Proxy all important events on the pixwin to ourselves */

	pixwin.signal_button_press_event().connect(mem_fun (*this,&MotionFeedback::pixwin_button_press_event));
	pixwin.signal_button_release_event().connect(mem_fun (*this,&MotionFeedback::pixwin_button_release_event));
	pixwin.signal_motion_notify_event().connect(mem_fun (*this,&MotionFeedback::pixwin_motion_notify_event));
	pixwin.signal_enter_notify_event().connect(mem_fun (*this,&MotionFeedback::pixwin_enter_notify_event));
	pixwin.signal_leave_notify_event().connect(mem_fun (*this,&MotionFeedback::pixwin_leave_notify_event));
	pixwin.signal_key_press_event().connect(mem_fun (*this,&MotionFeedback::pixwin_key_press_event));
	pixwin.signal_scroll_event().connect(mem_fun (*this,&MotionFeedback::pixwin_scroll_event));
	pixwin.signal_expose_event().connect(mem_fun (*this,&MotionFeedback::pixwin_expose_event), true);
	pixwin.signal_size_request().connect(mem_fun (*this,&MotionFeedback::pixwin_size_request));
	pixwin.signal_realize().connect(mem_fun (*this,&MotionFeedback::pixwin_realized));
}

MotionFeedback::~MotionFeedback()

{
	if (i_own_my_adjustment) {
		delete adjustment;
	}

	delete value;
        delete value_packer;
}

void
MotionFeedback::set_adjustment (Adjustment *adj)
{
	adjustment = adj;

	if (value) {
		value->set_adjustment (*adj);
	}

	_lower = adj->get_lower();
	_upper = adj->get_upper();
	_range = _upper - _lower;
	step_inc = adj->get_step_increment();
	page_inc = adj->get_page_increment();
}

bool
MotionFeedback::pixwin_button_press_event (GdkEventButton *ev) 
{ 
        if (binding_proxy.button_press_handler (ev)) {
                return true;
	}

	switch (ev->button) {
	case 2:
		return FALSE;  /* XXX why ? */

	case 1:
		grab_is_fine = false;
		break;
	case 3:
		grab_is_fine = true;
		break;
	}

	gtk_grab_add(GTK_WIDGET(pixwin.gobj()));
	grabbed_y = ev->y_root;
	grabbed_x = ev->x_root;

	/* XXX should we return TRUE ? */

	return FALSE;
}

bool
MotionFeedback::pixwin_button_release_event (GdkEventButton *ev) 
{ 
	switch (ev->button) {
	case 1:
		if (pixwin.has_grab()) {
			if (!grab_is_fine) {
				gtk_grab_remove
					(GTK_WIDGET(pixwin.gobj()));
			}
		}
                if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
                        /* shift click back to the default */
                        adjustment->set_value (default_value);
                        return true;
                }
		break;
		
	case 3:
		if (pixwin.has_grab()) {
			if (grab_is_fine) {
				gtk_grab_remove
					(GTK_WIDGET(pixwin.gobj()));
			}
		}
		break;
	}

	return VBox::on_button_release_event (ev); 
}

bool
MotionFeedback::pixwin_motion_notify_event (GdkEventMotion *ev) 
{ 
	gfloat multiplier;
	gfloat x_delta;
	gfloat y_delta;

	if (!pixwin.has_grab()) {
		return VBox::on_motion_notify_event (ev); 
	}

	multiplier = ((ev->state & Keyboard::TertiaryModifier) ? 100 : 1) *
                ((ev->state & Keyboard::SecondaryModifier) ? 10 : 1) * 
                ((ev->state & Keyboard::PrimaryModifier) ? 2 : 1);


        if (ev->state & Gdk::BUTTON1_MASK) {

                y_delta = grabbed_y - ev->y_root;
                grabbed_y = ev->y_root;
                
                x_delta = ev->x_root - grabbed_x;
                
                if (y_delta == 0) return TRUE;
                
                y_delta *= 1 + (x_delta/100);
                y_delta *= multiplier;
                y_delta /= 10;
                
                adjustment->set_value (adjustment->get_value() + 
                                       ((grab_is_fine ? step_inc : page_inc) * y_delta));
                
        } else if (ev->state & Gdk::BUTTON3_MASK) {

                double range = adjustment->get_upper() - adjustment->get_lower();
                double x = ev->x - subwidth/2;
                double y = - ev->y + subwidth/2;
                double angle = std::atan2 (y, x) / M_PI;
                
                if (angle < -0.5) {
                        angle += 2.0;
                }
                
                angle = -(2.0/3.0) * (angle - 1.25);
                angle *= range;
                angle *= multiplier;
                angle += adjustment->get_lower();
                
                adjustment->set_value (angle);
        }


	return true;
}

bool
MotionFeedback::pixwin_enter_notify_event (GdkEventCrossing *ev) 
{
	pixwin.grab_focus();
	return false;
}

bool
MotionFeedback::pixwin_leave_notify_event (GdkEventCrossing *ev) 
{
	pixwin.unset_flags (HAS_FOCUS);
	return false;
}

bool
MotionFeedback::pixwin_key_press_event (GdkEventKey *ev) 
{
	bool retval = false;
	gfloat curval;
	gfloat multiplier;

	multiplier = ((ev->state & Keyboard::TertiaryModifier) ? 100 : 1) *
                ((ev->state & Keyboard::SecondaryModifier) ? 10 : 1) * 
                ((ev->state & Keyboard::PrimaryModifier) ? 2 : 1);

	switch (ev->keyval) {
	case GDK_Page_Up:
	        retval = true;
		curval = adjustment->get_value();
		adjustment->set_value (curval + (multiplier * page_inc));
		break;

	case GDK_Page_Down:
	        retval = true;
		curval = adjustment->get_value();
		adjustment->set_value (curval - (multiplier * page_inc));
		break;

	case GDK_Up:
	        retval = true;
		curval = adjustment->get_value();
		adjustment->set_value (curval + (multiplier * step_inc));
		break;

	case GDK_Down:
	        retval = true;
		curval = adjustment->get_value();
		adjustment->set_value (curval - (multiplier * step_inc));
		break;

	case GDK_Home:
	        retval = true;
		adjustment->set_value (_lower);
		break;

	case GDK_End:
	        retval = true;
		adjustment->set_value (_upper);
		break;
	}
	
	return retval;
}

void
MotionFeedback::adjustment_changed ()
{
	pixwin.queue_draw ();
}

void
MotionFeedback::core_draw (cairo_t* cr, int phase, double size, double progress_width, double xorigin, double yorigin,
                           const GdkColor* bright, const GdkColor* dark)
{
	double xc;
	double yc;
	double start_angle;
	double end_angle;
	double value_angle;
	double value;
	double value_x;
	double value_y;
	double start_angle_x;
	double start_angle_y;
	double end_angle_x;
	double end_angle_y;
	double progress_radius;
	double progress_radius_inner;
	double progress_radius_outer;
	double knob_disc_radius;
	cairo_pattern_t* pattern;
	double progress_rim_width;
	cairo_pattern_t* progress_shine;
	double degrees;
	cairo_pattern_t* knob_ripples;
        double pxs;
        double pys;

	g_return_if_fail (cr != NULL);
        
	progress_radius = 40.0;
	progress_radius_inner = progress_radius - (progress_width / 2.0);
	progress_radius_outer = progress_radius + (progress_width / 2.0);
	knob_disc_radius = progress_radius_inner - 5.0;

        const double pad = 2.0; /* line width for boundary of progress ring */
        const double actual_width = ((2.0 * pad) + (2.0 * progress_radius_outer));
        const double scale_factor = size / actual_width;

        /* knob center is at middle of the area bounded by (xorigin,yorigin) and (xorigin+size, yorigin+size)
           but the coordinates will be scaled by the scale factor when cairo uses them so first
           adjust them by the reciprocal of the scale factor.
        */

	xc = (xorigin + (size / 2.0)) * (1.0/scale_factor);
        yc = (yorigin + (size / 2.0)) * (1.0/scale_factor);

        pxs = xorigin * (1.0/scale_factor);
        pys = yorigin * (1.0/scale_factor);

	start_angle = 0.0;
	end_angle = 0.0;
	value_angle = 0.0;
	value = (phase * 1.0) / (65 - 1);

        start_angle = ((180 - 65) * G_PI) / 180;
        end_angle = ((360 + 65) * G_PI) / 180;

	value_angle = start_angle + (value * (end_angle - start_angle));
	value_x = cos (value_angle);
	value_y = sin (value_angle);
	start_angle_x = cos (start_angle);
	start_angle_y = sin (start_angle);
	end_angle_x = cos (end_angle);
	end_angle_y = sin (end_angle);

	cairo_scale (cr, scale_factor, scale_factor);

        pattern = prolooks_create_gradient_str (pxs + 32.0, pys + 16.0, pxs + 75.0, pys + 16.0, "#d4c8b9", "#ae977b", 1.0, 1.0);
        cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_set_line_width (cr, 2.0);
	cairo_arc (cr, xc, yc, 31.5, 0.0, 2 * G_PI);
	cairo_stroke (cr);

        pattern = prolooks_create_gradient_str (pxs + 20.0, pys + 20.0, pxs + 89.0, pys + 87.0, "#2f2f4c", "#090a0d", 1.0, 1.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_set_line_width (cr, progress_width);
	cairo_arc (cr, xc, yc, progress_radius, start_angle, end_angle);
	cairo_stroke (cr);

        pattern = prolooks_create_gradient (pxs + 20.0, pys + 20.0, pxs + 89.0, pys + 87.0, bright, dark, 1.0, 1.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_set_line_width (cr, progress_width);
	cairo_arc (cr, xc, yc, progress_radius, start_angle, value_angle);
	cairo_stroke (cr);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	progress_rim_width = 2.0;
	cairo_set_line_width (cr, progress_rim_width);
        pattern = prolooks_create_gradient_str (pxs + 18.0, pys + 79.0, pxs + 35.0, pys + 79.0, "#dfd5c9", "#dfd5c9", 1.0, 0.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_move_to (cr, xc + (progress_radius_outer * start_angle_x), yc + (progress_radius_outer * start_angle_y));
	cairo_line_to (cr, xc + (progress_radius_inner * start_angle_x), yc + (progress_radius_inner * start_angle_y));
	cairo_stroke (cr);

	prolooks_set_source_color_string (cr, "#000000", 1.0);
	cairo_move_to (cr, xc + (progress_radius_outer * end_angle_x), yc + (progress_radius_outer * end_angle_y));
	cairo_line_to (cr, xc + (progress_radius_inner * end_angle_x), yc + (progress_radius_inner * end_angle_y));
	cairo_stroke (cr);

        // pattern = prolooks_create_gradient_str (95.0, 6.0, 5.0, 44.0, "#dfd5c9", "#b0a090", 1.0, 1.0);
        pattern = prolooks_create_gradient_str (pxs + 95.0, pys + 6.0, pxs + 5.0, pys + 44.0, "#000000", "#000000", 1.0, 1.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_arc (cr, xc, yc, progress_radius_outer, start_angle, end_angle);
	cairo_stroke (cr);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
        pattern = prolooks_create_gradient (pxs + 20.0, pys + 20.0, pxs + 89.0, pys + 87.0, bright, dark, 0.25, 0.25);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_set_line_width (cr, progress_width);
	cairo_arc (cr, xc, yc, progress_radius, start_angle, value_angle + (G_PI / 180.0));
	cairo_stroke (cr);

        progress_shine = prolooks_create_gradient_str (pxs + 89.0, pys + 73.0, pxs + 34.0, pys + 16.0, "#ffffff", "#ffffff", 0.3, 0.04);
        cairo_pattern_add_color_stop_rgba (progress_shine, 0.5, 1.0, 1.0, 1.0, 0.0);
        if (size > 50) {
                cairo_pattern_add_color_stop_rgba (progress_shine, 0.75, 1.0, 1.0, 1.0, 0.3);
        } else {
                cairo_pattern_add_color_stop_rgba (progress_shine, 0.75, 1.0, 1.0, 1.0, 0.2);
        }
        cairo_set_source (cr, progress_shine);
        cairo_set_line_width (cr, progress_width);
        cairo_arc (cr, xc, yc, progress_radius, start_angle, end_angle);
        cairo_stroke (cr);
        cairo_pattern_destroy (progress_shine);

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_arc (cr, xc, yc, progress_radius_inner, 0.0, 2 * G_PI);
        pattern = prolooks_create_gradient_str (pxs + 35.0, pys + 31.0, pxs + 75.0, pys + 72.0, "#68625c", "#44494b", 1.0, 1.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_fill (cr);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_arc (cr, xc, yc, progress_radius_inner, 0.0, 2 * G_PI);
	cairo_stroke (cr);

        pattern = prolooks_create_gradient_str (pxs + 42.0, pys + 34.0, pxs + 68.0, pys + 70.0, "#e7ecef", "#9cafb8", 1.0, 1.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_arc (cr, xc, yc, knob_disc_radius, 0.0, 2 * G_PI);
	cairo_fill (cr);

	cairo_set_line_width (cr, 2.0);
	degrees = G_PI / 180.0;
        pattern = prolooks_create_gradient_str (pxs + 38.0, pys + 34.0, pxs + 70.0, pys + 68.0, "#ffffff", "#caddf2", 0.2, 0.2);
        cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_move_to (cr, xc, yc);
	cairo_arc (cr, xc, yc, knob_disc_radius - 1, (-154) * degrees, (-120) * degrees);
	cairo_move_to (cr, xc, yc);
	cairo_arc (cr, xc, yc, knob_disc_radius - 1, (G_PI / 2.0) - (60 * degrees), (G_PI / 2.0) - (29 * degrees));
	cairo_fill (cr);

        pattern = prolooks_create_gradient_str (pxs + 50.0, pys + 40.0, pxs + 62.0, pys + 60.0, "#a1adb6", "#47535c", 0.07, 0.15);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_move_to (cr, xc, yc);
	cairo_arc (cr, xc, yc, knob_disc_radius - 1, (-67) * degrees, (-27) * degrees);
	cairo_move_to (cr, xc, yc);
	cairo_arc (cr, xc, yc, knob_disc_radius - 1, G_PI - (67 * degrees), G_PI - (27 * degrees));
	cairo_fill (cr);

	knob_ripples = cairo_pattern_create_radial (xc, yc, 0.0, xc, yc, 4.0);
	prolooks_add_color_stop_str (knob_ripples, 0.0, "#e7ecef", 0.05);
	prolooks_add_color_stop_str (knob_ripples, 0.5, "#58717d", 0.05);
	prolooks_add_color_stop_str (knob_ripples, 0.75, "#d1d9de", 0.05);
	prolooks_add_color_stop_str (knob_ripples, 1.0, "#5d7682", 0.05);
	cairo_pattern_set_extend (knob_ripples, CAIRO_EXTEND_REPEAT);
	cairo_set_line_width (cr, 0.0);
	cairo_set_source (cr, knob_ripples);
	cairo_arc (cr, xc, yc, knob_disc_radius, 0.0, 2 * G_PI);
	cairo_fill (cr);

	cairo_save (cr);
	cairo_translate (cr, xc + (knob_disc_radius * value_x), yc + (knob_disc_radius * value_y));
	cairo_rotate (cr, value_angle - G_PI);
        pattern = prolooks_create_gradient_str (pxs + 16.0, pys + -2.0, pxs + 9.0, pys + 13.0, "#e7ecef", "#9cafb8", 0.8, 0.8);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_move_to (cr, 0.0, 4.0);
	cairo_line_to (cr, 17.0, 4.0);
	cairo_curve_to (cr, 19.0, 4.0, 21.0, 2.0, 21.0, 0.0);
	cairo_curve_to (cr, 21.0, -2.0, 19.0, -4.0, 17.0, -4.0);
	cairo_line_to (cr, 0.0, -4.0);
	cairo_close_path (cr);
	cairo_fill (cr);

        pattern = prolooks_create_gradient_str (pxs + 9.0, pys + -2.0, pxs + 9.0, pys + 2.0, "#68625c", "#44494b", 1.0, 1.0);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_move_to (cr, 0.0, 2.0);
	cairo_line_to (cr, 16.0, 2.0);
	cairo_curve_to (cr, 17.0, 2.0, 18.0, 1.0, 18.0, 0.0);
	cairo_curve_to (cr, 18.0, -1.0, 17.0, -2.0, 16.0, -2.0);
	cairo_line_to (cr, 0.0, -2.0);
	cairo_close_path (cr);
	cairo_fill (cr);

	cairo_restore (cr);
	cairo_set_line_width (cr, 2.0);
        pattern = prolooks_create_gradient_str (pxs + 38.0, pys + 32.0, pxs + 70.0, pys + 67.0, "#3d3d3d", "#000000", 1.0, 1.0); 
        cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	cairo_arc (cr, xc, yc, knob_disc_radius, 0.0, 2 * G_PI);
	cairo_stroke (cr);

	cairo_pattern_destroy (knob_ripples);
}

bool
MotionFeedback::pixwin_expose_event (GdkEventExpose* ev)
{
	GdkWindow *window = pixwin.get_window()->gobj();
	GtkAdjustment* adj = adjustment->gobj();

	int phase = (int)((adj->value - adj->lower) * 64 / 
			  (adj->upper - adj->lower));

	// skip middle phase except for true middle value

	if (type == Rotary && phase == 32) {
		double pt = (adj->value - adj->lower) * 2.0 / 
			(adj->upper - adj->lower) - 1.0;
		if (pt < 0)
			phase = 31;
		if (pt > 0)
			phase = 33;
	}

	// endless knob: skip 90deg highlights unless the value is really a multiple of 90deg

	if (type == Endless && !(phase % 16)) {
		if (phase == 64) {
			phase = 0;
                }

		double nom = adj->lower + phase * (adj->upper - adj->lower) / 64.0;
		double diff = (adj->value - nom) / (adj->upper - adj->lower);

		if (diff > 0.0001)
			phase = (phase + 1) % 64;
		if (diff < -0.0001)
			phase = (phase + 63) % 64;
	}

        phase = std::min (phase, 63);

        GtkWidget* widget = GTK_WIDGET(pixwin.gobj());
        gdk_draw_pixbuf (GDK_DRAWABLE(window), widget->style->fg_gc[0], 
                         pixbuf->gobj(), 
                         phase * subwidth, type * subheight, 
                         0, 0, subwidth, subheight, GDK_RGB_DITHER_NORMAL, 0, 0);

	return true;
}

bool
MotionFeedback::pixwin_scroll_event (GdkEventScroll* ev)
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
		adjustment->set_value (adjustment->get_value() + (scale * adjustment->get_step_increment()));
		break;

	case GDK_SCROLL_DOWN:
	case GDK_SCROLL_LEFT:
		adjustment->set_value (adjustment->get_value() - (scale * adjustment->get_step_increment()));
		break;
	}

        return true;
}

void
MotionFeedback::pixwin_size_request (GtkRequisition* req)
{
	req->width = subwidth;
	req->height = subheight;
}

void
MotionFeedback::pixwin_realized ()
{
        set_lamp_color (Gdk::Color ("#b9feff"));
}

void
MotionFeedback::set_lamp_color (const Gdk::Color& c)
{
	GdkColor col2 = {0,0,0,0};
	GdkColor col3 = {0,0,0,0};

	_lamp_color = c;
	lamp_hsv = prolooks_hsv_new_for_gdk_color (_lamp_color.gobj());
	lamp_bright = (prolooks_hsv_to_gdk_color (lamp_hsv, &col2), col2);
	prolooks_hsv_set_saturation (lamp_hsv, 0.66);
	prolooks_hsv_set_value (lamp_hsv, 0.67);
	lamp_dark = (prolooks_hsv_to_gdk_color (lamp_hsv, &col3), col3);
}

Glib::RefPtr<Gdk::Pixbuf>
MotionFeedback::render_pixbuf (int size)
{
        Glib::RefPtr<Gdk::Pixbuf> pixbuf;
        char path[32];
        int fd;

        snprintf (path, sizeof (path), "/tmp/mfimg%dXXXXXX", size);
        
        if ((fd = mkstemp (path)) < 0) {
                return pixbuf;
        }
        
	GdkColor col2 = {0,0,0,0};
	GdkColor col3 = {0,0,0,0};
        Gdk::Color base ("#b9feff");
        GdkColor dark;
        GdkColor bright;
        ProlooksHSV* hsv;

	hsv = prolooks_hsv_new_for_gdk_color (base.gobj());
	bright = (prolooks_hsv_to_gdk_color (hsv, &col2), col2);
	prolooks_hsv_set_saturation (hsv, 0.66);
	prolooks_hsv_set_value (hsv, 0.67);
	dark = (prolooks_hsv_to_gdk_color (hsv, &col3), col3);

        cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size * 64, size);
        cairo_t* cr = cairo_create (surface);

        for (int i = 0; i < 64; ++i) {
                cairo_save (cr);
                core_draw (cr, i, size, 20, size*i, 0, &bright, &dark);
                cairo_restore (cr);
        }

        if (cairo_surface_write_to_png (surface, path) != CAIRO_STATUS_SUCCESS) {
                std::cerr << "could not save image set to " << path << std::endl;
                return pixbuf;
        }

        close (fd);

        cairo_destroy (cr);
        cairo_surface_destroy (surface);

	try {
		pixbuf = Gdk::Pixbuf::create_from_file (path);
	} catch (const Gdk::PixbufError &e) {
                std::cerr << "Caught PixbufError: " << e.what() << std::endl;
                unlink (path);
                throw;
	} catch (...) {
                unlink (path);
		g_message("Caught ... ");
                throw;
	}

        unlink (path);

        return pixbuf;
} 

void
MotionFeedback::set_controllable (boost::shared_ptr<PBD::Controllable> c)
{
        binding_proxy.set_controllable (c);
}

boost::shared_ptr<PBD::Controllable>
MotionFeedback::controllable () const 
{
        return binding_proxy.get_controllable ();
}
       
