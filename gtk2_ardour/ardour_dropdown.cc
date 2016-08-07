/*
    Copyright (C) 2014 Paul Davis

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
#include "pbd/stacktrace.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "ardour_dropdown.h"

#include "pbd/i18n.h"

#define REFLECTION_HEIGHT 2

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace std;


ArdourDropdown::ArdourDropdown (Element e)
	: _scrolling_disabled(false)
{
//	signal_button_press_event().connect (sigc::mem_fun(*this, &ArdourDropdown::on_mouse_pressed));

	add_elements(e);
	add_elements(ArdourButton::Menu);
}

ArdourDropdown::~ArdourDropdown ()
{
}

void
ArdourDropdown::position_menu(int& x, int& y, bool& push_in) {
	 /* TODO: lacks support for rotated dropdown buttons */

	if (!has_screen () || !get_has_window ()) {
		return;
	}

	Rectangle monitor;
	{
		const int monitor_num = get_screen ()->get_monitor_at_window (get_window ());
		get_screen ()->get_monitor_geometry ((monitor_num < 0) ? 0 : monitor_num,
		                                     monitor);
	}

	const Requisition menu_req = _menu.size_request();
	const Rectangle allocation = get_allocation();

	/* The x and y position are handled separately.
	 *
	 * For the x position if the direction is LTR (or RTL), then we try in order:
	 *  a) align the left (right) of the menu with the left (right) of the button
	 *     if there's enough room until the right (left) border of the screen;
	 *  b) align the right (left) of the menu with the right (left) of the button
	 *     if there's enough room until the left (right) border of the screen;
	 *  c) align the right (left) border of the menu with the right (left) border
	 *     of the screen if there's enough space;
	 *  d) align the left (right) border of the menu with the left (right) border
	 *     of the screen, with the rightmost (leftmost) part of the menu that
	 *     overflows the screen.
	 *     XXX We always align left regardless of the direction because if x is
	 *     left of the current monitor, the menu popup code after us notices it
	 *     and enforces that the menu stays in the monitor that's at the left...*/

	get_window ()->get_origin (x, y);

	if (get_direction() == TEXT_DIR_RTL) {
		if (monitor.get_x() <= x + allocation.get_width() - menu_req.width) {
			/* a) align menu right and button right */
			x += allocation.get_width() - menu_req.width;
		} else if (x + menu_req.width <= monitor.get_x() + monitor.get_width()) {
			/* b) align menu left and button left: nothing to do*/
		} else if (menu_req.width > monitor.get_width()) {
			/* c) align menu left and screen left, guaranteed to fit */
			x = monitor.get_x();
		} else {
			/* d) XXX align left or the menu might change monitors */
			x = monitor.get_x();
		}
	} else { /* LTR */
		if (x + menu_req.width <= monitor.get_x() + monitor.get_width()) {
			/* a) align menu left and button left: nothing to do*/
		} else if (monitor.get_x() <= x + allocation.get_width() - menu_req.width) {
			/* b) align menu right and button right */
			x += allocation.get_width() - menu_req.width;
		} else if (menu_req.width > monitor.get_width()) {
			/* c) align menu right and screen right, guaranteed to fit */
			x = monitor.get_x() + monitor.get_width() - menu_req.width;
		} else {
			/* d) align left */
			x = monitor.get_x();
		}
	}

	/* For the y position, try in order:
	 *  a) align the top of the menu with the bottom of the button if there is
	 *     enough room below the button;
	 *  b) align the bottom of the menu with the top of the button if there is
	 *     enough room above the button;
	 *  c) align the bottom of the menu with the bottom of the monitor if there
	 *     is enough room, but avoid moving the menu to another monitor */

	if (y + allocation.get_height() + menu_req.height <= monitor.get_y() + monitor.get_height()) {
		y += allocation.get_height(); /* a) */
	} else if ((y - menu_req.height) >= monitor.get_y()) {
		y -= menu_req.height; /* b) */
	} else {
		y = monitor.get_y() + max(0, monitor.get_height() - menu_req.height);
	}

	push_in = false;
}

bool
ArdourDropdown::on_button_press_event (GdkEventButton* ev)
{
	if (ev->type == GDK_BUTTON_PRESS) {
		_menu.popup (sigc::mem_fun(this, &ArdourDropdown::position_menu),
		             1, ev->time);
	}
	return true;
}

bool
ArdourDropdown::on_scroll_event (GdkEventScroll* ev)
{
	using namespace Menu_Helpers;

	if (_scrolling_disabled) {
		return false;
	}

	const MenuItem * current_active = _menu.get_active();
	const MenuList& items = _menu.items ();
	int c = 0;

	if (!current_active) {
		return true;
	}

	/* work around another gtkmm API clusterfuck
	 * const MenuItem* get_active () const
	 * void set_active (guint index)
	 *
	 * also MenuList.activate_item does not actually
	 * set it as active in the menu.
	 *
	 */

	switch (ev->direction) {
		case GDK_SCROLL_UP:

			for (MenuList::const_reverse_iterator i = items.rbegin(); i != items.rend(); ++i, ++c) {
				if ( &(*i) != current_active) {
					continue;
				}
				if (++i != items.rend()) {
					c = items.size() - 2 - c;
					assert(c >= 0);
					_menu.set_active(c);
					_menu.activate_item(*i);
				}
				break;
			}
			break;
		case GDK_SCROLL_DOWN:
			for (MenuList::const_iterator i = items.begin(); i != items.end(); ++i, ++c) {
				if ( &(*i) != current_active) {
					continue;
				}
				if (++i != items.end()) {
					assert(c + 1 < (int) items.size());
					_menu.set_active(c + 1);
					_menu.activate_item(*i);
				}
				break;
			}
			break;
		default:
			break;
	}
	return true;
}

void
ArdourDropdown::clear_items ()
{
	_menu.items ().clear ();
}

void
ArdourDropdown::AddMenuElem (Menu_Helpers::Element e)
{
	using namespace Menu_Helpers;

	MenuList& items = _menu.items ();

	items.push_back (e);
}

void
ArdourDropdown::disable_scrolling()
{
	_scrolling_disabled = true;
}
