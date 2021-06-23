/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2013 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_gtk_splash_h__
#define __ardour_gtk_splash_h__

#include <set>

#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gdkmm/pixbuf.h>

#include "pbd/signals.h"

class ARDOUR_UI;

class Splash : public Gtk::Window
{
public:
	~Splash ();

	static Splash* instance();
	static void drop();
	static bool exists ();

	void display ();
	void pop_back_for (Gtk::Window&);
	void pop_front_for (Gtk::Window&);

	bool expose (GdkEventExpose*);
	bool on_button_release_event (GdkEventButton*);
	void on_realize ();
	bool on_map_event (GdkEventAny*);
	void message (const std::string& msg);
	void hide ();

private:
	Splash ();
	static Splash* the_splash;

	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
	Gtk::DrawingArea darea;
	Glib::RefPtr<Pango::Layout> layout;

	void pop_front ();
	std::set<Gtk::Window*> _window_stack;

	void boot_message (std::string);
	PBD::ScopedConnection msg_connection;

	sigc::connection idle_connection;
	volatile bool expose_done;
	bool expose_is_the_one;
	bool idle_after_expose ();
};

#endif /* __ardour_gtk_splash_h__ */
