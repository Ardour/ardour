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

#include <cmath>
#include <ctime>
#include <sys/stat.h>

#include <pbd/pathscanner.h>
#include <pbd/fastlog.h>
#include <gtkmmext/utils.h>
#include <gtkmmext/selector.h>

#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/port.h>
#include <ardour/utils.h>

#include "meter_bridge_strip.h"
#include "ardour_ui.h"
#include "prompter.h"
#include "logmeter.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmmext;

MeterBridgeStrip::MeterBridgeStrip (AudioEngine &eng, 
				    Session& s,
				    Route& r, 
				    string name,
				    jack_nframes_t long_over,
				    jack_nframes_t short_over, 
				    jack_nframes_t meter_hold)
	: engine (eng),
	  session (s),
	  _route (r),
	  meter (meter_hold, 5, FastMeter::Vertical)
{
	char buf[64];

	label.set_text (name);
	label.set_name ("ChannelMeterLabel");

	label_ebox.set_name ("MeterBridgeWindow");
	label_ebox.set_events (GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
	label_ebox.add (label);

	label_ebox.button_release_event.connect (slot (*this, &MeterBridgeStrip::label_button_press_release));
	ARDOUR_UI::instance()->tooltips().set_tip (label_ebox, _route.name());

	over_long_label.set_text ("0");
	over_long_label.set_name ("OverMeterLabel");
	over_long_button.add (over_long_label);
	over_long_button.set_name ("OverMeter");
	over_long_frame.add (over_long_button);
	over_long_frame.set_shadow_type (GTK_SHADOW_IN);
	over_long_frame.set_name ("BaseFrame");
	over_long_hbox.pack_start (over_long_frame, false, false);

	snprintf(buf, sizeof(buf)-1, _("# of %u-sample overs"), long_over);
	ARDOUR_UI::instance()->tooltips().set_tip (over_long_button, buf);

	set_usize_to_display_given_text (over_long_button, "88g", 2, 2);

	over_short_label.set_text ("0");
	over_short_label.set_name ("OverMeterLabel");
	over_short_button.add (over_short_label);
	over_short_button.set_name ("OverMeter");
	over_short_frame.add (over_short_button);
	over_short_frame.set_shadow_type (GTK_SHADOW_IN);
	over_short_frame.set_name ("BaseFrame");
	over_short_hbox.pack_start (over_short_frame, false, false);

	snprintf(buf, sizeof(buf)-1, _("# of %u-sample overs"), short_over);
	ARDOUR_UI::instance()->tooltips().set_tip (over_short_button, buf);

	set_usize_to_display_given_text (over_short_button, "88g", 2, 2);
	above_meter_vbox.set_spacing (5);
	below_meter_vbox.set_spacing (5);

	above_meter_vbox.pack_start (over_long_hbox, false, false);
	above_meter_vbox.pack_start (over_short_hbox, false, false);

	below_meter_vbox.pack_start (label_ebox);

	over_short_button.button_release_event.connect (slot (*this,&MeterBridgeStrip::gui_clear_overs));
	over_long_button.button_release_event.connect (slot (*this,&MeterBridgeStrip::gui_clear_overs));

	last_over_short = 0;
	last_over_long = 0;

	meter_clear_pending = false;
	over_clear_pending = false;

	meter_on = true;
}

void
MeterBridgeStrip::update ()
{
	string buf;
	Port *port;
	guint32 over_short = 0;
	guint32 over_long = 0;

	if ((port = _route.input (0)) == 0) {
		meter.set (0.0);
		return;
	} else {
		over_short = port->short_overs ();
		over_long = port->long_overs ();
	}

	if (meter_on) {
		float power = minus_infinity();

		if ((power = _route.peak_input_power (0)) != minus_infinity()) {
			meter.set (log_meter (power));

			if (over_short != last_over_short) {
				buf = compose("%1", over_short);
				over_short_label.set_text (buf);
				last_over_short = over_short;
			}
			
			if (over_long != last_over_long) {
				buf = compose("%1", over_long);
				over_long_label.set_text (buf);
				last_over_long = over_long;
			}
			
		} else {
			meter.set (0.0);
		}
		
	}

	if (meter_clear_pending) {
		meter_clear_pending = false;
		meter.clear ();
	}

	if (over_clear_pending) {
		over_clear_pending = false;
		port->reset_overs ();
		over_long_label.set_text ("0");
		over_short_label.set_text ("0");
		last_over_short = 0;
		last_over_long = 0;
	}
}

gint
MeterBridgeStrip::gui_clear_overs (GdkEventButton *ev)
{
	clear_overs ();
	return FALSE;
}

void
MeterBridgeStrip::clear_meter ()

{
	meter_clear_pending = true;
}

void
MeterBridgeStrip::clear_overs ()

{
	over_clear_pending = true;
}

void
MeterBridgeStrip::set_meter_on (bool yn)
{
	Port* port;
	meter_on = yn;
	
	if (!meter_on) {
		meter_clear_pending = true;
		over_clear_pending = true;
	}
	
	if (meter.is_visible()) {
		if ((port = _route.input (0)) != 0) {
			if (meter_on) {
				port->enable_metering ();
			} else {
				port->disable_metering ();
			}
		}
	}
}

gint
MeterBridgeStrip::label_button_press_release (GdkEventButton *ev)
{
	ArdourPrompter prompter (true);
	prompter.set_prompt (_("New name for meter:"));
	prompter.set_initial_text (label.get_text());
	prompter.done.connect (Gtk::Main::quit.slot());
	prompter.show_all();
	
	Gtk::Main::run();
	
	if (prompter.status == Gtkmmext::Prompter::entered) {
		string name;

		prompter.get_result (name);

		if (name.length()) {
			label.set_text(name);
		}
	}
	
	return FALSE;
}

