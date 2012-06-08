/*
    Copyright (C) 2011 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#ifndef __ardour_visibility_group__
#define __ardour_visibility_group__

#include <gtkmm/liststore.h>
#include "pbd/signals.h"

class XMLNode;
class XMLProperty;

/** A class to manage a group of widgets where the visibility of each
 *  can be configured by the user.  The class can generate a menu to
 *  set up visibility, and save and restore visibility state to XML.
 */

class VisibilityGroup
{
public:
	VisibilityGroup (std::string const &);
	
	void add (
		Gtk::Widget *,
		std::string const &,
		std::string const &,
		bool visible = false,
		boost::function<boost::optional<bool> ()> = 0
		);
	
	Gtk::Menu* menu ();
	Gtk::Widget* list_view ();
	bool button_press_event (GdkEventButton *);
	void update ();
	void set_state (XMLNode const &);
	void set_state (std::string);
	std::string get_state_name () const;
	std::string get_state_value () const;

	PBD::Signal0<void> VisibilityChanged;
	
private:

	struct Member {
		Gtk::Widget* widget;
		std::string  id;
		std::string  name;
		bool         visible;
		boost::function<boost::optional<bool> ()> override;
	};

	class ModelColumns : public Gtk::TreeModelColumnRecord {
	public:
		ModelColumns () {
			add (_visible);
			add (_name);
			add (_iterator);
		}

		Gtk::TreeModelColumn<bool> _visible;
		Gtk::TreeModelColumn<std::string> _name;
		Gtk::TreeModelColumn<std::vector<Member>::iterator> _iterator;
	};

	void toggle (std::vector<Member>::iterator);
	void list_view_visible_changed (std::string const &);
	void update_list_view ();
	bool should_actually_be_visible (Member const &) const;

	std::vector<Member> _members;
	std::string _xml_property_name;
	ModelColumns _model_columns;
	Glib::RefPtr<Gtk::ListStore> _model;
	bool _ignore_list_view_change;
};

#endif
