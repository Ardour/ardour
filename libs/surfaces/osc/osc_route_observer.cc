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

#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/meter.h"

#include "osc.h"
#include "osc_route_observer.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCRouteObserver::OSCRouteObserver (boost::shared_ptr<Stripable> s, lo_address a, uint32_t ss, uint32_t gm, std::bitset<32> fb)
	: _strip (s)
	,ssid (ss)
	,gainmode (gm)
	,feedback (fb)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));

	if (feedback[0]) { // buttons are separate feedback
		_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::name_changed, this, boost::lambda::_1), OSC::instance());
		name_changed (ARDOUR::Properties::name);

		_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/mute"), _strip->mute_control()), OSC::instance());
		send_change_message ("/strip/mute", _strip->mute_control());

		_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo"), _strip->solo_control()), OSC::instance());
		send_change_message ("/strip/solo", _strip->solo_control());

		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_strip);
		if (track) {
		track->monitoring_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_monitor_status, this, track->monitoring_control()), OSC::instance());
		send_monitor_status (track->monitoring_control());
		}

		boost::shared_ptr<AutomationControl> rec_controllable = _strip->rec_enable_control ();
		if (rec_controllable) {
			rec_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/recenable"), _strip->rec_enable_control()), OSC::instance());
			send_change_message ("/strip/recenable", _strip->rec_enable_control());
		}
		boost::shared_ptr<AutomationControl> recsafe_controllable = _strip->rec_safe_control ();
		if (rec_controllable) {
			recsafe_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/record_safe"), _strip->rec_safe_control()), OSC::instance());
			send_change_message ("/strip/record_safe", _strip->rec_safe_control());
		}
	}

	if (feedback[1]) { // level controls
		if (gainmode) {
			_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_gain_message, this, X_("/strip/fader"), _strip->gain_control()), OSC::instance());
			send_gain_message ("/strip/fader", _strip->gain_control());
		} else {
			_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_gain_message, this, X_("/strip/gain"), _strip->gain_control()), OSC::instance());
			send_gain_message ("/strip/gain", _strip->gain_control());
		}

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->trim_control());
		if (trim_controllable) {
			trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_trim_message, this, X_("/strip/trimdB"), _strip->trim_control()), OSC::instance());
			send_trim_message ("/strip/trimdB", _strip->trim_control());
		}

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/pan_stereo_position"), _strip->pan_azimuth_control()), OSC::instance());
			send_change_message ("/strip/pan_stereo_position", _strip->pan_azimuth_control());
		}
	}
	tick();
}

OSCRouteObserver::~OSCRouteObserver ()
{

	strip_connections.drop_connections ();
	// all strip buttons should be off and faders 0 and etc.
	if (feedback[0]) { // buttons are separate feedback
		lo_message msg = lo_message_new ();
		// name is a string do it first
		string path = "/strip/name";
		if (feedback[2]) {
			path = set_path (path);
		} else {
			lo_message_add_int32 (msg, ssid);
		}
		lo_message_add_string (msg, " ");

		lo_send_message (addr, path.c_str(), msg);
		lo_message_free (msg);
		clear_strip ("/strip/mute", 0);
		clear_strip ("/strip/solo", 0);
		clear_strip ("/strip/recenable", 0);
		clear_strip ("/strip/record_safe", 0);
		clear_strip ("/strip/monitor_input", 0);
		clear_strip ("/strip/monitor_disk", 0);
	}
	if (feedback[1]) { // level controls
		if (gainmode) {
			clear_strip ("/strip/fader", 0);
		} else {
			clear_strip ("/strip/gain", -193);
		}
		clear_strip ("/strip/trimdB", 0);
		clear_strip ("/strip/pan_stereo_position", 0.5);
	}
	if (feedback[9]) {
		clear_strip ("/strip/signal", 0);
	}
	if (feedback[7]) {
		if (gainmode) {
			clear_strip ("/strip/meter", 0);
		} else {
			clear_strip ("/strip/meter", -193);
		}
	}else if (feedback[8]) {
		clear_strip ("/strip/meter", 0);
	}

	lo_address_free (addr);
}

void
OSCRouteObserver::tick ()
{
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled
		// the only meter here is master
		float now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
		if (now_meter < -193) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				string path = "/strip/meter";
				lo_message msg = lo_message_new ();
				if (feedback[2]) {
					path = set_path (path);
				} else {
					lo_message_add_int32 (msg, ssid);
				}
				if (gainmode && feedback[7]) {
					uint32_t lev1023 = (uint32_t)((now_meter + 54) * 17.05);
					lo_message_add_int32 (msg, lev1023);
					lo_send_message (addr, path.c_str(), msg);
				} else if ((!gainmode) && feedback[7]) {
					lo_message_add_float (msg, now_meter);
					lo_send_message (addr, path.c_str(), msg);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint16_t ledbits = ~(0xfff<<ledlvl);
					lo_message_add_int32 (msg, ledbits);
					lo_send_message (addr, path.c_str(), msg);
				}
				lo_message_free (msg);
			}
			if (feedback[9]) {
				string path = "/strip/signal";
				lo_message msg = lo_message_new ();
				if (feedback[2]) {
					path = set_path (path);
				} else {
					lo_message_add_int32 (msg, ssid);
				}
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				lo_message_add_float (msg, signal);
				lo_send_message (addr, path.c_str(), msg);
				lo_message_free (msg);
			}
		}
		_last_meter = now_meter;

	}

}

void
OSCRouteObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}

	if (!_strip) {
		return;
	}

	lo_message msg = lo_message_new ();

	// ssid is the strip on the surface this observer refers to
	// not part of the internal ordering.
	string path = "/strip/name";
	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_string (msg, _strip->name().c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCRouteObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_float (msg, (float) controllable->get_value());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
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
		default:
			disk = 0;
			input = 0;
	}

	lo_message msg = lo_message_new ();
	string path = "/strip/monitor_input";
	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_int32 (msg, (float) input);
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	path = "/strip/monitor_disk";
	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_int32 (msg, (float) disk);
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

void
OSCRouteObserver::send_trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}

	lo_message_add_float (msg, (float) accurate_coefficient_to_dB (controllable->get_value()));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCRouteObserver::send_gain_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}

	if (gainmode) {
		if (controllable->get_value() == 1) {
			lo_message_add_int32 (msg, 800);
		} else {
			lo_message_add_int32 (msg, gain_to_slider_position (controllable->get_value()) * 1023);
		}
	} else {
		if (controllable->get_value() < 1e-15) {
			lo_message_add_float (msg, -200);
		} else {
			lo_message_add_float (msg, accurate_coefficient_to_dB (controllable->get_value()));
		}
	}

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

string
OSCRouteObserver::set_path (string path)
{
	if (feedback[2]) {
  ostringstream os;
  os << path << "/" << ssid;
  path = os.str();
	}
	return path;
}

void
OSCRouteObserver::clear_strip (string path, float val)
{
	lo_message msg = lo_message_new ();
	if (feedback[2]) {
		path = set_path (path);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_float (msg, val);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}
