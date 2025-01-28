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

#pragma once

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "widgets/ardour_button.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API MetaButton : public ArdourButton
{
public:
	MetaButton ();
	virtual ~MetaButton ();

	void add_item (std::string const& label, std::string const& menutext, sigc::slot<void> const&);
	void clear_items ();

	void set_active (std::string const&);
	void set_index (guint);

	guint index () const
	{
		return _active;
	}

protected:
	bool on_button_press_event (GdkEventButton*);
	void menu_size_request (Gtk::Requisition*);

private:
	class MetaMenuItem : public Gtk::MenuItem
	{
	public:
		MetaMenuItem (std::string const& label, std::string const& menutext, sigc::slot<void> const& cb)
			: Gtk::MenuItem (menutext, false)
			, _label (label)
			, _cb (cb)
		{
		}

		void activate () const
		{
			_cb ();
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
		std::string      _label;
		sigc::slot<void> _cb;
	};

	class MetaElement : public Gtk::Menu_Helpers::MenuElem
	{
	public:
		typedef sigc::slot<void, MetaMenuItem const*> SlotActivate;

		MetaElement (std::string const& label, std::string const& menutext, sigc::slot<void> const& cb, SlotActivate const& wrap)
		    : Gtk::Menu_Helpers::MenuElem ("")
		{
			MetaMenuItem* mmi = manage (new MetaMenuItem (label, menutext, cb));
			child_->unreference ();
			set_child (mmi);
			child_->signal_activate ().connect (sigc::bind (wrap, mmi));
			child_->show ();
		}
	};

	void activate_item (MetaMenuItem const*);

	Gtk::Menu _menu;
	guint     _active;
};

} // namespace ArdourWidgets
