/*
    Copyright (C) 1999 Paul Davis 

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

#ifndef __ardour_meterbridgestrip_h__
#define __ardour_meterbridgestrip_h__

#include <sigc++/signal.h>

#include <gtkmm/box.h>
#include <gtkmm/widget.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>
#include <gtkmm/frame.h>

#include <gtkmm2ext/fastmeter.h>

namespace ARDOUR {
	class AudioEngine;
	class Session;
	class Route;
}

namespace Gtkmm2ext {
	class Selector;
	struct SelectionResult;
}

class MeterBridgeStrip : public sigc::trackable

{
  public:
	MeterBridgeStrip (ARDOUR::AudioEngine &, 
			  ARDOUR::Session&,
			  ARDOUR::Route&,
			  string label,
			  jack_nframes_t long_over,
			  jack_nframes_t short_over,
			  jack_nframes_t meter_hold);
	
	void update ();  /* called by meter timeout handler from ARDOUR_UI */

	Gtk::Box &above_box() { return above_meter_vbox; }
	Gtk::Box &below_box() { return below_meter_vbox; }
	Gtk::Widget &meter_widget() { return meter; }
	
	guint32 meter_width() const { return 8; }

	void clear_meter ();
	void clear_overs ();

	void set_meter_on (bool yn);
	bool get_meter_on () const { return meter_on; }
	
	ARDOUR::Route& route() const { return _route; }

  private:
	ARDOUR::AudioEngine&            engine;
	ARDOUR::Session&                session;
	ARDOUR::Route&                 _route;

	Gtk::EventBox           label_ebox;
	Gtk::Label              label;
	bool                    meter_clear_pending;
	bool                    over_clear_pending;

	Gtkmm2ext::FastMeter meter;
	bool                meter_on;

	Gtk::VBox          above_meter_vbox;
	Gtk::VBox          below_meter_vbox;

	Gtk::HBox          over_long_hbox;
	Gtk::HBox          over_long_vbox;
	Gtk::EventBox      over_long_button;
	Gtk::Frame         over_long_frame;
	Gtk::Label         over_long_label;

	Gtk::HBox          over_short_hbox;
	Gtk::HBox          over_short_vbox;
	Gtk::EventBox      over_short_button;
	Gtk::Frame         over_short_frame;
	Gtk::Label         over_short_label;

	guint32            last_over_short;
	guint32            last_over_long;

	gint gui_clear_overs (GdkEventButton *);
	gint label_button_press_release (GdkEventButton *);
};

#endif  /* __ardour_meterbridgestrip_h__ */


