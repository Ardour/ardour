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

#include "ardour/track.h"
#include "ardour/dB.h"
#include "ardour/meter.h"

#include "osc.h"
#include "osc_cue_observer.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCCueObserver::OSCCueObserver (boost::shared_ptr<Stripable> s, std::vector<boost::shared_ptr<ARDOUR::Stripable> >& snds, lo_address a)
	: sends (snds)
	, _strip (s)
	, tick_enable (false)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));

	_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::name_changed, this, boost::lambda::_1, 0), OSC::instance());
	name_changed (ARDOUR::Properties::name, 0);

	_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::send_change_message, this, X_("/cue/mute"), 0, _strip->mute_control()), OSC::instance());
	send_change_message ("/cue/mute", 0, _strip->mute_control());

	gain_timeout.push_back (0);
	_last_gain.push_back (0.0);
	_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::send_gain_message, this, 0, _strip->gain_control()), OSC::instance());
	send_gain_message (0, _strip->gain_control());

	send_init ();

	tick_enable = true;
	tick ();
}

OSCCueObserver::~OSCCueObserver ()
{
	tick_enable = false;

	strip_connections.drop_connections ();
	send_end ();
	// all strip buttons should be off and faders 0 and etc.
	text_with_id ("/cue/name", 0, " ");
	clear_strip ("/cue/mute", 0);
	clear_strip ("/cue/fader", 0);
	clear_strip ("/cue/signal", 0);

	lo_address_free (addr);
}

void
OSCCueObserver::tick ()
{
	if (!tick_enable) {
		return;
	}
	float now_meter;
	if (_strip->peak_meter()) {
		now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
	} else {
		now_meter = -193;
	}
	if (now_meter < -120) now_meter = -193;
	if (_last_meter != now_meter) {
		string path = "/cue/signal";
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
	_last_meter = now_meter;

	for (uint32_t i = 0; i < gain_timeout.size(); i++) {
		if (gain_timeout[i]) {
			if (gain_timeout[i] == 1) {
				name_changed (ARDOUR::Properties::name, i);
			}
			gain_timeout[i]--;
		}
	}

}

void
OSCCueObserver::send_init()
{
	for (uint32_t i = 0; i < sends.size(); i++) {
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (sends[i]);
		boost::shared_ptr<Send> send = r->internal_send_for (boost::dynamic_pointer_cast<Route> (_strip));
		if (r) {
			r->processors_changed.connect  (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::send_restart, this), OSC::instance());
		}

		if (send) {
			// send name
			if (r) {
				sends[i]->PropertyChanged.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::name_changed, this, boost::lambda::_1, i + 1), OSC::instance());
				name_changed (ARDOUR::Properties::name, i + 1);
			}
				

			if (send->gain_control()) {
				gain_timeout.push_back (0);
				_last_gain.push_back (0.0);
				send->gain_control()->Changed.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::send_gain_message, this, i + 1, send->gain_control()), OSC::instance());
				send_gain_message (i + 1, send->gain_control());
			}
			
			boost::shared_ptr<Processor> proc = boost::dynamic_pointer_cast<Processor> (send);
				proc->ActiveChanged.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCCueObserver::send_enabled_message, this, X_("/cue/send/enable"), i + 1, proc), OSC::instance());
				send_enabled_message (X_("/cue/send/enable"), i + 1, proc);
		}
	}

}

void
OSCCueObserver::send_end ()
{
	send_connections.drop_connections ();
	for (uint32_t i = 1; i <= sends.size(); i++) {
		clear_strip (string_compose ("/cue/send/fader/%1", i), 0);
		clear_strip (string_compose ("/cue/send/enable/%1", i), 0);
		text_with_id ("/cue/send/name", i, " ");
	}
}

void
OSCCueObserver::send_restart ()
{
	tick_enable = false;
	send_end();
	send_init();
	tick_enable = true;
}

void
OSCCueObserver::name_changed (const PBD::PropertyChange& what_changed, uint32_t id)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}

	if (!_strip) {
		return;
	}
	if (id) {
		text_with_id ("/cue/send/name", id, sends[id - 1]->name());
	} else {
		text_with_id ("/cue/name", 0, _strip->name());
	}
}

void
OSCCueObserver::send_change_message (string path, uint32_t id, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	if (id) {
		path = string_compose("%1/%2", path, id);
	}
	float val = controllable->get_value();
	lo_message_add_float (msg, (float) controllable->internal_to_interface (val));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCCueObserver::text_with_id (string path, uint32_t id, string val)
{
	lo_message msg = lo_message_new ();
	if (id) {
		path = string_compose("%1/%2", path, id);
	}

	lo_message_add_string (msg, val.c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCCueObserver::send_gain_message (uint32_t id,  boost::shared_ptr<Controllable> controllable)
{
	if (_last_gain[id] != controllable->get_value()) {
		_last_gain[id] = controllable->get_value();
	} else {
		return;
	}
	string path = "/cue";
	if (id) {
		path = "/cue/send";
	}

	text_with_id (string_compose ("%1/name", path), id, string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (controllable->get_value())));
	path = string_compose ("%1/fader", path);
	if (id) {
		path = string_compose ("%1/%2", path, id);
	}
	lo_message msg = lo_message_new ();
	lo_message_add_float (msg, controllable->internal_to_interface (controllable->get_value()));
	gain_timeout[id] = 8;

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCCueObserver::send_enabled_message (std::string path, uint32_t id, boost::shared_ptr<ARDOUR::Processor> proc)
{
	lo_message msg = lo_message_new ();

	if (id) {
		path = string_compose("%1/%2", path, id);
	}
	lo_message_add_float (msg, (float) proc->enabled());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
	
}

void
OSCCueObserver::clear_strip (string path, float val)
{
	lo_message msg = lo_message_new ();
	lo_message_add_float (msg, val);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

