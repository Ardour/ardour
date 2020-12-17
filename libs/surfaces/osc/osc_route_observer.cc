/*
 * Copyright (C) 2010-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2020 Len Ovens <len@ovenwerks.net>
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

#include "boost/lambda/lambda.hpp"

#include "pbd/control_math.h"
#include <glibmm.h>


#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/pannable.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
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
	,_expand (2048)
{
	addr = lo_address_new_from_url (sur->remote_url.c_str());
	gainmode = sur->gainmode;
	feedback = sur->feedback;
	in_line = feedback[2];
	uint32_t sid = sur->bank + ssid - 2;
	uint32_t not_ready = 0;
	if (sur->linkset) {
		not_ready = _osc.link_sets[sur->linkset].not_ready;
	}
	if (not_ready) {
		set_link_ready (not_ready);
	} else if (sid >= sur->strips.size ()) {
		// this _should_ only occure if the number of strips is less than banksize
		_strip = boost::shared_ptr<ARDOUR::Stripable>();
		clear_strip ();
	} else {
		_strip = sur->strips[sid];
		refresh_strip (_strip, true);
	}
	if (sur->expand_enable) {
		set_expand (sur->expand);
	} else {
		set_expand (0);
	}
	_send = boost::shared_ptr<ARDOUR::Send> ();
}

OSCRouteObserver::~OSCRouteObserver ()
{
	_init = true;
	pan_connections.drop_connections ();
	strip_connections.drop_connections ();

	lo_address_free (addr);
}

void
OSCRouteObserver::no_strip ()
{
	// This gets called on drop references
	_init = true;

	pan_connections.drop_connections ();
	strip_connections.drop_connections ();
	_gain_control = boost::shared_ptr<ARDOUR::GainControl> ();
	_send = boost::shared_ptr<ARDOUR::Send> ();
	_strip = boost::shared_ptr<Stripable> ();
	/*
	 * The strip will sit idle at this point doing nothing until
	 * the surface has recalculated it's strip list and then calls
	 * refresh_strip. Otherwise refresh strip will get a strip address
	 * that does not exist... Crash
	 */
 }

void
OSCRouteObserver::refresh_strip (boost::shared_ptr<ARDOUR::Stripable> new_strip, bool force)
{
	_init = true;
	if (_tick_busy) {
		Glib::usleep(100); // let tick finish
	}
	_last_gain =-1.0;
	_last_trim =-1.0;
	_send = boost::shared_ptr<ARDOUR::Send> ();

	send_select_status (ARDOUR::Properties::selected);

	if ((new_strip == _strip) && !force) {
		// no change don't send feedback
		_init = false;
		return;
	}
	pan_connections.drop_connections ();
	strip_connections.drop_connections ();
	_strip = new_strip;
	if (!_strip) {
		// this strip is blank and should be cleared
		clear_strip ();
		return;
	}
	_strip->DropReferences.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::no_strip, this), OSC::instance());
	as = ARDOUR::Off;

	if (feedback[0]) { // buttons are separate feedback
		_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::name_changed, this, boost::lambda::_1), OSC::instance());
		name_changed (ARDOUR::Properties::name);

		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
		if (rt) {
			rt->route_group_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::group_name, this), OSC::instance());
			group_name ();
		}

		_strip->presentation_info().PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::pi_changed, this, _1), OSC::instance());
		_osc.int_message_with_id (X_("/strip/hide"), ssid, _strip->is_hidden (), in_line, addr);

		_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/mute"), _strip->mute_control()), OSC::instance());
		_strip->mute_control()->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_automation, this, X_("/strip/mute"), _strip->mute_control()), OSC::instance());
		send_automation (X_("/strip/mute"), _strip->mute_control());
		send_change_message (X_("/strip/mute"), _strip->mute_control());

		_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo"), _strip->solo_control()), OSC::instance());
		send_change_message (X_("/strip/solo"), _strip->solo_control());

		if (_strip->solo_isolate_control()) {
			_strip->solo_isolate_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo_iso"), _strip->solo_isolate_control()), OSC::instance());
			send_change_message (X_("/strip/solo_iso"), _strip->solo_isolate_control());
		}

		if (_strip->solo_safe_control()) {
			_strip->solo_safe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/strip/solo_safe"), _strip->solo_safe_control()), OSC::instance());
			send_change_message (X_("/strip/solo_safe"), _strip->solo_safe_control());
		}

		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_strip);
		if (track) {
			track->monitoring_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_monitor_status, this, track->monitoring_control()), OSC::instance());
			send_monitor_status (track->monitoring_control());
		}

		boost::shared_ptr<AutomationControl> rec_controllable = _strip->rec_enable_control ();
		if (rec_controllable) {
			rec_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/recenable"), _strip->rec_enable_control()), OSC::instance());
			send_change_message (X_("/strip/recenable"), _strip->rec_enable_control());
		}
		boost::shared_ptr<AutomationControl> recsafe_controllable = _strip->rec_safe_control ();
		if (rec_controllable) {
			recsafe_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/record_safe"), _strip->rec_safe_control()), OSC::instance());
			send_change_message (X_("/strip/record_safe"), _strip->rec_safe_control());
		}
		_strip->presentation_info().PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_select_status, this, _1), OSC::instance());
		send_select_status (ARDOUR::Properties::selected);
	}

	if (feedback[1]) { // level controls
		_gain_control = _strip->gain_control();
		_gain_control->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::gain_automation, this), OSC::instance());
		_gain_control->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_gain_message, this), OSC::instance());
		gain_automation ();

		boost::shared_ptr<Controllable> trim_control = boost::dynamic_pointer_cast<Controllable>(_strip->trim_control());
		if (trim_control) {
			_strip->trim_control()->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_automation, this, X_("/strip/trimdB"), _strip->trim_control()), OSC::instance());
			send_automation (X_("/strip/trimdB"), _strip->trim_control());
			trim_control->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_trim_message, this), OSC::instance());
			send_trim_message ();
		}

		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
		if (rt) {
			boost::shared_ptr<PannerShell> pan_sh =  rt->panner_shell();
			current_pan_shell = pan_sh;
			if (pan_sh) {

				pan_sh->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::panner_changed, this, current_pan_shell), OSC::instance());
			}
			panner_changed (pan_sh);
		} else {
			current_pan_shell = boost::shared_ptr<PannerShell> ();
		}

	}
	_init = false;
	tick();

}

void
OSCRouteObserver::refresh_send (boost::shared_ptr<ARDOUR::Send> new_send, bool force)
{
	_init = true;
	if (_tick_busy) {
		Glib::usleep(100); // let tick finish
	}
	_last_gain =-1.0;
	_last_trim =-1.0;

	send_select_status (ARDOUR::Properties::selected);

	if ((new_send == _send) && !force) {
		// no change don't send feedback
		_init = false;
		return;
	}
	pan_connections.drop_connections ();
	strip_connections.drop_connections ();
	if (!_strip) {
		// this strip is blank and should be cleared
		clear_strip ();
		return;
	}
	_send = new_send;
	send_clear ();
	_strip->DropReferences.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::no_strip, this), OSC::instance());
	as = ARDOUR::Off;

	if (feedback[0]) { // buttons are separate feedback
		_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::name_changed, this, boost::lambda::_1), OSC::instance());
		name_changed (ARDOUR::Properties::name);
	}

	if (feedback[1]) { // level controls
		_gain_control = _send->gain_control();
		_gain_control->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::gain_automation, this), OSC::instance());
		_gain_control->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_gain_message, this), OSC::instance());
		gain_automation ();

		boost::shared_ptr<PannerShell> pan_sh =  _send->panner_shell();
		current_pan_shell = pan_sh;
		if (pan_sh) {
			pan_sh->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::panner_changed, this, current_pan_shell), OSC::instance());
		}
		panner_changed (pan_sh);

	}
	_init = false;
	tick();

}

void
OSCRouteObserver::set_expand (uint32_t expand)
{
	if (expand != _expand) {
		_expand = expand;
		if (expand == ssid) {
			_osc.float_message_with_id (X_("/strip/expand"), ssid, 1.0, in_line, addr);
		} else {
			_osc.float_message_with_id (X_("/strip/expand"), ssid, 0.0, in_line, addr);
		}
	}
}

void
OSCRouteObserver::set_link_ready (uint32_t not_ready)
{
	if (not_ready) {
		clear_strip ();
		switch (ssid) {
			case 1:
				_osc.text_message_with_id (X_("/strip/name"), ssid, "Device", in_line, addr);
				break;
			case 2:
				_osc.text_message_with_id (X_("/strip/name"), ssid, string_compose ("%1", not_ready), in_line, addr);
				break;
			case 3:
				_osc.text_message_with_id (X_("/strip/name"), ssid, "Missing", in_line, addr);
				break;
			case 4:
				_osc.text_message_with_id (X_("/strip/name"), ssid, "from", in_line, addr);
				break;
			case 5:
				_osc.text_message_with_id (X_("/strip/name"), ssid, "Linkset", in_line, addr);
				break;
			default:
				break;
		}
	} else {
		refresh_strip (_strip, true);
	}
}

void
OSCRouteObserver::clear_strip ()
{
	send_clear ();
	if (feedback[0]) { // buttons are separate feedback
		_osc.text_message_with_id (X_("/strip/name"), ssid, " ", in_line, addr);
	}
	if (feedback[1]) { // level controls
		if (gainmode) {
			_osc.float_message_with_id (X_("/strip/fader"), ssid, 0, in_line, addr);
		} else {
			_osc.float_message_with_id (X_("/strip/gain"), ssid, -193, in_line, addr);
		}
		_osc.float_message_with_id (X_("/strip/pan_stereo_position"), ssid, 0.5, in_line, addr);
	}
}

void
OSCRouteObserver::send_clear ()
{
	_init = true;

	strip_connections.drop_connections ();

	// all strip buttons should be off and faders 0 and etc.
	_osc.float_message_with_id (X_("/strip/expand"), ssid, 0, in_line, addr);
	if (feedback[0]) { // buttons are separate feedback
		_osc.text_message_with_id (X_("/strip/group"), ssid, "none", in_line, addr);
		_osc.float_message_with_id (X_("/strip/mute"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/solo"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/recenable"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/record_safe"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/monitor_input"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/monitor_disk"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/gui_select"), ssid, 0, in_line, addr);
		_osc.float_message_with_id (X_("/strip/select"), ssid, 0, in_line, addr);
	}
	if (feedback[1]) { // level controls
		_osc.float_message_with_id (X_("/strip/trimdB"), ssid, 0, in_line, addr);
	}
	if (feedback[9]) {
		_osc.float_message_with_id (X_("/strip/signal"), ssid, 0, in_line, addr);
	}
	if (feedback[7]) {
		if (gainmode) {
			_osc.float_message_with_id (X_("/strip/meter"), ssid, 0, in_line, addr);
		} else {
			_osc.float_message_with_id (X_("/strip/meter"), ssid, -193, in_line, addr);
		}
	}else if (feedback[8]) {
		_osc.float_message_with_id (X_("/strip/meter"), ssid, 0, in_line, addr);
	}
}


void
OSCRouteObserver::tick ()
{
	if (_init) {
		return;
	}
	_tick_busy = true;
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled
		// the only meter here is master
		/* XXXX need to add send meter for send mode or
		 * disable for send mode
		 */
		float now_meter;
		if (_strip->peak_meter()) {
			now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
		} else {
			now_meter = -193;
		}
		if (now_meter < -120) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				if (gainmode && feedback[7]) {
					_osc.float_message_with_id (X_("/strip/meter"), ssid, ((now_meter + 94) / 100), in_line, addr);
				} else if ((!gainmode) && feedback[7]) {
					_osc.float_message_with_id (X_("/strip/meter"), ssid, now_meter, in_line, addr);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint16_t ledbits = ~(0xfff<<ledlvl);
					_osc.int_message_with_id (X_("/strip/meter"), ssid, ledbits, in_line, addr);
				}
			}
			if (feedback[9]) {
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				_osc.float_message_with_id (X_("/strip/signal"), ssid, signal, in_line, addr);
			}
		}
		_last_meter = now_meter;

	}
	if (feedback[1]) {
		if (gain_timeout) {
			if (gain_timeout == 1) {
				name_changed (ARDOUR::Properties::name);
			}
			gain_timeout--;
		}
	}
	_tick_busy = false;
}

void
OSCRouteObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}
	string name = "";
	if (!_send) {
		name = _strip->name();
	} else {
		name = string_compose ("%1-Send", _strip->name());
	}

	if (_strip) {
		_osc.text_message_with_id (X_("/strip/name"), ssid, name, in_line, addr);
	}
}

void
OSCRouteObserver::panner_changed (boost::shared_ptr<ARDOUR::PannerShell> pan_sh)
{
	pan_connections.drop_connections ();

	if (feedback[1]) {
		if (pan_sh){
			string pt = pan_sh->current_panner_uri();
			if (pt.size()){
				string ptype = pt.substr(pt.find_last_of ('/') + 1);
				_osc.text_message_with_id (X_("/strip/pan_type"), ssid, ptype, in_line, addr);
			} else {
				_osc.text_message_with_id (X_("/strip/pan_type"), ssid, "none", in_line, addr);
				_osc.float_message_with_id (X_("/strip/pan_stereo_position"), ssid, 0.5, in_line, addr);
				_osc.float_message_with_id (X_("/strip/pan_stereo_width"), ssid, 1.0, in_line, addr);
				return;
			}
			boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(pan_sh->panner()->pannable()->pan_azimuth_control);
			if (pan_controllable) {
				boost::shared_ptr<AutomationControl>at = boost::dynamic_pointer_cast<AutomationControl> (pan_controllable);

				pan_controllable->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/pan_stereo_position"), current_pan_shell->panner()->pannable()->pan_azimuth_control), OSC::instance());
				at->alist()->automation_state_changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_automation, this, X_("/strip/pan_stereo_position"), current_pan_shell->panner()->pannable()->pan_azimuth_control), OSC::instance());
				send_change_message (X_("/strip/pan_stereo_position"), pan_controllable);
				send_automation (X_("/strip/pan_stereo_position"), pan_controllable);
			} else {
				_osc.float_message_with_id (X_("/strip/pan_stereo_position"), ssid, 0.5, in_line, addr);
			}
			boost::shared_ptr<Controllable> width_controllable = boost::dynamic_pointer_cast<Controllable>(pan_sh->panner()->pannable()->pan_width_control);
			if (width_controllable) {
				boost::shared_ptr<AutomationControl>at = boost::dynamic_pointer_cast<AutomationControl> (width_controllable);
				width_controllable->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_change_message, this, X_("/strip/pan_stereo_width"), current_pan_shell->panner()->pannable()->pan_width_control), OSC::instance());
				at->alist()->automation_state_changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::send_automation, this, X_("/strip/pan_stereo_width"), current_pan_shell->panner()->pannable()->pan_width_control), OSC::instance());
				send_change_message (X_("/strip/pan_stereo_width"), width_controllable);
				send_automation (X_("/strip/pan_stereo_width"), width_controllable);
			} else {
				_osc.float_message_with_id (X_("/strip/pan_stereo_width"), ssid, 1.0, in_line, addr);
			}
		} else {
			_osc.text_message_with_id (X_("/strip/pan_type"), ssid, "none", in_line, addr);
			_osc.float_message_with_id (X_("/strip/pan_stereo_position"), ssid, 0.5, in_line, addr);
			_osc.float_message_with_id (X_("/strip/pan_stereo_width"), ssid, 1.0, in_line, addr);
		}
	}
}

void
OSCRouteObserver::group_name ()
{
	boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);

	RouteGroup *rg = rt->route_group();
	if (rg) {
		_osc.text_message_with_id (X_("/strip/group"), ssid, rg->name(), in_line, addr);
	} else {
		_osc.text_message_with_id (X_("/strip/group"), ssid, " ", in_line, addr);
	}
}

void
OSCRouteObserver::pi_changed (PBD::PropertyChange const& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::hidden)) {
		return;
	}
	_osc.int_message_with_id (X_("/strip/hide"), ssid, _strip->is_hidden (), in_line, addr);
}

void
OSCRouteObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	_osc.float_message_with_id (path, ssid, (float) controllable->internal_to_interface (val), in_line, addr);
}

void
OSCRouteObserver::send_automation (string path, boost::shared_ptr<PBD::Controllable> control)
{
	boost::shared_ptr<AutomationControl>automate = boost::dynamic_pointer_cast<AutomationControl> (control);

	AutoState as = automate->alist()->automation_state();
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
	_osc.float_message_with_id (string_compose (X_("%1/automation"), path), ssid, output, in_line, addr);
	_osc.text_message_with_id (string_compose (X_("%1/automation_name"), path), ssid, auto_name, in_line, addr);
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
	_osc.int_message_with_id (X_("/strip/monitor_input"), ssid, input, in_line, addr);
	_osc.int_message_with_id (X_("/strip/monitor_disk"), ssid, disk, in_line, addr);

}

void
OSCRouteObserver::send_trim_message ()
{
	if (_last_trim != _strip->trim_control()->get_value()) {
		_last_trim = _strip->trim_control()->get_value();
	} else {
		return;
	}
	_osc.float_message_with_id (X_("/strip/trimdB"), ssid, (float) accurate_coefficient_to_dB (_last_trim), in_line, addr);
}

void
OSCRouteObserver::send_gain_message ()
{
	if (_last_gain != _gain_control->get_value()) {
		_last_gain = _gain_control->get_value();
	} else {
		return;
	}

	if (gainmode) {
		_osc.float_message_with_id (X_("/strip/fader"), ssid, _gain_control->internal_to_interface (_last_gain), in_line, addr);
		if (gainmode == 1) {
			_osc.text_message_with_id (X_("/strip/name"), ssid, string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (_last_gain)), in_line, addr);
			gain_timeout = 8;
		}
	}
	if (!gainmode || gainmode == 2) {
		if (_last_gain < 1e-15) {
			_osc.float_message_with_id (X_("/strip/gain"), ssid, -200, in_line, addr);
		} else {
			_osc.float_message_with_id (X_("/strip/gain"), ssid, accurate_coefficient_to_dB (_last_gain), in_line, addr);
		}
	}
}

void
OSCRouteObserver::gain_automation ()
{
	string path = X_("/strip/gain");
	if (gainmode) {
		path = X_("/strip/fader");
	}
	send_gain_message ();
	as = _gain_control->alist()->automation_state();
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
	_osc.float_message_with_id (string_compose (X_("%1/automation"), path), ssid, output, in_line, addr);
	_osc.text_message_with_id (string_compose (X_("%1/automation_name"), path), ssid, auto_name, in_line, addr);
}

void
OSCRouteObserver::send_select_status (const PropertyChange& what)
{
	if (what == PropertyChange(ARDOUR::Properties::selected)) {
		if (_strip) {
			_osc.float_message_with_id (X_("/strip/select"), ssid, _strip->is_selected(), in_line, addr);
		}
	}
}
