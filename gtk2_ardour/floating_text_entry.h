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

#ifndef __floating_text_entry_h__
#define __floating_text_entry_h__

#include <gtkmm/entry.h>
#include <gtkmm/window.h>

class FloatingTextEntry : public Gtk::Window
{
public:
	FloatingTextEntry (Gtk::Window* parent, const std::string& initial_contents);

	/* 1st argument to handler is the new text
	 * 2nd argument is 0, 1 or -1 to indicate:
	 *  - do not move to next editable field
	 *  - move to next editable field
	 *  - move to previous editable field.
	 */
	sigc::signal2<void,std::string,int> use_text;

private:
	Gtk::Entry entry;
	bool entry_changed;

	/* handlers for Entry events */
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
};

#endif // __ardour_window_h__
