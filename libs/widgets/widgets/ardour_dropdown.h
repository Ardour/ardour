/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_ARDOUR_DROPDOWN_H_
#define _WIDGETS_ARDOUR_DROPDOWN_H_

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "widgets/ardour_button.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourDropdown : public ArdourButton
{
public:

	ArdourDropdown (Element e = default_elements);
	virtual ~ArdourDropdown ();

	bool on_button_press_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	void menu_size_request(Gtk::Requisition*);

	void clear_items ();
	void AddMenuElem (Gtk::Menu_Helpers::Element e);

	void disable_scrolling();

	Gtk::Menu_Helpers::MenuList& items () { return _menu.items (); }

	void append_text_item (std::string const& text);
	void set_active (std::string const& text);

protected:
	void default_text_handler (std::string const&);

private:
	Gtk::Menu      _menu;

	bool _scrolling_disabled;
};

} /* end namespace */

#endif
