/*
    Copyright (C) 2002 Paul Davis 

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

#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/audio_diskstream.h>
#include <ardour/audio_track.h>

#include "ardour_ui.h"
#include "meter_bridge.h"
#include "meter_bridge_strip.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace sigc;

#define FRAME_SHADOW_STYLE Gtk::SHADOW_IN
#define FRAME_NAME "BaseFrame"

MeterBridge::MeterBridge ()
	: ArdourDialog ("meter bridge"),
	  hadjustment (0.0, 0.0, 0.0),
	  vadjustment (0.0, 0.0, 0.0),
	meter_viewport (hadjustment, vadjustment)
{
	meter_base.set_name ("MeterBase");
	meter_frame.set_shadow_type (FRAME_SHADOW_STYLE);
	meter_frame.set_name (FRAME_NAME);
	meter_frame.add (meter_base);

	upper_metering_box.set_name ("AboveMeterZone");
	lower_metering_box.set_name ("BelowMeterZone");

	metering_vbox.set_spacing (5);
	metering_vbox.set_border_width (10);
	metering_vbox.pack_start (upper_metering_box, false, false);
	metering_vbox.pack_start (meter_frame, false, false);
	metering_vbox.pack_start (lower_metering_box, false, false);

	metering_hbox.pack_start (metering_vbox, false, false);

	meter_scroll_base.set_name ("MeterScrollBase");
	meter_scroll_base.add (metering_hbox);

	meter_viewport.add (meter_scroll_base);
	meter_viewport.set_shadow_type (Gtk::SHADOW_NONE);

	meter_scroller.add (meter_viewport);
	meter_scroller.set_name ("MeterBridgeWindow");
	meter_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	meter_scroller.set_border_width (5);

	add (meter_scroller);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_name ("MeterBridgeWindow");
	set_title (_("ardour: meter bridge"));
	set_wmclass (X_("ardour_meter_bridge"), "Ardour");
	// set_policy (false, false, false); // no user resizing of any kind

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), static_cast<Gtk::Window*>(this)));

	metering = false;

	/* don't show: this window doesn't come up by default */
}

MeterBridge::~MeterBridge ()
{
	stop_metering ();
}

void
MeterBridge::set_session (Session *s)
{
	ArdourDialog::set_session (s);

	if (session) {
		// XXX this stuff has to be fixed if we ever use this code again
		// (refs vs. ptrs)
		// session->foreach_route (this, &MeterBridge::add_route);
		session->RouteAdded.connect (mem_fun(*this, &MeterBridge::add_route));
		session->GoingAway.connect (mem_fun(*this, &MeterBridge::session_gone));
		start_metering ();
	}
}

void
MeterBridge::session_gone ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MeterBridge::session_gone));
	
	stop_metering ();
	hide_all ();

	list<MeterBridgeStrip *>::iterator i;

	for (i = meters.begin(); i != meters.end(); ++i) {

		upper_metering_box.remove ((*i)->above_box());
		meter_base.remove ((*i)->meter_widget());
		lower_metering_box.remove ((*i)->below_box());

//		delete (*i);
	}

	meters.clear ();

	ArdourDialog::session_gone();
}	

void
MeterBridge::add_route (ARDOUR::Route* route)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MeterBridge::add_route), route));
	
	uint32_t n;
	char buf[32];

	if (!session || route->hidden() || dynamic_cast<AudioTrack*>(route) == 0) {
		return;
	}

	n = meters.size();
	snprintf (buf, sizeof (buf), "%u", n+1);

	MeterBridgeStrip *meter = new MeterBridgeStrip (session->engine(), 
							*session,
							*route,
							buf,
							session->over_length_long,
							session->over_length_short,
							200);
	
#define packing_factor 30

	upper_metering_box.put (meter->above_box(), n * packing_factor, 0);

	meter_base.put (meter->meter_widget(), (n * packing_factor) + (meter->meter_width()/2), 0);
	lower_metering_box.put (meter->below_box(), n * packing_factor, 0);

	meter->above_box().show_all ();
	meter->meter_widget().show ();
	meter->below_box().show_all ();

	route->GoingAway.connect (bind (mem_fun(*this, &MeterBridge::remove_route), route));
	meters.insert (meters.begin(), meter);

	set_default_size (30 + ((n+1) * packing_factor), 315);
    
	meter->set_meter_on(true);
	
	session->GoingAway.connect (mem_fun(*this, &MeterBridge::session_gone));
}

void
MeterBridge::remove_route (Route* route)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MeterBridge::remove_route), route));
	list<MeterBridgeStrip *>::iterator i;

	for (i = meters.begin(); i != meters.end(); ++i) {
		if (&((*i)->route()) == route) {
			delete *i;
			meters.erase (i);
			return;
		}
	}
}

void
MeterBridge::clear_all_meters ()
{
	list<MeterBridgeStrip *>::iterator i;

	for (i = meters.begin(); i != meters.end(); ++i) {
		(*i)->clear_meter ();
	}
}

void
MeterBridge::update ()
{
	list<MeterBridgeStrip *>::iterator i;

	for (i = meters.begin(); i != meters.end(); ++i) {
		(*i)->update ();
	}
}

void
MeterBridge::start_metering ()
{
	list<MeterBridgeStrip *>::iterator i;
	
	for (i = meters.begin(); i != meters.end(); ++i) {
		(*i)->set_meter_on (true);
	}
	metering_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect 
		(mem_fun(*this, &MeterBridge::update));
	metering = true;
}

void
MeterBridge::stop_metering ()
{
	list<MeterBridgeStrip *>::iterator i;
	
	for (i = meters.begin(); i != meters.end(); ++i) {
		(*i)->set_meter_on (false);
	}
	metering_connection.disconnect();
	metering = false;
}

void
MeterBridge::toggle_metering ()
{
	if (!metering) {
		start_metering ();
	} else {
		stop_metering ();
	}
}

void
MeterBridge::on_map ()
{
	start_metering ();
	return Window::on_map ();
}

void
MeterBridge::on_unmap ()
{
	stop_metering ();
	return Window::on_unmap ();
}

