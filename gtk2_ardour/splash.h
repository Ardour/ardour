/*
    Copyright (C) 2008 Paul Davis

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

#ifndef __ardour_gtk_splash_h__
#define __ardour_gtk_splash_h__

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
	Splash ();
	~Splash ();

	static Splash* instance() { return the_splash; }

	void pop_back_for (Gtk::Window&);
	void pop_front ();

	bool expose (GdkEventExpose*);
	bool on_button_release_event (GdkEventButton*);
	void on_realize ();

	void message (const std::string& msg);

  private:
	static Splash* the_splash;

	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
	Gtk::DrawingArea darea;
	Glib::RefPtr<Pango::Layout> layout;

	void boot_message (std::string);
	PBD::ScopedConnection msg_connection;
};

#endif /* __ardour_gtk_splash_h__ */
