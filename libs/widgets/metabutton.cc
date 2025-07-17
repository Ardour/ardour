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

#include "pbd/unwind.h"

#include "widgets/metabutton.h"
#include "widgets/ui_config.h"

using namespace Gtk;
using namespace std;
using namespace ArdourWidgets;

MetaButton::MetaButton ()
	: _active (0)
	, _hover_dropdown (false)
{
	_menu.signal_size_request ().connect (sigc::mem_fun (*this, &MetaButton::menu_size_request));
	_menu.set_reserve_toggle_size (false);

	add_elements (default_elements);
	add_elements (ArdourButton::Menu);
	add_elements (ArdourButton::MetaMenu);
	add_events (Gdk::POINTER_MOTION_MASK);
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
		update_button (dynamic_cast<MetaMenuItem*> (&items.back ()));
		set_text (label);
	}
}

void
MetaButton::add_item (std::string const& label, std::string const & menutext, Gtk::Menu& submenu, sigc::slot<void> const & cb)
{
	using namespace Menu_Helpers;

	add_sizing_text (label);
	MenuList& items = _menu.items ();
	items.push_back (MetaElement (label, menutext, cb, sigc::mem_fun (*this, &MetaButton::activate_item), submenu));
	if (items.size () == 1) {
		_menu.set_active (0);
		update_button (dynamic_cast<MetaMenuItem*> (&items.back ()));
		set_text (label);
	}
}

bool
MetaButton::is_menu_popup_event (GdkEventButton* ev) const
{
	return ((ev->type == GDK_BUTTON_PRESS && ev->button == 3) ||
	        (ev->type == GDK_BUTTON_PRESS && ev->button == 1 && ev->x > (get_width () - _diameter - 7)));
}

bool
MetaButton::on_button_press_event (GdkEventButton* ev)
{
	MetaMenuItem const* current_active = dynamic_cast<MetaMenuItem*> (_menu.get_active ());

	if (is_menu_popup_event (ev)) {
		Gtkmm2ext::anchored_menu_popup (&_menu, this, current_active ? current_active->menutext () : "", ev->button, ev->time);
		return true;
	}

	if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
		if (current_active) {
			current_active->activate ();
		}
	}

	return true;
}

bool
MetaButton::on_motion_notify_event (GdkEventMotion* ev)
{
	bool hover_dropdown = ev->x > get_width () - _diameter - 7;
	if (hover_dropdown != _hover_dropdown) {
		_hover_dropdown = hover_dropdown;
		CairoWidget::set_dirty ();
	}
	return false;
}

void
MetaButton::activate_item (MetaMenuItem const* e)
{
	update_button (e);
	e->activate ();

	_active = 0;
	for (auto& i : _menu.items ()) {
		if (e == dynamic_cast<MetaMenuItem*> (&i)) {
			break;
		}
		++_active;
	}
}

void
MetaButton::update_button (MetaMenuItem const* e)
{
	set_text (e->label ());
}

void
MetaButton::set_active (std::string const& menulabel)
{
	MetaMenuItem const* current_active = dynamic_cast<MetaMenuItem*> (_menu.get_active ());
	if (!current_active) {
		set_active_state (Gtkmm2ext::Off);
		return;
	}
	if (current_active->menutext () == menulabel) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		set_active_state (Gtkmm2ext::Off);
	}
}

void
MetaButton::set_by_menutext (std::string const & mt)
{
	guint c = 0;
	for (auto & i : _menu.items()) {
		if (i.get_label() == mt) {
			_menu.set_active (c);
			_active = c;
			update_button (dynamic_cast<MetaMenuItem*> (&i));
			break;
		}
		++c;
	}
}

void
MetaButton::set_index (guint index)
{
	guint c = 0;
	for (auto& i : _menu.items ()) {
		if (c == index) {
			_menu.set_active (c);
			_active = c;
			update_button (dynamic_cast<MetaMenuItem*> (&i));
			break;
		}
		++c;
	}
}

void
MetaButton::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t* rect)
{
	{
		PBD::Unwinder uw (_hovering, false);
		ArdourButton::render (ctx, rect);
	}
	if (_hovering && UIConfigurationBase::instance ().get_widget_prelight ()) {
		const bool  boxy          = (_tweaks & ForceBoxy) | boxy_buttons ();
		const float corner_radius = boxy ? 0 : std::max (2.f, _corner_radius * UIConfigurationBase::instance ().get_ui_scale ());
		cairo_t*    cr            = ctx->cobj ();
		if (_hover_dropdown) {
			Gtkmm2ext::rounded_right_half_rectangle (cr, get_width () - _diameter - 6, 1, _diameter + 5, get_height () - 2, corner_radius);
		} else {
			Gtkmm2ext::rounded_left_half_rectangle (cr, 1, 1, get_width () - _diameter - 7, get_height () - 2, corner_radius);
		}
		cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.2);
		cairo_fill (cr);
	}
}
