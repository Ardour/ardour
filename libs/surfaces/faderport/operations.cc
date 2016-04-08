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
#include "ardour/pannable.h"
#include "ardour/plugin_insert.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/types.h"

#include "faderport.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;

/* this value is chosen to given smooth motion from 0..1.0 in about 270 degrees
 * of encoder rotation.
 */
static const double encoder_divider = 24.0;

void
FaderPort::left ()
{
	access_action ("Editor/select-prev-route");

	//ToDo:  bank by 8?
	//if ( (button_state & ShiftDown) == ShiftDown )

}

void
FaderPort::right ()
{
	access_action ("Editor/select-next-route");

	//ToDo:  bank by 8?
	//if ( (button_state & ShiftDown) == ShiftDown )
}


void
FaderPort::read ()
{
	if (_current_route) {
		boost::shared_ptr<AutomationControl> gain = _current_route->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Play );
		}
	}
}

void
FaderPort::write ()
{
	if (_current_route) {
		boost::shared_ptr<AutomationControl> gain = _current_route->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Write );
		}
	}
}

void
FaderPort::touch ()
{
	if (_current_route) {
		boost::shared_ptr<AutomationControl> gain = _current_route->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Touch );
		}
	}
}

void
FaderPort::off ()
{
	if (_current_route) {
		boost::shared_ptr<AutomationControl> gain = _current_route->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Off );
		}
	}
}




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

	boost::shared_ptr<ControlList> cl (new ControlList);
	cl->push_back (_current_route->mute_control());
	session->set_controls (cl, !_current_route->muted(), PBD::Controllable::UseGroup);
}

void
FaderPort::solo ()
{
	if (!_current_route) {
		return;
	}

	bool yn;

	if (Config->get_solo_control_is_listen_control()) {
		yn = !_current_route->listening_via_monitor();
	} else {
		yn = !_current_route->soloed();
	}

	_current_route->solo_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::UseGroup);
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

	t->rec_enable_control()->set_value (!t->rec_enable_control()->get_value(), Controllable::UseGroup);
}

void
FaderPort::use_master ()
{
	boost::shared_ptr<Route> r = session->master_out();
	if (r) {
		if (_current_route == r) {
			r = pre_master_route.lock();
			set_current_route (r);
			get_button(Output).set_led_state (_output_port, false);
			blinkers.remove (Output);
		} else {
			if (_current_route != session->master_out() && _current_route != session->monitor_out()) {
				pre_master_route = boost::weak_ptr<Route> (_current_route);
			}
			set_current_route (r);
			get_button(Output).set_led_state (_output_port, true);
			blinkers.remove (Output);
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
			get_button(Output).set_led_state (_output_port, false);
			blinkers.remove (Output);
		} else {
			if (_current_route != session->master_out() && _current_route != session->monitor_out()) {
				pre_monitor_route = boost::weak_ptr<Route> (_current_route);
			}
			set_current_route (r);
			get_button(Output).set_led_state (_output_port, true);
			blinkers.push_back (Output);
		}
	} else {
	}
}

void
FaderPort::ardour_pan_azimuth (int delta)
{
	if (!_current_route) {
		return;
	}

	boost::shared_ptr<Pannable> pannable = _current_route->pannable ();

	if (!pannable) {
		return;
	}

	boost::shared_ptr<AutomationControl> azimuth = pannable->pan_azimuth_control;

	if (!azimuth) {
		return;
	}

	azimuth->set_value (azimuth->interface_to_internal (azimuth->internal_to_interface (azimuth->get_value()) + (delta / encoder_divider)), Controllable::NoGroup);
}


void
FaderPort::ardour_pan_width(int delta)
{
	if (!_current_route) {
		return;
	}

	boost::shared_ptr<Pannable> pannable = _current_route->pannable ();

	if (!pannable) {
		return;
	}

	boost::shared_ptr<AutomationControl> width = pannable->pan_width_control;

	if (!width) {
		return;
	}

	width->set_value (width->interface_to_internal (width->internal_to_interface (width->get_value()) + (delta / encoder_divider)), Controllable::NoGroup);
}

void
FaderPort::mixbus_pan (int delta)
{
#ifdef MIXBUS
	if (!_current_route) {
		return;
	}

	const uint32_t port_channel_post_pan = 2; // gtk2_ardour/mixbus_ports.h
	boost::shared_ptr<ARDOUR::PluginInsert> plug = _current_route->ch_post();

	if (!plug) {
		return;
	}

	boost::shared_ptr<AutomationControl> azimuth = boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (plug->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_channel_post_pan)));

	if (!azimuth) {
		return;
	}

	azimuth->set_value (azimuth->interface_to_internal (azimuth->internal_to_interface (azimuth->get_value()) + (delta / encoder_divider)), Controllable::NoGroup);
#endif
}

void
FaderPort::punch ()
{
	access_action ("Transport/TogglePunch");
}
