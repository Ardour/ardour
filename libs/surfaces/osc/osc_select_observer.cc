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

#include <vector>
#include "boost/lambda/lambda.hpp"

#include "pbd/control_math.h"

#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/phase_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/readonly_control.h"

#include "osc.h"
#include "osc_select_observer.h"

#include <glibmm.h>

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCSelectObserver::OSCSelectObserver (OSC& o, ArdourSurface::OSC::OSCSurface* su)
	: _osc (o)
	,sur (su)
	,nsends (0)
	,_last_gain (-1.0)
	,_last_trim (-1.0)
	,_init (true)
	,eq_bands (0)
{
	addr = lo_address_new_from_url 	(sur->remote_url.c_str());
	gainmode = sur->gainmode;
	feedback = sur->feedback;
	in_line = feedback[2];
	refresh_strip (true);
	send_page_size = sur->send_page_size;
}

OSCSelectObserver::~OSCSelectObserver ()
{
	_init = true;
	no_strip ();
	lo_address_free (addr);
}

void
OSCSelectObserver::no_strip ()
{
	// This gets called on drop references
	_init = true;

	strip_connections.drop_connections ();
	send_connections.drop_connections ();
	plugin_connections.drop_connections ();
	eq_connections.drop_connections ();
	/*
	 * The strip will sit idle at this point doing nothing until
	 * the surface has recalculated it's strip list and then calls
	 * refresh_strip. Otherwise refresh strip will get a strip address
	 * that does not exist... Crash
	 */
 }

void
OSCSelectObserver::refresh_strip (bool force)
{
	_init = true;
	if (_tick_busy) {
		Glib::usleep(100); // let tick finish
	}

	// this has to be done first because expand may change with no strip change
	if (sur->expand_enable) {
		_osc.float_message ("/select/expand", 1.0, addr);
	} else {
		_osc.float_message ("/select/expand", 0.0, addr);
	}

	boost::shared_ptr<ARDOUR::Stripable> new_strip = sur->select;
	if (_strip && (new_strip == _strip) && !force) {
		_init = false;
		return;
	}

	_strip = new_strip;
	if (!_strip) {
		clear_observer ();
		return;
	}

	_strip->DropReferences.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::no_strip, this), OSC::instance());
	as = ARDOUR::Off;
	send_size = 0;
	plug_size = 0;
	_comp_redux = 1;
	nsends = 0;
	_last_gain = -1.0;
	_last_trim = -1.0;

	_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::name_changed, this, boost::lambda::_1), OSC::instance());
	name_changed (ARDOUR::Properties::name);

	_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/mute"), _strip->mute_control()), OSC::instance());
	change_message ("/select/mute", _strip->mute_control());

	_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo"), _strip->solo_control()), OSC::instance());
	change_message ("/select/solo", _strip->solo_control());

	if (_strip->solo_isolate_control()) {
		_strip->solo_isolate_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_iso"), _strip->solo_isolate_control()), OSC::instance());
		change_message ("/select/solo_iso", _strip->solo_isolate_control());
	}

	if (_strip->solo_safe_control()) {
		_strip->solo_safe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_safe"), _strip->solo_safe_control()), OSC::instance());
		change_message ("/select/solo_safe", _strip->solo_safe_control());
	}

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

	_strip->gain_control()->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_automation, this), OSC::instance());
	_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_message, this), OSC::instance());
	gain_automation ();

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

	// Rest of possible pan controls... Untested because I can't find a way to get them in the GUI :)
	if (_strip->pan_elevation_control ()) {
		_strip->pan_elevation_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_elevation_position"), _strip->pan_elevation_control()), OSC::instance());
		change_message ("/select/pan_elevation_position", _strip->pan_elevation_control());
	}
	if (_strip->pan_frontback_control ()) {
		_strip->pan_frontback_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_frontback_position"), _strip->pan_frontback_control()), OSC::instance());
		change_message ("/select/pan_frontback_position", _strip->pan_frontback_control());
	}
	if (_strip->pan_lfe_control ()) {
		_strip->pan_lfe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_lfe_control"), _strip->pan_lfe_control()), OSC::instance());
		change_message ("/select/pan_lfe_control", _strip->pan_lfe_control());
	}

	// sends, plugins and eq
	// detecting processor changes is now in osc.cc

	// but... MB master send enable is different
	if (_strip->master_send_enable_controllable ()) {
		_strip->master_send_enable_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/master_send_enable"), _strip->master_send_enable_controllable()), OSC::instance());
		enable_message ("/select/master_send_enable", _strip->master_send_enable_controllable());
	}

	// Compressor
	if (_strip->comp_enable_controllable ()) {
		_strip->comp_enable_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/comp_enable"), _strip->comp_enable_controllable()), OSC::instance());
		enable_message ("/select/comp_enable", _strip->comp_enable_controllable());
	}
	if (_strip->comp_threshold_controllable ()) {
		_strip->comp_threshold_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_threshold"), _strip->comp_threshold_controllable()), OSC::instance());
		change_message ("/select/comp_threshold", _strip->comp_threshold_controllable());
	}
	if (_strip->comp_speed_controllable ()) {
		_strip->comp_speed_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_speed"), _strip->comp_speed_controllable()), OSC::instance());
		change_message ("/select/comp_speed", _strip->comp_speed_controllable());
	}
	if (_strip->comp_mode_controllable ()) {
		_strip->comp_mode_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::comp_mode, this), OSC::instance());
		comp_mode ();
	}
	if (_strip->comp_makeup_controllable ()) {
		_strip->comp_makeup_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_makeup"), _strip->comp_makeup_controllable()), OSC::instance());
		change_message ("/select/comp_makeup", _strip->comp_makeup_controllable());
	}
	renew_sends ();
	renew_plugin ();
	eq_restart(0);
	_init = false;

	tick();
}

void
OSCSelectObserver::clear_observer ()
{
	_init = true;
	strip_connections.drop_connections ();
	// all strip buttons should be off and faders 0 and etc.
	_osc.float_message ("/select/expand", 0, addr);
	_osc.text_message ("/select/name", " ", addr);
	_osc.text_message ("/select/comment", " ", addr);
	_osc.float_message ("/select/mute", 0, addr);
	_osc.float_message ("/select/solo", 0, addr);
	_osc.float_message ("/select/recenable", 0, addr);
	_osc.float_message ("/select/record_safe", 0, addr);
	_osc.float_message ("/select/monitor_input", 0, addr);
	_osc.float_message ("/select/monitor_disk", 0, addr);
	_osc.float_message ("/select/polarity", 0, addr);
	_osc.float_message ("/select/n_inputs", 0, addr);
	_osc.float_message ("/select/n_outputs", 0, addr);
	if (gainmode) {
		_osc.float_message ("/select/fader", 0, addr);
	} else {
		_osc.float_message ("/select/gain", -193, addr);
	}
	_osc.float_message ("/select/trimdB", 0, addr);
	_osc.float_message ("/select/pan_stereo_position", 0.5, addr);
	_osc.float_message ("/select/pan_stereo_width", 1, addr);
	if (feedback[9]) {
		_osc.float_message ("/select/signal", 0, addr);
	}
	if (feedback[7]) {
		if (gainmode) {
			_osc.float_message ("/select/meter", 0, addr);
		} else {
			_osc.float_message ("/select/meter", -193, addr);
		}
	}else if (feedback[8]) {
		_osc.float_message ("/select/meter", 0, addr);
	}
	_osc.float_message ("/select/pan_elevation_position", 0, addr);
	_osc.float_message ("/select/pan_frontback_position", .5, addr);
	_osc.float_message ("/select/pan_lfe_control", 0, addr);
	_osc.float_message ("/select/comp_enable", 0, addr);
	_osc.float_message ("/select/comp_threshold", 0, addr);
	_osc.float_message ("/select/comp_speed", 0, addr);
	_osc.float_message ("/select/comp_mode", 0, addr);
	_osc.text_message ("/select/comp_mode_name", " ", addr);
	_osc.text_message ("/select/comp_speed_name", " ", addr);
	_osc.float_message ("/select/comp_makeup", 0, addr);
	_osc.float_message ("/select/expand", 0.0, addr);
	send_end();
	plugin_end();
	eq_end();
}

void
OSCSelectObserver::renew_sends () {
	send_end();
	send_init();
}

void
OSCSelectObserver::renew_plugin () {
	plugin_end();
	plugin_init();
}

void
OSCSelectObserver::send_init()
{
	// we don't know how many there are, so find out.
	bool sends;
	nsends  = 0;
	do {
		sends = false;
		if (_strip->send_level_controllable (nsends)) {
			sends = true;
			nsends++;
		}
	} while (sends);
	if (!nsends) {
		return;
	}

	// paging should be done in osc.cc in case there is no feedback
	send_size = nsends;
	if (send_page_size) {
		send_size = send_page_size;
	}
	// check limits
	uint32_t max_page = (uint32_t)(nsends / send_size) + 1;
	if (sur->send_page < 1) {
		sur->send_page = 1;
	} else if ((uint32_t)sur->send_page > max_page) {
		sur->send_page = max_page;
	}
	uint32_t page_start = ((sur->send_page - 1) * send_size);
	uint32_t last_send = sur->send_page * send_size;
	uint32_t c = 1;
	send_timeout.push_back (2);
	_last_send.clear();
	_last_send.push_back (0.0);

	for (uint32_t s = page_start; s < last_send; ++s, ++c) {

		bool send_valid = false;
		if (_strip->send_level_controllable (s)) {
			_strip->send_level_controllable(s)->Changed.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_gain, this, c, _strip->send_level_controllable(s)), OSC::instance());
			send_timeout.push_back (2);
			_last_send.push_back (0.0);
			send_gain (c, _strip->send_level_controllable(s));
			send_valid = true;
		}

		if (_strip->send_enable_controllable (s)) {
			_strip->send_enable_controllable(s)->Changed.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message_with_id, this, X_("/select/send_enable"), c, _strip->send_enable_controllable(s)), OSC::instance());
			enable_message_with_id ("/select/send_enable", c, _strip->send_enable_controllable(s));
		} else if (send_valid) {
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (_strip);
			if (!r) {
				// should never get here
				_osc.float_message_with_id ("/select/send_enable", c, 0, in_line, addr);
			}
			boost::shared_ptr<Send> snd = boost::dynamic_pointer_cast<Send> (r->nth_send(s));
			if (snd) {
				boost::shared_ptr<Processor> proc = boost::dynamic_pointer_cast<Processor> (snd);
				proc->ActiveChanged.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_enable, this, X_("/select/send_enable"), c, proc), OSC::instance());
				_osc.float_message_with_id ("/select/send_enable", c, proc->enabled(), in_line, addr);
			}
		}
		if (!gainmode && send_valid) {
			_osc.text_message_with_id ("/select/send_name", c, _strip->send_name(s), in_line, addr);
		}
	}
}

void
OSCSelectObserver::plugin_init()
{
	if (!sur->plugin_id || !sur->plugins.size ()) {
		return;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(_strip);
	if (!r) {
		return;
	}

	// we have a plugin number now get the processor
	boost::shared_ptr<Processor> proc = r->nth_plugin (sur->plugins[sur->plugin_id - 1]);
	boost::shared_ptr<PluginInsert> pi;
	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(proc))) {
		return;
	}
	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();

	bool ok = false;
	nplug_params = sur->plug_params.size ();

	// default of 0 page size means show all
	plug_size = nplug_params;
	if (sur->plug_page_size) {
		plug_size = sur->plug_page_size;
	}
	_osc.text_message ("/select/plugin/name", pip->name(), addr);
	uint32_t page_end = nplug_params;
	uint32_t max_page = 1;
	if (plug_size && nplug_params) {
		max_page = (uint32_t)((nplug_params - 1) / plug_size) + 1;
	}

	if (sur->plug_page < 1) {
		sur->plug_page = 1;
	}
	if ((uint32_t)sur->plug_page > max_page) {
		sur->plug_page = max_page;
	}
	uint32_t page_start = ((sur->plug_page - 1) * plug_size);
	page_end = sur->plug_page * plug_size;

	int pid = 1;
	for ( uint32_t ppi = page_start;  ppi < page_end; ++ppi, ++pid) {
		if (ppi >= nplug_params) {
			_osc.text_message_with_id ("/select/plugin/parameter/name", pid, " ", in_line, addr);
			_osc.float_message_with_id ("/select/plugin/parameter", pid, 0, in_line, addr);
			continue;
		}

		uint32_t controlid = pip->nth_parameter(sur->plug_params[ppi], ok);
		if (!ok) {
			continue;
		}
		ParameterDescriptor pd;
		pip->get_parameter_descriptor(controlid, pd);
		_osc.text_message_with_id ("/select/plugin/parameter/name", pid, pd.label, in_line, addr);
		if ( pip->parameter_is_input(controlid)) {
			boost::shared_ptr<AutomationControl> c = pi->automation_control(Evoral::Parameter(PluginAutomation, 0, controlid));
			if (c) {
				bool swtch = false;
				if (pd.integer_step && pd.upper == 1) {
					swtch = true;
				}
				c->Changed.connect (plugin_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::plugin_parameter_changed, this, pid, swtch, c), OSC::instance());
				plugin_parameter_changed (pid, swtch, c);
			}
		}
	}
}

void
OSCSelectObserver::send_end ()
{
	send_connections.drop_connections ();
	for (uint32_t i = 1; i <= send_size; i++) {
		if (gainmode) {
			_osc.float_message_with_id ("/select/send_fader", i, 0, in_line, addr);
		} else {
			_osc.float_message_with_id ("/select/send_gain", i, -193, in_line, addr);
		}
		// next enable
		_osc.float_message_with_id ("/select/send_enable", i, 0, in_line, addr);
		// next name
		_osc.text_message_with_id ("/select/send_name", i, " ", in_line, addr);
	}
	// need to delete or clear send_timeout
	send_timeout.clear();
	nsends = 0;
}

void
OSCSelectObserver::plugin_parameter_changed (int pid, bool swtch, boost::shared_ptr<PBD::Controllable> controllable)
{
	if (swtch) {
		enable_message_with_id ("/select/plugin/parameter", pid, controllable);
	} else {
		change_message_with_id ("/select/plugin/parameter", pid, controllable);
	}
}

void
OSCSelectObserver::plugin_end ()
{
	plugin_connections.drop_connections ();
	_osc.text_message ("/select/plugin/name", " ", addr);
	for (uint32_t i = 1; i <= plug_size; i++) {
		_osc.float_message_with_id ("/select/plugin/parameter", i, 0, in_line, addr);
		// next name
		_osc.text_message_with_id ("/select/plugin/parameter/name", i, " ", in_line, addr);
	}
	nplug_params = 0;
}

void
OSCSelectObserver::tick ()
{
	if (_init) {
		return;
	}
	_tick_busy = true;
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled
		float now_meter;
		if (_strip->peak_meter()) {
			now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
		} else {
			now_meter = -193;
		}
		if (now_meter < -144) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				string path = "/select/meter";
				if (gainmode && feedback[7]) {
					_osc.float_message (path, ((now_meter + 94) / 100), addr);
				} else if ((!gainmode) && feedback[7]) {
					_osc.float_message (path, now_meter, addr);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint16_t ledbits = ~(0xfff<<ledlvl);
					_osc.float_message (path, ledbits, addr);
				}
			}
			if (feedback[9]) {
				string path = "/select/signal";
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				_osc.float_message (path, signal, addr);
			}
		}
		_last_meter = now_meter;

	}
	if (gain_timeout) {
		if (gain_timeout == 1) {
			_osc.text_message ("/select/name", _strip->name(), addr);
		}
		gain_timeout--;
	}

	if (as == ARDOUR::Play ||  as == ARDOUR::Touch) {
		if(_last_gain != _strip->gain_control()->get_value()) {
			_last_gain = _strip->gain_control()->get_value();
				gain_message ();
		}
	}
	if (_strip->comp_redux_controllable() && _strip->comp_enable_controllable() && _strip->comp_enable_controllable()->get_value()) {
		float new_value = _strip->comp_redux_controllable()->get_parameter();
		if (_comp_redux != new_value) {
			_osc.float_message ("/select/comp_redux", new_value, addr);
			_comp_redux = new_value;
		}
	}
	for (uint32_t i = 1; i <= send_timeout.size(); i++) {
		if (send_timeout[i]) {
			if (send_timeout[i] == 1) {
				uint32_t pg_offset = (sur->send_page - 1) * send_page_size;
				_osc.text_message_with_id ("/select/send_name", i, _strip->send_name(pg_offset + i - 1), in_line, addr);
			}
			send_timeout[i]--;
		}
	}
	_tick_busy = false;
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

	_osc.text_message ("/select/name", _strip->name(), addr);
	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (_strip);
	if (route) {
		//spit out the comment at the same time
		_osc.text_message ("/select/comment", route->comment(), addr);
		// lets tell the surface how many inputs this strip has
		_osc.float_message ("/select/n_inputs", (float) route->n_inputs().n_total(), addr);
		// lets tell the surface how many outputs this strip has
		_osc.float_message ("/select/n_outputs", (float) route->n_outputs().n_total(), addr);
	}
}

void
OSCSelectObserver::change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();

	_osc.float_message (path, (float) controllable->internal_to_interface (val), addr);
}

void
OSCSelectObserver::enable_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	if (val) {
		_osc.float_message (path, 1, addr);
	} else {
		_osc.float_message (path, 0, addr);
	}

}

void
OSCSelectObserver::change_message_with_id (string path, uint32_t id, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();

	_osc.float_message_with_id (path, id, (float) controllable->internal_to_interface (val), in_line, addr);
}

void
OSCSelectObserver::enable_message_with_id (string path, uint32_t id, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	if (val) {
		_osc.float_message_with_id (path, id, 1, in_line, addr);
	} else {
		_osc.float_message_with_id (path, id, 0, in_line, addr);
	}
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

	_osc.float_message ("/select/monitor_input", (float) input, addr);
	_osc.float_message ("/select/monitor_disk", (float) disk, addr);
}

void
OSCSelectObserver::trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	if (_last_trim != controllable->get_value()) {
		_last_trim = controllable->get_value();
	} else {
		return;
	}

	_osc.float_message (path, (float) accurate_coefficient_to_dB (controllable->get_value()), addr);
}

void
OSCSelectObserver::gain_message ()
{
	float value = _strip->gain_control()->get_value();
	if (_last_gain != value) {
		_last_gain = value;
	} else {
		return;
	}

	if (gainmode) {
		_osc.text_message ("/select/name", string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (value)), addr);
		gain_timeout = 8;
		_osc.float_message ("/select/fader", _strip->gain_control()->internal_to_interface (value), addr);
	} else {
		if (value < 1e-15) {
			_osc.float_message ("/select/gain", -200, addr);
		} else {
			_osc.float_message ("/select/gain", accurate_coefficient_to_dB (value), addr);
		}
	}
}

void
OSCSelectObserver::gain_automation ()
{
	float output = 0;
	as = _strip->gain_control()->alist()->automation_state();
	string auto_name;
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
		default:
			break;
	}

	if (gainmode) {
		_osc.float_message ("/select/fader/automation", output, addr);
		_osc.text_message ("/select/fader/automation_name", auto_name, addr);
	} else {
		_osc.float_message ("/select/gain/automation", output, addr);
		_osc.text_message ("/select/gain/automation_name", auto_name, addr);
	}

	gain_message ();
}

void
OSCSelectObserver::send_gain (uint32_t id, boost::shared_ptr<PBD::Controllable> controllable)
{
	if (_last_send[id] != controllable->get_value()) {
		_last_send[id] = controllable->get_value();
	} else {
		return;
	}
	string path;
	float value;
	float db;
#ifdef MIXBUS
		db = controllable->get_value();
#else
		if (controllable->get_value() < 1e-15) {
			db = -193;
		} else {
			db = accurate_coefficient_to_dB (controllable->get_value());
		}
#endif

	if (gainmode) {
		path = "/select/send_fader";
		value = controllable->internal_to_interface (controllable->get_value());
		_osc.text_message_with_id ("/select/send_name" , id, string_compose ("%1%2%3", std::fixed, std::setprecision(2), db), in_line, addr);
	if (send_timeout.size() > id) {
		send_timeout[id] = 8;
	}
	} else {
		path = "/select/send_gain";
		value = db;
	}

	_osc.float_message_with_id (path, id, value, in_line, addr);
}

void
OSCSelectObserver::send_enable (string path, uint32_t id, boost::shared_ptr<Processor> proc)
{
	// with no delay value is wrong
	Glib::usleep(10);

	_osc.float_message_with_id ("/select/send_enable", id, proc->enabled(), in_line, addr);
}

void
OSCSelectObserver::comp_mode ()
{
	change_message ("/select/comp_mode", _strip->comp_mode_controllable());
	_osc.text_message ("/select/comp_mode_name", _strip->comp_mode_name(_strip->comp_mode_controllable()->get_value()), addr);
	_osc.text_message ("/select/comp_speed_name", _strip->comp_speed_name(_strip->comp_mode_controllable()->get_value()), addr);
}

void
OSCSelectObserver::eq_init()
{
	// HPF and enable are special case, rest are in bands
	if (_strip->filter_enable_controllable (true)) {
		_strip->filter_enable_controllable (true)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf/enable"), _strip->filter_enable_controllable (true)), OSC::instance());
		change_message ("/select/eq_hpf/enable", _strip->filter_enable_controllable(true));
	}

	if (_strip->filter_enable_controllable (false)) {
		_strip->filter_enable_controllable (false)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_lpf/enable"), _strip->filter_enable_controllable (false)), OSC::instance());
		change_message ("/select/eq_lpf/enable", _strip->filter_enable_controllable(false));
	}

	if (_strip->filter_freq_controllable (true)) {
		_strip->filter_freq_controllable (true)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf/freq"), _strip->filter_freq_controllable (true)), OSC::instance());
		change_message ("/select/eq_hpf/freq", _strip->filter_freq_controllable(true));
	}

	if (_strip->filter_freq_controllable (false)) {
		_strip->filter_freq_controllable (false)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_lpf/freq"), _strip->filter_freq_controllable (false)), OSC::instance());
		change_message ("/select/eq_lpf/freq", _strip->filter_freq_controllable(false));
	}

	if (_strip->filter_slope_controllable (true)) {
		_strip->filter_slope_controllable (true)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf/slope"), _strip->filter_slope_controllable (true)), OSC::instance());
		change_message ("/select/eq_hpf/slope", _strip->filter_slope_controllable(true));
	}

	if (_strip->filter_slope_controllable (false)) {
		_strip->filter_slope_controllable (false)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_lpf/slope"), _strip->filter_slope_controllable (false)), OSC::instance());
		change_message ("/select/eq_lpf/slope", _strip->filter_slope_controllable(false));
	}

	if (_strip->eq_enable_controllable ()) {
		_strip->eq_enable_controllable ()->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/eq_enable"), _strip->eq_enable_controllable()), OSC::instance());
		enable_message ("/select/eq_enable", _strip->eq_enable_controllable());
	}

	eq_bands = _strip->eq_band_cnt ();
	if (eq_bands < 0) {
		eq_bands = 0;
	}
	if (!eq_bands) {
		return;
	}

	for (int i = 0; i < eq_bands; i++) {
		if (_strip->eq_band_name(i).size()) {
			_osc.text_message_with_id ("/select/eq_band_name", i + 1, _strip->eq_band_name (i), in_line, addr);
		}
		if (_strip->eq_gain_controllable (i)) {
			_strip->eq_gain_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_gain"), i + 1, _strip->eq_gain_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_gain", i + 1, _strip->eq_gain_controllable(i));
		}
		if (_strip->eq_freq_controllable (i)) {
			_strip->eq_freq_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_freq"), i + 1, _strip->eq_freq_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_freq", i + 1, _strip->eq_freq_controllable(i));
		}
		if (_strip->eq_q_controllable (i)) {
			_strip->eq_q_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_q"), i + 1, _strip->eq_q_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_q", i + 1, _strip->eq_q_controllable(i));
		}
		if (_strip->eq_shape_controllable (i)) {
			_strip->eq_shape_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_shape"), i + 1, _strip->eq_shape_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_shape", i + 1, _strip->eq_shape_controllable(i));
		}
	}
}

void
OSCSelectObserver::eq_end ()
{
	eq_connections.drop_connections ();
		_osc.float_message ("/select/eq_hpf", 0, addr);
		_osc.float_message ("/select/eq_enable", 0, addr);

	for (int i = 1; i <= eq_bands; i++) {
		_osc.text_message_with_id ("/select/eq_band_name", i, " ", in_line, addr);
		_osc.float_message_with_id ("/select/eq_gain", i, 0, in_line, addr);
		_osc.float_message_with_id ("/select/eq_freq", i, 0, in_line, addr);
		_osc.float_message_with_id ("/select/eq_q", i, 0, in_line, addr);
		_osc.float_message_with_id ("/select/eq_shape", i, 0, in_line, addr);


	}
}

void
OSCSelectObserver::eq_restart(int x)
{
	eq_end();
	eq_init();
}
