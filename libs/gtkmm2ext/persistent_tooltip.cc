/*
    Copyright (C) 2012 Paul Davis

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

#include <gtkmm/window.h>
#include <gtkmm/label.h>
#include "gtkmm2ext/persistent_tooltip.h"

#include "pbd/stacktrace.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

/** @param target The widget to provide the tooltip for */
PersistentTooltip::PersistentTooltip (Gtk::Widget* target, bool  draggable, int margin_y)
	: _target (target)
	, _window (0)
	, _label (0)
	, _draggable (draggable)
	, _maybe_dragging (false)
	, _align_to_center (true)
	, _margin_y (margin_y)
{
	target->signal_enter_notify_event().connect (sigc::mem_fun (*this, &PersistentTooltip::enter), false);
	target->signal_leave_notify_event().connect (sigc::mem_fun (*this, &PersistentTooltip::leave), false);
	target->signal_button_press_event().connect (sigc::mem_fun (*this, &PersistentTooltip::press), false);
	target->signal_button_release_event().connect (sigc::mem_fun (*this, &PersistentTooltip::release), false);
}

PersistentTooltip::~PersistentTooltip ()
{
	delete _window;
}

bool
PersistentTooltip::enter (GdkEventCrossing *)
{
	if (_timeout.connected()) {
		leave(NULL);
	}
	_timeout = Glib::signal_timeout().connect (sigc::mem_fun (*this, &PersistentTooltip::timeout), 500);
	return false;
}

bool
PersistentTooltip::timeout ()
{
	show ();
	return true;
}

bool
PersistentTooltip::leave (GdkEventCrossing *)
{
	_timeout.disconnect ();
	if (!dragging ()) {
		hide ();
	}

	return false;
}

bool
PersistentTooltip::press (GdkEventButton* ev)
{
	if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
		_maybe_dragging = true;
	}

	return false;
}

bool
PersistentTooltip::release (GdkEventButton* ev)
{
	if (ev->type == GDK_BUTTON_RELEASE && ev->button == 1) {
		_maybe_dragging = false;
	}

	return false;
}

bool
PersistentTooltip::dragging () const
{
	return _maybe_dragging && _draggable;
}

void
PersistentTooltip::hide ()
{
	if (_window) {
		_window->hide ();
	}
}

void
PersistentTooltip::show ()
{
	if (_tip.empty()) {
		return;
	}

	if (!_window) {
		_window = new Window (WINDOW_POPUP);
		_window->set_name (X_("ContrastingPopup"));
		_window->set_position (WIN_POS_MOUSE);
		_window->set_decorated (false);

		_label = manage (new Label);
		_label->modify_font (_font);
		_label->set_use_markup (true);

		_window->set_border_width (6);
		_window->add (*_label);
		_label->show ();

		Gtk::Window* tlw = dynamic_cast<Gtk::Window*> (_target->get_toplevel ());
		if (tlw) {
			_window->set_transient_for (*tlw);
		}
	}
	
	set_tip (_tip);

	if (!_window->is_visible ()) {
		int rx, ry;
		int sw = gdk_screen_width ();

		_target->get_window()->get_origin (rx, ry);
		
		/* the window needs to be realized first
		 * for _window->get_width() to be correct.
		 */

		_window->present ();

		if (sw < rx + _window->get_width()) {
			/* right edge of window would be off the right edge of
			   the screen, so don't show it in the usual place.
			*/
			rx = sw - _window->get_width();
			_window->move (rx, ry + _target->get_height() + _margin_y);
		} else {
			if (_align_to_center) {
				_window->move (rx + (_target->get_width () - _window->get_width ()) / 2, ry + _target->get_height());
			} else {
				_window->move (rx, ry + _target->get_height());
			}
		}
	}
}

void
PersistentTooltip::set_tip (string t)
{
	_tip = t;

	if (_label) {
		_label->set_markup (t);
	}
}

void
PersistentTooltip::set_font (Pango::FontDescription font)
{
	_font = font;

	if (_label) {
		_label->modify_font (_font);
	}
}

void
PersistentTooltip::set_center_alignment (bool align_to_center)
{
	_align_to_center = align_to_center;
}
