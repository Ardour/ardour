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
#include "ardour/phase_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/route.h"

#include "osc.h"
#include "osc_select_observer.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCSelectObserver::OSCSelectObserver (boost::shared_ptr<Stripable> s, lo_address a, uint32_t ss, uint32_t gm, std::bitset<32> fb)
	: _strip (s)
	,ssid (ss)
	,gainmode (gm)
	,feedback (fb)
	,nsends (0)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));

	if (feedback[0]) { // buttons are separate feedback
		_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::name_changed, this, boost::lambda::_1), OSC::instance());
		name_changed (ARDOUR::Properties::name);

		_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/mute"), _strip->mute_control()), OSC::instance());
		change_message ("/select/mute", _strip->mute_control());

		_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo"), _strip->solo_control()), OSC::instance());
		change_message ("/select/solo", _strip->solo_control());

		_strip->solo_isolate_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_iso"), _strip->solo_isolate_control()), OSC::instance());
		change_message ("/select/solo_iso", _strip->solo_isolate_control());

		_strip->solo_safe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_safe"), _strip->solo_safe_control()), OSC::instance());
		change_message ("/select/solo_safe", _strip->solo_safe_control());

		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_strip);
		if (track) {
		track->monitoring_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::monitor_status, this, track->monitoring_control()), OSC::instance());
		monitor_status (track->monitoring_control());
		}

		boost::shared_ptr<AutomationControl> rec_controllable = _strip->rec_enable_control ();
		if (rec_controllable) {
			rec_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/recenable"), _strip->rec_enable_control()), OSC::instance());
			change_message ("/select/recenable", _strip->rec_enable_control());
		}

		boost::shared_ptr<AutomationControl> recsafe_controllable = _strip->rec_safe_control ();
		if (recsafe_controllable) {
			recsafe_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/record_safe"), _strip->rec_safe_control()), OSC::instance());
			change_message ("/select/record_safe", _strip->rec_safe_control());
		}

		boost::shared_ptr<AutomationControl> phase_controllable = _strip->phase_control ();
		if (phase_controllable) {
			phase_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/polarity"), _strip->phase_control()), OSC::instance());
			change_message ("/select/polarity", _strip->phase_control());
		}

	}

	if (feedback[1]) { // level controls
		if (gainmode) {
			_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_message, this, X_("/select/fader"), _strip->gain_control()), OSC::instance());
			gain_message ("/select/fader", _strip->gain_control());
		} else {
			_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_message, this, X_("/select/gain"), _strip->gain_control()), OSC::instance());
			gain_message ("/select/gain", _strip->gain_control());
		}

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->trim_control());
		if (trim_controllable) {
			trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::trim_message, this, X_("/select/trimdB"), _strip->trim_control()), OSC::instance());
			trim_message ("/select/trimdB", _strip->trim_control());
		}

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_stereo_position"), _strip->pan_azimuth_control()), OSC::instance());
			change_message ("/select/pan_stereo_position", _strip->pan_azimuth_control());
		}

		boost::shared_ptr<Controllable> width_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_width_control());
		if (width_controllable) {
			width_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_stereo_width"), _strip->pan_width_control()), OSC::instance());
			change_message ("/select/pan_stereo_width", _strip->pan_width_control());
		}

		// detecting processor changes requires cast to route
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(_strip);
		r->processors_changed.connect  (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_restart, this, -1), OSC::instance());
		send_init();
	}
	tick();
}

OSCSelectObserver::~OSCSelectObserver ()
{

	strip_connections.drop_connections ();
	// all strip buttons should be off and faders 0 and etc.
	if (feedback[0]) { // buttons are separate feedback
		lo_message msg = lo_message_new ();
		// name is a string do it first
		string path = "/select/name";
		lo_message_add_string (msg, " ");
		lo_send_message (addr, path.c_str(), msg);
		lo_message_free (msg);
		clear_strip ("/select/mute", 0);
		clear_strip ("/select/solo", 0);
		clear_strip ("/select/recenable", 0);
		clear_strip ("/select/record_safe", 0);
		clear_strip ("/select/monitor_input", 0);
		clear_strip ("/select/monitor_disk", 0);
		clear_strip ("/select/polarity", 0);
	}
	if (feedback[1]) { // level controls
		if (gainmode) {
			clear_strip ("/select/fader", 0);
		} else {
			clear_strip ("/select/gain", -193);
		}
		clear_strip ("/select/trimdB", 0);
		clear_strip ("/select/pan_stereo_position", 0.5);
		clear_strip ("/select/pan_stereo_width", 1);
	}
	if (feedback[9]) {
		clear_strip ("/select/signal", 0);
	}
	if (feedback[7]) {
		if (gainmode) {
			clear_strip ("/select/meter", 0);
		} else {
			clear_strip ("/select/meter", -193);
		}
	}else if (feedback[8]) {
		clear_strip ("/select/meter", 0);
	}
	send_end();

	lo_address_free (addr);
}

void
OSCSelectObserver::send_init()
{
	// we don't know how many there are, so find out.
	bool sends;
	do {
		sends = false;
		if (_strip->send_level_controllable (nsends)) {
			_strip->send_level_controllable(nsends)->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_gain, this, nsends, _strip->send_level_controllable(nsends)), OSC::instance());
			send_gain (nsends, _strip->send_level_controllable(nsends));
			sends = true;
		}

		if (_strip->send_enable_controllable (nsends)) {
			_strip->send_enable_controllable(nsends)->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_enable, this, X_("/select/send_enable"), nsends, _strip->send_enable_controllable(nsends)), OSC::instance());
			send_enable ("/select/send_enable", nsends, _strip->send_enable_controllable(nsends));
			sends = true;
		} else if (sends) {
			// not used by Ardour, just mixbus so in Ardour always true
			lo_message msg = lo_message_new ();
			path = "/select/send_enable";
			if (feedback[2]) {
				path = set_path (path, nsends + 1);
			} else {
				lo_message_add_int32 (msg, nsends + 1);
			}
			lo_message_add_int32 (msg, 1);
			lo_send_message (addr, path.c_str(), msg);
			lo_message_free (msg);
		}
		// this should get signalled by the route the send goes to, (TODO)
		if (sends) { // if the gain control is there, this is too
			send_name ("/select/send_name", nsends, _strip->send_name(nsends));
		}
		// Send numbers are 0 based, OSC is 1 based so this gets incremented at the end
		if (sends) {
			nsends++;
		}
	} while (sends);
}

void
OSCSelectObserver::send_end ()
{
	send_connections.drop_connections ();
	for (uint32_t i = 1; i <= nsends; i++) {
		lo_message msg = lo_message_new ();
		string path = "/select/send_gain";
		if (feedback[2]) {
			path = set_path (path, i);
		} else {
			lo_message_add_int32 (msg, i);
		}

		if (gainmode) {
			lo_message_add_int32 (msg, 0);
		} else {
			lo_message_add_float (msg, -193);
		}
		lo_send_message (addr, path.c_str(), msg);
		lo_message_free (msg);
		// next enable
		msg = lo_message_new ();
		path = "/select/send_enable";
		if (feedback[2]) {
			path = set_path (path, i);
		} else {
			lo_message_add_int32 (msg, i);
		}
		lo_message_add_int32 (msg, 0);
		lo_send_message (addr, path.c_str(), msg);
		lo_message_free (msg);
		// next name
		msg = lo_message_new ();
		path = "/select/send_name";
		if (feedback[2]) {
			path = set_path (path, i);
		} else {
			lo_message_add_int32 (msg, i);
		}
		lo_message_add_string (msg, " ");
		lo_send_message (addr, path.c_str(), msg);
		lo_message_free (msg);
	}
	nsends = 0;
}

void
OSCSelectObserver::send_restart(int x)
{
	send_end();
	send_init();
}

void
OSCSelectObserver::tick ()
{
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled

		float now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
		if (now_meter < -193) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				string path = "/select/meter";
				lo_message msg = lo_message_new ();
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
				string path = "/select/signal";
				lo_message msg = lo_message_new ();
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
OSCSelectObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}

	if (!_strip) {
		return;
	}

	lo_message msg = lo_message_new ();

	string path = "/select/name";
	lo_message_add_string (msg, _strip->name().c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

	//spit out the comment at the same time
	msg = lo_message_new ();
	path = "/select/comment";
	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (_strip);
	lo_message_add_string (msg, route->comment().c_str());
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

	// lets tell the surface how many inputs this strip has
	msg = lo_message_new ();
	path = "/select/n_inputs";
	lo_message_add_int32 (msg, route->n_inputs().n_total());
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
	// lets tell the surface how many outputs this strip has
	msg = lo_message_new ();
	path = "/select/n_outputs";
	lo_message_add_int32 (msg, route->n_outputs().n_total());
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

void
OSCSelectObserver::change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, (float) controllable->get_value());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::monitor_status (boost::shared_ptr<Controllable> controllable)
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
	string path = "/select/monitor_input";
	lo_message_add_int32 (msg, (float) input);
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	path = "/select/monitor_disk";
	lo_message_add_int32 (msg, (float) disk);
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

void
OSCSelectObserver::trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, (float) accurate_coefficient_to_dB (controllable->get_value()));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::gain_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

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

void
OSCSelectObserver::send_gain (uint32_t id, boost::shared_ptr<PBD::Controllable> controllable)
{
	lo_message msg = lo_message_new ();
	string path;
	float value;

	if (gainmode) {
		path = "/select/send_fader";
		if (controllable->get_value() == 1) {
			value = 800;
		} else {
			value = gain_to_slider_position (controllable->get_value());
		}
	} else {
		path = "/select/send_gain";
		if (controllable->get_value() < 1e-15) {
			value = -193;
		} else {
			value = accurate_coefficient_to_dB (controllable->get_value());
		}
	}

	if (feedback[2]) {
		path = set_path (path, id + 1);
	} else {
		lo_message_add_int32 (msg, id + 1);
	}

	lo_message_add_float (msg, value);
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::send_enable (string path, uint32_t id, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();
	if (feedback[2]) {
		path = set_path (path, id + 1);
	} else {
		lo_message_add_int32 (msg, id + 1);
	}

	lo_message_add_float (msg, (float) controllable->get_value());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::send_name (string path, uint32_t id, string name)
{
	lo_message msg = lo_message_new ();
	if (feedback[2]) {
		path = set_path (path, id + 1);
	} else {
		lo_message_add_int32 (msg, id + 1);
	}

	lo_message_add_string (msg, name.c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

string
OSCSelectObserver::set_path (string path, uint32_t id)
{
	if (feedback[2]) {
  ostringstream os;
  os << path << "/" << id;
  path = os.str();
	}
	return path;
}

void
OSCSelectObserver::clear_strip (string path, float val)
{
	lo_message msg = lo_message_new ();
	lo_message_add_float (msg, val);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

