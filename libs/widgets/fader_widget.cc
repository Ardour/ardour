/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/keyboard.h"
#include "widgets/fader_widget.h"

using namespace Gtk;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;

FaderWidget::FaderWidget (Gtk::Adjustment& adj, int orien)
	: _adjustment (adj)
	, _tweaks (Tweaks(0))
	, _orien (orien)
	, _dragging (false)
	, _hovering (false)
	, _grab_window (0)
	, _grab_loc (0)
	, _grab_start (0)
{
	_default_value = _adjustment.get_value();

	add_events (Gdk::BUTTON_PRESS_MASK
	            | Gdk::BUTTON_RELEASE_MASK
	            | Gdk::POINTER_MOTION_MASK
	            | Gdk::SCROLL_MASK
	            | Gdk::ENTER_NOTIFY_MASK
	            | Gdk::LEAVE_NOTIFY_MASK
	           );

	_adjustment.signal_value_changed().connect (mem_fun (*this, &FaderWidget::adjustment_changed));
	_adjustment.signal_changed().connect (mem_fun (*this, &FaderWidget::adjustment_changed));
	signal_grab_broken_event ().connect (mem_fun (*this, &FaderWidget::on_grab_broken_event));
}

void
FaderWidget::set_tweaks (Tweaks t)
{
	bool need_redraw = false;
	if ((_tweaks & NoShowUnityLine) ^ (t & NoShowUnityLine)) {
		need_redraw = true;
	}
	_tweaks = t;
	if (need_redraw) {
		queue_draw();
	}
}

bool
FaderWidget::on_button_press_event (GdkEventButton* ev)
{
	if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS && (_tweaks & DoubleClickReset)) {
		_adjustment.set_value (_default_value);
		return true;
	}

	if (ev->type != GDK_BUTTON_PRESS) {
		if (_dragging) {
			remove_modal_grab();
			_dragging = false;
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			StopGesture (ev->state);
		}
		return (_tweaks & NoButtonForward) ? true : false;
	}

	if (ev->button != 1 && ev->button != 2) {
		return false;
	}

	add_modal_grab ();
	StartGesture (ev->state);
	_grab_loc = (_orien == VERT) ? ev->y : ev->x;
	_grab_start = (_orien == VERT) ? ev->y : ev->x;
	_grab_window = ev->window;
	_dragging = true;
	gdk_pointer_grab (ev->window,false,
			GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
			NULL,NULL,ev->time);

	if (ev->button == 2) {
		set_adjustment_from_event (ev);
	}

	return (_tweaks & NoButtonForward) ? true : false;
}

bool
FaderWidget::on_enter_notify_event (GdkEventCrossing*)
{
	_hovering = true;
	if (!(_tweaks & NoVerticalScroll)) {
		Keyboard::magic_widget_grab_focus ();
	}
	queue_draw ();
	return false;
}

bool
FaderWidget::on_leave_notify_event (GdkEventCrossing*)
{
	if (!_dragging) {
		_hovering = false;
		if (!(_tweaks & NoVerticalScroll)) {
			Keyboard::magic_widget_drop_focus();
		}
		queue_draw ();
	}
	return false;
}

bool
FaderWidget::on_button_release_event (GdkEventButton* ev)
{
	double ev_pos = (_orien == VERT) ? ev->y : ev->x;

	switch (ev->button) {
	case 1:
		if (_dragging) {
			remove_modal_grab();
			_dragging = false;
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			StopGesture (ev->state);

			if (!_hovering) {
				if (!(_tweaks & NoVerticalScroll)) {
					Keyboard::magic_widget_drop_focus();
				}
				queue_draw ();
			}

			if (ev_pos == _grab_start) {
				/* no motion - just a click */
				ev_pos = rint(ev_pos);

				if (ev->state & Keyboard::TertiaryModifier) {
					_adjustment.set_value (_default_value);
				} else if (ev->state & Keyboard::GainFineScaleModifier) {
					_adjustment.set_value (_adjustment.get_lower());
#if 0 // ignore clicks
				} else if (ev_pos == slider_pos) {
					; // click on current position, no move.
				} else if ((_orien == VERT && ev_pos < slider_pos) || (_orien == HORIZ && ev_pos > slider_pos)) {
					/* above the current display height, remember X Window coords */
					_adjustment.set_value (_adjustment.get_value() + _adjustment.get_step_increment());
				} else {
					_adjustment.set_value (_adjustment.get_value() - _adjustment.get_step_increment());
#endif
				}
			}
			return true;
		}
		break;

	case 2:
		if (_dragging) {
			remove_modal_grab();
			_dragging = false;
			StopGesture (ev->state);
			set_adjustment_from_event (ev);
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}

bool
FaderWidget::on_scroll_event (GdkEventScroll* ev)
{
	double increment = 0;
	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			increment = 0.05 * _adjustment.get_step_increment();
		} else {
			increment = _adjustment.get_step_increment();
		}
	} else {
		increment = _adjustment.get_page_increment();
	}

	bool vertical = false;
	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_DOWN:
			vertical = !(ev->state & Keyboard::ScrollHorizontalModifier);
			break;
		default:
			break;
	}
	if ((_orien == VERT && !vertical) ||
	    ((_tweaks & NoVerticalScroll) && vertical)) {
		return false;
	}

	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_RIGHT:
			_adjustment.set_value (_adjustment.get_value() + increment);
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_LEFT:
			_adjustment.set_value (_adjustment.get_value() - increment);
			break;
		default:
			return false;
	}

	return true;
}

void
FaderWidget::adjustment_changed ()
{
	queue_draw ();
}

bool
FaderWidget::on_grab_broken_event (GdkEventGrabBroken* ev)
{
	if (_dragging) {
		remove_modal_grab();
		_dragging = false;
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		StopGesture (0);
	}
	return (_tweaks & NoButtonForward) ? true : false;
}
