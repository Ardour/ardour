/*
    Copyright (C) 2006 Paul Davis 

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

#include <ardour/basic_ui.h>
#include <ardour/session.h>
#include <ardour/location.h>

#include "i18n.h"

using namespace ARDOUR;

BasicUI::BasicUI (Session& s)
	: session (s)
{
}

BasicUI::~BasicUI ()
{
	
}

void
BasicUI::loop_toggle () 
{
	if (session.get_auto_loop()) {
		session.request_auto_loop (false);
	} else {
		session.request_auto_loop (true);
		if (!session.transport_rolling()) {
			session.request_transport_speed (1.0);
		}
	}
}

void
BasicUI::goto_start ()
{
	session.goto_start ();
}

void
BasicUI::goto_end ()
{
	session.goto_end ();
}

void       
BasicUI::add_marker ()
{
	jack_nframes_t when = session.audible_frame();
	session.locations()->add (new Location (when, when, _("unnamed"), Location::IsMark));
}

void
BasicUI::rewind ()
{
	session.request_transport_speed (-2.0f);
}

void
BasicUI::ffwd ()
{
	session.request_transport_speed (2.0f);
}

void
BasicUI::transport_stop ()
{
	session.request_transport_speed (0.0);
}

void
BasicUI::transport_play ()
{
	bool rolling = session.transport_rolling ();

	if (session.get_auto_loop()) {
		session.request_auto_loop (false);
	} 

	if (session.get_play_range ()) {
		session.request_play_range (false);
	}
	
	if (rolling) {
		session.request_locate (session.last_transport_start(), true);

	}

	session.request_transport_speed (1.0f);
}

void
BasicUI::rec_enable_toggle ()
{
	switch (session.record_status()) {
	case Session::Disabled:
		if (session.ntracks() == 0) {
			// string txt = _("Please create 1 or more track\nbefore trying to record.\nCheck the Session menu.");
			// MessageDialog msg (*editor, txt);
			// msg.run ();
			return;
		}
		session.maybe_enable_record ();
		break;
	case Session::Recording:
	case Session::Enabled:
		session.disable_record (true);
	}
}

void
BasicUI::save_state ()
{
	session.save_state ("");
}

void
BasicUI::prev_marker ()
{
	Location *location = session.locations()->first_location_before (session.transport_frame());
	
	if (location) {
		session.request_locate (location->start(), session.transport_rolling());
	} else {
		session.goto_start ();
	}
}

void
BasicUI::next_marker ()
{
	Location *location = session.locations()->first_location_after (session.transport_frame());

	if (location) {
		session.request_locate (location->start(), session.transport_rolling());
	} else {
		session.request_locate (session.current_end_frame());
	}
}

void
BasicUI::move_at (float speed)
{
	session.request_transport_speed (speed);
}

void
BasicUI::undo ()
{
	session.undo (1);
}

void
BasicUI::redo ()
{
	session.redo (1);
}

void
BasicUI::toggle_all_rec_enables ()
{
	if (session.get_record_enabled()) {
		session.record_disenable_all ();
	} else {
		session.record_enable_all ();
	}
}

		


