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

#include <cairo/cairo.h>

#include <pbd/stacktrace.h>

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour_ui.h"
#include "shuttle_control.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using std::min;
using std::max;

ShuttleControl::ShuttleControl ()
        : _controllable (new ShuttleControllable (*this))
        , binding_proxy (_controllable)
{
        ARDOUR_UI::instance()->set_tip (*this, _("Shuttle speed control (Context-click for options)"));

        pattern = 0;
	last_shuttle_request = 0;
	last_speed_displayed = -99999999;
	shuttle_grabbed = false;
	shuttle_fract = 0.0;
	shuttle_max_speed = 8.0f;
	shuttle_style_menu = 0;
	shuttle_unit_menu = 0;
        shuttle_context_menu = 0;

        set_flags (CAN_FOCUS);
        add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);
        set_size_request (100, 15);
	set_name (X_("ShuttleControl"));

        Config->ParameterChanged.connect (parameter_connection, MISSING_INVALIDATOR, ui_bind (&ShuttleControl::parameter_changed, this, _1), gui_context());
}

ShuttleControl::~ShuttleControl ()
{
	cairo_pattern_destroy (pattern);
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
        }

	pattern = cairo_pattern_create_linear (0, 0, alloc.get_width(), alloc.get_height());
	
	/* add 3 color stops */

	cairo_pattern_add_color_stop_rgb (pattern, 0.0, 0, 0, 0);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, 0.0, 0.0, 1.0);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, 0, 0, 0);

        DrawingArea::on_size_allocate (alloc);
}

void
ShuttleControl::map_transport_state ()
{
	float speed = _session->transport_speed ();

	if (speed != 0.0) {
		shuttle_fract = SHUTTLE_FRACT_SPEED1;  /* speed = 1.0, believe it or not */
	} else {
		shuttle_fract = 0;
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

	RadioMenuItem::Group group;

	speed_items.push_back (RadioMenuElem (group, "8", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 8.0f)));
	if (shuttle_max_speed == 8.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (group, "6", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 6.0f)));
	if (shuttle_max_speed == 6.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (group, "4", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 4.0f)));
	if (shuttle_max_speed == 4.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (group, "3", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 3.0f)));
	if (shuttle_max_speed == 3.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (group, "2", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 2.0f)));
	if (shuttle_max_speed == 2.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}
	speed_items.push_back (RadioMenuElem (group, "1.5", sigc::bind (sigc::mem_fun (*this, &ShuttleControl::set_shuttle_max_speed), 1.5f)));
	if (shuttle_max_speed == 1.5) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();
	}

	items.push_back (MenuElem (_("Maximum speed"), *speed_menu));
        
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
		add_modal_grab ();
		shuttle_grabbed = true;
		mouse_shuttle (ev->x, true);
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
		mouse_shuttle (ev->x, true);
		shuttle_grabbed = false;
		remove_modal_grab ();
		if (Config->get_shuttle_behaviour() == Sprung) {
			if (_session->config.get_auto_play()) {
				shuttle_fract = SHUTTLE_FRACT_SPEED1;
				_session->request_transport_speed (1.0);
			} else {
				shuttle_fract = 0;
				_session->request_transport_speed (0.0);
			}
			queue_draw ();
		}
		return true;

	case 2:
		if (_session->transport_rolling()) {
			shuttle_fract = SHUTTLE_FRACT_SPEED1;
			_session->request_transport_speed (1.0);
		} else {
			shuttle_fract = 0;
		}
		queue_draw ();
		return true;

	case 3:
	default:
		return true;

	}

	use_shuttle_fract (true);

	return true;
}

bool
ShuttleControl::on_scroll_event (GdkEventScroll* ev)
{
	if (!_session) {
		return true;
	}

	switch (ev->direction) {

	case GDK_SCROLL_UP:
		shuttle_fract += 0.005;
		break;
	case GDK_SCROLL_DOWN:
		shuttle_fract -= 0.005;
		break;
	default:
		/* scroll left/right */
		return false;
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
	double const half_width = get_width() / 2.0;
	double distance = x - half_width;

	if (distance > 0) {
		distance = min (distance, half_width);
	} else {
		distance = max (distance, -half_width);
	}

	shuttle_fract = distance / half_width;
	use_shuttle_fract (force);
	return true;
}

void
ShuttleControl::set_shuttle_fract (double f)
{
	shuttle_fract = f;
	use_shuttle_fract (false);
}

void
ShuttleControl::use_shuttle_fract (bool force)
{
	microseconds_t now = get_microseconds();

	/* do not attempt to submit a motion-driven transport speed request
	   more than once per process cycle.
	 */

	if (!force && (last_shuttle_request - now) < (microseconds_t) AudioEngine::instance()->usecs_per_cycle()) {
		return;
	}

	last_shuttle_request = now;

	double speed = 0;

	if (Config->get_shuttle_units() == Semitones) {

		double const step = 1.0 / 24.0; // range is 24 semitones up & down
		double const semitones = round (shuttle_fract / step);
		speed = pow (2.0, (semitones / 12.0));

	} else {

		bool const neg = (shuttle_fract < 0.0);
		double fract = 1 - sqrt (1 - (shuttle_fract * shuttle_fract)); // Formula A1

		if (neg) {
			fract = -fract;
		}

		speed = shuttle_max_speed * fract;
	}

	_session->request_transport_speed_nonzero (speed);
}

bool
ShuttleControl::on_expose_event (GdkEventExpose* event)
{
        cairo_text_extents_t extents;
        Glib::RefPtr<Gdk::Window> win (get_window());
	Glib::RefPtr<Gtk::Style> style (get_style());

	cairo_t* cr = gdk_cairo_create (win->gobj());	

	cairo_set_source (cr, pattern);
	cairo_rectangle (cr, 0.0, 0.0, get_width(), get_height());
	cairo_fill_preserve (cr);

	cairo_set_source_rgb (cr, 0, 0, 0.0);
	cairo_stroke (cr);

	/* Marker */
	
	double x = (get_width() / 2.0) + (0.5 * (get_width() * shuttle_fract));
	cairo_move_to (cr, x, 0);
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_line_to (cr, x, get_height());
	cairo_stroke (cr);

	/* speed text */

	char buf[32];
	float speed = 0.0;

        if (_session) {
                speed = _session->transport_speed ();
        }

	if (speed != 0) {
		if (Config->get_shuttle_units() == Percentage) {
			snprintf (buf, sizeof (buf), "%d%%", (int) round (speed * 100));
		} else {
			if (speed < 0) {
				snprintf (buf, sizeof (buf), "< %d semitones", (int) round (12.0 * fast_log2 (-speed)));
			} else {
				snprintf (buf, sizeof (buf), "> %d semitones", (int) round (12.0 * fast_log2 (speed)));
			}
		}
	} else {
		snprintf (buf, sizeof (buf), _("Stopped"));
	}

	last_speed_displayed = speed;

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
        cairo_text_extents (cr, buf, &extents);
	cairo_move_to (cr, 10, extents.height + 2);
	cairo_show_text (cr, buf);

	/* style text */


	switch (Config->get_shuttle_behaviour()) {
	case Sprung:
		snprintf (buf, sizeof (buf), _("Sprung"));
		break;
	case Wheel:
		snprintf (buf, sizeof (buf), _("Wheel"));
		break;
	}

        cairo_text_extents (cr, buf, &extents);

	cairo_move_to (cr, get_width() - (fabs(extents.x_advance) + 5), extents.height + 2);
	cairo_show_text (cr, buf);

	cairo_destroy (cr);

	return true;
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
ShuttleControl::ShuttleControllable::set_id (const std::string& str)
{
        _id = str;
}

void
ShuttleControl::ShuttleControllable::set_value (double val)
{
        double fract;
        
        if (val == 0.5) {
                fract = 0.0;
        } else {
                if (val < 0.5) {
                        fract = -((0.5 - val)/0.5);
                } else {
                        fract = ((val - 0.5)/0.5);
                }
        }

        sc.set_shuttle_fract (fract);
}

double 
ShuttleControl::ShuttleControllable::get_value () const
{
        return sc.get_shuttle_fract ();
}

void
ShuttleControl::parameter_changed (std::string p)
{
        if (p == "shuttle-behaviour") {
		switch (Config->get_shuttle_behaviour ()) {
		case Sprung:
			shuttle_fract = 0.0;
			if (_session) {
				if (_session->transport_rolling()) {
					shuttle_fract = SHUTTLE_FRACT_SPEED1;
					_session->request_transport_speed (1.0);
				}
			}
			break;
		case Wheel:
			break;
		}
                queue_draw ();
                        
	} else if (p == "shuttle-units") {
                queue_draw ();
        }
}
