/*
    Copyright (C) 2011 Paul Davis

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

#include <algorithm>

#include <cairo.h>

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/cairocell.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ardour_ui.h"
#include "rgb_macros.h"
#include "shuttle_control.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using std::min;
using std::max;

gboolean qt (gboolean, gint, gint, gboolean, Gtk::Tooltip*, gpointer)
{
	return FALSE;
}

ShuttleControl::ShuttleControl ()
	: _controllable (new ShuttleControllable (*this))
	, binding_proxy (_controllable)
{
	ARDOUR_UI::instance()->set_tip (*this, _("Shuttle speed control (Context-click for options)"));

	pattern = 0;
	shine_pattern = 0;
	last_shuttle_request = 0;
	last_speed_displayed = -99999999;
	shuttle_grabbed = false;
	shuttle_speed_on_grab = 0;
	shuttle_fract = 0.0;
	shuttle_max_speed = 8.0f;
	shuttle_style_menu = 0;
	shuttle_unit_menu = 0;
	shuttle_context_menu = 0;
	_hovering = false;

	set_flags (CAN_FOCUS);
	add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);
	set_size_request (85, 20);
	set_name (X_("ShuttleControl"));

	Config->ParameterChanged.connect (parameter_connection, MISSING_INVALIDATOR, boost::bind (&ShuttleControl::parameter_changed, this, _1), gui_context());

	/* gtkmm 2.4: the C++ wrapper doesn't work */
	g_signal_connect ((GObject*) gobj(), "query-tooltip", G_CALLBACK (qt), NULL);
	// signal_query_tooltip().connect (sigc::mem_fun (*this, &ShuttleControl::on_query_tooltip));
}

ShuttleControl::~ShuttleControl ()
{
	cairo_pattern_destroy (pattern);
	cairo_pattern_destroy (shine_pattern);
}

void
ShuttleControl::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		set_sensitive (true);
		_session->add_controllable (_controllable);
	} else {
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

	CairoWidget::on_size_allocate ( alloc);

	//background
	pattern = cairo_pattern_create_linear (0, 0, 0, alloc.get_height());
	uint32_t col = ARDOUR_UI::config()->get_canvasvar_Shuttle();
	int r,b,g,a;
	UINT_TO_RGBA(col, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, r/400.0, g/400.0, b/400.0);
	cairo_pattern_add_color_stop_rgb (pattern, 0.4, r/255.0, g/255.0, b/255.0);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, r/512.0, g/512.0, b/512.0);

	//reflection
	shine_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, 10);
	cairo_pattern_add_color_stop_rgba (shine_pattern, 0, 1,1,1,0.0);
	cairo_pattern_add_color_stop_rgba (shine_pattern, 0.2, 1,1,1,0.4);
	cairo_pattern_add_color_stop_rgba (shine_pattern, 1, 1,1,1,0.1);
}

void
ShuttleControl::map_transport_state ()
{
	float speed = _session->transport_speed ();

	if (fabs(speed) <= (2*DBL_EPSILON)) {
		shuttle_fract = 0;
	} else {
		if (Config->get_shuttle_units() == Semitones) {
			bool reverse;
			int semi = speed_as_semitones (speed, reverse);
			shuttle_fract = semitones_as_fract (semi, reverse);
		} else {
			shuttle_fract = speed/shuttle_max_speed;
		}
	}

	queue_draw ();
}

void
ShuttleControl::build_shuttle_context_menu ()
{
	using namespace Menu_Helpers;

	shuttle_context_menu = new Menu();
	MenuList& items = shuttle_context_menu->items();

	Menu* speed_menu = manage (new Menu());
	MenuList& speed_items = speed_menu->items();

	Menu* units_menu = manage (new Menu);
	MenuList& units_items = units_menu->items();
	RadioMenuItem::Group units_group;

	units_items.push_back (RadioMenuElem (units_group, _("Percent"), sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_units), Percentage)));
	if (Config->get_shuttle_units() == Percentage) {
		static_cast<RadioMenuItem*>(&units_items.back())->set_active();
	}
	units_items.push_back (RadioMenuElem (units_group, _("Semitones"), sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_units), Semitones)));
	if (Config->get_shuttle_units() == Semitones) {
		static_cast<RadioMenuItem*>(&units_items.back())->set_active();
	}
	items.push_back (MenuElem (_("Units"), *units_menu));

	Menu* style_menu = manage (new Menu);
	MenuList& style_items = style_menu->items();
	RadioMenuItem::Group style_group;

	style_items.push_back (RadioMenuElem (style_group, _("Sprung"), sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_style), Sprung)));
	if (Config->get_shuttle_behaviour() == Sprung) {
		static_cast<RadioMenuItem*>(&style_items.back())->set_active();
	}
	style_items.push_back (RadioMenuElem (style_group, _("Wheel"), sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_style), Wheel)));
	if (Config->get_shuttle_behaviour() == Wheel) {
		static_cast<RadioMenuItem*>(&style_items.back())->set_active();
	}

	items.push_back (MenuElem (_("Mode"), *style_menu));

	RadioMenuItem::Group speed_group;

	speed_items.push_back (RadioMenuElem (speed_group, "8", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 8.0f)));
	if (shuttle_max_speed == 8.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (speed_group, "6", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 6.0f)));
	if (shuttle_max_speed == 6.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (speed_group, "4", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 4.0f)));
	if (shuttle_max_speed == 4.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (speed_group, "3", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 3.0f)));
	if (shuttle_max_speed == 3.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (speed_group, "2", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 2.0f)));
	if (shuttle_max_speed == 2.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (speed_group, "1.5", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 1.5f)));
	if (shuttle_max_speed == 1.5) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}

	items.push_back (MenuElem (_("Maximum speed"), *speed_menu));

}

void
ShuttleControl::show_shuttle_context_menu ()
{
	if (shuttle_context_menu == 0) {
		build_shuttle_context_menu ();
	}

	shuttle_context_menu->popup (1, gtk_get_current_event_time());
}

void
ShuttleControl::set_shuttle_max_speed (float speed)
{
	shuttle_max_speed = speed;
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
		show_shuttle_context_menu ();
		return true;
	}

	switch (ev->button) {
	case 1:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			if (_session->transport_rolling()) {
				_session->request_transport_speed (1.0);
			}
		} else {
			add_modal_grab ();
			shuttle_grabbed = true;
			shuttle_speed_on_grab = _session->transport_speed ();
			mouse_shuttle (ev->x, true);
			gdk_pointer_grab(ev->window,false,
					GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
					NULL,NULL,ev->time);
		}
		break;

	case 2:
	case 3:
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
			
			if (Config->get_shuttle_behaviour() == Sprung) {
				if (shuttle_speed_on_grab == 0 ) {
					_session->request_transport_speed (1.0);
				}
				_session->request_transport_speed (shuttle_speed_on_grab);
			} else {
				mouse_shuttle (ev->x, true);
			}
		}
		return true;

	case 2:
		if (_session->transport_rolling()) {
			_session->request_transport_speed (1.0, Config->get_shuttle_behaviour() == Wheel);
		}
		return true;

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
ShuttleControl::on_scroll_event (GdkEventScroll* ev)
{
	if (!_session || Config->get_shuttle_behaviour() != Wheel) {
		return true;
	}

	bool semis = (Config->get_shuttle_units() == Semitones);

	switch (ev->direction) {
	case GDK_SCROLL_UP:
	case GDK_SCROLL_RIGHT:
		if (semis) {
			if (shuttle_fract == 0) {
				shuttle_fract = semitones_as_fract (1, false);
			} else {
				bool rev;
				int st = fract_as_semitones (shuttle_fract, rev);
				shuttle_fract = semitones_as_fract (st + 1, rev);
			}
		} else {
			shuttle_fract += 0.00125;
		}
		break;
	case GDK_SCROLL_DOWN:
	case GDK_SCROLL_LEFT:
		if (semis) {
			if (shuttle_fract == 0) {
				shuttle_fract = semitones_as_fract (1, true);
			} else {
				bool rev;
				int st = fract_as_semitones (shuttle_fract, rev);
				shuttle_fract = semitones_as_fract (st - 1, rev);
			}
		} else {
			shuttle_fract -= 0.00125;
		}
		break;
	default:
		return false;
	}
	
	if (semis) {

		float lower_side_of_dead_zone = semitones_as_fract (-24, true);
		float upper_side_of_dead_zone = semitones_as_fract (-24, false);

		/* if we entered the "dead zone" (-24 semitones in forward or reverse), jump
		   to the far side of it.
		*/

		if (shuttle_fract > lower_side_of_dead_zone && shuttle_fract < upper_side_of_dead_zone) {
			switch (ev->direction) {
			case GDK_SCROLL_UP:
			case GDK_SCROLL_RIGHT:
				shuttle_fract = upper_side_of_dead_zone;
				break;
			case GDK_SCROLL_DOWN:
			case GDK_SCROLL_LEFT:
				shuttle_fract = lower_side_of_dead_zone;
				break;
			default:
				/* impossible, checked above */
				return false;
			}
		}
	}

	use_shuttle_fract (true);

	return true;
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
	double const center = get_width() / 2.0;
	double distance_from_center = x - center;

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
		return (int) round (12.0 * fast_log2 (-speed));
	} else {
		reverse = false;
		return (int) round (12.0 * fast_log2 (speed));
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
	return speed/4.0; /* 4.0 is the maximum speed for a 24 semitone shift */
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
	microseconds_t now = get_microseconds();

	shuttle_fract = max (-1.0f, shuttle_fract);
	shuttle_fract = min (1.0f, shuttle_fract);

	/* do not attempt to submit a motion-driven transport speed request
	   more than once per process cycle.
	*/

	if (!force && (last_shuttle_request - now) < (microseconds_t) AudioEngine::instance()->usecs_per_cycle()) {
		return;
	}

	last_shuttle_request = now;

	double speed = 0;

	if (Config->get_shuttle_units() == Semitones) {
		if (shuttle_fract != 0.0) {
			bool reverse;
			int semi = fract_as_semitones (shuttle_fract, reverse);
			speed = semitones_as_speed (semi, reverse);
		} else {
			speed = 0.0;
		}
	} else {
		speed = shuttle_max_speed * shuttle_fract;
	}

	if (zero_ok) {
		_session->request_transport_speed (speed, Config->get_shuttle_behaviour() == Wheel);
	} else {
		_session->request_transport_speed_nonzero (speed, Config->get_shuttle_behaviour() == Wheel);
	}
}

void
ShuttleControl::render (cairo_t* cr)
{
	cairo_text_extents_t extents;

	//black border
	cairo_set_source_rgb (cr, 0, 0.0, 0.0);
	rounded_rectangle (cr, 0, 0, get_width(), get_height(), 4);
//	cairo_fill_preserve (cr);
//	cairo_stroke (cr);
	cairo_fill (cr);

	float speed = 0.0;

	if (_session) {
		speed = _session->transport_speed ();
	}

	/* Marker */
	float visual_fraction = std::min (1.0f, speed/shuttle_max_speed);
	float marker_size = get_height()-4;
	float avail_width = get_width() - marker_size;
	float x = get_width()*0.5 + visual_fraction * avail_width*0.5;
	float offset = x - marker_size*0.5;
//	cairo_set_source_rgb (cr, 0, 1, 0.0);
	cairo_set_source (cr, pattern);
	if (speed == 1.0) {
		cairo_move_to( cr, offset-4, 2);
		cairo_line_to( cr, offset+4, 2+marker_size*0.5);
		cairo_line_to( cr, offset-4, 2+marker_size);
		cairo_line_to( cr, offset-4, 2);
	} else if ( speed ==0.0 )
		rounded_rectangle (cr, offset, 4, marker_size-2, marker_size-2, 1);
	else
		cairo_arc (cr, offset + marker_size*0.5, 2 + marker_size*0.5, marker_size*0.5, 0, 360);
	cairo_set_line_width (cr, 2);
	cairo_stroke (cr);

	/* speed text */

	char buf[32];

	if (speed != 0) {

		if (Config->get_shuttle_units() == Percentage) {

			if (speed == 1.0) {
				snprintf (buf, sizeof (buf), "%s", _("Playing"));
			} else {
				if (speed < 0.0) {
					snprintf (buf, sizeof (buf), "<<< %d%%", (int) round (-speed * 100));
				} else {
					snprintf (buf, sizeof (buf), ">>> %d%%", (int) round (speed * 100));
				}
			}

		} else {

			bool reversed;
			int semi = speed_as_semitones (speed, reversed);

			if (reversed) {
				snprintf (buf, sizeof (buf), _("<<< %+d semitones"), semi);
			} else {
				snprintf (buf, sizeof (buf), _(">>> %+d semitones"), semi);
			}
		}

	} else {
		snprintf (buf, sizeof (buf), "%s", _("Stopped"));
	}

	last_speed_displayed = speed;

	cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
	cairo_text_extents (cr, buf, &extents);
	cairo_move_to (cr, 10, extents.height + 4);
	cairo_set_font_size (cr, 13.0);
	cairo_show_text (cr, buf);

	/* style text */


	switch (Config->get_shuttle_behaviour()) {
	case Sprung:
		snprintf (buf, sizeof (buf), "%s", _("Sprung"));
		break;
	case Wheel:
		snprintf (buf, sizeof (buf), "%s", _("Wheel"));
		break;
	}

	cairo_text_extents (cr, buf, &extents);
	cairo_move_to (cr, get_width() - (fabs(extents.x_advance) + 5), extents.height + 4);
	cairo_show_text (cr, buf);

	float _corner_radius = 4.0;

/*	//reflection
	float rheight = 10.0;
	Gtkmm2ext::rounded_rectangle (cr, 2, 1, get_width()-4, rheight, _corner_radius);
	cairo_set_source (cr, shine_pattern);
	cairo_fill (cr);
*/
	if (ARDOUR::Config->get_widget_prelight()) {
		if (_hovering) {
			rounded_rectangle (cr, 1, 1, get_width()-2, get_height()-2, _corner_radius);
			cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
			cairo_fill (cr);
		}
	}
}

void
ShuttleControl::shuttle_unit_clicked ()
{
	if (shuttle_unit_menu == 0) {
		shuttle_unit_menu = dynamic_cast<Menu*> (ActionManager::get_widget ("/ShuttleUnitPopup"));
	}
	shuttle_unit_menu->popup (1, gtk_get_current_event_time());
}

void
ShuttleControl::set_shuttle_style (ShuttleBehaviour s)
{
	Config->set_shuttle_behaviour (s);
}

void
ShuttleControl::set_shuttle_units (ShuttleUnits s)
{
	Config->set_shuttle_units (s);
}

void
ShuttleControl::update_speed_display ()
{
	if (_session->transport_speed() != last_speed_displayed) {
		queue_draw ();
	}
}

ShuttleControl::ShuttleControllable::ShuttleControllable (ShuttleControl& s)
	: PBD::Controllable (X_("Shuttle"))
	, sc (s)
{
}

void
ShuttleControl::ShuttleControllable::set_value (double val)
{
	sc.set_shuttle_fract ((val - lower()) / (upper() - lower()), true);
}

double
ShuttleControl::ShuttleControllable::get_value () const
{
	return lower() + (sc.get_shuttle_fract () * (upper() - lower()));
}

void
ShuttleControl::parameter_changed (std::string p)
{
	if (p == "shuttle-behaviour") {
		switch (Config->get_shuttle_behaviour ()) {
		case Sprung:
			/* back to Sprung - reset to speed = 1.0 if playing
			 */
			if (_session) {
				if (_session->transport_rolling()) {
					if (_session->transport_speed() == 1.0) {
						queue_draw ();
					} else {
						/* reset current speed and
						   revert to 1.0 as the default
						*/
						_session->request_transport_speed (1.0);
						/* redraw when speed changes */
					}
				} else {
					queue_draw ();
				}
			}
			break;

		case Wheel:
			queue_draw ();
			break;
		}

	} else if (p == "shuttle-units") {
		queue_draw ();
	}
}


bool
ShuttleControl::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = true;

	if (ARDOUR::Config->get_widget_prelight()) {
		queue_draw ();
	}

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ShuttleControl::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	if (ARDOUR::Config->get_widget_prelight()) {
		queue_draw ();
	}

	return CairoWidget::on_leave_notify_event (ev);
}
