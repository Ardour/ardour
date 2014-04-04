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

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "waves_button.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

#define REFLECTION_HEIGHT 2

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;

WavesButton::WavesButton ()
	:  _text_width (0)
	, _text_height (0)
	, _corner_radius (0.0)
	, _corner_mask (0xf)
	, _angle (0)
	, _act_on_release (true)
	, _hovering (false)
	, _pushed (false)
	, _left_border_width (0)
	, _top_border_width (0)
	, _right_border_width (0)
	, _bottom_border_width (0)
{
	ColorsChanged.connect (sigc::mem_fun (*this, &WavesButton::color_handler));
}

WavesButton::WavesButton (const std::string& str)
	: _text_width (0)
	, _text_height (0)
	, _corner_radius (0.0)
	, _corner_mask (0xf)
	, _angle(0)
	, _act_on_release (true)
	, _hovering (false)
	, _pushed (false)
	, _left_border_width (0)
	, _top_border_width (0)
	, _right_border_width (0)
	, _bottom_border_width (0)
{
	set_text (str);
}

WavesButton::~WavesButton()
{
}

void
WavesButton::set_text (const std::string& str)
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
WavesButton::set_markup (const std::string& str)
{
	_text = str;

	if (!_layout) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	_layout->set_markup (str);
	queue_resize ();
}

void
WavesButton::set_angle (const double angle)
{
	_angle = angle;
}

void
WavesButton::render (cairo_t* cr)
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

	Glib::RefPtr<Gtk::Style> style = get_style();
	
	Gdk::Color bgcolor = style->get_bg ((get_state() == Gtk::STATE_INSENSITIVE) ? Gtk::STATE_INSENSITIVE : 
											(_hovering ? 
												(_pushed ? 
													Gtk::STATE_ACTIVE :
													Gtk::STATE_PRELIGHT ) :
												(get_active() ? 
													Gtk::STATE_ACTIVE :
													Gtk::STATE_NORMAL)));

	if ((_left_border_width != 0) ||
		(_top_border_width != 0) ||
		(_right_border_width != 0) ||
		(_bottom_border_width != 0)) {
		cairo_set_source_rgba (cr, _border_color.get_red_p(), _border_color.get_blue_p(), _border_color.get_green_p(), 1);
		rounded_function (cr, 0, 0, get_width(), get_height(), _corner_radius);
		cairo_fill (cr);
	}

	rounded_function (cr, _left_border_width, _top_border_width, get_width()-_left_border_width-_right_border_width, get_height()-_top_border_width-_bottom_border_width, _corner_radius);
	cairo_set_source_rgba (cr, bgcolor.get_red_p(), bgcolor.get_green_p(), bgcolor.get_blue_p(), 1);
	cairo_fill (cr);

	// text, if any

	if (!_text.empty()) {
		cairo_save (cr);
		cairo_rectangle (cr, 2, 1, get_width()-4, get_height()-2);
		cairo_clip (cr);

		cairo_new_path (cr);	

		Gdk::Color fgcolor = style->get_fg ((get_state() == Gtk::STATE_INSENSITIVE) ? Gtk::STATE_INSENSITIVE : 
											(_hovering ? 
												(_pushed ? 
													Gtk::STATE_ACTIVE :
													Gtk::STATE_PRELIGHT ) :
												(get_active() ? 
													Gtk::STATE_ACTIVE :
													Gtk::STATE_NORMAL)));
		cairo_set_source_rgba (cr, fgcolor.get_red_p(), fgcolor.get_green_p(), fgcolor.get_blue_p(), 1);

		/* align text */

		double ww, wh;
		double xa, ya;
		ww = get_width ();
		wh = get_height ();
		cairo_save (cr); // TODO retain rotataion.. adj. LED,...
		cairo_rotate(cr, _angle * M_PI / 180.0);
		cairo_device_to_user(cr, &ww, &wh);
		xa = (ww - _text_width) * 0.5;
		ya = (wh - _text_height) * 0.5;

		cairo_move_to (cr, xa, ya);
		pango_cairo_update_layout(cr, _layout->gobj());
		pango_cairo_show_layout (cr, _layout->gobj());
		cairo_restore (cr);

		/* use old center'ed layout for follow up items - until rotation/aligment code is completed */
		cairo_move_to (cr, (get_width() - _text_width)/2.0, get_height()/2.0 - _text_height/2.0);
		cairo_restore (cr);
	} 
}

void
WavesButton::set_corner_radius (float r)
{
	_corner_radius = fabs(r);
	set_dirty ();
}

void
WavesButton::set_border_width (float left_border_width,
							   float top_border_width,
							   float right_border_width,
							   float bottom_border_width)
{
	_left_border_width = fabs(left_border_width);
	_top_border_width = fabs(top_border_width);
	_right_border_width = fabs(right_border_width);
	_bottom_border_width = fabs(bottom_border_width);
	set_dirty ();
}

void
WavesButton::set_border_width (const char* definition)
{
	if (definition)
	{
		float left;
		float top;
		float right;
		float bottom;
		int nread = sscanf(definition, "%f%f%f%f", &left, &top, &right, &bottom);
		if (nread > 0) {
			_left_border_width = fabs(left);
			if (nread < 2) {
				_top_border_width = _left_border_width;
			} else {
				_top_border_width = fabs(top);
			}
			if (nread < 3) {
				_right_border_width = _top_border_width;
			} else {
				_right_border_width = fabs(right);
			}
			if (nread < 4) {
				_bottom_border_width = _right_border_width;
			} else {
				_bottom_border_width = fabs(bottom);
			}
			
			set_dirty ();
		}
	}
}

void WavesButton::set_border_color(const char* color)
{
	if (color) {
		_border_color = Gdk::Color(color);
	}
}

void
WavesButton::on_size_request (Gtk::Requisition* req)
{
	CairoWidget::on_size_request (req);

	if (!_text.empty()) {
		_layout->get_pixel_size (_text_width, _text_height);
	} else {
		_text_width = 0;
		_text_height = 0;
	}

	req->width += _corner_radius;
}



bool
WavesButton::on_button_press_event (GdkEventButton *ev)
{
	_pushed = true;
	queue_draw ();
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
WavesButton::on_button_release_event (GdkEventButton *ev)
{
	_pushed = false;
	queue_draw ();
	if (_hovering) {
		signal_clicked (this);
		
		if (_act_on_release) {
			if (_action) {
				_action->activate ();
				return true;
			}
		}
	}

	return false;
}

void
WavesButton::color_handler ()
{
	set_dirty ();
}

void
WavesButton::on_size_allocate (Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
}

void
WavesButton::set_controllable (boost::shared_ptr<Controllable> c)
{
        watch_connection.disconnect ();
        binding_proxy.set_controllable (c);
}

void
WavesButton::watch ()
{
        boost::shared_ptr<Controllable> c (binding_proxy.get_controllable ());

        if (!c) {
                warning << _("button cannot watch state of non-existing Controllable\n") << endmsg;
                return;
        }

        c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&WavesButton::controllable_changed, this), gui_context());
}

void
WavesButton::controllable_changed ()
{
        float val = binding_proxy.get_controllable()->get_value();

	if (fabs (val) >= 0.5f) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		unset_active_state ();
	}
}

void
WavesButton::set_related_action (RefPtr<Action> act)
{
	Gtkmm2ext::Activatable::set_related_action (act);

	if (_action) {

		action_tooltip_changed ();

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);
		if (tact) {
			action_toggled ();
			tact->signal_toggled().connect (sigc::mem_fun (*this, &WavesButton::action_toggled));
		} 

		_action->connect_property_changed ("sensitive", sigc::mem_fun (*this, &WavesButton::action_sensitivity_changed));
		_action->connect_property_changed ("visible", sigc::mem_fun (*this, &WavesButton::action_visibility_changed));
		_action->connect_property_changed ("tooltip", sigc::mem_fun (*this, &WavesButton::action_tooltip_changed));
	}
}

void
WavesButton::action_toggled ()
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
WavesButton::on_style_changed (const RefPtr<Gtk::Style>&)
{
	set_dirty();
}

void
WavesButton::on_name_changed ()
{
	set_dirty();
}

void
WavesButton::set_active_state (Gtkmm2ext::ActiveState s)
{
	CairoWidget::set_active_state (s);
	set_dirty();
}
	
void
WavesButton::set_visual_state (Gtkmm2ext::VisualState s)
{
	CairoWidget::set_visual_state (s);
	set_dirty();
}
	
bool
WavesButton::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = true;
	queue_draw ();
	return CairoWidget::on_enter_notify_event (ev);
}

bool
WavesButton::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;
	queue_draw ();
	return CairoWidget::on_leave_notify_event (ev);
}

void
WavesButton::action_sensitivity_changed ()
{
	if (_action->property_sensitive ()) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	}
	
}


void
WavesButton::action_visibility_changed ()
{
	if (_action->property_visible ()) {
		show ();
	} else {
		hide ();
	}
}

void
WavesButton::action_tooltip_changed ()
{
	string str = _action->property_tooltip().get_value();
	ARDOUR_UI::instance()->set_tip (*this, str);
}

void
WavesButton::set_rounded_corner_mask (int mask)
{
	_corner_mask = mask;
	queue_draw ();
}
