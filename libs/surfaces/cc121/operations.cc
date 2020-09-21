/*
 * Copyright (C) 2016 W.P. van Paassen
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 *
 * Thanks to Rolf Meyerhoff for reverse engineering the CC121 protocol.
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
#include "ardour/monitor_control.h"
#include "ardour/plugin_insert.h"
#include "ardour/rc_configuration.h"
#include "ardour/record_enable_control.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/types.h"

#include "cc121.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;

void
CC121::input_monitor ()
{
	if (_current_stripable && _current_stripable->monitoring_control()) {
	  MonitorChoice choice = _current_stripable->monitoring_control()->monitoring_choice ();
	  switch(choice) {
	  case MonitorAuto:
	    _current_stripable->monitoring_control()->set_value (MonitorInput, PBD::Controllable::NoGroup);
	    break;
	  case MonitorInput:
	    _current_stripable->monitoring_control()->set_value (MonitorDisk, PBD::Controllable::NoGroup);
	    break;
	  case MonitorDisk:
	    _current_stripable->monitoring_control()->set_value (MonitorCue, PBD::Controllable::NoGroup);
	    break;
	  case MonitorCue:
	    _current_stripable->monitoring_control()->set_value (MonitorAuto, PBD::Controllable::NoGroup);
	    break;
	  default:
	    break;
	  }
	}
}

void
CC121::left ()
{
	access_action ("Editor/select-prev-route");

	//ToDo:  bank by 8?
	//if ( (button_state & ShiftDown) == ShiftDown )

}

void
CC121::right ()
{
	access_action ("Editor/select-next-route");

	//ToDo:  bank by 8?
	//if ( (button_state & ShiftDown) == ShiftDown )
}


void
CC121::read ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Play );
		}
	}
}

void
CC121::write ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Write );
		}
	}
}

void
CC121::touch ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Touch );
		}
	}
}

void
CC121::off ()
{
	if (_current_stripable) {
		boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
		if (gain) {
			gain->set_automation_state( (ARDOUR::AutoState) ARDOUR::Off );
		}
	}
}




void
CC121::undo ()
{
	ControlProtocol::Undo (); /* EMIT SIGNAL */
}

void
CC121::redo ()
{
	ControlProtocol::Redo (); /* EMIT SIGNAL */
}

void
CC121::jog()
{
        if (_jogmode == scroll) {
             _jogmode = zoom;
        }
        else {
             _jogmode = scroll;
        }
        get_button (Jog).set_led_state (_output_port, _jogmode == scroll);
}

void
CC121::mute ()
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
CC121::solo ()
{
	if (!_current_stripable) {
		return;
	}
	_current_stripable->solo_control()->set_value (!_current_stripable->solo_control()->soloed(), PBD::Controllable::UseGroup);
}

void
CC121::rec_enable ()
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
CC121::use_master ()
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
CC121::use_monitor ()
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
	}
}

void
CC121::set_controllable (boost::shared_ptr<AutomationControl> ac, float delta)
{
	if (!ac || delta == 0) {
		return;
	}
	ac->start_touch (timepos_t (ac->session().transport_sample()));
	ac->set_interface ((ac->internal_to_interface (ac->get_value(), true) + delta), true);
}


void
CC121::punch ()
{
	access_action ("Transport/TogglePunch");
}
