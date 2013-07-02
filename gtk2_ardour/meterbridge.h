/*
    Copyright (C) 2012 Paul Davis
    Author: Robin Gareus

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
#ifndef __ardour_meterbridge_h__
#define __ardour_meterbridge_h__

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "gtkmm2ext/visibility_tracker.h"

#include "meter_strip.h"

class Meterbridge :
	public Gtk::Window,
	public PBD::ScopedConnectionList,
	public ARDOUR::SessionHandlePtr,
	public Gtkmm2ext::VisibilityTracker
{
  public:
	static Meterbridge* instance();
	~Meterbridge();

	void set_session (ARDOUR::Session *);

	XMLNode& get_state (void);
	int set_state (const XMLNode& );

	void show_window ();
	bool hide_window (GdkEventAny *ev);

  private:
	Meterbridge ();
	static Meterbridge* _instance;

	bool _visible;
	bool _show_busses;

	Gtk::ScrolledWindow scroller;
	Gtk::HBox global_hpacker;
	Gtk::VBox global_vpacker;

	gint start_updating ();
	gint stop_updating ();

	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void add_strips (ARDOUR::RouteList&);
	void remove_strip (MeterStrip *);

	void session_going_away ();
	void sync_order_keys (ARDOUR::RouteSortOrderKey src);

	std::list<MeterStrip *> strips;

	MeterStrip *metrics_left;
	MeterStrip *metrics_right;

	static const int32_t default_width = 600;
	static const int32_t default_height = 400;

	void update_title ();

	// for restoring window geometry.
	int m_root_x, m_root_y, m_width, m_height;

	void set_window_pos_and_size ();
	void get_window_pos_and_size ();

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

	void parameter_changed (std::string const & p);
};

#endif
