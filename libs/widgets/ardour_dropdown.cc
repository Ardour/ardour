/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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
#include "pbd/error.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "widgets/ardour_dropdown.h"

#include "pbd/i18n.h"

#define REFLECTION_HEIGHT 2

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace ArdourWidgets;
using namespace std;


ArdourDropdown::ArdourDropdown (Element e)
	: _scrolling_disabled(false)
{
	_menu.signal_size_request().connect (sigc::mem_fun(*this, &ArdourDropdown::menu_size_request));

	_menu.set_reserve_toggle_size(false);

	add_elements(e);
	add_elements(ArdourButton::Menu);
}

ArdourDropdown::~ArdourDropdown ()
{
}

void
ArdourDropdown::menu_size_request(Requisition *req) {
	req->width = max(req->width, get_allocation().get_width());
}

bool
ArdourDropdown::on_button_press_event (GdkEventButton* ev)
{
	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
		Gtkmm2ext::anchored_menu_popup(&_menu, this, get_text(), 1, ev->time);
	}

	return true;
}

void
ArdourDropdown::set_active (std::string const& text)
{
	const MenuItem* current_active = _menu.get_active();
	if (current_active && current_active->get_label() == text) {
		set_text (text);
		return;
	}
	using namespace Menu_Helpers;
	const MenuList& items = _menu.items ();
	int c = 0;
	for (MenuList::const_iterator i = items.begin(); i != items.end(); ++i, ++c) {
		if (i->get_label() == text) {
			_menu.set_active(c);
			_menu.activate_item(*i);
			break;
		}
	}
	set_text (text);
	StateChanged (); /* EMIT SIGNAL */
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

void
ArdourDropdown::append_text_item (std::string const& text) {
	using namespace Gtkmm2ext;
	AddMenuElem (MenuElemNoMnemonic (text, sigc::bind (sigc::mem_fun (*this, &ArdourDropdown::default_text_handler), text)));
}

void
ArdourDropdown::default_text_handler (std::string const& text) {
	set_text (text);
	StateChanged (); /* EMIT SIGNAL */
}
