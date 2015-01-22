/*
    Copyright (C) 2014 Waves Audio Ltd.

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

void
WavesButton::__prop_style_watcher(WavesButton *that)
{
	that->_prop_style_watcher();
}

void
WavesButton::_prop_style_watcher()
{
	_layout->set_font_description(get_style()->get_font());
}

WavesButton::WavesButton ()
	: _text_width (0)
	, _text_height (0)
	, _corner_radius (0.0)
	, _corner_mask (0xf)
	, _left_border_width (0)
	, _top_border_width (0)
	, _right_border_width (0)
	, _bottom_border_width (0)
	, _angle (0)
	, _act_on_release (true)
	, _hovering (false)
	, _pushed (false)
	, _layout (Pango::Layout::create (get_pango_context()))
{
	property_style ().signal_changed ().connect (bind (sigc::ptr_fun (__prop_style_watcher), this));
        ARDOUR_UI_UTILS::ColorsChanged.connect (sigc::mem_fun (*this, &WavesButton::color_handler));
}

WavesButton::WavesButton (const std::string& str)
	: _text_width (0)
	, _text_height (0)
	, _corner_radius (0.0)
	, _corner_mask (0xf)
	, _left_border_width (0)
	, _top_border_width (0)
	, _right_border_width (0)
	, _bottom_border_width (0)
	, _angle (0)
	, _toggleable (false)
	, _act_on_release (true)
	, _hovering (false)
	, _pushed (false)
	, _layout (Pango::Layout::create (get_pango_context()))
{
	property_style ().signal_changed ().connect (bind (sigc::ptr_fun (__prop_style_watcher), this));
	set_text (str);
}

WavesButton::~WavesButton()
{
}

void
WavesButton::set_text (const std::string& str)
{
	_text = str;
    Gtk::Label* label = _find_label (this);
	if (label) {
		label->set_text (str);
	} else {
		Gtk::Entry* entry = _find_entry (this);
		if (entry) {
			entry->set_text (str);
		} 
	}
	_layout->set_text (str);
	queue_resize ();
}

void
WavesButton::set_angle (const double angle)
{
	_angle = angle;
}

void
WavesButton::render (cairo_t* cr, cairo_rectangle_t*)
{
	Glib::RefPtr<Gtk::Style> style = get_style();
	
	Gdk::Color bgcolor = style->get_bg ((get_state() == Gtk::STATE_INSENSITIVE) ? Gtk::STATE_INSENSITIVE :
												(_pushed ? (get_active() ? Gtk::STATE_NORMAL :
																		   Gtk::STATE_ACTIVE) :
														   (get_active() ? Gtk::STATE_ACTIVE :
																		   Gtk::STATE_NORMAL)));

	int width = get_width ();
	int height = get_height();

	cairo_rectangle (cr, 
					 0,
					 0,
					 width,
					 height);

	cairo_set_source_rgb (cr, bgcolor.get_red_p(), bgcolor.get_green_p(), bgcolor.get_blue_p());
	cairo_fill (cr);

	cairo_set_source_rgba (cr, _border_color.red, _border_color.green, _border_color.blue, _border_color.alpha);
	if (_left_border_width) {
		cairo_set_line_width (cr, _left_border_width);
		cairo_move_to (cr, _left_border_width/2.0, height);
		cairo_line_to (cr, _left_border_width/2.0, 0);
		cairo_stroke (cr);
	}

	if (_top_border_width) {
		cairo_set_line_width (cr, _top_border_width);
		cairo_move_to (cr, 0, _top_border_width/2.0);
		cairo_line_to (cr, width, _top_border_width/2.0);
		cairo_stroke (cr);
	}

	if (_right_border_width) {
		cairo_set_line_width (cr, _right_border_width);
		cairo_move_to (cr, width-_right_border_width/2.0, 0);
		cairo_line_to (cr, width-_right_border_width/2.0, height);
		cairo_stroke (cr);
	}

	if (_bottom_border_width != 0) {
		cairo_set_line_width (cr, _bottom_border_width);
		cairo_move_to (cr, width, height-_bottom_border_width/2.0);
		cairo_line_to (cr, 0, height-_bottom_border_width/2.0);
		cairo_stroke (cr);
	}

    render_text (cr);
}

void
WavesButton::render_text (cairo_t* cr)
{
	// text, if any
	
	if ((!_find_label (this)) && (!_find_entry (this)) &&
		(!_text.empty ())) {
        
        Glib::RefPtr<Gtk::Style> style = get_style();

		cairo_save (cr);
		cairo_rectangle (cr, 2, 1, get_width()-4, get_height()-2);
		cairo_clip (cr);

		cairo_new_path (cr);	

		Gdk::Color fgcolor = style->get_fg ((get_state() == Gtk::STATE_INSENSITIVE) ? Gtk::STATE_INSENSITIVE :
												(_pushed ? (get_active() ? Gtk::STATE_NORMAL :
																		   Gtk::STATE_ACTIVE) :
														   (get_active() ? Gtk::STATE_ACTIVE :
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
	unsigned ucolor = 0;

	if ((!color) || (*color != '#')) {
		return;
	}

    char* where_stopped;
    ucolor = strtoul(color+1, &where_stopped, 16);
    if (*where_stopped != 0) {
        return;
    }

	switch (strlen (color))
	{
	case 7:
		UINT_TO_RGB (ucolor, 
					 &_border_color.red,
					 &_border_color.green,
					 &_border_color.blue);
		_border_color.alpha = 255;
		break;
	case 9:
		UINT_TO_RGBA (ucolor, 
					  &_border_color.red,
					  &_border_color.green,
					  &_border_color.blue,
					  &_border_color.alpha);
		break;
	default:
		return;
	}

	_border_color.red /= 255;
	_border_color.green /= 255;
	_border_color.blue /= 255;
	_border_color.alpha /= 255;
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
    focus_handler ();

	if (ev->type == GDK_2BUTTON_PRESS) {
		signal_double_clicked (this);
	} else {
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
	}
	return false;
}

bool
WavesButton::on_button_release_event (GdkEventButton *)
{
	if (_pushed) {
        _pushed = false;
        queue_draw ();
        if (_hovering) {
            if (_toggleable) {
                set_active_state (active_state () == Gtkmm2ext::ExplicitActive ? Gtkmm2ext::Off : Gtkmm2ext::ExplicitActive);
            }
            signal_clicked (this);

            if (_act_on_release) {
                if (_action) {
                    _action->activate ();
                    return true;
                }
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
WavesButton::on_realize ()
{
    Gtk::EventBox::on_realize ();
    Gtk::Label* label = _find_label (this);
	if (label) {
		label->set_text (get_text());
	} else {
		Gtk::Entry* entry = _find_entry (this);
		if (entry) {
			entry->set_text (get_text());
		} 
	}
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
	// so far, we orient UI on XML defined attributes.
	// Let the code be here for a time; perhaps, it will
	// be removed in the end. And now let's just return:
	return;
	string str = _action->property_tooltip().get_value();
    if (str.empty()) {
        str = get_tooltip_text();
    }
	ARDOUR_UI::instance()->set_tip (*this, str);
}

void
WavesButton::set_rounded_corner_mask (int mask)
{
	_corner_mask = mask;
	queue_draw ();
}

Gtk::Label*
WavesButton::_find_label (Gtk::Container *container)
{
	Gtk::Label* label = NULL;
	if (container) {
		std::list<Gtk::Widget*> children = container->get_children ();
		for (std::list<Gtk::Widget*>::iterator i = children.begin(); i != children.end(); ++i) {
			label = dynamic_cast<Gtk::Label*>(*i);
			if (!label) {
				label = _find_label (dynamic_cast<Gtk::Container*>(*i));
			}
			if (label) {
				break;
			}
		}
	}
	return label;
}

Gtk::Entry*
WavesButton::_find_entry (Gtk::Container *container)
{
	Gtk::Entry* entry = NULL;
	if (container) {
		std::list<Gtk::Widget*> children = container->get_children ();
		for (std::list<Gtk::Widget*>::iterator i = children.begin(); i != children.end(); ++i) {
			entry = dynamic_cast<Gtk::Entry*>(*i);
			if (!entry) {
				entry = _find_entry (dynamic_cast<Gtk::Container*>(*i));
			}
			if (entry) {
				break;
			}
		}
	}
	return entry;
}
