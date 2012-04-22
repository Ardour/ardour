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

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "ardour_button.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;

ArdourButton::Element ArdourButton::default_elements = ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text);
ArdourButton::Element ArdourButton::led_default_elements = ArdourButton::Element (ArdourButton::default_elements|ArdourButton::Indicator);
ArdourButton::Element ArdourButton::just_led_default_elements = ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Indicator);

ArdourButton::ArdourButton (Element e)
	: _elements (e)
	, _tweaks (Tweaks (0))
	, _text_width (0)
	, _text_height (0)
	, _diameter (11.0)
	, _corner_radius (5.0)
	, _corner_mask (0xf)
	, edge_pattern (0)
	, active_pattern (0)
	, inactive_pattern (0)
	, led_inset_pattern (0)
	, reflection_pattern (0)
	, _led_rect (0)
	, _act_on_release (true)
	, _led_left (false)
	, _fixed_diameter (true)
	, _distinct_led_click (false)
	, _hovering (false)
{
	ColorsChanged.connect (sigc::mem_fun (*this, &ArdourButton::color_handler));
}

ArdourButton::ArdourButton (const std::string& str, Element e)
	: _elements (e)
	, _tweaks (Tweaks (0))
	, _text_width (0)
	, _text_height (0)
	, _diameter (11.0)
	, _corner_radius (9.0)
	, _corner_mask (0xf)
	, edge_pattern (0)
	, active_pattern (0)
	, inactive_pattern (0)
	, led_inset_pattern (0)
	, reflection_pattern (0)
	, _led_rect (0)
	, _act_on_release (true)
	, _led_left (false)
	, _fixed_diameter (true)
	, _distinct_led_click (false)
	, _hovering (false)
{
	set_text (str);
}

ArdourButton::~ArdourButton()
{
	delete _led_rect;

	if (edge_pattern) {
		cairo_pattern_destroy (edge_pattern);
	}

	if (active_pattern) {
		cairo_pattern_destroy (active_pattern);
	}

	if (inactive_pattern) {
		cairo_pattern_destroy (inactive_pattern);
	}
}

void
ArdourButton::set_text (const std::string& str)
{
	_text = str;

	if (!_layout && !_text.empty()) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	if (_layout) {
		_layout->set_text (str);
	}

	queue_resize ();
}

void
ArdourButton::set_markup (const std::string& str)
{
	_text = str;

	if (!_layout) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	_layout->set_text (str);
	queue_resize ();
}

void
ArdourButton::render (cairo_t* cr)
{
	void (*rounded_function)(cairo_t*, double, double, double, double, double);

	switch (_corner_mask) {
	case 0x1: /* upper left only */
		rounded_function = Gtkmm2ext::rounded_top_left_rectangle;
		break;
	case 0x2: /* upper right only */
		rounded_function = Gtkmm2ext::rounded_top_right_rectangle;
		break;
	case 0x3: /* upper only */
		rounded_function = Gtkmm2ext::rounded_top_rectangle;
		break;
		/* should really have functions for lower right, lower left,
		   lower only, but for now, we don't
		*/
	default:
		rounded_function = Gtkmm2ext::rounded_rectangle;
	}

	if (!_fixed_diameter) {
		_diameter = std::min (get_width(), get_height());
	}

	if (_elements & Edge) {
		rounded_function (cr, 0, 0, get_width(), get_height(), _corner_radius);
		cairo_set_source (cr, edge_pattern);
		cairo_fill (cr);
	}

	if (_elements & Body) {
		if (_elements & Edge) {
			rounded_function (cr, 1, 1, get_width()-2, get_height()-2, _corner_radius - 1.0);
		} else {
			rounded_function (cr, 0, 0, get_width(), get_height(), _corner_radius - 1.0);
		}

		if (active_state() == Gtkmm2ext::ImplicitActive) {
			cairo_set_source (cr, inactive_pattern);
			cairo_fill_preserve (cr);
			cairo_set_line_width (cr, 2.0);
			cairo_set_source (cr, active_pattern);
			cairo_stroke (cr);
		} else if (active_state() == Gtkmm2ext::ExplicitActive) {
			cairo_set_source (cr, active_pattern);
			cairo_fill (cr);
		} else {
			cairo_set_source (cr, inactive_pattern);
			cairo_fill (cr);
		}
	}

	if (_pixbuf) {

		double x,y;
		x = (get_width() - _pixbuf->get_width())/2.0;
		y = (get_height() - _pixbuf->get_height())/2.0;

		cairo_rectangle (cr, x, y, _pixbuf->get_width(), _pixbuf->get_height());
		gdk_cairo_set_source_pixbuf (cr, _pixbuf->gobj(), x, y);
		cairo_fill (cr);
	}

	/* text, if any */

	int text_margin;

	if (get_width() < 75) {
		text_margin = 3;
	} else {
		text_margin = 10;
	}

	if ((_elements & Text) && !_text.empty()) {

		cairo_new_path (cr);	

		if (_elements & Indicator) {
			if (_led_left) {
				cairo_move_to (cr, text_margin + _diameter + 4, get_height()/2.0 - _text_height/2.0);
			} else {
				cairo_move_to (cr, text_margin, get_height()/2.0 - _text_height/2.0);
			}
		} else {
			/* center text */
			cairo_move_to (cr, (get_width() - _text_width)/2.0, get_height()/2.0 - _text_height/2.0);
		}

		cairo_set_source_rgba (cr, text_r, text_g, text_b, text_a);
		pango_cairo_show_layout (cr, _layout->gobj());
	} 

	if (_elements & Indicator) {

		/* move to the center of the indicator/led */

		cairo_save (cr);

		if (_elements & Text) {
			if (_led_left) {
				cairo_translate (cr, text_margin + (_diameter/2.0), get_height()/2.0);
			} else {
				cairo_translate (cr, get_width() - ((_diameter/2.0) + 4.0), get_height()/2.0);
			}
		} else {
			cairo_translate (cr, get_width()/2.0, get_height()/2.0);
		}
		
		//inset
		cairo_arc (cr, 0, 0, _diameter/2, 0, 2 * M_PI);
		cairo_set_source (cr, led_inset_pattern);
		cairo_fill (cr);
		
		//black ring
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_arc (cr, 0, 0, _diameter/2-2, 0, 2 * M_PI);
		cairo_fill(cr);
		
		//led color
		cairo_set_source_rgba (cr, led_r, led_g, led_b, led_a);
		cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
		cairo_fill(cr);
		
		//reflection
		cairo_scale(cr, 0.7, 0.7);
		cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
		cairo_set_source (cr, reflection_pattern);
		cairo_fill (cr);

		cairo_restore (cr);

	}


	/* a partially transparent gray layer to indicate insensitivity */

	if ((visual_state() & Gtkmm2ext::Insensitive)) {
		rounded_function (cr, 0, 0, get_width(), get_height(), _corner_radius);
		cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.5);
		cairo_fill (cr);
	}

	/* if requested, show hovering */
	
	if (ARDOUR::Config->get_widget_prelight()) {
		if (_hovering) {
			rounded_function (cr, 0, 0, get_width(), get_height(), _corner_radius);
			cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.2);
			cairo_fill (cr);
		}
	}
}

void
ArdourButton::set_diameter (float d)
{
	_diameter = (d*2) + 5.0;

	if (_diameter != 0.0) {
		_fixed_diameter = true;
	}

	set_colors ();
}

void
ArdourButton::set_corner_radius (float r)
{
	_corner_radius = r;
	set_dirty ();
}

void
ArdourButton::on_size_request (Gtk::Requisition* req)
{
	int xpad = 0;
	int ypad = 6;

	CairoWidget::on_size_request (req);

	if ((_elements & Text) && !_text.empty()) {
		_layout->get_pixel_size (_text_width, _text_height);
		if (_text_width + _diameter < 75) {
			xpad = 7;
		} else {
			xpad = 20;
		}
	} else {
		_text_width = 0;
		_text_height = 0;
	}

	if (_pixbuf) {
		xpad = 6;
	}

        if ((_elements & Indicator) && _fixed_diameter) {
		if (_pixbuf) {
			req->width = _pixbuf->get_width() + lrint (_diameter) + xpad;
			req->height = max (_pixbuf->get_height(), (int) lrint (_diameter)) + ypad;
		} else {
			req->width = _text_width + lrint (_diameter) + xpad;
			req->height = max (_text_height, (int) lrint (_diameter)) + ypad;
		}
        } else {
		if (_pixbuf) {
			req->width = _pixbuf->get_width() + xpad;
			req->height = _pixbuf->get_height() + ypad;
		}  else {
			req->width = _text_width + xpad;
			req->height = _text_height + ypad;
		}
	}
}

void
ArdourButton::set_colors ()
{
	uint32_t start_color;
	uint32_t end_color;
	uint32_t r, g, b, a;
	uint32_t text_color;
	uint32_t led_color;

	/* we use the edge of the button to show Selected state, so the
	 * color/pattern used there will vary depending on that
	 */
	
	if (edge_pattern) {
		cairo_pattern_destroy (edge_pattern);
		edge_pattern = 0;
	}

	if (_elements & Edge) {

		edge_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, get_height());
		if (visual_state() & Gtkmm2ext::Selected) {
			start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: border start selected", get_name()));
			end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: border end selected", get_name()));
		} else {
			start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: border start", get_name()));
			end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: border end", get_name()));
		}
		UINT_TO_RGBA (start_color, &r, &g, &b, &a);
		cairo_pattern_add_color_stop_rgba (edge_pattern, 0, r/255.0,g/255.0,b/255.0, 0.7);
		UINT_TO_RGBA (end_color, &r, &g, &b, &a);
		cairo_pattern_add_color_stop_rgba (edge_pattern, 1, r/255.0,g/255.0,b/255.0, 0.7);
	}

	if (active_pattern) {
		cairo_pattern_destroy (active_pattern);
		active_pattern = 0;
	}

	if (inactive_pattern) {
		cairo_pattern_destroy (inactive_pattern);
		inactive_pattern = 0;
	}

	if (_elements & Body) {

		active_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, get_height());
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill start active", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill end active", get_name()));
		UINT_TO_RGBA (start_color, &r, &g, &b, &a);

		active_r = r/255.0;
		active_g = g/255.0;
		active_b = b/255.0;
		active_a = a/255.0;

		cairo_pattern_add_color_stop_rgba (active_pattern, 0, r/255.0,g/255.0,b/255.0, a/255.0);
		UINT_TO_RGBA (end_color, &r, &g, &b, &a);
		cairo_pattern_add_color_stop_rgba (active_pattern, 1, r/255.0,g/255.0,b/255.0, a/255.0);

		inactive_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, get_height());
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill start", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill end", get_name()));
		UINT_TO_RGBA (start_color, &r, &g, &b, &a);
		cairo_pattern_add_color_stop_rgba (inactive_pattern, 0, r/255.0,g/255.0,b/255.0, a/255.0);
		UINT_TO_RGBA (end_color, &r, &g, &b, &a);
		cairo_pattern_add_color_stop_rgba (inactive_pattern, 1, r/255.0,g/255.0,b/255.0, a/255.0);
	}

	if (led_inset_pattern) {
		cairo_pattern_destroy (led_inset_pattern);
	}
	
	if (reflection_pattern) {
		cairo_pattern_destroy (reflection_pattern);
	}

	if (_elements & Indicator) {
		led_inset_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter);
		cairo_pattern_add_color_stop_rgba (led_inset_pattern, 0, 0,0,0, 0.4);
		cairo_pattern_add_color_stop_rgba (led_inset_pattern, 1, 1,1,1, 0.7);

		reflection_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter/2-3);
		cairo_pattern_add_color_stop_rgba (reflection_pattern, 0, 1,1,1, active_state() ? 0.4 : 0.2);
		cairo_pattern_add_color_stop_rgba (reflection_pattern, 1, 1,1,1, 0.0);
	}
	
	/* text and LED colors */

	if (active_state() == Gtkmm2ext::ExplicitActive) {
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: text active", get_name()));
		led_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: led active", get_name()));
	} else {
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: text", get_name()));
		led_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: led", get_name()));
	}

	UINT_TO_RGBA (text_color, &r, &g, &b, &a);
	text_r = r/255.0;
	text_g = g/255.0;
	text_b = b/255.0;
	text_a = a/255.0;
	UINT_TO_RGBA (led_color, &r, &g, &b, &a);
	led_r = r/255.0;
	led_g = g/255.0;
	led_b = b/255.0;
	led_a = a/255.0;

	set_dirty ();
}

void
ArdourButton::set_led_left (bool yn)
{
	_led_left = yn;
}

bool
ArdourButton::on_button_press_event (GdkEventButton *ev)
{
	if ((_elements & Indicator) && _led_rect && _distinct_led_click) {
		if (ev->x >= _led_rect->x && ev->x < _led_rect->x + _led_rect->width && 
		    ev->y >= _led_rect->y && ev->y < _led_rect->y + _led_rect->height) {
			return true;
		}
	}

	if (_tweaks & ShowClick) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	}

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	if (!_act_on_release) {
		if (_action) {
			_action->activate ();
			return true;
		}
	}

	return false;
}

bool
ArdourButton::on_button_release_event (GdkEventButton *ev)
{
	if ((_elements & Indicator) && _led_rect && _distinct_led_click) {
		if (ev->x >= _led_rect->x && ev->x < _led_rect->x + _led_rect->width && 
		    ev->y >= _led_rect->y && ev->y < _led_rect->y + _led_rect->height) {
			signal_led_clicked(); /* EMIT SIGNAL */
			return true;
		}
	}

	if (_tweaks & ShowClick) {
		unset_active_state ();
	}

	signal_clicked ();

	if (_act_on_release) {
		if (_action) {
			_action->activate ();
			return true;
		}
	}


	return false;
}

void
ArdourButton::set_distinct_led_click (bool yn)
{
	_distinct_led_click = yn;
	setup_led_rect ();
}

void
ArdourButton::color_handler ()
{
	set_colors ();
	set_dirty ();
}

void
ArdourButton::on_size_allocate (Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
	setup_led_rect ();
	set_colors ();
}

void
ArdourButton::set_controllable (boost::shared_ptr<Controllable> c)
{
        watch_connection.disconnect ();
        binding_proxy.set_controllable (c);
}

void
ArdourButton::watch ()
{
        boost::shared_ptr<Controllable> c (binding_proxy.get_controllable ());

        if (!c) {
                warning << _("button cannot watch state of non-existing Controllable\n") << endmsg;
                return;
        }

        c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourButton::controllable_changed, this), gui_context());
}

void
ArdourButton::controllable_changed ()
{
        float val = binding_proxy.get_controllable()->get_value();

	if (fabs (val) >= 0.5f) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		unset_active_state ();
	}
}

void
ArdourButton::set_related_action (RefPtr<Action> act)
{
	Gtkmm2ext::Activatable::set_related_action (act);

	if (_action) {

		action_tooltip_changed ();

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);
		if (tact) {
			action_toggled ();
			tact->signal_toggled().connect (sigc::mem_fun (*this, &ArdourButton::action_toggled));
		} 

		_action->connect_property_changed ("sensitive", sigc::mem_fun (*this, &ArdourButton::action_sensitivity_changed));
		_action->connect_property_changed ("visible", sigc::mem_fun (*this, &ArdourButton::action_visibility_changed));
		_action->connect_property_changed ("tooltip", sigc::mem_fun (*this, &ArdourButton::action_tooltip_changed));
	}
}

void
ArdourButton::action_toggled ()
{
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);

	if (tact) {
		if (tact->get_active()) {
			set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			unset_active_state ();
		}
	}
}	

void
ArdourButton::on_style_changed (const RefPtr<Gtk::Style>&)
{
	set_colors ();
}

void
ArdourButton::setup_led_rect ()
{
	int text_margin;

	if (get_width() < 75) {
		text_margin = 3;
	} else {
		text_margin = 10;
	}

	if (_elements & Indicator) {
		_led_rect = new cairo_rectangle_t;
		
		if (_elements & Text) {
			if (_led_left) {
				_led_rect->x = text_margin;
			} else {
				_led_rect->x = get_width() - text_margin - _diameter/2.0;
			}
		} else {
			/* centered */
			_led_rect->x = get_width()/2.0 - _diameter/2.0;
		}

		_led_rect->y = get_height()/2.0 - _diameter/2.0;
		_led_rect->width = _diameter;
		_led_rect->height = _diameter;

	} else {
		delete _led_rect;
		_led_rect = 0;
	}
}

void
ArdourButton::set_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_pixbuf = img;
	queue_draw ();
}

void
ArdourButton::set_active_state (Gtkmm2ext::ActiveState s)
{
	bool changed = (_active_state != s);
	CairoWidget::set_active_state (s);
	if (changed) {
		set_colors ();
	}
}
	
void
ArdourButton::set_visual_state (Gtkmm2ext::VisualState s)
{
	bool changed = (_visual_state != s);
	CairoWidget::set_visual_state (s);
	if (changed) {
		set_colors ();
	}
}
	
bool
ArdourButton::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = true;

	if (ARDOUR::Config->get_widget_prelight()) {
		queue_draw ();
	}

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ArdourButton::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	if (ARDOUR::Config->get_widget_prelight()) {
		queue_draw ();
	}

	return CairoWidget::on_leave_notify_event (ev);
}

void
ArdourButton::set_tweaks (Tweaks t)
{
	if (_tweaks != t) {
		_tweaks = t;
		queue_draw ();
	}
}

void
ArdourButton::action_sensitivity_changed ()
{
	if (_action->property_sensitive ()) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	}
	
}


void
ArdourButton::action_visibility_changed ()
{
	if (_action->property_visible ()) {
		show ();
	} else {
		hide ();
	}
}

void
ArdourButton::action_tooltip_changed ()
{
	string str = _action->property_tooltip().get_value();
	ARDOUR_UI::instance()->set_tip (*this, str);
}

void
ArdourButton::set_rounded_corner_mask (int mask)
{
	_corner_mask = mask;
	queue_draw ();
}

void
ArdourButton::set_elements (Element e)
{
	_elements = e;
	set_colors ();
}
