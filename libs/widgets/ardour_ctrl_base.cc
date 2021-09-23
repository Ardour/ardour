/*
 * Copyright (C) 2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2021 Robin Gareus <robin@gareus.org>
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

#include "widgets/ardour_ctrl_base.h"
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

ArdourCtrlBase::ArdourCtrlBase (Flags flags)
	: _req_width (0)
	, _req_height (0)
	, _hovering (false)
	, _val (0)
	, _normal (0)
	, _flags (flags)
	, _tooltip (this)
	, _grabbed_x (0)
	, _grabbed_y (0)
	, _dead_zone_delta (0)
{
	UIConfigurationBase::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ArdourCtrlBase::color_handler));

#ifdef VBM
	_flags = (Flags)(static_cast <int>(_flags) | (int)NoHorizontal);
#endif
}

ArdourCtrlBase::~ArdourCtrlBase()
{
}

void
ArdourCtrlBase::set_size_request (int w, int h)
{
	if (_req_width == w && _req_height == h) {
		return;
	}
	_req_width = w;
	_req_height = h;
	queue_resize ();
}

void
ArdourCtrlBase::on_size_request (Gtk::Requisition* req)
{
	req->width = _req_width;
	req->height = _req_height;
	if (req->width < 1) { req->width = 13; }
	if (req->height < 1) { req->height = 13; }
}

bool
ArdourCtrlBase::on_scroll_event (GdkEventScroll* ev)
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
	if (_flags & Reverse) {
		scale *= -1;
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
ArdourCtrlBase::on_motion_notify_event (GdkEventMotion *ev)
{
	if (!(ev->state & Gdk::BUTTON1_MASK)) {
		return true;
	}

	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
	if (!c) {
		return true;
	}


	/* scale the adjustment based on keyboard modifiers & GUI size */
	const float ui_scale = max (1.f, UIConfigurationBase::instance().get_ui_scale());
	float scale = 0.0025 / ui_scale;

	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale *= 0.01;
		} else {
			scale *= 0.10;
		}
	}

	/* calculate the travel of the mouse */
	int delta = 0;
	if ((_flags & NoVertical) == 0) {
		delta += (_grabbed_y - ev->y);
	}
	if ((_flags & NoHorizontal) == 0) {
		delta -= (_grabbed_x - ev->x);
	}
	if (delta == 0) {
		return true;
	}
	if (_flags & Reverse) {
		delta *= -1;
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
ArdourCtrlBase::on_button_press_event (GdkEventButton *ev)
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
ArdourCtrlBase::on_button_release_event (GdkEventButton *ev)
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
ArdourCtrlBase::color_handler ()
{
	set_dirty ();
}

void
ArdourCtrlBase::set_controllable (boost::shared_ptr<Controllable> c)
{
	watch_connection.disconnect ();  //stop watching the old controllable

	if (!c) return;

	binding_proxy.set_controllable (c);

	c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourCtrlBase::controllable_changed, this, false), gui_context());

	_normal = c->internal_to_interface(c->normal());

	controllable_changed();
}

void
ArdourCtrlBase::controllable_changed (bool force_update)
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
ArdourCtrlBase::on_style_changed (const RefPtr<Gtk::Style>&)
{
	set_dirty ();
}

void
ArdourCtrlBase::on_name_changed ()
{
	set_dirty ();
}


void
ArdourCtrlBase::set_active_state (Gtkmm2ext::ActiveState s)
{
	if (_active_state != s)
		CairoWidget::set_active_state (s);
}

void
ArdourCtrlBase::set_visual_state (Gtkmm2ext::VisualState s)
{
	if (_visual_state != s)
		CairoWidget::set_visual_state (s);
}


bool
ArdourCtrlBase::on_focus_in_event (GdkEventFocus* ev)
{
	set_dirty ();
	return CairoWidget::on_focus_in_event (ev);
}

bool
ArdourCtrlBase::on_focus_out_event (GdkEventFocus* ev)
{
	set_dirty ();
	return CairoWidget::on_focus_out_event (ev);
}

bool
ArdourCtrlBase::on_enter_notify_event (GdkEventCrossing* ev)
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
ArdourCtrlBase::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	set_dirty ();

	if (binding_proxy.get_controllable()) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> ());
	}

	return CairoWidget::on_leave_notify_event (ev);
}

CtrlPersistentTooltip::CtrlPersistentTooltip (Gtk::Widget* w)
	: PersistentTooltip (w, true, 3)
	, _dragging (false)
{
}

void
CtrlPersistentTooltip::start_drag ()
{
	_dragging = true;
}

void
CtrlPersistentTooltip::stop_drag ()
{
	_dragging = false;
}

bool
CtrlPersistentTooltip::dragging () const
{
	return _dragging;
}
