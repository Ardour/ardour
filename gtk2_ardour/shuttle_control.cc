/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#define BASELINESTRETCH (1.25)

#include <algorithm>

#include <cairo.h>

#include "pbd/unwind.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "actions.h"
#include "rgb_macros.h"
#include "shuttle_control.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::max;
using std::min;

gboolean
qt (gboolean, gint, gint, gboolean, Gtk::Tooltip*, gpointer)
{
	return FALSE;
}

ShuttleInfoButton::~ShuttleInfoButton ()
{
	delete disp_context_menu;
}

ShuttleInfoButton::ShuttleInfoButton ()
	: disp_context_menu (0)
	, _ignore_change (false)
{
	set_layout_font (UIConfiguration::instance ().get_NormalFont ());
	set_tooltip (*this, _("Speed Display (Context-click for options)"));
	set_visual_state (Gtkmm2ext::NoVisualState);
	set_elements (ArdourButton::Text);
	parameter_changed ("shuttle-units");

	Config->ParameterChanged.connect (parameter_connection, MISSING_INVALIDATOR, boost::bind (&ShuttleInfoButton::parameter_changed, this, _1), gui_context ());

	add_events (Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::POINTER_MOTION_MASK | Gdk::SCROLL_MASK);
}

void
ShuttleInfoButton::set_shuttle_units (ShuttleUnits s)
{
	if (_ignore_change) {
		return;
	}
	Config->set_shuttle_units (s);
}

void
ShuttleInfoButton::parameter_changed (std::string p)
{
	/* units changed; recreate the menu when it is next opened to show the current selection*/
	if (p == "shuttle-units") {
		delete disp_context_menu;
		disp_context_menu = 0;
		if (Config->get_shuttle_units() == Percentage) {
			set_sizing_text (S_("LogestShuttle|> 888.9%"));
		} else {
			set_sizing_text (S_("LogestShuttle|+00 st"));
		}
	}
}

bool
ShuttleInfoButton::on_button_press_event (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		if (disp_context_menu == 0) {
			build_disp_context_menu ();
		}
		disp_context_menu->popup (ev->button, ev->time);
		return true;
	}

	return true;
}

void
ShuttleInfoButton::build_disp_context_menu ()
{
	PBD::Unwinder<bool> uw (_ignore_change, true);

	using namespace Menu_Helpers;

	disp_context_menu = new Gtk::Menu ();
	MenuList& items   = disp_context_menu->items ();

	RadioMenuItem::Group units_group;

	items.push_back (RadioMenuElem (units_group, _("Percent"), sigc::bind (sigc::mem_fun (*this, &ShuttleInfoButton::set_shuttle_units), Percentage)));
	if (Config->get_shuttle_units () == Percentage) {
		static_cast<RadioMenuItem*> (&items.back ())->set_active ();
	}
	items.push_back (RadioMenuElem (units_group, _("Semitones"), sigc::bind (sigc::mem_fun (*this, &ShuttleInfoButton::set_shuttle_units), Semitones)));
	if (Config->get_shuttle_units () == Semitones) {
		static_cast<RadioMenuItem*> (&items.back ())->set_active ();
	}
}

/* ****************************************************************************/

ShuttleControl::ShuttleControl ()
	: _controllable (new ShuttleControllable (*this))
	, binding_proxy (_controllable)
{
	set_tooltip (*this, _("Shuttle speed control (Context-click for options)"));

	pattern               = 0;
	shine_pattern         = 0;
	last_shuttle_request  = 0;
	last_speed_displayed  = -99999999;
	shuttle_grabbed       = false;
	shuttle_speed_on_grab = 0;
	shuttle_fract         = 0.0;
	shuttle_max_speed     = Config->get_max_transport_speed ();
	shuttle_context_menu  = 0;
	_hovering             = false;
	_ignore_change        = false;

	set_flags (CAN_FOCUS);
	add_events (Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::POINTER_MOTION_MASK | Gdk::SCROLL_MASK);
	set_name (X_("ShuttleControl"));

	ensure_style ();

	shuttle_max_speed = Config->get_shuttle_max_speed ();

	if (shuttle_max_speed >= Config->get_max_transport_speed ()) {
		shuttle_max_speed = Config->get_max_transport_speed ();
	} else if (shuttle_max_speed >= 6.f) {
		shuttle_max_speed = 6.0f;
	} else if (shuttle_max_speed >= 4.f) {
		shuttle_max_speed = 4.0f;
	} else if (shuttle_max_speed >= 3.f) {
		shuttle_max_speed = 3.0f;
	} else if (shuttle_max_speed >= 2.f) {
		shuttle_max_speed = 2.0f;
	} else {
		shuttle_max_speed = 1.5f;
	}

	Config->ParameterChanged.connect (parameter_connection, MISSING_INVALIDATOR, boost::bind (&ShuttleControl::parameter_changed, this, _1), gui_context ());
	UIConfiguration::instance ().ColorsChanged.connect (sigc::mem_fun (*this, &ShuttleControl::set_colors));
	Timers::blink_connect (sigc::mem_fun (*this, &ShuttleControl::do_blink));

	set_tooltip (_vari_button, _("Varispeed: change the default playback and recording speed"));

	_vari_button.set_name ("vari button");
	_vari_button.set_text (S_("VariSpeed|VS"));
	_vari_button.signal_clicked.connect (sigc::mem_fun (*this, &ShuttleControl::varispeed_button_clicked));
	_vari_button.signal_scroll_event ().connect (sigc::mem_fun (*this, &ShuttleControl::varispeed_button_scroll_event), false);

	/* gtkmm 2.4: the C++ wrapper doesn't work */
	g_signal_connect ((GObject*)gobj (), "query-tooltip", G_CALLBACK (qt), NULL);
	// signal_query_tooltip().connect (sigc::mem_fun (*this, &ShuttleControl::on_query_tooltip));
}

ShuttleControl::~ShuttleControl ()
{
	cairo_pattern_destroy (pattern);
	cairo_pattern_destroy (shine_pattern);
	delete shuttle_context_menu;
}

void
ShuttleControl::varispeed_button_clicked ()
{
	if (_session->default_play_speed () == 1.0 && !_vari_dialog.is_visible ()) {
		_vari_dialog.present ();
	} else {
		_vari_dialog.hide ();
	}
}

bool
ShuttleControl::varispeed_button_scroll_event (GdkEventScroll* ev)
{
	double semi = 1.0;

	if (ev->state & Gtkmm2ext::Keyboard::GainFineScaleModifier) {
		if (ev->state & Gtkmm2ext::Keyboard::GainExtraFineScaleModifier) {
			semi = 0.1;
		} else {
			semi = 0.5;
		}
	}

	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_RIGHT:
			_vari_dialog.present ();
			_vari_dialog.adj_semi (semi);
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_LEFT:
			_vari_dialog.present ();
			_vari_dialog.adj_semi (-semi);
			break;
	}
	return true;
}

void
ShuttleControl::do_blink (bool onoff)
{
	if (!shuttle_grabbed && _session && _session->default_play_speed () != 1.0) {
		_vari_button.set_active (onoff);
		if (_session->actual_speed () == 0) {
			/* update info button text */
			queue_draw ();
		}
	} else {
		_vari_button.set_active (false);
	}
}

void
ShuttleControl::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	_vari_dialog.set_session (_session);

	if (_session) {
		_session->add_controllable (_controllable);
		_info_button.set_session (s);
		_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ShuttleControl::parameter_changed, this, _1), gui_context());
		/* set sensitivity */
		parameter_changed ("external-sync");
	} else {
		_vari_dialog.hide ();
		_vari_button.set_sensitive (false);
		set_sensitive (false);
	}
}

void
ShuttleControl::on_size_allocate (Gtk::Allocation& alloc)
{
	if (pattern) {
		cairo_pattern_destroy (pattern);
		pattern = 0;
		cairo_pattern_destroy (shine_pattern);
		shine_pattern = 0;
	}

	CairoWidget::on_size_allocate (alloc);

	/* background */
	pattern      = cairo_pattern_create_linear (0, 0, 0, alloc.get_height ());
	uint32_t col = UIConfiguration::instance ().color ("shuttle");
	int      r, b, g, a;
	UINT_TO_RGBA (col, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, r / 400.0, g / 400.0, b / 400.0);
	cairo_pattern_add_color_stop_rgb (pattern, 0.4, r / 255.0, g / 255.0, b / 255.0);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, r / 512.0, g / 512.0, b / 512.0);

	/* reflection */
	shine_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, 10);
	cairo_pattern_add_color_stop_rgba (shine_pattern, 0, 1, 1, 1, 0.0);
	cairo_pattern_add_color_stop_rgba (shine_pattern, 0.2, 1, 1, 1, 0.4);
	cairo_pattern_add_color_stop_rgba (shine_pattern, 1, 1, 1, 1, 0.1);
}

void
ShuttleControl::map_transport_state ()
{
	float speed        = 0.0;

	if (_session) {
		speed = _session->actual_speed ();
	}

	if ((fabsf (speed - last_speed_displayed) < 0.005f) // dead-zone
	    && !(speed == 1.f && last_speed_displayed != 1.f)
	    && !(speed == 0.f && last_speed_displayed != 0.f)) {
		return; // nothing to see here, move along.
	}

	// Q: is there a good reason why we  re-calculate this every time?
	if (fabs (speed) <= (2 * DBL_EPSILON)) {
		shuttle_fract = 0;
	} else {
		if (Config->get_shuttle_units () == Semitones) {
			bool reverse;
			int  semi     = speed_as_semitones (speed, reverse);
			semi          = std::max (-24, std::min (24, semi));
			shuttle_fract = semitones_as_fract (semi, reverse);
		} else {
			shuttle_fract = speed / shuttle_max_speed;
		}
	}

	queue_draw ();
}

void
ShuttleControl::build_shuttle_context_menu ()
{
	PBD::Unwinder<bool> uw (_ignore_change, true);

	using namespace Menu_Helpers;

	shuttle_context_menu = new Menu ();
	MenuList& items      = shuttle_context_menu->items ();

	{
		RadioMenuItem::Group speed_group;

		/* XXX this code assumes that Config->get_max_transport_speed() returns 8 */
		Menu*     speed_menu  = manage (new Menu ());
		MenuList& speed_items = speed_menu->items ();

		speed_items.push_back (RadioMenuElem (speed_group, "8", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 8.0f)));
		if (shuttle_max_speed == 8.0) {
			static_cast<RadioMenuItem*> (&speed_items.back ())->set_active ();
		}
		speed_items.push_back (RadioMenuElem (speed_group, "6", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 6.0f)));
		if (shuttle_max_speed == 6.0) {
			static_cast<RadioMenuItem*> (&speed_items.back ())->set_active ();
		}
		speed_items.push_back (RadioMenuElem (speed_group, "4", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 4.0f)));
		if (shuttle_max_speed == 4.0) {
			static_cast<RadioMenuItem*> (&speed_items.back ())->set_active ();
		}
		speed_items.push_back (RadioMenuElem (speed_group, "3", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 3.0f)));
		if (shuttle_max_speed == 3.0) {
			static_cast<RadioMenuItem*> (&speed_items.back ())->set_active ();
		}
		speed_items.push_back (RadioMenuElem (speed_group, "2", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 2.0f)));
		if (shuttle_max_speed == 2.0) {
			static_cast<RadioMenuItem*> (&speed_items.back ())->set_active ();
		}
		speed_items.push_back (RadioMenuElem (speed_group, "1.5", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 1.5f)));
		if (shuttle_max_speed == 1.5) {
			static_cast<RadioMenuItem*> (&speed_items.back ())->set_active ();
		}

		items.push_back (MenuElem (_("Maximum speed"), *speed_menu));
	}
}

void
ShuttleControl::set_shuttle_max_speed (float speed)
{
	if (_ignore_change) {
		return;
	}
	Config->set_shuttle_max_speed (speed);
}

bool
ShuttleControl::on_button_press_event (GdkEventButton* ev)
{
	if (!_session) {
		return true;
	}

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		if (shuttle_context_menu == 0) {
			build_shuttle_context_menu ();
		}
		shuttle_context_menu->popup (ev->button, ev->time);
		return true;
	}

	switch (ev->button) {
		case 1:
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
				_session->reset_transport_speed ();
			} else {
				add_modal_grab ();
				shuttle_grabbed       = true;
				shuttle_speed_on_grab = _session->actual_speed ();
				requested_speed       = shuttle_speed_on_grab;
				mouse_shuttle (ev->x, true);
				gdk_pointer_grab (ev->window, false,
				                  GdkEventMask (Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK),
				                  NULL, NULL, ev->time);
			}
			break;

		case 2:
		case 3:
		default:
			return true;
			break;
	}

	return true;
}

bool
ShuttleControl::on_button_release_event (GdkEventButton* ev)
{
	if (!_session) {
		return true;
	}

	switch (ev->button) {
		case 1:
			if (shuttle_grabbed) {
				shuttle_grabbed = false;
				remove_modal_grab ();
				gdk_pointer_ungrab (GDK_CURRENT_TIME);

				if (shuttle_speed_on_grab == 0) {
					_session->request_stop ();
				} else {
					_session->request_transport_speed (shuttle_speed_on_grab);
				}
			}
			return true;

		case 2:
		case 3:
		default:
			return true;
	}

	return true;
}

bool
ShuttleControl::on_query_tooltip (int, int, bool, const Glib::RefPtr<Gtk::Tooltip>&)
{
	return false;
}

bool
ShuttleControl::on_motion_notify_event (GdkEventMotion* ev)
{
	if (!_session || !shuttle_grabbed) {
		return true;
	}

	return mouse_shuttle (ev->x, false);
}

gint
ShuttleControl::mouse_shuttle (double x, bool force)
{
	double const center               = get_width () / 2.0;
	double       distance_from_center = x - center;

	if (distance_from_center > 0) {
		distance_from_center = min (distance_from_center, center);
	} else {
		distance_from_center = max (distance_from_center, -center);
	}

	/* compute shuttle fract as expressing how far between the center
	   and the edge we are. positive values indicate we are right of
	   center, negative values indicate left of center
	*/

	shuttle_fract = distance_from_center / center; // center == half the width
	use_shuttle_fract (force);
	return true;
}

void
ShuttleControl::set_shuttle_fract (double f, bool zero_ok)
{
	shuttle_fract = f;
	use_shuttle_fract (false, zero_ok);
}

int
ShuttleControl::speed_as_semitones (float speed, bool& reverse)
{
	assert (speed != 0.0);

	if (speed < 0.0) {
		reverse = true;
		return (int)round (12.0 * fast_log2 (-speed));
	} else {
		reverse = false;
		return (int)round (12.0 * fast_log2 (speed));
	}
}

int
ShuttleControl::speed_as_cents (float speed, bool& reverse)
{
	assert (speed != 0.0);

	if (speed < 0.0) {
		reverse = true;
		return (int)ceilf (1200.0 * fast_log2 (-speed));
	} else {
		reverse = false;
		return (int)ceilf (1200.0 * fast_log2 (speed));
	}
}

float
ShuttleControl::cents_as_speed (int cents, bool reverse)
{
	if (reverse) {
		return -pow (2.0, (cents / 1200.0));
	} else {
		return pow (2.0, (cents / 1200.0));
	}
}

float
ShuttleControl::semitones_as_speed (int semi, bool reverse)
{
	if (reverse) {
		return -pow (2.0, (semi / 12.0));
	} else {
		return pow (2.0, (semi / 12.0));
	}
}

float
ShuttleControl::semitones_as_fract (int semi, bool reverse)
{
	float speed = semitones_as_speed (semi, reverse);
	return speed / 4.0; /* 4.0 is the maximum speed for a 24 semitone shift */
}

int
ShuttleControl::fract_as_semitones (float fract, bool& reverse)
{
	assert (fract != 0.0);
	return speed_as_semitones (fract * 4.0, reverse);
}

void
ShuttleControl::use_shuttle_fract (bool force, bool zero_ok)
{
	PBD::microseconds_t now = PBD::get_microseconds ();

	shuttle_fract = max (-1.0f, shuttle_fract);
	shuttle_fract = min (1.0f, shuttle_fract);

	/* do not attempt to submit a motion-driven transport speed request
	   more than once per process cycle.
	*/

	if (!force && (now - last_shuttle_request) < (PBD::microseconds_t)AudioEngine::instance ()->usecs_per_cycle ()) {
		return;
	}

	last_shuttle_request = now;

	double speed = 0;

	if (Config->get_shuttle_units () == Semitones) {
		if (shuttle_fract != 0.0) {
			bool reverse;
			int  semi = fract_as_semitones (shuttle_fract, reverse);
			speed     = semitones_as_speed (semi, reverse);
		} else {
			speed = 0.0;
		}
	} else {
		shuttle_fract = shuttle_fract * shuttle_fract * shuttle_fract; // ^3 preserves the sign;
		speed         = shuttle_max_speed * shuttle_fract;
	}

	requested_speed = speed;

	if (_session) {
		if (zero_ok) {
			_session->request_transport_speed (speed);
		} else {
			_session->request_transport_speed_nonzero (speed);
		}

		if (speed != 0 && !_session->transport_state_rolling ()) {
			_session->request_roll ();
		} else if (speed == 0 && zero_ok && _session->transport_state_rolling ()) {
			_session->request_stop ();
		}
	}
}

void
ShuttleControl::set_colors ()
{
	int r, g, b, a;

	uint32_t bg_color = UIConfiguration::instance ().color (X_("shuttle bg"));

	UINT_TO_RGBA (bg_color, &r, &g, &b, &a);
	bg_r = r / 255.0;
	bg_g = g / 255.0;
	bg_b = b / 255.0;
}

void
ShuttleControl::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj ();
	// center slider line
	float yc = get_height () / 2;
	float lw = 3;
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_width (cr, 3);
	cairo_move_to (cr, lw, yc);
	cairo_line_to (cr, get_width () - lw, yc);
	cairo_set_source_rgb (cr, bg_r, bg_g, bg_b);
	if (UIConfiguration::instance ().get_widget_prelight () && _hovering) {
		cairo_stroke_preserve (cr);
		cairo_set_source_rgba (cr, 1, 1, 1, 0.15);
	}
	cairo_stroke (cr);

	float speed         = 0.0;
	float actual_speed  = 0.0;
	float default_speed = 1.0;
	char buf[32];

	if (_session) {
		speed         = _session->actual_speed ();
		actual_speed  = speed;
		default_speed = _session->default_play_speed ();
		if (shuttle_grabbed) {
			speed = requested_speed;
		}
	}

	/* marker */
	float visual_fraction = std::max (-1.0f, std::min (1.0f, speed / shuttle_max_speed));
	float marker_size     = round (get_height () * 0.66);
	float avail_width     = get_width () - marker_size;
	float x               = 0.5 * (get_width () + visual_fraction * avail_width - marker_size);

	rounded_rectangle (cr, x, 0, marker_size, get_height (), 5);
	cairo_set_source_rgba (cr, 0, 0, 0, 1);
	cairo_fill (cr);
	rounded_rectangle (cr, x + 1, 1, marker_size - 2, get_height () - 2, 3.5);
	if (flat_buttons ()) {
		uint32_t col = UIConfiguration::instance ().color ("shuttle");
		Gtkmm2ext::set_source_rgba (cr, col);
	} else {
		cairo_set_source (cr, pattern);
	}
	if (UIConfiguration::instance ().get_widget_prelight () && _hovering) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 1, 1, 1, 0.15);
	}
	cairo_fill (cr);

	/* text */
	if (actual_speed == 1.0) {
		snprintf (buf, sizeof (buf), "%s", _("Play"));
	} else if (actual_speed == 0 && default_speed == 1.0) {
		snprintf (buf, sizeof (buf), "%s", _("Stop"));
	} else {
		if (actual_speed == 0) {
			/*default_play_speed (varispeed) is always forward */
			actual_speed = default_speed;
		}
		if (Config->get_shuttle_units() == Percentage) {
			if (actual_speed < 0.0) {
				snprintf (buf, sizeof (buf), "< %.1f%%", -actual_speed * 100.f);
			} else {
				snprintf (buf, sizeof (buf), "> %.1f%%", actual_speed * 100.f);
			}
		} else {
			bool reversed;
			int semi = speed_as_semitones (actual_speed, reversed);
			if (reversed) {
				snprintf (buf, sizeof (buf), _("< %+2d st"), semi);
			} else {
				snprintf (buf, sizeof (buf), _("> %+2d st"), semi);
			}
		}
	}

	last_speed_displayed = actual_speed;

	_info_button.set_text (buf);

#if 0
	if (UIConfiguration::instance().get_widget_prelight()) {
		if (_hovering) {
			rounded_rectangle (cr, 0, 0, get_width(), get_height(), 3.5);
			cairo_set_source_rgba (cr, 1, 1, 1, 0.15);
			cairo_fill (cr);
		}
	}
#endif
}

ShuttleControl::ShuttleControllable::ShuttleControllable (ShuttleControl& s)
	: PBD::Controllable (X_("Shuttle"))
	, sc (s)
{
}

void
ShuttleControl::ShuttleControllable::set_value (double val, PBD::Controllable::GroupControlDisposition /*group_override*/)
{
	sc.set_shuttle_fract ((val - lower ()) / (upper () - lower ()), true);
}

double
ShuttleControl::ShuttleControllable::get_value () const
{
	return lower () + (sc.get_shuttle_fract () * (upper () - lower ()));
}

void
ShuttleControl::parameter_changed (std::string p)
{
	if (p == "shuttle-units") {
		map_transport_state ();
	} else if (p == "shuttle-max-speed") {
		shuttle_max_speed    = Config->get_shuttle_max_speed ();
		last_speed_displayed = -99999999;
		map_transport_state ();
		use_shuttle_fract (true);
		delete shuttle_context_menu;
		shuttle_context_menu = 0;
	} else if (p == "external-sync") {
		if (_session->config.get_external_sync()) {
			_vari_dialog.hide ();
			_vari_button.set_sensitive (false);
			if (shuttle_grabbed) {
				shuttle_grabbed = false;
				remove_modal_grab ();
				gdk_pointer_ungrab (GDK_CURRENT_TIME);
			}
			set_sensitive (false);
		} else {
			_vari_button.set_sensitive (true);
			set_sensitive (true);
		}
	}
}

bool
ShuttleControl::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = true;

	if (UIConfiguration::instance ().get_widget_prelight ()) {
		queue_draw ();
	}

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ShuttleControl::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	if (UIConfiguration::instance ().get_widget_prelight ()) {
		queue_draw ();
	}

	return CairoWidget::on_leave_notify_event (ev);
}
