/*
 * Copyright (C) 2017-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

OSCCueObserver::OSCCueObserver (OSC& o, ArdourSurface::OSC::OSCSurface* su)
	:  _osc (o)
	,sur (su)
	, tick_enable (false)
{
	addr = lo_address_new_from_url 	(sur->remote_url.c_str());
	uint32_t sid = sur->aux - 1;
	if (sid >= sur->strips.size ()) {
		sid = 0;
	}

	_strip = sur->strips[sid];
	sends = sur->sends;
	_last_signal = -1;
	_last_meter = -200;
	refresh_strip (_strip, sends, true);
}

OSCCueObserver::~OSCCueObserver ()
{
	tick_enable = false;
	clear_observer ();
	lo_address_free (addr);
}

void
OSCCueObserver::clear_observer ()
{
	tick_enable = false;

	strip_connections.drop_connections ();
	_strip = std::shared_ptr<ARDOUR::Stripable> ();
	send_end (0);
	// all strip buttons should be off and faders 0 and etc.
	_osc.text_message_with_id (X_("/cue/name"), 0, " ", true, addr);
	_osc.float_message (X_("/cue/mute"), 0, addr);
	_osc.float_message (X_("/cue/fader"), 0, addr);
	_osc.float_message (X_("/cue/signal"), 0, addr);

}

void
OSCCueObserver::refresh_strip (std::shared_ptr<ARDOUR::Stripable> new_strip, Sorted new_sends, bool force)
{
	tick_enable = false;

	strip_connections.drop_connections ();

	send_end (new_sends.size ());
	_strip = new_strip;
	_strip->DropReferences.connect (strip_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::clear_observer, this), OSC::instance());
	sends = new_sends;

	_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::name_changed, this,_1, 0), OSC::instance());
	name_changed (ARDOUR::Properties::name, 0);

	_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::send_change_message, this, X_("/cue/mute"), 0, std::weak_ptr<Controllable>(_strip->mute_control())), OSC::instance());
	send_change_message (X_("/cue/mute"), 0, _strip->mute_control());

	gain_timeout[0] = 0;
	_last_gain[0] = -1; // unused
	_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::send_gain_message, this, 0, std::weak_ptr<Controllable>(_strip->gain_control()), false), OSC::instance());
	send_gain_message (0, _strip->gain_control(), true);

	send_init ();

	tick_enable = true;
	tick ();
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
		float signal;
		if (now_meter < -45) {
			signal = 0;
		} else {
			signal = 1;
		}
		if (_last_signal != signal) {
			_osc.float_message (X_("/cue/signal"), signal, addr);
			_last_signal = signal;
		}
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
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (sends[i]);
		std::shared_ptr<Send> send = r->internal_send_for (std::dynamic_pointer_cast<Route> (_strip));
		if (r) {
			r->processors_changed.connect  (send_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::send_restart, this), OSC::instance());
		}

		if (send) {
			// send name
			if (r) {
				sends[i]->PropertyChanged.connect (send_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::name_changed, this,_1, i + 1), OSC::instance());
				name_changed (ARDOUR::Properties::name, i + 1);
			}


			if (send->gain_control()) {
				gain_timeout[i + 1] = 0;
				_last_gain[i + 1] = -1.0;
				send->gain_control()->Changed.connect (send_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::send_gain_message, this, i + 1, std::weak_ptr<Controllable>(send->gain_control()), false), OSC::instance());
				send_gain_message (i + 1, send->gain_control(), true);
			}

			std::shared_ptr<Processor> proc = std::dynamic_pointer_cast<Processor> (send);
			std::weak_ptr<Processor> wproc (proc);
			proc->ActiveChanged.connect (send_connections, MISSING_INVALIDATOR, std::bind (&OSCCueObserver::send_enabled_message, this, X_("/cue/send/enable"), i + 1, wproc), OSC::instance());
			send_enabled_message (X_("/cue/send/enable"), i + 1, wproc);
		}
	}

}

void
OSCCueObserver::send_end (uint32_t new_size)
{
	send_connections.drop_connections ();
	if (new_size < sends.size()) {
		for (uint32_t i = new_size + 1; i <= sends.size(); i++) {
			_osc.float_message (string_compose (X_("/cue/send/fader/%1"), i), 0, addr);
			_osc.float_message (string_compose (X_("/cue/send/enable/%1"), i), 0, addr);
			_osc.text_message_with_id (X_("/cue/send/name"), i, " ", true, addr);
		}
	}
	gain_timeout.clear ();
	_last_gain.clear ();
	sends.clear ();
}

void
OSCCueObserver::send_restart ()
{
	tick_enable = false;
	send_end(sends.size());
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
		_osc.text_message_with_id (X_("/cue/send/name"), id, sends[id - 1]->name(), true, addr);
	} else {
		_osc.text_message (X_("/cue/name"), _strip->name(), addr);
	}
}

void
OSCCueObserver::send_change_message (string path, uint32_t id, std::weak_ptr<Controllable> weak_controllable)
{
	std::shared_ptr<Controllable> controllable = weak_controllable.lock ();
	if (!controllable) {
		return;
	}
	if (id) {
		path = string_compose("%1/%2", path, id);
	}
	float val = controllable->get_value();
	_osc.float_message (path, (float) controllable->internal_to_interface (val), addr);
}

void
OSCCueObserver::send_gain_message (uint32_t id,  std::weak_ptr<Controllable> weak_controllable, bool force)
{
	std::shared_ptr<Controllable> controllable = weak_controllable.lock ();
	if (!controllable) {
		return;
	}
	if (_last_gain[id] != controllable->get_value()) {
		_last_gain[id] = controllable->get_value();
	} else {
		return;
	}
	if (id) {
		_osc.float_message_with_id (X_("/cue/send/fader"), id, controllable->internal_to_interface (controllable->get_value()), true, addr);
	} else {
		_osc.float_message (X_("/cue/fader"), controllable->internal_to_interface (controllable->get_value()), addr);
	}

	gain_timeout[id] = 8;
}

void
OSCCueObserver::send_enabled_message (std::string path, uint32_t id, std::weak_ptr<ARDOUR::Processor> weak_proc)
{
	std::shared_ptr<ARDOUR::Processor> proc = weak_proc.lock ();
	if (!proc) {
		return;
	}
	if (id) {
		_osc.float_message_with_id (path, id, (float) proc->enabled(), true, addr);
	} else {
		_osc.float_message (path, (float) proc->enabled(), addr);
	}
}
