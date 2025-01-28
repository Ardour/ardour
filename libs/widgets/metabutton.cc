/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/utils.h"

#include "widgets/metabutton.h"

using namespace Gtk;
using namespace std;
using namespace ArdourWidgets;

MetaButton::MetaButton ()
	: _active (0)
{
	_menu.signal_size_request ().connect (sigc::mem_fun (*this, &MetaButton::menu_size_request));
	_menu.set_reserve_toggle_size (false);

	add_elements (default_elements);
	add_elements (ArdourButton::Menu);
}

MetaButton::~MetaButton ()
{
}

void
MetaButton::menu_size_request (Requisition* req)
{
	req->width = max (req->width, get_allocation ().get_width ());
}

void
MetaButton::clear_items ()
{
	_menu.items ().clear ();
}

void
MetaButton::add_item (std::string const& label, std::string const& menutext, sigc::slot<void> const& cb)
{
	using namespace Menu_Helpers;

	add_sizing_text (label);
	MenuList& items = _menu.items ();
	items.push_back (MetaElement (label, menutext, cb, sigc::mem_fun (*this, &MetaButton::activate_item)));
	if (items.size () == 1) {
		_menu.set_active (0);
		set_text (label);
	}
}

bool
MetaButton::on_button_press_event (GdkEventButton* ev)
{
	MetaMenuItem const* current_active = dynamic_cast<MetaMenuItem*> (_menu.get_active ());

	if (ev->type == GDK_BUTTON_PRESS && ev->button == 3) {
		Gtkmm2ext::anchored_menu_popup (&_menu, this, current_active ? current_active->menutext () : "", 3, ev->time);
	}

	if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
		if (current_active) {
			current_active->activate ();
		}
	}

	return true;
}

void
MetaButton::activate_item (MetaMenuItem const* e)
{
	set_text (e->label ());
	e->activate ();
}

void
MetaButton::set_active (std::string const& menulabel)
{
	guint c     = 0;
	bool  found = false;

	for (auto& i : _menu.items ()) {
		if (i.get_label () == menulabel) {
			if (_menu.get_active () != &i) {
				_menu.set_active (c);
			}
			set_text (dynamic_cast<MetaMenuItem*> (&i)->label ());
			set_active_state (Gtkmm2ext::ExplicitActive);
			_active = c;
			found   = true;
			break;
		}
		++c;
	}
	if (!found) {
		set_active_state (Gtkmm2ext::Off);
	}
	StateChanged (); /* EMIT SIGNAL */
}

void
MetaButton::set_index (guint index)
{
	guint c = 0;
	for (auto& i : _menu.items ()) {
		if (c == index) {
			_menu.set_active (c);
			set_text (dynamic_cast<MetaMenuItem*> (&i)->label ());
			break;
		}
		++c;
	}
}
