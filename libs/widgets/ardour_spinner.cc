/*
 * Copyright (C) 2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/keyboard.h"

#include "widgets/ardour_spinner.h"

//#include "ardour/value_as_string.h" // XXX

using namespace ArdourWidgets;

ArdourSpinner::ArdourSpinner (boost::shared_ptr<PBD::Controllable> c, Gtk::Adjustment* adj)
	: _btn (ArdourButton::Text)
	, _ctrl_adj (adj)
	, _spin_adj (0, c->lower (), c->upper (), .1, .01)
	, _spinner (_spin_adj)
	, _switching (false)
	, _switch_on_release (false)
	, _ctrl_ignore (false)
	, _spin_ignore (false)
	, _controllable (c)
{
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	set (.5, .5, 1.0, 1.0);
	set_border_width (0);

	_btn.set_controllable (c);
	_btn.set_fallthrough_to_parent (true);

	_spinner.signal_activate().connect (mem_fun (*this, &ArdourSpinner::entry_activated));
	_spinner.signal_focus_out_event().connect (mem_fun (*this, &ArdourSpinner::entry_focus_out));
	_spinner.set_digits (4);
	_spinner.set_numeric (true);
	_spinner.set_name ("BarControlSpinner");

	_spin_adj.set_step_increment(c->interface_to_internal(_ctrl_adj->get_step_increment()) - c->lower ());
	_spin_adj.set_page_increment(c->interface_to_internal(_ctrl_adj->get_page_increment()) - c->lower ());

	_spin_adj.signal_value_changed().connect (sigc::mem_fun(*this, &ArdourSpinner::spin_adjusted));
	adj->signal_value_changed().connect (sigc::mem_fun(*this, &ArdourSpinner::ctrl_adjusted));
	c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourSpinner::controllable_changed, this), gui_context());

#if 0
	// this assume the "upper" value needs most space.
	std::string txt = ARDOUR::value_as_string (c->desc(), c->upper ());
	Gtkmm2ext::set_size_request_to_display_given_text (*this, txt, 2, 2);
#endif

	add (_btn);
	show_all ();

	controllable_changed();
	ctrl_adjusted ();
}


ArdourSpinner::~ArdourSpinner ()
{
}

bool
ArdourSpinner::on_button_press_event (GdkEventButton* ev)
{
	if (get_child() != &_btn) {
		return false;
	}

	if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
		_switch_on_release = true;
		return true;
	} else {
		_switch_on_release = false;
	}
	return false;
}

bool
ArdourSpinner::on_button_release_event (GdkEventButton* ev)
{
	if (get_child() != &_btn) {
		return false;
	}
	if (ev->button == 1 && _switch_on_release) {
		Glib::signal_idle().connect (mem_fun (*this, &ArdourSpinner::switch_to_spinner));
		return true;
	}
	return false;
}

bool
ArdourSpinner::on_scroll_event (GdkEventScroll* ev)
{
	float scale = 1.0;
	if (ev->state & Gtkmm2ext::Keyboard::GainFineScaleModifier) {
		if (ev->state & Gtkmm2ext::Keyboard::GainExtraFineScaleModifier) {
			scale *= 0.01;
		} else {
			scale *= 0.10;
		}
	}

	boost::shared_ptr<PBD::Controllable> c = _btn.get_controllable();
	if (c) {
		float val = c->get_interface();

		if ( ev->direction == GDK_SCROLL_UP )
			val += 0.05 * scale;  //by default, we step in 1/20ths of the knob travel
		else
			val -= 0.05 * scale;

		c->set_interface(val);
	}

	return true;
}

gint
ArdourSpinner::switch_to_button ()
{
	if (_switching || get_child() == &_btn) {
		return false;
	}
	_switching = true;
	remove ();
	add (_btn);
	_btn.show ();
	_btn.set_dirty ();
	_switching = false;
	return false;
}

gint
ArdourSpinner::switch_to_spinner ()
{
	if (_switching || get_child() != &_btn) {
		return false;
	}
	_switching = true;
	remove ();
	add (_spinner);
	_spinner.show ();
	_spinner.select_region (0, _spinner.get_text_length ());
	_spinner.grab_focus ();
	_switching = false;
	return false;
}

void
ArdourSpinner::entry_activated ()
{
	switch_to_button ();
}

bool
ArdourSpinner::entry_focus_out (GdkEventFocus* /*ev*/)
{
	entry_activated ();
	return true;
}

void
ArdourSpinner::ctrl_adjusted ()
{
	if (_spin_ignore) {
		return;
	}
	_ctrl_ignore = true;
	_spin_adj.set_value (_controllable->interface_to_internal (_ctrl_adj->get_value ()));
	_ctrl_ignore = false;
}

void
ArdourSpinner::spin_adjusted ()
{
	if (_ctrl_ignore) {
		return;
	}
	_spin_ignore = true;
	_ctrl_adj->set_value (_controllable->internal_to_interface (_spin_adj.get_value ()));
	_spin_ignore = false;
}

void
ArdourSpinner::controllable_changed ()
{
	_btn.set_text (_controllable->get_user_string());
	_btn.set_dirty();
}
