/*
    Copyright (C) 2015 Paul Davis

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

#include "ardour/async_midi_port.h"
#include "ardour/monitor_processor.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/track.h"

#include "faderport.h"

using namespace ARDOUR;
using namespace ArdourSurface;

void
FaderPort::undo ()
{
	ControlProtocol::Undo (); /* EMIT SIGNAL */
}

void
FaderPort::redo ()
{
	ControlProtocol::Redo (); /* EMIT SIGNAL */
}

void
FaderPort::mute ()
{
	if (!_current_route) {
		return;
	}

	if (_current_route == session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mp = _current_route->monitor_control();
		mp->set_cut_all (!mp->cut_all());
		return;
	}

	boost::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (_current_route);
	session->set_mute (rl, !_current_route->muted());
}

void
FaderPort::solo ()
{
	if (!_current_route) {
		return;
	}

	boost::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (_current_route);

	if (Config->get_solo_control_is_listen_control()) {
		session->set_listen (rl, !_current_route->listening_via_monitor());
	} else {
		session->set_solo (rl, !_current_route->soloed());
	}
}

void
FaderPort::rec_enable ()
{
	if (!_current_route) {
		return;
	}

	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_current_route);

	if (!t) {
		return;
	}

	boost::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (_current_route);

	session->set_record_enabled (rl, !t->record_enabled());
}

void
FaderPort::use_master ()
{
	boost::shared_ptr<Route> r = session->master_out();
	if (r) {
		if (_current_route == r) {
			r = pre_master_route.lock();
			set_current_route (r);
			if (r == session->monitor_out() || r == session->master_out()) {
				button_info(Output).set_led_state (_output_port, true);
			} else {
				button_info(Output).set_led_state (_output_port, false);
			}
		} else {
			pre_master_route = boost::weak_ptr<Route> (_current_route);
			set_current_route (r);
			button_info(Output).set_led_state (_output_port, true);
		}
	}
}

void
FaderPort::use_monitor ()
{
	boost::shared_ptr<Route> r = session->monitor_out();
	if (r) {
		if (_current_route == r) {
			r = pre_monitor_route.lock();
			set_current_route (r);
			if (r == session->monitor_out() || r == session->master_out()) {
				button_info(Output).set_led_state (_output_port, true);
			} else {
				button_info(Output).set_led_state (_output_port, false);
			}
		} else {
			pre_monitor_route = boost::weak_ptr<Route> (_current_route);
			set_current_route (r);
			button_info(Output).set_led_state (_output_port, true);
		}
	}
}
