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

#include <ytkmm/action.h>
#include <ytkmm/menu.h>
#include <ytkmm/menuitem.h>

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
	void add_menu_elem (Gtk::Menu_Helpers::Element e);
	void append_text_item (std::string const& text);
	void add_separator ();
	void append (Glib::RefPtr<Gtk::Action>);
	void append (Gtk::Menu&, Glib::RefPtr<Gtk::Action>);

	void disable_scrolling();

	Gtk::Menu_Helpers::MenuList& items () { return _menu.items (); }
	Gtk::Menu& menu () { return _menu; }

	void set_active (std::string const& text);

protected:
	void default_text_handler (std::string const&);

private:
	class LblMenuItem : public Gtk::MenuItem
	{
	public:
		LblMenuItem (std::string const& label, std::string const& menutext)
			: Gtk::MenuItem (menutext, false)
			, _label (label)
		{
		}

		std::string label () const
		{
			return _label;
		}

		std::string menutext () const
		{
			return get_label ();
		}

	private:
		std::string _label;
	};

	class LblMenuElement : public Gtk::Menu_Helpers::MenuElem
	{
	public:
		LblMenuElement (Glib::RefPtr<Gtk::Action> action)
			: Gtk::Menu_Helpers::MenuElem ("")
		{
			LblMenuItem* mmi = manage (new LblMenuItem (action->get_short_label(), action->get_label()));
			child_->unreference ();
			set_child (mmi);
			child_->signal_activate ().connect (sigc::mem_fun (action.get(), &Gtk::Action::activate));
			child_->show ();
		}
	};

	Gtk::Menu      _menu;

	bool _scrolling_disabled;
};

} /* end namespace */

#endif
