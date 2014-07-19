/*
    Copyright (C) 2014 Waves Audio Ltd.

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
#ifndef __ardour_compact_meter_bridge_h__
#define __ardour_compact_meter_bridge_h__

#include <glibmm/thread.h>

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

#include "waves_ui.h"
#include "compact_meter_strip.h"

class CompactMeterbridge :
	public Gtk::EventBox,
	public WavesUI,
	public PBD::ScopedConnectionList,
	public ARDOUR::SessionHandlePtr
{
  public:
	CompactMeterbridge ();
	~CompactMeterbridge();
	void set_session (ARDOUR::Session *);

  private:

	Gtk::Box& _compact_meter_strips_home;

	gint start_updating ();
	gint stop_updating ();

	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void add_strips (ARDOUR::RouteList&);
	void remove_strip (CompactMeterStrip *);

	void session_going_away ();

	std::list<CompactMeterStrip*> _strips;
};

#endif //__ardour_compact_meter_bridge_h__
