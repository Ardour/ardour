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

*/

#ifndef __gtkmm2ext_tearoff_h__
#define __gtkmm2ext_tearoff_h__

#include <gtkmm/window.h>
#include <gtkmm/arrow.h>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>

#include "gtkmm2ext/visibility.h"

class XMLNode;

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API TearOff : public Gtk::HBox
{
  public:
	TearOff (Gtk::Widget& contents, bool allow_resize = false);
	virtual ~TearOff ();

	void set_visible (bool yn, bool force = false);
	void set_can_be_torn_off (bool);
	bool can_be_torn_off () const { return _can_be_torn_off; }
	bool visible () const { return _visible; }

	sigc::signal<void> Detach;
	sigc::signal<void> Attach;
	sigc::signal<void> Visible;
	sigc::signal<void> Hidden;

	Gtk::Window& tearoff_window() { return own_window; }
	bool torn_off() const;
        void tear_it_off ();
        void put_it_back ();
        void hide_visible ();

        void set_state (const XMLNode&);
        void add_state (XMLNode&) const;

  private:
	Gtk::Widget&   contents;
	Gtk::Window    own_window;
	Gtk::Arrow     tearoff_arrow;
	Gtk::Arrow     close_arrow;
	Gtk::HBox      window_box;
	Gtk::EventBox  tearoff_event_box;
	Gtk::EventBox  close_event_box;
	double         drag_x;
	double         drag_y;
	bool           dragging;
	bool          _visible;
	bool          _torn;
	bool          _can_be_torn_off;
        int            own_window_width;
        int            own_window_height;
        int            own_window_xpos;
        int            own_window_ypos;

	gint tearoff_click (GdkEventButton*);
	gint close_click (GdkEventButton*);

	gint window_motion (GdkEventMotion*);
	gint window_button_press (GdkEventButton*);
	gint window_button_release (GdkEventButton*);
	gint window_delete_event (GdkEventAny*);

        void own_window_realized ();
        bool own_window_configured (GdkEventConfigure*);
};

} /* namespace */

#endif  // __gtkmm2ext_tearoff_h__
