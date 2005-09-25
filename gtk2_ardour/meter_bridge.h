/*
    Copyright (C) 1999-2002 Paul Davis 

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

#ifndef __ardour_meter_bridge_h__
#define __ardour_meter_bridge_h__

#include <list>

#include <gtk--/eventbox.h>
#include <gtk--/viewport.h>
#include <gtk--/scrolledwindow.h>
#include <gtk--/box.h>
#include <gtk--/fixed.h>
#include <gtk--/frame.h>

#include "keyboard_target.h"
#include "ardour_dialog.h"

class MeterBridgeStrip;

namespace ARDOUR {
	class Session;
	class Route;
}

class MeterBridge : public ArdourDialog
{

  public:
	MeterBridge ();
	~MeterBridge ();

	void set_session (ARDOUR::Session*);
	void clear_all_meters ();
	void start_metering ();
	void stop_metering ();
	void toggle_metering ();

  protected:
	gint map_event_impl (GdkEventAny *);
	gint unmap_event_impl (GdkEventAny *);

  private:
	/* diskstream/recorder display */

	Gtk::Viewport            meter_viewport;
	Gtk::ScrolledWindow      meter_scroller;
	Gtk::EventBox            meter_scroll_base;
	Gtk::HBox                meter_scroller_hpacker;
	Gtk::VBox                meter_scroller_vpacker;
	Gtk::VBox                metering_vpacker;
	Gtk::VBox                metering_hpacker;

	Gtk::VBox                metering_vbox;
	Gtk::HBox                metering_hbox;
	Gtk::Fixed               upper_metering_box;
	Gtk::Fixed               lower_metering_box;
	Gtk::Fixed               meter_base;
	Gtk::Frame               meter_frame;

	list<MeterBridgeStrip*>  meters;
	
	bool                    metering;
	SigC::Connection        metering_connection;

	void update ();

	void add_route (ARDOUR::Route*);
	void remove_route (ARDOUR::Route*);
	void session_gone(); /* overrides ArdourDialog::session_gone() */
};

#endif /* __ardour_meter_bridge_h__ (*/
