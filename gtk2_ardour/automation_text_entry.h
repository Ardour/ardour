/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <ytkmm/entry.h>
#include <ytkmm/window.h>

class AutomationTextEntry : public Gtk::Window
{
public:
	AutomationTextEntry (Gtk::Window* parent, std::string const& initial_contents);
	~AutomationTextEntry();

	/* 1st argument to handler is the new text
	 * 2nd argument is 0, 1 or -1 to indicate:
	 *  - do not move to next editable field
	 *  - move to next editable field
	 *  - move to previous editable field.
	 */
	sigc::signal2<void,std::string,int> use_text;
	void delete_on_focus_out ();
	sigc::signal1<void,AutomationTextEntry*> going_away;

	/* grabs focus to be setup for editing */
	void activate_entry ();

private:
	Gtk::Entry entry;
	Gtk::Label units;
	bool entry_changed;

	/* handlers for Entry events */
	bool entry_focus_in (GdkEventFocus*);
	bool entry_focus_out (GdkEventFocus*);
	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	void activated ();
	bool button_press (GdkEventButton*);
	void changed ();
	void idle_delete_self ();
	void disconect_signals ();

	std::list<sigc::connection> _connections;

	/* handlers for window events */

	void on_realize ();
	void on_hide ();

	void split_units (std::string const &, std::string & numeric_part, std::string & units_part);
};

