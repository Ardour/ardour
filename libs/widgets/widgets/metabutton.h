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

#include <ytkmm/menu.h>
#include <ytkmm/menuitem.h>

#include "widgets/ardour_button.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API MetaButton : public ArdourButton
{
public:
	MetaButton ();
	virtual ~MetaButton ();

	void add_item (std::string const& label, std::string const& menutext, sigc::slot<void> const&);
	void add_item (std::string const& label, std::string const& menutext, Gtk::Menu&, sigc::slot<void> const&);
	void clear_items ();

	void set_active (std::string const&);
	void set_index (guint);
	void set_by_menutext (std::string const & mt);

	guint index () const
	{
		return _active;
	}

	Gtk::Menu& menu() { return _menu; }

	bool is_menu_popup_event (GdkEventButton* ev) const;

protected:
	bool on_button_press_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	void menu_size_request (Gtk::Requisition*);
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);

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

		MetaMenuItem (std::string const& label, std::string const& menutext, sigc::slot<void> const & cb, Gtk::Menu& submenu)
			: Gtk::MenuItem (menutext, false)
			, _label (label)
			, _cb (cb)
		{
			set_submenu (submenu);
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
		MetaElement (std::string const& label, std::string const & menutext, sigc::slot<void> const & cb, SlotActivate const & wrap, Gtk::Menu& submenu)
			: Gtk::Menu_Helpers::MenuElem ("")
		{
			MetaMenuItem* mmi = manage (new MetaMenuItem (label, menutext, cb, submenu));
			child_->unreference ();
			set_child (mmi);
			child_->signal_activate ().connect (sigc::bind (wrap, mmi));
			child_->show ();
		}

	};

	void activate_item (MetaMenuItem const*);
	void update_button (MetaMenuItem const*);

	Gtk::Menu _menu;
	guint     _active;
	bool      _hover_dropdown;
};

} // namespace ArdourWidgets
