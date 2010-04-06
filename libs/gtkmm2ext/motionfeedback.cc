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
#include <unistd.h>
#include <stdio.h> /* for snprintf, grrr */

#include <gdk/gdkkeysyms.h>
#include <gtkmm.h>

#include "gtkmm2ext/motionfeedback.h"
#include "gtkmm2ext/keyboard.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

MotionFeedback::MotionFeedback (Glib::RefPtr<Gdk::Pixbuf> pix,
				Type t,
				const char *widget_name, 
				Adjustment *adj,
				bool with_numeric_display, int subw, int subh) 
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

	if(!pixwin.has_grab()) {
		return VBox::on_motion_notify_event (ev); 
	}

	multiplier = ((ev->state & Keyboard::TertiaryModifier) ? 100 : 1) *
                ((ev->state & Keyboard::SecondaryModifier) ? 10 : 1) * 
                ((ev->state & Keyboard::PrimaryModifier) ? 2 : 1);

	y_delta = grabbed_y - ev->y_root;
	grabbed_y = ev->y_root;

	x_delta = ev->x_root - grabbed_x;

	if (y_delta == 0) return TRUE;

	y_delta *= 1 + (x_delta/100);
	y_delta *= multiplier;
	y_delta /= 10;

	adjustment->set_value (adjustment->get_value() + 
			       ((grab_is_fine ? step_inc : page_inc) * y_delta));

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

bool
MotionFeedback::pixwin_expose_event (GdkEventExpose* ev)
{
	GtkWidget* widget = GTK_WIDGET(pixwin.gobj());
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
