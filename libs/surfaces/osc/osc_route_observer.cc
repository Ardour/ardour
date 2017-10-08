/*
    Copyright (C) 2009 Paul Davis

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

#include "boost/lambda/lambda.hpp"

#include "pbd/control_math.h"

#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/solo_isolate_control.h"

#include "osc.h"
#include "osc_route_observer.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCRouteObserver::OSCRouteObserver (OSC& o, uint32_t ss, ArdourSurface::OSC::OSCSurface* su)
	: _osc (o)
	,ssid (ss)
	,sur (su)
	,_last_gain (-1.0)
	,_last_trim (-1.0)
	,_init (true)
{
	addr = lo_address_new_from_url 	(sur->remote_url.c_str());
	refresh_strip (true);
}

OSCRouteObserver::~OSCRouteObserver ()
{
	_init = true;

	strip_connections.drop_connections ();

	lo_address_free (addr);
}

void
OSCRouteObserver::refresh_strip (bool force)
{
	_init = true;

	_strip = sur->strips[sur->bank + ssid - 2];
	as = ARDOUR::Off;

	if (sur->feedback[0]) { // buttons are separate feedback
		_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::name_changed, this, boost::lambda::_1), OSC::instance());
		name_changed (ARDOUR::Properties::name);

		_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/mute"), _strip->mute_control()), OSC::instance());
		send_change_message ("/strip/mute", _strip->mute_control());

		_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo"), _strip->solo_control()), OSC::instance());
		send_change_message ("/strip/solo", _strip->solo_control());

		if (_strip->solo_isolate_control()) {
			_strip->solo_isolate_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo_iso"), _strip->solo_isolate_control()), OSC::instance());
			send_change_message ("/strip/solo_iso", _strip->solo_isolate_control());
		}

		if (_strip->solo_safe_control()) {
			_strip->solo_safe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo_safe"), _strip->solo_safe_control()), OSC::instance());
			send_change_message ("/strip/solo_safe", _strip->solo_safe_control());
		}

		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_strip);
		if (track) {
			track->monitoring_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_monitor_status, this, track->monitoring_control()), OSC::instance());
			send_monitor_status (track->monitoring_control());
		}

		boost::shared_ptr<AutomationControl> rec_controllable = _strip->rec_enable_control ();
		if (rec_controllable) {
			rec_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/recenable"), _strip->rec_enable_control()), OSC::instance());
			send_change_message ("/strip/recenable", _strip->rec_enable_control());
		}
		boost::shared_ptr<AutomationControl> recsafe_controllable = _strip->rec_safe_control ();
		if (rec_controllable) {
			recsafe_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/record_safe"), _strip->rec_safe_control()), OSC::instance());
			send_change_message ("/strip/record_safe", _strip->rec_safe_control());
		}
		_strip->presentation_info().PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_select_status, this, _1), OSC::instance());
		send_select_status (ARDOUR::Properties::selected);
	}

	if (sur->feedback[1]) { // level controls
		boost::shared_ptr<GainControl> gain_cont = _strip->gain_control();
		gain_cont->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::gain_automation, this), OSC::instance());
		gain_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_gain_message, this), OSC::instance());
		gain_automation ();

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->trim_control());
		if (trim_controllable) {
			trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_trim_message, this), OSC::instance());
			send_trim_message ();
		}

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/pan_stereo_position"), _strip->pan_azimuth_control()), OSC::instance());
			send_change_message ("/strip/pan_stereo_position", _strip->pan_azimuth_control());
		}
	}
	_init = false;
	tick();

}

void
OSCRouteObserver::tick ()
{
	if (_init) {
		return;
	}
	if (sur->feedback[7] || sur->feedback[8] || sur->feedback[9]) { // meters enabled
		// the only meter here is master
		float now_meter;
		if (_strip->peak_meter()) {
			now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
		} else {
			now_meter = -193;
		}
		if (now_meter < -120) now_meter = -193;
		if (_last_meter != now_meter) {
			if (sur->feedback[7] || sur->feedback[8]) {
				if (sur->gainmode && sur->feedback[7]) {
					_osc.float_message_with_id ("/strip/meter", ssid, ((now_meter + 94) / 100), addr);
				} else if ((!sur->gainmode) && sur->feedback[7]) {
					_osc.float_message_with_id ("/strip/meter", ssid, now_meter, addr);
				} else if (sur->feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint16_t ledbits = ~(0xfff<<ledlvl);
					_osc.int_message_with_id ("/strip/meter", ssid, ledbits, addr);
				}
			}
			if (sur->feedback[9]) {
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				_osc.float_message_with_id ("/strip/signal", ssid, signal, addr);
			}
		}
		_last_meter = now_meter;

	}
	if (sur->feedback[1]) {
		if (gain_timeout) {
			if (gain_timeout == 1) {
				_osc.text_message_with_id ("/strip/name", ssid, _strip->name(), addr);
			}
			gain_timeout--;
		}
		if (trim_timeout) {
			if (trim_timeout == 1) {
				_osc.text_message_with_id ("/strip/name", ssid, _strip->name(), addr);
			}
			trim_timeout--;
		}
		if (as == ARDOUR::Play ||  as == ARDOUR::Touch) {
			if(_last_gain != _strip->gain_control()->get_value()) {
				_last_gain = _strip->gain_control()->get_value();
				send_gain_message ();
			}
		}
	}

}

void
OSCRouteObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}

	if (_strip) {
		_osc.text_message_with_id ("/strip/name", ssid, _strip->name(), addr);
	}
}

void
OSCRouteObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	_osc.float_message_with_id (path, ssid, (float) controllable->internal_to_interface (val), addr);
}

void
OSCRouteObserver::send_monitor_status (boost::shared_ptr<Controllable> controllable)
{
	int disk, input;
	float val = controllable->get_value();
	switch ((int) val) {
		case 1:
			disk = 0;
			input = 1;
			break;
		case 2:
			disk = 1;
			input = 0;
			break;
		case 3:
			disk = 1;
			input = 1;
			break;
		default:
			disk = 0;
			input = 0;
	}
	_osc.int_message_with_id ("/strip/monitor_input", ssid, input, addr);
	_osc.int_message_with_id ("/strip/monitor_disk", ssid, disk, addr);

}

void
OSCRouteObserver::send_trim_message ()
{
	if (_last_trim != _strip->trim_control()->get_value()) {
		_last_trim = _strip->trim_control()->get_value();
	} else {
		return;
	}
	if (sur->gainmode) {
		_osc.text_message_with_id ("/strip/name", ssid, string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (_last_trim)), addr);
		trim_timeout = 8;
	}

	_osc.float_message_with_id ("/strip/trimdB", ssid, (float) accurate_coefficient_to_dB (_last_trim), addr);
}

void
OSCRouteObserver::send_gain_message ()
{
	boost::shared_ptr<Controllable> controllable = _strip->gain_control();
	if (_last_gain != controllable->get_value()) {
		_last_gain = controllable->get_value();
	} else {
		return;
	}

	if (sur->gainmode) {
		_osc.float_message_with_id ("/strip/fader", ssid, controllable->internal_to_interface (_last_gain), addr);
		_osc.text_message_with_id ("/strip/name", ssid, string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (controllable->get_value())), addr);
		gain_timeout = 8;
	} else {
		if (controllable->get_value() < 1e-15) {
			_osc.float_message_with_id ("/strip/gain", ssid, -200, addr);
		} else {
			_osc.float_message_with_id ("/strip/gain", ssid, accurate_coefficient_to_dB (_last_gain), addr);
		}
	}
}

void
OSCRouteObserver::gain_automation ()
{
	string path = "/strip/gain";
	if (sur->gainmode) {
		path = "/strip/fader";
	}
	send_gain_message ();
	as = _strip->gain_control()->alist()->automation_state();
	string auto_name;
	float output = 0;
	switch (as) {
		case ARDOUR::Off:
			output = 0;
			auto_name = "Manual";
			break;
		case ARDOUR::Play:
			output = 1;
			auto_name = "Play";
			break;
		case ARDOUR::Write:
			output = 2;
			auto_name = "Write";
			break;
		case ARDOUR::Touch:
			output = 3;
			auto_name = "Touch";
			break;
		case ARDOUR::Latch:
			output = 4;
			auto_name = "Latch";
			break;
		default:
			break;
	}
	_osc.float_message_with_id (string_compose ("%1/automation", path), ssid, output, addr);
	_osc.text_message_with_id (string_compose ("%1/automation_name", path), ssid, auto_name, addr);
}

void
OSCRouteObserver::send_select_status (const PropertyChange& what)
{
	if (what == PropertyChange(ARDOUR::Properties::selected)) {
		if (_strip) {
			_osc.float_message_with_id ("/strip/select", ssid, _strip->is_selected(), addr);
		}
	}
}
