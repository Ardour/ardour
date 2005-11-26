/*
    Copyright (C) 2003 Paul Barton-Davis
 
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

    $Id$
*/

#ifndef __gtkmm2ext_tearoff_h__
#define __gtkmm2ext_tearoff_h__

#include <gtkmm.h>

namespace Gtkmm2ext {

class TearOff : public Gtk::HBox
{
  public:
	TearOff (Gtk::Widget& contents);
	virtual ~TearOff ();

	sigc::signal<void> Detach;
	sigc::signal<void> Attach;

	Gtk::Window* tearoff_window() const { return own_window; }
	bool torn_off() const;


  private:
	Gtk::Widget&   contents;
	Gtk::Window*   own_window;
	Gtk::Arrow     tearoff_arrow;
	Gtk::Arrow     close_arrow;
	Gtk::HBox      window_box;
	Gtk::EventBox  tearoff_event_box;
	Gtk::EventBox  close_event_box;
	double         drag_x;
	double         drag_y;
	bool           dragging;

	gint tearoff_click (GdkEventButton*);
	gint close_click (GdkEventButton*);

	gint window_motion (GdkEventMotion*);
	gint window_button_press (GdkEventButton*);
	gint window_button_release (GdkEventButton*);
	gint window_delete_event (GdkEventAny*);
};

} /* namespace */

#endif  // __gtkmm2ext_tearoff_h__
