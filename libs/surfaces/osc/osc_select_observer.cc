/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2020 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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
#include "ardour/route_group.h"
#include "ardour/route_group_member.h"
#include "ardour/send.h"
#include "ardour/panner_shell.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/readonly_control.h"
#include "ardour/vca.h"

#include "osc.h"
#include "osc_select_observer.h"

#include <glibmm.h>

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCSelectObserver::OSCSelectObserver (OSC& o, ARDOUR::Session& s, ArdourSurface::OSC::OSCSurface* su)
	: _osc (o)
	,sur (su)
	,nsends (0)
	,_last_gain (-1.0)
	,_last_trim (-1.0)
	,_init (true)
	,eq_bands (0)
	,_expand (2048)
{
	session = &s;
	addr = lo_address_new_from_url 	(sur->remote_url.c_str());
	gainmode = sur->gainmode;
	feedback = sur->feedback;
	in_line = feedback[2];
	send_page_size = sur->send_page_size;
	send_size = send_page_size;
	send_page = sur->send_page;
	plug_page_size = sur->plug_page_size;
	plug_size = plug_page_size;
	plug_page = sur->plug_page;
	if (sur->plugins.size () > 0) {
		plug_id = sur->plugins[sur->plugin_id - 1];
	} else {
		plug_id = -1;
	}
	_group_sharing[15] = 1;
	refresh_strip (sur->select, sur->nsends, gainmode, true);
	set_expand (sur->expand_enable);
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

	pan_connections.drop_connections ();
	strip_connections.drop_connections ();
	send_connections.drop_connections ();
	plugin_connections.drop_connections ();
	eq_connections.drop_connections ();
	_strip = boost::shared_ptr<Stripable> ();
	/*
	 * The strip will sit idle at this point doing nothing until
	 * the surface has recalculated it's strip list and then calls
	 * refresh_strip. Otherwise refresh strip will get a strip address
	 * that does not exist... Crash
	 */
 }

void
OSCSelectObserver::refresh_strip (boost::shared_ptr<ARDOUR::Stripable> new_strip, uint32_t s_nsends, uint32_t gm, bool force)
{
	_init = true;
	if (_tick_busy) {
		Glib::usleep(100); // let tick finish
	}
	gainmode = gm;

	if (_strip && (new_strip == _strip) && !force) {
		_init = false;
		return;
	}

	no_strip();
	_strip = new_strip;
	if (!_strip) {
		clear_observer ();
		return;
	}

	_strip->DropReferences.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::no_strip, this), OSC::instance());
	as = ARDOUR::Off;
	_comp_redux = 1;
	nsends = s_nsends;
	_last_gain = -1.0;
	_last_trim = -1.0;

	_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::name_changed, this, boost::lambda::_1), OSC::instance());
	name_changed (ARDOUR::Properties::name);

	boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
	if (rt) {
		rt->route_group_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::group_name, this), OSC::instance());
		group_name ();

		rt->comment_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::comment_changed, this), OSC::instance());
		comment_changed ();

		session->RouteGroupPropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::group_sharing, this, _1), OSC::instance());
		group_sharing (rt->route_group ());

		boost::shared_ptr<PannerShell> pan_sh =  rt->panner_shell();
		if (pan_sh) {
			pan_sh->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::panner_changed, this), OSC::instance());
		}
		panner_changed ();

	} else {
		group_sharing (0);
	}

	_strip->presentation_info().PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::pi_changed, this, _1), OSC::instance());
	_osc.float_message (X_("/select/hide"), _strip->is_hidden (), addr);

	_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/mute"), _strip->mute_control()), OSC::instance());
	_strip->mute_control()->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_automation, this, X_("/select/mute"), _strip->mute_control()), OSC::instance());
	change_message (X_("/select/mute"), _strip->mute_control());
	send_automation (X_("/select/mute"), _strip->mute_control());

	_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo"), _strip->solo_control()), OSC::instance());
	change_message (X_("/select/solo"), _strip->solo_control());

	if (_strip->solo_isolate_control()) {
		_strip->solo_isolate_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_iso"), _strip->solo_isolate_control()), OSC::instance());
		change_message (X_("/select/solo_iso"), _strip->solo_isolate_control());
	}

	if (_strip->solo_safe_control()) {
		_strip->solo_safe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_safe"), _strip->solo_safe_control()), OSC::instance());
		change_message (X_("/select/solo_safe"), _strip->solo_safe_control());
	}

	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_strip);
	if (track) {
		track->monitoring_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::monitor_status, this, track->monitoring_control()), OSC::instance());
		monitor_status (track->monitoring_control());
	}

	boost::shared_ptr<AutomationControl> rec_controllable = _strip->rec_enable_control ();
	if (rec_controllable) {
		rec_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/recenable"), _strip->rec_enable_control()), OSC::instance());
		change_message (X_("/select/recenable"), _strip->rec_enable_control());
	}

	boost::shared_ptr<AutomationControl> recsafe_controllable = _strip->rec_safe_control ();
	if (recsafe_controllable) {
		recsafe_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/record_safe"), _strip->rec_safe_control()), OSC::instance());
		change_message (X_("/select/record_safe"), _strip->rec_safe_control());
	}

	boost::shared_ptr<AutomationControl> phase_controllable = _strip->phase_control ();
	if (phase_controllable) {
		phase_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/polarity"), _strip->phase_control()), OSC::instance());
		change_message (X_("/select/polarity"), _strip->phase_control());
	}

	_strip->gain_control()->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_automation, this), OSC::instance());
	_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_message, this), OSC::instance());
	gain_automation ();

	boost::shared_ptr<Slavable> slv = boost::dynamic_pointer_cast<Slavable> (_strip);
	slv->AssignmentChange.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::slaved_changed, this, _1, _2), OSC::instance());
	slaved_changed (boost::shared_ptr<VCA>(), false);

	boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->trim_control());
	if (trim_controllable) {
		trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::trim_message, this, X_("/select/trimdB"), _strip->trim_control()), OSC::instance());
		_strip->trim_control()->alist()->automation_state_changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_automation, this, X_("/select/trimdB"), _strip->trim_control()), OSC::instance());
		trim_message (X_("/select/trimdB"), _strip->trim_control());
		send_automation (X_("/select/trimdB"), _strip->trim_control());
	}

	// sends, plugins and eq
	// detecting processor changes is now in osc.cc

	// but... MB master send enable is different
	if (_strip->master_send_enable_controllable ()) {
		_strip->master_send_enable_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/master_send_enable"), _strip->master_send_enable_controllable()), OSC::instance());
		enable_message (X_("/select/master_send_enable"), _strip->master_send_enable_controllable());
	}

	// Compressor
	if (_strip->comp_enable_controllable ()) {
		_strip->comp_enable_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/comp_enable"), _strip->comp_enable_controllable()), OSC::instance());
		enable_message (X_("/select/comp_enable"), _strip->comp_enable_controllable());
	}
	if (_strip->comp_threshold_controllable ()) {
		_strip->comp_threshold_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_threshold"), _strip->comp_threshold_controllable()), OSC::instance());
		change_message (X_("/select/comp_threshold"), _strip->comp_threshold_controllable());
	}
	if (_strip->comp_speed_controllable ()) {
		_strip->comp_speed_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_speed"), _strip->comp_speed_controllable()), OSC::instance());
		change_message (X_("/select/comp_speed"), _strip->comp_speed_controllable());
	}
	if (_strip->comp_mode_controllable ()) {
		_strip->comp_mode_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::comp_mode, this), OSC::instance());
		comp_mode ();
	}
	if (_strip->comp_makeup_controllable ()) {
		_strip->comp_makeup_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_makeup"), _strip->comp_makeup_controllable()), OSC::instance());
		change_message (X_("/select/comp_makeup"), _strip->comp_makeup_controllable());
	}
	renew_sends ();
	renew_plugin ();
	eq_restart(0);
	_init = false;

	tick();
}

void
OSCSelectObserver::set_expand (uint32_t expand)
{
	if (expand != _expand) {
		_expand = expand;
		if (expand) {
			_osc.float_message (X_("/select/expand"), 1.0, addr);
		} else {
			_osc.float_message (X_("/select/expand"), 0.0, addr);
		}
	}
}

void
OSCSelectObserver::clear_observer ()
{
	_init = true;
	strip_connections.drop_connections ();
	// all strip buttons should be off and faders 0 and etc.
	_osc.float_message (X_("/select/expand"), 0, addr);
	_osc.text_message (X_("/select/name"), " ", addr);
	_osc.text_message (X_("/select/group"), " ", addr);
	_osc.text_message (X_("/select/comment"), " ", addr);
	_osc.float_message (X_("/select/mute"), 0, addr);
	_osc.float_message (X_("/select/solo"), 0, addr);
	_osc.float_message (X_("/select/recenable"), 0, addr);
	_osc.float_message (X_("/select/record_safe"), 0, addr);
	_osc.float_message (X_("/select/monitor_input"), 0, addr);
	_osc.float_message (X_("/select/monitor_disk"), 0, addr);
	_osc.float_message (X_("/select/polarity"), 0, addr);
	_osc.float_message (X_("/select/n_inputs"), 0, addr);
	_osc.float_message (X_("/select/n_outputs"), 0, addr);
	_osc.int_message (X_("/select/group/gain"), 0, addr);
	_osc.int_message (X_("/select/group/relative"), 0, addr);
	_osc.int_message (X_("/select/group/mute"), 0, addr);
	_osc.int_message (X_("/select/group/solo"), 0, addr);
	_osc.int_message (X_("/select/group/recenable"), 0, addr);
	_osc.int_message (X_("/select/group/select"), 0, addr);
	_osc.int_message (X_("/select/group/active"), 0, addr);
	_osc.int_message (X_("/select/group/color"), 0, addr);
	_osc.int_message (X_("/select/group/monitoring"), 0, addr);
	_osc.int_message (X_("/select/group/enable"), 0, addr);
	if (gainmode) {
		_osc.float_message (X_("/select/fader"), 0, addr);
	} else {
		_osc.float_message (X_("/select/gain"), -193, addr);
	}
	_osc.float_message (X_("/select/trimdB"), 0, addr);
	_osc.float_message (X_("/select/pan_stereo_position"), 0.5, addr);
	_osc.float_message (X_("/select/pan_stereo_width"), 1, addr);
	if (feedback[9]) {
		_osc.float_message (X_("/select/signal"), 0, addr);
	}
	if (feedback[7]) {
		if (gainmode) {
			_osc.float_message (X_("/select/meter"), 0, addr);
		} else {
			_osc.float_message (X_("/select/meter"), -193, addr);
		}
	}else if (feedback[8]) {
		_osc.float_message (X_("/select/meter"), 0, addr);
	}
	_osc.float_message (X_("/select/pan_elevation_position"), 0, addr);
	_osc.float_message (X_("/select/pan_frontback_position"), .5, addr);
	_osc.float_message (X_("/select/pan_lfe_control"), 0, addr);
	_osc.float_message (X_("/select/comp_enable"), 0, addr);
	_osc.float_message (X_("/select/comp_threshold"), 0, addr);
	_osc.float_message (X_("/select/comp_speed"), 0, addr);
	_osc.float_message (X_("/select/comp_mode"), 0, addr);
	_osc.text_message (X_("/select/comp_mode_name"), " ", addr);
	_osc.text_message (X_("/select/comp_speed_name"), " ", addr);
	_osc.float_message (X_("/select/comp_makeup"), 0, addr);
	_osc.float_message (X_("/select/expand"), 0.0, addr);
	send_end();
	plugin_end();
	eq_end();
}

void
OSCSelectObserver::set_send_page (uint32_t page)
{
	if (send_page != page) {
		send_page = page;
		renew_sends ();
	}
}

void
OSCSelectObserver::set_send_size (uint32_t size)
{
	send_page_size = size;
	renew_sends ();
}

void
OSCSelectObserver::renew_sends () {
	send_connections.drop_connections ();
	send_timeout.clear();
	send_init();
}

void
OSCSelectObserver::send_init()
{
	send_size = nsends;
	if (send_page_size) {
		send_size = send_page_size;
	}
	if (!send_size) {
		return;
	}
	uint32_t page_start = ((send_page - 1) * send_size);
	uint32_t last_send = send_page * send_size;
	uint32_t c = 1;
	send_timeout.push_back (2);
	_last_send.clear();
	_last_send.push_back (0.0);

	for (uint32_t s = page_start; s < last_send; ++s, ++c) {

		bool send_valid = false;
		if (_strip->send_level_controllable (s)) {
			_strip->send_level_controllable(s)->Changed.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_gain, this, c, _strip->send_level_controllable(s)), OSC::instance());
			send_timeout.push_back (2);
			_last_send.push_back (20.0);
			send_gain (c, _strip->send_level_controllable(s));
			send_valid = true;
		} else {
			send_gain (c, _strip->send_level_controllable(s));
			_osc.float_message_with_id (X_("/select/send_enable"), c, 0, in_line, addr);
			_osc.text_message_with_id (X_("/select/send_name"), c, " ", in_line, addr);
		}

		if (_strip->send_enable_controllable (s)) {
			_strip->send_enable_controllable(s)->Changed.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message_with_id, this, X_("/select/send_enable"), c, _strip->send_enable_controllable(s)), OSC::instance());
			enable_message_with_id (X_("/select/send_enable"), c, _strip->send_enable_controllable(s));
		} else if (send_valid) {
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (_strip);
			if (!r) {
				// should never get here
				_osc.float_message_with_id (X_("/select/send_enable"), c, 0, in_line, addr);
			}
			boost::shared_ptr<Send> snd = boost::dynamic_pointer_cast<Send> (r->nth_send(s));
			if (snd) {
				boost::shared_ptr<Processor> proc = boost::dynamic_pointer_cast<Processor> (snd);
				proc->ActiveChanged.connect (send_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_enable, this, X_("/select/send_enable"), c, proc), OSC::instance());
				_osc.float_message_with_id (X_("/select/send_enable"), c, proc->enabled(), in_line, addr);
			}
		}
		if ((gainmode != 1) && send_valid) {
			_osc.text_message_with_id (X_("/select/send_name"), c, _strip->send_name(s), in_line, addr);
		}
	}
}

void
OSCSelectObserver::send_end ()
{
	send_connections.drop_connections ();
	for (uint32_t i = 1; i <= send_size; i++) {
		if (gainmode) {
			_osc.float_message_with_id (X_("/select/send_fader"), i, 0, in_line, addr);
		} else {
			_osc.float_message_with_id (X_("/select/send_gain"), i, -193, in_line, addr);
		}
		// next enable
		_osc.float_message_with_id (X_("/select/send_enable"), i, 0, in_line, addr);
		// next name
		_osc.text_message_with_id (X_("/select/send_name"), i, " ", in_line, addr);
	}
	// need to delete or clear send_timeout
	send_size = 0;
	send_timeout.clear();
}

void
OSCSelectObserver::set_plugin_id (int id, uint32_t page)
{
	plug_id = id;
	plug_page = page;
	renew_plugin ();
}

void
OSCSelectObserver::set_plugin_page (uint32_t page)
{
	plug_page = page;
	renew_plugin ();
}

void
OSCSelectObserver::set_plugin_size (uint32_t size)
{
	plug_page_size = size;
	renew_plugin ();
}

void
OSCSelectObserver::renew_plugin () {
	plugin_connections.drop_connections ();
	plugin_init();
}

void
OSCSelectObserver::plugin_init()
{
	if (plug_id < 0) {
		plugin_end ();
		return;
	}
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(_strip);
	if (!r) {
		plugin_end ();
		return;
	}

	// we have a plugin number now get the processor
	boost::shared_ptr<Processor> proc = r->nth_plugin (plug_id);
	boost::shared_ptr<PluginInsert> pi;
	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(proc))) {
		plugin_end ();
		return;
	}
	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	// we have a plugin we can ask if it is activated
	proc->ActiveChanged.connect (plugin_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::plug_enable, this, X_("/select/plugin/activate"), proc), OSC::instance());
	_osc.float_message (X_("/select/plugin/activate"), proc->enabled(), addr);

	bool ok = false;
	// put only input controls into a vector
	plug_params.clear ();
	uint32_t nplug_params  = pip->parameter_count();
	for ( uint32_t ppi = 0;  ppi < nplug_params; ++ppi) {
		uint32_t controlid = pip->nth_parameter(ppi, ok);
		if (!ok) {
			continue;
		}
		if (pip->parameter_is_input(controlid)) {
			plug_params.push_back (ppi);
		}
	}
	nplug_params = plug_params.size ();

	// default of 0 page size means show all
	plug_size = nplug_params;
	if (plug_page_size) {
		plug_size = plug_page_size;
	}
	_osc.text_message (X_("/select/plugin/name"), pip->name(), addr);
	uint32_t page_start = plug_page - 1;
	uint32_t page_end = page_start + plug_size;

	int pid = 1;
	for ( uint32_t ppi = page_start;  ppi < page_end; ++ppi, ++pid) {
		if (ppi >= nplug_params) {
			_osc.text_message_with_id (X_("/select/plugin/parameter/name"), pid, " ", in_line, addr);
			_osc.float_message_with_id (X_("/select/plugin/parameter"), pid, 0, in_line, addr);
			continue;
		}

		uint32_t controlid = pip->nth_parameter(plug_params[ppi], ok);
		if (!ok) {
			continue;
		}
		ParameterDescriptor pd;
		pip->get_parameter_descriptor(controlid, pd);
		_osc.text_message_with_id (X_("/select/plugin/parameter/name"), pid, pd.label, in_line, addr);
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
OSCSelectObserver::plugin_parameter_changed (int pid, bool swtch, boost::shared_ptr<PBD::Controllable> controllable)
{
	if (swtch) {
		enable_message_with_id (X_("/select/plugin/parameter"), pid, controllable);
	} else {
		change_message_with_id (X_("/select/plugin/parameter"), pid, controllable);
	}
}

void
OSCSelectObserver::plugin_end ()
{
	plugin_connections.drop_connections ();
	_osc.float_message (X_("/select/plugin/activate"), 0, addr);
	_osc.text_message (X_("/select/plugin/name"), " ", addr);
	for (uint32_t i = 1; i <= plug_size; i++) {
		_osc.float_message_with_id (X_("/select/plugin/parameter"), i, 0, in_line, addr);
		// next name
		_osc.text_message_with_id (X_("/select/plugin/parameter/name"), i, " ", in_line, addr);
	}
	plug_size = 0;
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
		if (now_meter < -120) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				string path = X_("/select/meter");
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
				string path = X_("/select/signal");
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
			_osc.text_message (X_("/select/name"), _strip->name(), addr);
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
			_osc.float_message (X_("/select/comp_redux"), new_value, addr);
			_comp_redux = new_value;
		}
	}
	for (uint32_t i = 1; i <= send_timeout.size(); i++) {
		if (send_timeout[i]) {
			if (send_timeout[i] == 1) {
				uint32_t pg_offset = (send_page - 1) * send_page_size;
				_osc.text_message_with_id (X_("/select/send_name"), i, _strip->send_name(pg_offset + i - 1), in_line, addr);
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

	_osc.text_message (X_("/select/name"), _strip->name(), addr);
	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (_strip);
	if (route) {
		// lets tell the surface how many inputs this strip has
		_osc.float_message (X_("/select/n_inputs"), (float) route->n_inputs().n_total(), addr);
		// lets tell the surface how many outputs this strip has
		_osc.float_message (X_("/select/n_outputs"), (float) route->n_outputs().n_total(), addr);
	}
}

void
OSCSelectObserver::panner_changed ()
{
	pan_connections.drop_connections ();

	if (feedback[1]) {

		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
		if (!rt) {
			return;
		}
		boost::shared_ptr<PannerShell> pan_sh =  rt->panner_shell();
		if (pan_sh) {
			string pt = pan_sh->current_panner_uri();
			if (pt.size()){
				string ptype = pt.substr(pt.find_last_of ('/') + 1);
				_osc.text_message (X_("/strip/pan_type"), ptype, addr);
			} else {
				_osc.text_message (X_("/select/pan_type"), "none", addr);
				_osc.float_message (X_("/strip/pan_stereo_position"), 0.5, addr);
				_osc.float_message (X_("/strip/pan_stereo_width"), 1.0, addr);
				return;
			}
			boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_azimuth_control());
			if (pan_controllable) {
				boost::shared_ptr<AutomationControl>at = boost::dynamic_pointer_cast<AutomationControl> (pan_controllable);
				pan_controllable->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_stereo_position"), _strip->pan_azimuth_control()), OSC::instance());
				at->alist()->automation_state_changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_automation, this, X_("/select/pan_stereo_position"), _strip->pan_azimuth_control()), OSC::instance());
				change_message (X_("/select/pan_stereo_position"), _strip->pan_azimuth_control());
				send_automation (X_("/select/pan_stereo_position"), _strip->pan_azimuth_control());
			}

			boost::shared_ptr<Controllable> width_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_width_control());
			if (width_controllable) {
				boost::shared_ptr<AutomationControl>at = boost::dynamic_pointer_cast<AutomationControl> (width_controllable);
				width_controllable->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_stereo_width"), _strip->pan_width_control()), OSC::instance());
				at->alist()->automation_state_changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_automation, this, X_("/select/pan_stereo_width"), _strip->pan_width_control()), OSC::instance());
				change_message (X_("/select/pan_stereo_width"), _strip->pan_width_control());
				send_automation (X_("/select/pan_stereo_width"), _strip->pan_width_control());
			}

			// Rest of possible pan controls... Untested because I can't find a way to get them in the GUI :)
			if (_strip->pan_elevation_control ()) {
				_strip->pan_elevation_control()->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_elevation_position"), _strip->pan_elevation_control()), OSC::instance());
				change_message (X_("/select/pan_elevation_position"), _strip->pan_elevation_control());
			}
			if (_strip->pan_frontback_control ()) {
				_strip->pan_frontback_control()->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_frontback_position"), _strip->pan_frontback_control()), OSC::instance());
				change_message (X_("/select/pan_frontback_position"), _strip->pan_frontback_control());
			}
			if (_strip->pan_lfe_control ()) {
				_strip->pan_lfe_control()->Changed.connect (pan_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_lfe_control"), _strip->pan_lfe_control()), OSC::instance());
				change_message (X_("/select/pan_lfe_control"), _strip->pan_lfe_control());
			}
		} else {
			_osc.text_message (X_("/select/pan_type"), "none", addr);
			_osc.float_message (X_("/strip/pan_stereo_position"), 0.5, addr);
			_osc.float_message (X_("/strip/pan_stereo_width"), 1.0, addr);
		}
	}
}

void
OSCSelectObserver::group_name ()
{
	boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
	RouteGroup *rg = rt->route_group();
	group_sharing (rg);
}

void
OSCSelectObserver::group_sharing (RouteGroup *rgc)
{
	_group_sharing[15] = 1;
	boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
	string new_name = "none";
	RouteGroup* rg = NULL;
	if (rt) {
		rg = rt->route_group();
	}
	if (rg) {
		new_name = rg->name();
		_osc.text_message (X_("/select/group"), new_name, addr);
		_osc.send_group_list (addr);
		if (rg->is_gain () != _group_sharing[0] || _group_sharing[15]) {
			_group_sharing[0] = rg->is_gain ();
			_osc.int_message (X_("/select/group/gain"), _group_sharing[0], addr);
		}
		if (rg->is_relative () != _group_sharing[1] || _group_sharing[15]) {
			_group_sharing[1] = rg->is_relative ();
			_osc.int_message (X_("/select/group/relative"), _group_sharing[1], addr);
		}
		if (rg->is_mute () != _group_sharing[2] || _group_sharing[15]) {
			_group_sharing[2] = rg->is_mute ();
			_osc.int_message (X_("/select/group/mute"), _group_sharing[2], addr);
		}
		if (rg->is_solo () != _group_sharing[3] || _group_sharing[15]) {
			_group_sharing[3] = rg->is_solo ();
			_osc.int_message (X_("/select/group/solo"), _group_sharing[3], addr);
		}
		if (rg->is_recenable () != _group_sharing[4] || _group_sharing[15]) {
			_group_sharing[4] = rg->is_recenable ();
			_osc.int_message (X_("/select/group/recenable"), _group_sharing[4], addr);
		}
		if (rg->is_select () != _group_sharing[5] || _group_sharing[15]) {
			_group_sharing[5] = rg->is_select ();
			_osc.int_message (X_("/select/group/select"), _group_sharing[5], addr);
		}
		if (rg->is_route_active () != _group_sharing[6] || _group_sharing[15]) {
			_group_sharing[6] = rg->is_route_active ();
			_osc.int_message (X_("/select/group/active"), _group_sharing[6], addr);
		}
		if (rg->is_color () != _group_sharing[7] || _group_sharing[15]) {
			_group_sharing[7] = rg->is_color ();
			_osc.int_message (X_("/select/group/color"), _group_sharing[7], addr);
		}
		if (rg->is_monitoring () != _group_sharing[8] || _group_sharing[15]) {
			_group_sharing[8] = rg->is_monitoring ();
			_osc.int_message (X_("/select/group/monitoring"), _group_sharing[8], addr);
		}
		if (rg->is_active () != _group_sharing[9] || _group_sharing[15]) {
			_group_sharing[9] = rg->is_active ();
			_osc.int_message (X_("/select/group/enable"), _group_sharing[9], addr);
		}
	} else {
		_osc.text_message (X_("/select/group"), new_name, addr);
		_osc.int_message (X_("/select/group/gain"), 0, addr);
		_osc.int_message (X_("/select/group/relative"), 0, addr);
		_osc.int_message (X_("/select/group/mute"), 0, addr);
		_osc.int_message (X_("/select/group/solo"), 0, addr);
		_osc.int_message (X_("/select/group/recenable"), 0, addr);
		_osc.int_message (X_("/select/group/select"), 0, addr);
		_osc.int_message (X_("/select/group/active"), 0, addr);
		_osc.int_message (X_("/select/group/color"), 0, addr);
		_osc.int_message (X_("/select/group/monitoring"), 0, addr);
		_osc.int_message (X_("/select/group/enable"), 0, addr);
	}
	_group_sharing[15] = 0;
}

void
OSCSelectObserver::comment_changed ()
{
	boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (_strip);
	if (rt) {
		_osc.text_message (X_("/select/comment"), rt->comment(), addr);
	}
}

void
OSCSelectObserver::pi_changed (PBD::PropertyChange const& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::hidden)) {
		return;
	}
	_osc.float_message (X_("/select/hide"), _strip->is_hidden (), addr);
}

void
OSCSelectObserver::change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();

	_osc.float_message (path, (float) controllable->internal_to_interface (val), addr);
}

void
OSCSelectObserver::send_automation (string path, boost::shared_ptr<PBD::Controllable> control)
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
	_osc.float_message (string_compose (X_("%1/automation"), path), output, addr);
	_osc.text_message (string_compose (X_("%1/automation_name"), path), auto_name, addr);
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
OSCSelectObserver::plug_enable (string path, boost::shared_ptr<Processor> proc)
{
	// with no delay value is wrong
	Glib::usleep(10);

	_osc.float_message (path, proc->enabled(), addr);
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

	_osc.float_message (X_("/select/monitor_input"), (float) input, addr);
	_osc.float_message (X_("/select/monitor_disk"), (float) disk, addr);
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
		_osc.float_message (X_("/select/fader"), _strip->gain_control()->internal_to_interface (value), addr);
		if (gainmode == 1) {
			_osc.text_message (X_("/select/name"), string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (value)), addr);
			gain_timeout = 8;
		}
	}
	if (!gainmode || gainmode == 2) {
		if (value < 1e-15) {
			_osc.float_message (X_("/select/gain"), -200, addr);
		} else {
			_osc.float_message (X_("/select/gain"), accurate_coefficient_to_dB (value), addr);
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
		case ARDOUR::Latch:
			output = 4;
			auto_name = "Latch";
			break;
		default:
			break;
	}

	if (gainmode) {
		_osc.float_message (X_("/select/fader/automation"), output, addr);
		_osc.text_message (X_("/select/fader/automation_name"), auto_name, addr);
	} else {
		_osc.float_message (X_("/select/gain/automation"), output, addr);
		_osc.text_message (X_("/select/gain/automation_name"), auto_name, addr);
	}

	gain_message ();
}

void
OSCSelectObserver::send_gain (uint32_t id, boost::shared_ptr<PBD::Controllable> controllable)
{
	float raw_value = 0.0;
	if (controllable) {
		raw_value = controllable->get_value();
	}
	if (_last_send[id] != raw_value) {
		_last_send[id] = raw_value;
	} else {
		return;
	}
	string path;
	float value = 0.0;
	float db;
	if (raw_value < 1e-15) {
		db = -193;
	} else {
		db = accurate_coefficient_to_dB (raw_value);
	}

	if (gainmode) {
		if (controllable) {
			value = controllable->internal_to_interface (raw_value);
		}
		_osc.float_message_with_id (X_("/select/send_fader"), id, value, in_line, addr);
		if (gainmode == 1) {
			_osc.text_message_with_id (X_("/select/send_name") , id, string_compose ("%1%2%3", std::fixed, std::setprecision(2), db), in_line, addr);
			if (send_timeout.size() > id) {
				send_timeout[id] = 8;
			}
		}
	}
	if (!gainmode || gainmode == 2) {
		_osc.float_message_with_id (X_("/select/send_gain"), id, db, in_line, addr);
	}

}

void
OSCSelectObserver::send_enable (string path, uint32_t id, boost::shared_ptr<Processor> proc)
{
	// with no delay value is wrong
	Glib::usleep(10);

	_osc.float_message_with_id (X_("/select/send_enable"), id, proc->enabled(), in_line, addr);
}

void
OSCSelectObserver::comp_mode ()
{
	change_message (X_("/select/comp_mode"), _strip->comp_mode_controllable());
	_osc.text_message (X_("/select/comp_mode_name"), _strip->comp_mode_name(_strip->comp_mode_controllable()->get_value()), addr);
	_osc.text_message (X_("/select/comp_speed_name"), _strip->comp_speed_name(_strip->comp_mode_controllable()->get_value()), addr);
}

void
OSCSelectObserver::eq_init()
{
	// HPF and enable are special case, rest are in bands
	if (_strip->filter_enable_controllable (true)) {
		_strip->filter_enable_controllable (true)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf/enable"), _strip->filter_enable_controllable (true)), OSC::instance());
		change_message (X_("/select/eq_hpf/enable"), _strip->filter_enable_controllable(true));
	}

	if (_strip->filter_enable_controllable (false)) {
		_strip->filter_enable_controllable (false)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_lpf/enable"), _strip->filter_enable_controllable (false)), OSC::instance());
		change_message (X_("/select/eq_lpf/enable"), _strip->filter_enable_controllable(false));
	}

	if (_strip->filter_freq_controllable (true)) {
		_strip->filter_freq_controllable (true)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf/freq"), _strip->filter_freq_controllable (true)), OSC::instance());
		change_message (X_("/select/eq_hpf/freq"), _strip->filter_freq_controllable(true));
	}

	if (_strip->filter_freq_controllable (false)) {
		_strip->filter_freq_controllable (false)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_lpf/freq"), _strip->filter_freq_controllable (false)), OSC::instance());
		change_message (X_("/select/eq_lpf/freq"), _strip->filter_freq_controllable(false));
	}

	if (_strip->filter_slope_controllable (true)) {
		_strip->filter_slope_controllable (true)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf/slope"), _strip->filter_slope_controllable (true)), OSC::instance());
		change_message (X_("/select/eq_hpf/slope"), _strip->filter_slope_controllable(true));
	}

	if (_strip->filter_slope_controllable (false)) {
		_strip->filter_slope_controllable (false)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_lpf/slope"), _strip->filter_slope_controllable (false)), OSC::instance());
		change_message (X_("/select/eq_lpf/slope"), _strip->filter_slope_controllable(false));
	}

	if (_strip->eq_enable_controllable ()) {
		_strip->eq_enable_controllable ()->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/eq_enable"), _strip->eq_enable_controllable()), OSC::instance());
		enable_message (X_("/select/eq_enable"), _strip->eq_enable_controllable());
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
			_osc.text_message_with_id (X_("/select/eq_band_name"), i + 1, _strip->eq_band_name (i), in_line, addr);
		}
		if (_strip->eq_gain_controllable (i)) {
			_strip->eq_gain_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_gain"), i + 1, _strip->eq_gain_controllable(i)), OSC::instance());
			change_message_with_id (X_("/select/eq_gain"), i + 1, _strip->eq_gain_controllable(i));
		}
		if (_strip->eq_freq_controllable (i)) {
			_strip->eq_freq_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_freq"), i + 1, _strip->eq_freq_controllable(i)), OSC::instance());
			change_message_with_id (X_("/select/eq_freq"), i + 1, _strip->eq_freq_controllable(i));
		}
		if (_strip->eq_q_controllable (i)) {
			_strip->eq_q_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_q"), i + 1, _strip->eq_q_controllable(i)), OSC::instance());
			change_message_with_id (X_("/select/eq_q"), i + 1, _strip->eq_q_controllable(i));
		}
		if (_strip->eq_shape_controllable (i)) {
			_strip->eq_shape_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_shape"), i + 1, _strip->eq_shape_controllable(i)), OSC::instance());
			change_message_with_id (X_("/select/eq_shape"), i + 1, _strip->eq_shape_controllable(i));
		}
	}
}

void
OSCSelectObserver::eq_end ()
{
	eq_connections.drop_connections ();
		_osc.float_message (X_("/select/eq_hpf"), 0, addr);
		_osc.float_message (X_("/select/eq_enable"), 0, addr);

	for (int i = 1; i <= eq_bands; i++) {
		_osc.text_message_with_id (X_("/select/eq_band_name"), i, " ", in_line, addr);
		_osc.float_message_with_id (X_("/select/eq_gain"), i, 0, in_line, addr);
		_osc.float_message_with_id (X_("/select/eq_freq"), i, 0, in_line, addr);
		_osc.float_message_with_id (X_("/select/eq_q"), i, 0, in_line, addr);
		_osc.float_message_with_id (X_("/select/eq_shape"), i, 0, in_line, addr);


	}
}

void
OSCSelectObserver::eq_restart(int x)
{
	eq_connections.drop_connections ();
	//eq_end();
	eq_init();
}

void
OSCSelectObserver::slaved_changed (boost::shared_ptr<VCA> vca, bool state)
{
	lo_message reply;
	reply = lo_message_new ();
	StripableList stripables;
	session->get_stripables (stripables);
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {
		boost::shared_ptr<Stripable> s = *it;

		// we only want VCAs
		boost::shared_ptr<VCA> v = boost::dynamic_pointer_cast<VCA> (s);
		if (v) {
			string name;
			if (_strip->slaved_to (v)) {
				name = string_compose ("%1 [X]", v->name());
			} else {
				name = string_compose ("%1 [_]", v->name());
			}
			lo_message_add_string (reply, name.c_str());
		}
	}
	lo_send_message (addr, X_("/select/vcas"), reply);
	lo_message_free (reply);
}
