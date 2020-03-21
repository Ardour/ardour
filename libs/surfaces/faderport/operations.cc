/*
 * Copyright (C) 2015-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/async_midi_port.h"
#include "ardour/monitor_processor.h"
#include "ardour/plugin_insert.h"
#include "ardour/rc_configuration.h"
#include "ardour/record_enable_control.h"
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
	access_action ("Editor/select-prev-stripable");

	//ToDo:  bank by 8?
	//if ( (button_state & ShiftDown) == ShiftDown )

}

void
FaderPort::right ()
{
	access_action ("Editor/select-next-stripable");

	//ToDo:  bank by 8?
	//if ( (button_state & ShiftDown) == ShiftDown )
}


void
FaderPort::read ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Play );
		}
	}
}

void
FaderPort::write ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Write );
		}
	}
}

void
FaderPort::touch ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Touch );
		}
	}
}

void
FaderPort::off ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
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
	if (!_current_stripable) {
		return;
	}

	if (_current_stripable == session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mp = _current_stripable->monitor_control();
		mp->set_cut_all (!mp->cut_all());
		return;
	}

	_current_stripable->mute_control()->set_value (!_current_stripable->mute_control()->muted(), PBD::Controllable::UseGroup);
}

void
FaderPort::solo ()
{
	if (!_current_stripable) {
		return;
	}

	session->set_control (_current_stripable->solo_control(), !_current_stripable->solo_control()->self_soloed(), PBD::Controllable::UseGroup);
}

void
FaderPort::rec_enable ()
{
	if (!_current_stripable) {
		return;
	}

	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_current_stripable);

	if (!t) {
		return;
	}

	t->rec_enable_control()->set_value (!t->rec_enable_control()->get_value(), Controllable::UseGroup);
}

void
FaderPort::use_master ()
{
	boost::shared_ptr<Stripable> r = session->master_out();
	if (r) {
		if (_current_stripable == r) {
			r = pre_master_stripable.lock();
			set_current_stripable (r);
			get_button(Output).set_led_state (_output_port, false);
			blinkers.remove (Output);
		} else {
			if (_current_stripable != session->master_out() && _current_stripable != session->monitor_out()) {
				pre_master_stripable = boost::weak_ptr<Stripable> (_current_stripable);
			}
			set_current_stripable (r);
			get_button(Output).set_led_state (_output_port, true);
			blinkers.remove (Output);
		}
	}
}

void
FaderPort::use_monitor ()
{
	boost::shared_ptr<Stripable> r = session->monitor_out();

	if (r) {
		if (_current_stripable == r) {
			r = pre_monitor_stripable.lock();
			set_current_stripable (r);
			get_button(Output).set_led_state (_output_port, false);
			blinkers.remove (Output);
		} else {
			if (_current_stripable != session->master_out() && _current_stripable != session->monitor_out()) {
				pre_monitor_stripable = boost::weak_ptr<Stripable> (_current_stripable);
			}
			set_current_stripable (r);
			get_button(Output).set_led_state (_output_port, true);
			blinkers.push_back (Output);
		}
	} else {
	}
}

void
FaderPort::pan_azimuth (int delta)
{
	if (!_current_stripable) {
		return;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (_current_stripable);

	if (!r) {
		return;
	}

	boost::shared_ptr<AutomationControl> azimuth = r->pan_azimuth_control ();

	if (!azimuth) {
		return;
	}

	azimuth->set_interface ((azimuth->internal_to_interface (azimuth->get_value(),true) + (delta / encoder_divider)), true);
}


void
FaderPort::pan_width(int delta)
{
	if (!_current_stripable) {
		return;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (_current_stripable);

	if (!r) {
		return;
	}

	boost::shared_ptr<AutomationControl> width = r->pan_width_control ();

	if (!width) {
		return;
	}

	width->set_value (width->interface_to_internal (width->internal_to_interface (width->get_value()) + (delta / encoder_divider)), Controllable::NoGroup);
}

void
FaderPort::punch ()
{
	access_action ("Transport/TogglePunch");
}
