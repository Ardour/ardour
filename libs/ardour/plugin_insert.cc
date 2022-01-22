/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2018 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <string>

#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/types_convert.h"

#include "ardour/audio_buffer.h"
#include "ardour/automation_list.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/event_type_map.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/luaproc.h"
#include "ardour/lv2_plugin.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/port.h"

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#endif

#ifdef LXVST_SUPPORT
#include "ardour/lxvst_plugin.h"
#endif

#ifdef MACVST_SUPPORT
#include "ardour/mac_vst_plugin.h"
#endif

#ifdef VST3_SUPPORT
#include "ardour/vst3_plugin.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#endif

#include "ardour/session.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

const string PluginInsert::port_automation_node_name = "PortAutomation";

PluginInsert::PluginInsert (Session& s, Temporal::TimeDomain td, boost::shared_ptr<Plugin> plug)
	: Processor (s, (plug ? plug->name() : string ("toBeRenamed")), td)
	, _sc_playback_latency (0)
	, _sc_capture_latency (0)
	, _plugin_signal_latency (0)
	, _signal_analysis_collect_nsamples (0)
	, _signal_analysis_collect_nsamples_max (0)
	, _configured (false)
	, _no_inplace (false)
	, _strict_io (false)
	, _custom_cfg (false)
	, _maps_from_state (false)
	, _latency_changed (false)
	, _bypass_port (UINT32_MAX)
	, _inverted_bypass_enable (false)
{
	g_atomic_int_set (&_stat_reset, 0);
	g_atomic_int_set (&_flush, 0);

	/* the first is the master */
	if (plug) {
		add_plugin (plug);
		create_automatable_parameters ();
		const ChanCount& sc (sidechain_input_pins ());
		if (sc.n_audio () > 0 || sc.n_midi () > 0) {
			add_sidechain (sc.n_audio (), sc.n_midi ());
		}
	}
}

PluginInsert::~PluginInsert ()
{
	for (CtrlOutMap::const_iterator i = _control_outputs.begin(); i != _control_outputs.end(); ++i) {
		boost::dynamic_pointer_cast<ReadOnlyControl>(i->second)->drop_references ();
	}
}

void
PluginInsert::drop_references ()
{
	if (!_impulseAnalysisPlugin.expired()) {
		_impulseAnalysisPlugin.lock()->drop_references ();
	}
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->drop_references ();
	}

	/* PluginInsert::_plugins must exist until PBD::Controllable
	 * has emitted drop_references. This is because
	 * AC::get_value() calls _plugin[0]->get_parameter(..)
	 *
	 * Usually ~Automatable, calls drop_references for each
	 * controllable, but that runs after ~PluginInsert.
	 */
	{
		Glib::Threads::Mutex::Lock lm (_control_lock);
		for (Controls::const_iterator li = _controls.begin(); li != _controls.end(); ++li) {
			boost::dynamic_pointer_cast<AutomationControl>(li->second)->drop_references ();
		}
		_controls.clear ();
	}
	Processor::drop_references ();
}

void
PluginInsert::set_strict_io (bool b)
{
	if (!_plugins.empty() && _plugins.front()->connect_all_audio_outputs ()) {
		/* Ignore route setting, allow plugin to add/remove ports */
		b = false;
	}

	bool changed = _strict_io != b;
	_strict_io = b;

	if (changed) {
		PluginConfigChanged (); /* EMIT SIGNAL */
	}
}

bool
PluginInsert::set_count (uint32_t num)
{
	bool require_state = !_plugins.empty();

	if (require_state && num > 1 && plugin (0)->get_info ()->type == ARDOUR::AudioUnit) {
		// we don't allow to replicate AUs
		return false;
	}

	/* this is a bad idea.... we shouldn't do this while active.
	 * only a route holding their redirect_lock should be calling this
	 */

	if (num == 0) {
		return false;
	} else if (num > _plugins.size()) {
		uint32_t diff = num - _plugins.size();

		for (uint32_t n = 0; n < diff; ++n) {
			boost::shared_ptr<Plugin> p = plugin_factory (_plugins[0]);
			add_plugin (p);

			if (require_state) {
				XMLNode& state = _plugins[0]->get_state ();
				p->set_state (state, Stateful::loading_state_version);
			}

			if (active ()) {
				p->activate ();
			}
		}
		PluginConfigChanged (); /* EMIT SIGNAL */

	} else if (num < _plugins.size()) {
		uint32_t diff = _plugins.size() - num;
		for (uint32_t n= 0; n < diff; ++n) {
			_plugins.back()->drop_references ();
			_plugins.pop_back();
		}
		PluginConfigChanged (); /* EMIT SIGNAL */
	}

	return true;
}


void
PluginInsert::set_sinks (const ChanCount& c)
{
	_custom_sinks = c;
	/* no signal, change will only be visible after re-config */
}

void
PluginInsert::set_outputs (const ChanCount& c)
{
	bool changed = (_custom_out != c) && _custom_cfg;
	_custom_out = c;
	if (changed) {
		PluginConfigChanged (); /* EMIT SIGNAL */
	}
}

void
PluginInsert::set_custom_cfg (bool b)
{
	bool changed = _custom_cfg != b;
	_custom_cfg = b;
	if (changed) {
		PluginConfigChanged (); /* EMIT SIGNAL */
	}
}

bool
PluginInsert::set_preset_out (const ChanCount& c)
{
	bool changed = _preset_out != c;
	_preset_out = c;
	if (changed && !_custom_cfg) {
		PluginConfigChanged (); /* EMIT SIGNAL */
	}
	return changed;
}

bool
PluginInsert::add_sidechain (uint32_t n_audio, uint32_t n_midi)
{
	/* Caller must not hold process lock, since add_port() takes the lock.
	 *
	 * Since the SC adds a port, an additional buffer may be needed.
	 * So Route::configure_processors() has to be called to set
	 * processor_max_streams -> _session.ensure_buffers ().
	 * SideChain::run () will do nothing before
	 * _sidechain->configure_io () is called.
	 */
	if (_sidechain) {
		return false;
	}
	std::ostringstream n;
	if (n_audio == 0 && n_midi == 0) {
		n << "TO BE RESET FROM XML";
	} else if (owner()) {
		n << "SC " << owner()->name() << "/" << name() << " " << Session::next_name_id ();
	} else {
		n << "toBeRenamed" << id().to_s();
	}
	_sidechain.reset (new SideChain (_session, n.str ()));
	_sidechain->activate ();
	for (uint32_t n = 0; n < n_audio; ++n) {
		_sidechain->input()->add_port ("", owner(), DataType::AUDIO); // add a port, don't connect.
	}
	for (uint32_t n = 0; n < n_midi; ++n) {
		_sidechain->input()->add_port ("", owner(), DataType::MIDI); // add a port, don't connect.
	}
	PluginConfigChanged (); /* EMIT SIGNAL */
	return true;
}

bool
PluginInsert::del_sidechain ()
{
	if (!_sidechain) {
		return false;
	}
	_sidechain.reset ();
	_sc_playback_latency = 0;
	_sc_capture_latency = 0;
	PluginConfigChanged (); /* EMIT SIGNAL */
	return true;
}

void
PluginInsert::update_sidechain_name ()
{
	if (!_sidechain) {
		return;
	}

	std::ostringstream n;

	n << "SC ";
	if (owner()) {
		n << owner()->name() << "/";
	}

	n << name() << " " << Session::next_name_id ();

	_sidechain->set_name (n.str());
}

void
PluginInsert::control_list_automation_state_changed (Evoral::Parameter which, AutoState s)
{
	if (which.type() != PluginAutomation)
		return;

	boost::shared_ptr<AutomationControl> c
			= boost::dynamic_pointer_cast<AutomationControl>(control (which));

	if (c && s != Off) {
		_plugins[0]->set_parameter (which.id(), c->list()->eval (timepos_t (_session.transport_sample())), 0);
	}
}

ChanCount
PluginInsert::output_streams() const
{
	assert (_configured);
	return _configured_out;
}

ChanCount
PluginInsert::input_streams() const
{
	assert (_configured);
	return _configured_in;
}

ChanCount
PluginInsert::internal_streams() const
{
	assert (_configured);
	return _configured_internal;
}

ChanCount
PluginInsert::internal_output_streams() const
{
	assert (!_plugins.empty());

	PluginInfoPtr info = _plugins.front()->get_info();

	if (info->reconfigurable_io()) {
		ChanCount out = _plugins.front()->output_streams ();
		// DEBUG_TRACE (DEBUG::Processors, string_compose ("Plugin insert, reconfigur(able) output streams = %1\n", out));
		return out;
	} else {
		ChanCount out = info->n_outputs;
		// DEBUG_TRACE (DEBUG::Processors, string_compose ("Plugin insert, static output streams = %1 for %2 plugins\n", out, _plugins.size()));
		out.set_audio (out.n_audio() * _plugins.size());
		out.set_midi (out.n_midi() * _plugins.size());
		return out;
	}
}

ChanCount
PluginInsert::internal_input_streams() const
{
	assert (!_plugins.empty());

	ChanCount in;

	PluginInfoPtr info = _plugins.front()->get_info();

	if (info->reconfigurable_io()) {
		in = _plugins.front()->input_streams();
	} else {
		in = info->n_inputs;
	}

	DEBUG_TRACE (DEBUG::Processors, string_compose ("Plugin insert, input streams = %1, match using %2\n", in, _match.method));

	if (_match.method == Split) {

		/* we are splitting 1 processor input to multiple plugin inputs,
		   so we have a maximum of 1 stream of each type.
		*/
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (in.get (*t) > 1) {
				in.set (*t, 1);
			}
		}
		return in;

	} else if (_match.method == Hide) {

		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			in.set (*t, in.get (*t) - _match.hide.get (*t));
		}
		return in;

	} else {

		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			in.set (*t, in.get (*t) * _plugins.size ());
		}

		return in;
	}
}

ChanCount
PluginInsert::natural_output_streams() const
{
#ifdef MIXBUS
	if (is_channelstrip ()) {
		return ChanCount::min (_configured_out, ChanCount (DataType::AUDIO, 2));
	}
#endif
	return _plugins[0]->get_info()->n_outputs;
}

ChanCount
PluginInsert::natural_input_streams() const
{
#ifdef MIXBUS
	if (is_channelstrip ()) {
		return ChanCount::min (_configured_in, ChanCount (DataType::AUDIO, 2));
	}
#endif
	return _plugins[0]->get_info()->n_inputs;
}

ChanCount
PluginInsert::sidechain_input_pins() const
{
	return _cached_sidechain_pins;
}

bool
PluginInsert::has_no_inputs() const
{
	return _plugins[0]->get_info()->n_inputs == ChanCount::ZERO;
}

bool
PluginInsert::has_no_audio_inputs() const
{
	return _plugins[0]->get_info()->n_inputs.n_audio() == 0;
}

samplecnt_t
PluginInsert::plugin_latency () const {
	return _plugins.front()->signal_latency ();
}

bool
PluginInsert::is_instrument() const
{
	PluginInfoPtr pip = _plugins[0]->get_info();
	return (pip->is_instrument ());
}

bool
PluginInsert::has_output_presets (ChanCount in, ChanCount out)
{
	if (!_configured && _plugins[0]->get_info ()->reconfigurable_io ()) {
		/* collect possible configurations, prefer given in/out */
		ChanCount aux_in;
		_plugins[0]->match_variable_io (in, aux_in, out);
	}

	PluginOutputConfiguration ppc (_plugins[0]->possible_output ());

	if (ppc.size () == 0) {
		return false;
	}
	if (!strict_io () && ppc.size () == 1) {
		return false;
	}

	if (strict_io () && ppc.size () == 1) {
		// "stereo" is currently preferred default for instruments
		if (ppc.find (2) != ppc.end ()) {
			return false;
		}
	}

	if (ppc.size () == 1 && ppc.find (0) != ppc.end () && !_plugins[0]->get_info ()->reconfigurable_io ()) {
		// some midi-sequencer (e.g. QMidiArp) or other midi-out plugin
		// pretending to be an "Instrument"
		return false;
	}

	if (!is_instrument ()) {
			return false;
	}
	return true;
}

void
PluginInsert::create_automatable_parameters ()
{
	assert (!_plugins.empty());

	boost::shared_ptr<Plugin> plugin = _plugins.front();
	set<Evoral::Parameter> a = _plugins.front()->automatable ();

	const uint32_t limit_automatables = Config->get_limit_n_automatables ();

	for (uint32_t i = 0; i < plugin->parameter_count(); ++i) {
		if (!plugin->parameter_is_control (i)) {
			continue;
		}

		ParameterDescriptor desc;
		plugin->get_parameter_descriptor(i, desc);

		if (!plugin->parameter_is_input (i)) {
			_control_outputs[i] = boost::shared_ptr<ReadOnlyControl> (new ReadOnlyControl (plugin, desc, i));
			continue;
		}
		Evoral::Parameter param (PluginAutomation, 0, i);

		const bool automatable = a.find(param) != a.end();

		boost::shared_ptr<AutomationList> list(new AutomationList(param, desc, time_domain()));
		boost::shared_ptr<AutomationControl> c (new PluginControl(this, param, desc, list));
		if (!automatable || (limit_automatables > 0 && what_can_be_automated ().size() > limit_automatables)) {
			c->set_flag (Controllable::NotAutomatable);
		}
		if (desc.inline_ctrl) {
			c->set_flag (Controllable::InlineControl);
		}
		add_control (c);
		plugin->set_automation_control (i, c);
	}


	const Plugin::PropertyDescriptors& pdl (plugin->get_supported_properties ());
	for (Plugin::PropertyDescriptors::const_iterator p = pdl.begin(); p != pdl.end(); ++p) {
		Evoral::Parameter param (PluginPropertyAutomation, 0, p->first);
		const ParameterDescriptor& desc = plugin->get_property_descriptor(param.id());
		if (desc.datatype != Variant::NOTHING) {
			boost::shared_ptr<AutomationList> list;
			if (Variant::type_is_numeric(desc.datatype)) {
				list = boost::shared_ptr<AutomationList>(new AutomationList(param, desc, time_domain()));
			}
			boost::shared_ptr<AutomationControl> c (new PluginPropertyControl(this, param, desc, list));
			if (!Variant::type_is_numeric(desc.datatype)) {
				c->set_flag (Controllable::NotAutomatable);
			}
			add_control (c);
		}
	}

	_bypass_port = plugin->designated_bypass_port ();

	/* special case VST effSetBypass */
	if (_bypass_port == UINT32_MAX -1) {
		// emulate VST Bypass
		Evoral::Parameter param (PluginAutomation, 0, _bypass_port);
		ParameterDescriptor desc;
		desc.label = _("Plugin Enable");
		desc.toggled  = true;
		desc.normal = 1;
		desc.lower  = 0;
		desc.upper  = 1;

		boost::shared_ptr<AutomationList> list(new AutomationList(param, desc, time_domain()));
		boost::shared_ptr<AutomationControl> c (new PluginControl(this, param, desc, list));

		add_control (c);
	}

	if (_bypass_port != UINT32_MAX) {
		_inverted_bypass_enable = type () == VST3;
		boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, _bypass_port));
		if (0 == (ac->flags () & Controllable::NotAutomatable)) {
			ac->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&PluginInsert::bypassable_changed, this));
			ac->Changed.connect_same_thread (*this, boost::bind (&PluginInsert::enable_changed, this));
		}
	}
	plugin->PresetPortSetValue.connect_same_thread (*this, boost::bind (&PluginInsert::preset_load_set_value, this, _1, _2));
}

/** Called when something outside of this host has modified a plugin
 * parameter. Responsible for propagating the change to two places:
 *
 *   1) anything listening to the Control itself
 *   2) any replicated plugins that make up this PluginInsert.
 *
 * The PluginInsert is connected to the ParameterChangedExternally signal for
 * the first (primary) plugin, and here broadcasts that change to any others.
 *
 * XXX We should probably drop this whole replication idea (Paul, October 2015)
 * since it isn't used by sensible plugin APIs (AU, LV2).
 */
void
PluginInsert::parameter_changed_externally (uint32_t which, float val)
{
	boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, which));

	/* First propagation: alter the underlying value of the control,
	 * without telling the plugin(s) that own/use it to set it.
	 */

	if (!ac) {
		return;
	}

	boost::shared_ptr<PluginControl> pc = boost::dynamic_pointer_cast<PluginControl> (ac);

	if (pc) {
		pc->catch_up_with_external_value (val);
	}

	/* Second propagation: tell all plugins except the first to
	   update the value of this parameter. For sane plugin APIs,
	   there are no other plugins, so this is a no-op in those
	   cases.
	*/

	Plugins::iterator i = _plugins.begin();

	/* don't set the first plugin, just all the slaves */

	if (i != _plugins.end()) {
		++i;
		for (; i != _plugins.end(); ++i) {
			(*i)->set_parameter (which, val, 0);
		}
	}
	boost::shared_ptr<Plugin> iasp = _impulseAnalysisPlugin.lock();
	if (iasp) {
		iasp->set_parameter (which, val, 0);
	}
}

int
PluginInsert::set_block_size (pframes_t nframes)
{
	int ret = 0;
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		if ((*i)->set_block_size (nframes) != 0) {
			ret = -1;
		}
	}
	return ret;
}

void
PluginInsert::automation_run (samplepos_t start, pframes_t nframes, bool only_active)
{
	// XXX does not work when rolling backwards
	if (_loop_location && nframes > 0) {
		const samplepos_t loop_start = _loop_location->start_sample ();
		const samplepos_t loop_end   = _loop_location->end_sample ();
		const samplecnt_t looplen    = loop_end - loop_start;

		samplecnt_t remain = nframes;
		samplepos_t start_pos = start;

		while (remain > 0) {
			if (start_pos >= loop_end) {
				sampleoffset_t start_off = (start_pos - loop_start) % looplen;
				start_pos = loop_start + start_off;
			}
			samplecnt_t move = std::min ((samplecnt_t)nframes, loop_end - start_pos);

			Automatable::automation_run (start_pos, move, only_active);
			remain -= move;
			start_pos += move;
		}
		return;
	}
	Automatable::automation_run (start, nframes, only_active);
}

bool
PluginInsert::find_next_event (timepos_t const & now, timepos_t const & end, Evoral::ControlEvent& next_event, bool only_active) const
{
	bool rv = Automatable::find_next_event (now, end, next_event, only_active);

	if (_loop_location && now < end) {
		const timepos_t loop_end = _loop_location->end ();
		assert (now < loop_end); // due to map_loop_range ()
		if (end > loop_end) {
			next_event.when = loop_end;
			rv = true;
		}
	}
	return rv;
}

void
PluginInsert::activate ()
{
	_timing_stats.reset ();
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->activate ();
	}

	Processor::activate ();
	/* when setting state e.g ProcessorBox::paste_processor_state ()
	 * the plugin is not yet owned by a route.
	 * but no matter.  Route::add_processors() will call activate () again
	 */
	if (!owner ()) {
		return;
	}

	const samplecnt_t l = effective_latency ();
	if (_plugin_signal_latency != l) {
		_plugin_signal_latency = l;
		latency_changed ();
	}
}

void
PluginInsert::deactivate ()
{
	_timing_stats.reset ();
	Processor::deactivate ();

	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->deactivate ();
	}

	const samplecnt_t l = effective_latency ();
	if (_plugin_signal_latency != l) {
		_plugin_signal_latency = l;
		latency_changed ();
	}
}

void
PluginInsert::flush ()
{
	g_atomic_int_set (&_flush, 1);
}

void
PluginInsert::enable (bool yn)
{
	if (_bypass_port == UINT32_MAX) {
		if (yn) {
			activate ();
		} else {
			deactivate ();
		}
	} else {
		if (!_pending_active) {
			activate ();
		}
		boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, _bypass_port));
		const double val = yn ^ _inverted_bypass_enable ? 1.0 : 0.0;
		ac->set_value (val, Controllable::NoGroup);

#ifdef ALLOW_VST_BYPASS_TO_FAIL // yet unused, see also vst_plugin.cc
		/* special case VST.. bypass may fail */
		if (_bypass_port == UINT32_MAX - 1) {
			/* check if bypass worked */
			if (ac->get_value () != val) {
				warning << _("PluginInsert: VST Bypass failed, falling back to host bypass.") << endmsg;
				// set plugin to enabled (not-byassed)
				ac->set_value (1.0, Controllable::NoGroup);
				// ..and use host-provided hard-bypass
				if (yn) {
					activate ();
				} else {
					deactivate ();
				}
				return;
			}
		}
#endif
		ActiveChanged ();
	}
}

bool
PluginInsert::enabled () const
{
	if (_bypass_port == UINT32_MAX) {
		return Processor::enabled ();
	} else {
		boost::shared_ptr<const AutomationControl> ac = boost::const_pointer_cast<AutomationControl> (automation_control (Evoral::Parameter (PluginAutomation, 0, _bypass_port)));
		return ((ac->get_value () > 0) ^ _inverted_bypass_enable) && _pending_active;
	}
}

bool
PluginInsert::bypassable () const
{
	if (_bypass_port == UINT32_MAX) {
		return true;
	} else {
		boost::shared_ptr<const AutomationControl> ac = boost::const_pointer_cast<AutomationControl> (automation_control (Evoral::Parameter (PluginAutomation, 0, _bypass_port)));

		return !ac->automation_playback ();
	}
}

void
PluginInsert::enable_changed ()
{
	ActiveChanged ();
}

void
PluginInsert::bypassable_changed ()
{
	BypassableChanged ();
}

bool
PluginInsert::write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf)
{
	bool rv = true;
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		if (!(*i)->write_immediate_event (event_type, size, buf)) {
			rv = false;
		}
	}
	return rv;
}

void
PluginInsert::preset_load_set_value (uint32_t p, float v)
{
	boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter(PluginAutomation, 0, p));
	if (!ac) {
		return;
	}

	if (ac->automation_state() & Play) {
		return;
	}

	start_touch (p);
	ac->set_value (v, Controllable::NoGroup);
	end_touch (p);
}

void
PluginInsert::inplace_silence_unconnected (BufferSet& bufs, const PinMappings& out_map, samplecnt_t nframes, samplecnt_t offset) const
{
	// TODO optimize: store "unconnected" in a fixed set.
	// it only changes on reconfiguration.
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (uint32_t out = 0; out < bufs.count().get (*t); ++out) {
			bool mapped = false;
			if (*t == DataType::MIDI && out == 0 && has_midi_bypass ()) {
				mapped = true; // in-place Midi bypass
			}
			for (uint32_t pc = 0; pc < get_count() && !mapped; ++pc) {
				PinMappings::const_iterator i = out_map.find (pc);
				if (i == out_map.end ()) {
					continue;
				}
				const ChanMapping& outmap (i->second);
				for (uint32_t o = 0; o < natural_output_streams().get (*t); ++o) {
					bool valid;
					uint32_t idx = outmap.get (*t, o, &valid);
					if (valid && idx == out) {
						mapped = true;
						break;
					}
				}
			}
			if (!mapped) {
				bufs.get_available (*t, out).silence (nframes, offset);
			}
		}
	}
}

void
PluginInsert::connect_and_run (BufferSet& bufs, samplepos_t start, samplepos_t end, double speed, pframes_t nframes, samplecnt_t offset, bool with_auto)
{
	// TODO: atomically copy maps & _no_inplace
	const bool no_inplace = _no_inplace;
	PinMappings in_map (_in_map); // TODO Split case below overrides, use const& in_map
	PinMappings const& out_map (_out_map);
	ChanMapping const& thru_map (_thru_map);

	if (_latency_changed) {
		/* delaylines are configured with the max possible latency (as reported by the plugin)
		 * so this won't allocate memory (unless the plugin lied about its max latency)
		 * It may still 'click' though, since the fixed delaylines are not de-clicked.
		 * Then again plugin-latency changes are not click-free to begin with.
		 *
		 * This is also worst case, there is currently no concept of per-stream latency.
		 *
		 * e.g.  Two identical latent plugins:
		 *   1st plugin: process left (latent), bypass right.
		 *   2nd plugin: bypass left, process right (latent).
		 * -> currently this yields 2 times latency of the plugin,
		 */
		_latency_changed = false;
		_delaybuffers.set (ChanCount::max(bufs.count(), _configured_out), plugin_latency ());
	}

	if (_match.method == Split && !no_inplace) {
		// TODO: also use this optimization if one source-buffer
		// feeds _all_ *connected* inputs.
		// currently this is *first* buffer to all only --
		// see PluginInsert::check_inplace
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_configured_internal.get (*t) == 0) {
				continue;
			}
			bool valid;
			uint32_t first_idx = in_map.p(0).get (*t, 0, &valid);
			assert (valid && first_idx == 0); // check_inplace ensures this
			/* copy the first stream's buffer contents to the others */
			for (uint32_t i = 1; i < natural_input_streams ().get (*t); ++i) {
				uint32_t idx = in_map.p(0).get (*t, i, &valid);
				if (valid) {
					assert (idx == 0);
					bufs.get_available (*t, i).read_from (bufs.get_available (*t, first_idx), nframes, offset, offset);
				}
			}
		}
		/* the copy operation produces a linear monotonic input map */
		in_map[0] = ChanMapping (natural_input_streams ());
	}

	bufs.set_count(ChanCount::max(bufs.count(), _configured_internal));
	bufs.set_count(ChanCount::max(bufs.count(), _configured_out));

	if (with_auto) {
		boost::shared_ptr<ControlList> cl = _automated_controls.reader ();
		for (ControlList::const_iterator ci = cl->begin(); ci != cl->end(); ++ci) {
			AutomationControl& c = *(ci->get());
			boost::shared_ptr<const Evoral::ControlList> clist (c.list());
			/* we still need to check for Touch and Latch */
			if (clist && (static_cast<AutomationList const&> (*clist)).automation_playback ()) {
				/* 1. Set value at [sub]cycle start */
				bool valid;
				float val = c.list()->rt_safe_eval (timepos_t (start), valid);

				if (valid) {
					c.set_value_unchecked(val);
				}

				if (_plugins.front()->get_info ()->type != ARDOUR::VST3) {
					continue;
				}

#if 1
				/* 2. VST3: events between now and end. */
				timepos_t start_time (start);
				timepos_t now (start_time);
				while (true) {
					timepos_t end_time (end);
					Evoral::ControlEvent next_event (end_time, 0.0f);
					find_next_ac_event (*ci, now, end_time, next_event);
					if (next_event.when >= end_time) {
						break;
					}
					now = next_event.when;
					const float val = c.list()->rt_safe_eval (now, valid);
					if (valid) {
						for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
							(*i)->set_parameter (clist->parameter().id(), val, now.samples() - start);
						}
					}
				}
#endif
#if 1
				/* 3. VST3: set value at cycle-end */
				val = c.list()->rt_safe_eval (timepos_t (end), valid);
				if (valid) {
					for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
						(*i)->set_parameter (clist->parameter().id(), val, end - start);
					}
				}
#endif
			}
		}
	}

	if (_signal_analysis_collect_nsamples_max > 0) {
		if (_signal_analysis_collect_nsamples < _signal_analysis_collect_nsamples_max) {
			samplecnt_t ns = std::min ((samplecnt_t) nframes, _signal_analysis_collect_nsamples_max - _signal_analysis_collect_nsamples);
			_signal_analysis_inputs.set_count (ChanCount (DataType::AUDIO, input_streams().n_audio()));

			for (uint32_t i = 0; i < input_streams().n_audio(); ++i) {
				_signal_analysis_inputs.get_audio(i).read_from (
						bufs.get_audio(i),
						ns,
						_signal_analysis_collect_nsamples);
			}
		}
		_signal_analysis_collect_nsamples += nframes;
	}

#ifdef MIXBUS
	if (is_channelstrip ()) {
		if (_configured_in.n_audio() > 0) {
			ChanMapping mb_in_map (ChanCount::min (_configured_in, ChanCount (DataType::AUDIO, 2)));
			ChanMapping mb_out_map (ChanCount::min (_configured_out, ChanCount (DataType::AUDIO, 2)));

			_plugins.front()->connect_and_run (bufs, start, end, speed, mb_in_map, mb_out_map, nframes, offset);

			for (uint32_t out = _configured_in.n_audio (); out < bufs.count().get (DataType::AUDIO); ++out) {
				bufs.get_available (DataType::AUDIO, out).silence (nframes, offset);
			}
		}
	} else
#endif
	if (no_inplace) {
		// TODO optimize -- build maps once.
		uint32_t pc = 0;
		BufferSet& inplace_bufs  = _session.get_noinplace_buffers();
		ARDOUR::ChanMapping used_outputs;

		assert (inplace_bufs.count () >= natural_input_streams () + _configured_out);

		/* build used-output map */
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				for (uint32_t out = 0; out < natural_output_streams().get (*t); ++out) {
					bool valid;
					uint32_t out_idx = out_map.p(pc).get (*t, out, &valid);
					if (valid) {
						used_outputs.set (*t, out_idx, 1); // mark as used
					}
				}
			}
		}
		/* copy thru data to outputs before processing in-place */
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < bufs.count().get (*t); ++out) {
				bool valid;
				uint32_t in_idx = thru_map.get (*t, out, &valid);
				uint32_t m = out + natural_input_streams ().get (*t);
				if (valid) {
					_delaybuffers.delay (*t, out, inplace_bufs.get_available (*t, m), bufs.get_available (*t, in_idx), nframes, offset, offset);
					used_outputs.set (*t, out, 1); // mark as used
				} else {
					used_outputs.get (*t, out, &valid);
					if (valid) {
						/* the plugin is expected to write here, but may not :(
						 * (e.g. drumgizmo w/o kit loaded)
						 */
						inplace_bufs.get_available (*t, m).silence (nframes);
					}
				}
			}
		}

		pc = 0;
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {

			ARDOUR::ChanMapping i_in_map (natural_input_streams());
			ARDOUR::ChanMapping i_out_map (out_map.p(pc));
			ARDOUR::ChanCount mapped;

			/* map inputs sequentially */
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				for (uint32_t in = 0; in < natural_input_streams().get (*t); ++in) {
					bool valid;
					uint32_t in_idx = in_map.p(pc).get (*t, in, &valid);
					uint32_t m = mapped.get (*t);
					if (valid) {
						inplace_bufs.get_available (*t, m).read_from (bufs.get_available (*t, in_idx), nframes, offset, offset);
					} else {
						inplace_bufs.get_available (*t, m).silence (nframes, offset);
					}
					mapped.set (*t, m + 1);
				}
			}

			/* outputs are mapped to inplace_bufs after the inputs */
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				i_out_map.offset_to (*t, natural_input_streams ().get (*t));
			}

			if ((*i)->connect_and_run (inplace_bufs, start, end, speed, i_in_map, i_out_map, nframes, offset)) {
				deactivate ();
			}
		}

		/* all instances have completed, now copy data that was written
		 * and zero unconnected buffers */
		ARDOUR::ChanMapping nonzero_out (used_outputs);
		if (has_midi_bypass ()) {
			nonzero_out.set (DataType::MIDI, 0, 1); // Midi bypass.
		}
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < bufs.count().get (*t); ++out) {
				bool valid;
				used_outputs.get (*t, out, &valid);
				if (!valid) {
					nonzero_out.get (*t, out, &valid);
					if (!valid) {
						bufs.get_available (*t, out).silence (nframes, offset);
					}
				} else {
					uint32_t m = out + natural_input_streams ().get (*t);
					bufs.get_available (*t, out).read_from (inplace_bufs.get_available (*t, m), nframes, offset, offset);
				}
			}
		}
	} else {
		/* in-place processing */
		uint32_t pc = 0;
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			if ((*i)->connect_and_run(bufs, start, end, speed, in_map.p(pc), out_map.p(pc), nframes, offset)) {
				deactivate ();
			}
		}
		// now silence unconnected outputs
		inplace_silence_unconnected (bufs, _out_map, nframes, offset);
	}

	const samplecnt_t l = effective_latency ();
	if (_plugin_signal_latency != l) {
		_plugin_signal_latency = l;
		_signal_analysis_collect_nsamples = 0;
		latency_changed ();
	}

	if (_signal_analysis_collect_nsamples > l) {
		assert (_signal_analysis_collect_nsamples_max > 0);
		assert (_signal_analysis_collect_nsamples >= nframes);
		samplecnt_t sample_pos = _signal_analysis_collect_nsamples - nframes;

		samplecnt_t dst_off = sample_pos >= l ? sample_pos - l : 0;
		samplecnt_t src_off = sample_pos >= l ? 0 : l - sample_pos;
		samplecnt_t n_copy = std::min ((samplecnt_t)nframes, _signal_analysis_collect_nsamples - l);
		n_copy = std::min (n_copy, _signal_analysis_collect_nsamples_max - dst_off);

		_signal_analysis_outputs.set_count (ChanCount (DataType::AUDIO, output_streams().n_audio()));

		for (uint32_t i = 0; i < output_streams().n_audio(); ++i) {
			_signal_analysis_outputs.get_audio(i).read_from(
				bufs.get_audio(i), n_copy, dst_off, src_off);
		}

		if (dst_off + n_copy == _signal_analysis_collect_nsamples_max) {
			_signal_analysis_collect_nsamples_max = 0;
			_signal_analysis_collect_nsamples     = 0;

			AnalysisDataGathered (&_signal_analysis_inputs, &_signal_analysis_outputs); /* EMIT SIGNAL */
		}
	}
}

void
PluginInsert::bypass (BufferSet& bufs, pframes_t nframes)
{
	/* bypass the plugin(s) not the whole processor.
	 * -> use mappings just like connect_and_run
	 */
	// TODO: atomically copy maps & _no_inplace
	const bool no_inplace = _no_inplace;
	ChanMapping const& in_map (no_sc_input_map ());
	ChanMapping const& out_map (output_map ());

	bufs.set_count(ChanCount::max(bufs.count(), _configured_internal));
	bufs.set_count(ChanCount::max(bufs.count(), _configured_out));

	if (no_inplace) {
		ChanMapping thru_map (_thru_map);

		BufferSet& inplace_bufs  = _session.get_noinplace_buffers();
		// copy all inputs
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t in = 0; in < _configured_internal.get (*t); ++in) {
				inplace_bufs.get_available (*t, in).read_from (bufs.get_available (*t, in), nframes, 0, 0);
			}
		}
		ARDOUR::ChanMapping used_outputs;
		// copy thru
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < _configured_out.get (*t); ++out) {
				bool valid;
				uint32_t in_idx = thru_map.get (*t, out, &valid);
				if (valid) {
					bufs.get_available (*t, out).read_from (inplace_bufs.get_available (*t, in_idx), nframes, 0, 0);
					used_outputs.set (*t, out, 1); // mark as used
				}
			}
		}
		// plugin no-op: assume every plugin has an internal identity map
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < _configured_out.get (*t); ++out) {
				bool valid;
				uint32_t src_idx = out_map.get_src (*t, out, &valid);
				if (!valid) {
					continue;
				}
				uint32_t in_idx = in_map.get (*t, src_idx, &valid);
				if (!valid) {
					continue;
				}
				bufs.get_available (*t, out).read_from (inplace_bufs.get_available (*t, in_idx), nframes, 0, 0);
				used_outputs.set (*t, out, 1); // mark as used
			}
		}
		// now silence all unused outputs
		if (has_midi_bypass ()) {
			used_outputs.set (DataType::MIDI, 0, 1); // Midi bypass.
		}
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < _configured_out.get (*t); ++out) {
				bool valid;
				used_outputs.get (*t, out, &valid);
				if (!valid) {
						bufs.get_available (*t, out).silence (nframes, 0);
				}
			}
		}
	} else {
		if (_match.method == Split) {
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				if (_configured_internal.get (*t) == 0) {
					continue;
				}
				// copy/feeds _all_ *connected* inputs, copy the first buffer
				bool valid;
				uint32_t first_idx = in_map.get (*t, 0, &valid);
				assert (valid && first_idx == 0); // check_inplace ensures this
				for (uint32_t i = 1; i < natural_input_streams ().get (*t); ++i) {
					uint32_t idx = in_map.get (*t, i, &valid);
					if (valid) {
						assert (idx == 0);
						bufs.get_available (*t, i).read_from (bufs.get_available (*t, first_idx), nframes, 0, 0);
					}
				}
			}
		}

		// apply output map and/or monotonic but not identity i/o mappings
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < _configured_out.get (*t); ++out) {
				bool valid;
				uint32_t src_idx = out_map.get_src (*t, out, &valid);
				if (!valid) {
					bufs.get_available (*t, out).silence (nframes, 0);
					continue;
				}
				uint32_t in_idx = in_map.get (*t, src_idx, &valid);
				if (!valid) {
					bufs.get_available (*t, out).silence (nframes, 0);
					continue;
				}
				if (in_idx != out) {
					bufs.get_available (*t, out).read_from (bufs.get_available (*t, in_idx), nframes, 0, 0);
				}
			}
		}
	}
}

void
PluginInsert::silence (samplecnt_t nframes, samplepos_t start_sample)
{
	automation_run (start_sample, nframes, true); // evaluate automation only

	if (!_active) {
		// XXX delaybuffers need to be offset by nframes
		return;
	}

	_delaybuffers.flush ();

	const ChanMapping in_map (natural_input_streams ());
	const ChanMapping out_map (natural_output_streams ());
	ChanCount maxbuf = ChanCount::max (natural_input_streams (), natural_output_streams());
#ifdef MIXBUS
	if (is_channelstrip ()) {
		if (_configured_in.n_audio() > 0) {
			_plugins.front()->connect_and_run (_session.get_scratch_buffers (maxbuf, true), start_sample, start_sample + nframes, 1.0, in_map, out_map, nframes, 0);
		}
	} else
#endif
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->connect_and_run (_session.get_scratch_buffers (maxbuf, true), start_sample, start_sample + nframes, 1.0, in_map, out_map, nframes, 0);
	}
}

void
PluginInsert::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool)
{
	if (_sidechain) {
		// collect sidechain input for complete cycle (!)
		// TODO we need delaylines here for latency compensation
		_sidechain->run (bufs, start_sample, end_sample, speed, nframes, true);
	}

	if (g_atomic_int_compare_and_exchange (&_stat_reset, 1, 0)) {
		_timing_stats.reset ();
	}

	if (g_atomic_int_compare_and_exchange (&_flush, 1, 0)) {
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
			(*i)->flush ();
		}
	}

	if (_pending_active) {
#if defined MIXBUS && defined NDEBUG
		if (!is_channelstrip ()) {
			_timing_stats.start ();
		}
#else
		_timing_stats.start ();
#endif
		/* run as normal if we are active or moving from inactive to active */

		if (_session.transport_rolling() || _session.bounce_processing()) {
			automate_and_run (bufs, start_sample, end_sample, speed, nframes);
		} else {
			Glib::Threads::Mutex::Lock lm (control_lock(), Glib::Threads::TRY_LOCK);
			connect_and_run (bufs, start_sample, end_sample, speed, nframes, 0, lm.locked());
		}
#if defined MIXBUS && defined NDEBUG
		if (!is_channelstrip ()) {
			_timing_stats.update ();
		}
#else
		_timing_stats.update ();
#endif

	} else {
		_timing_stats.reset ();
		// XXX should call ::silence() to run plugin(s) for consistent load.
		// We'll need to change this anyway when bypass can be automated
		bypass (bufs, nframes);
		automation_run (start_sample, nframes, true); // evaluate automation only
		_delaybuffers.flush ();
	}

	/* we have no idea whether the plugin generated silence or not, so mark
	 * all buffers appropriately.
	 */
}

void
PluginInsert::automate_and_run (BufferSet& bufs, samplepos_t start, samplepos_t end, double speed, pframes_t nframes)
{
	Evoral::ControlEvent next_event (timepos_t (Temporal::AudioTime), 0.0f);
	samplecnt_t offset = 0;

	Glib::Threads::Mutex::Lock lm (control_lock(), Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		connect_and_run (bufs, start, end, speed, nframes, offset, false);
		return;
	}

	/* map start back into loop-range, adjust end */
	map_loop_range (start, end);

	const bool no_split_cycle =_plugins.front()->requires_fixed_sized_buffers () || _plugins.front()->get_info ()->type == ARDOUR::VST3;

	if (no_split_cycle || !find_next_event (timepos_t (start), timepos_t (end), next_event)) {

		/* no events have a time within the relevant range */

		connect_and_run (bufs, start, end, speed, nframes, offset, true);
		return;
	}

	while (nframes) {

		samplecnt_t cnt = min (timepos_t (start).distance (next_event.when).samples(), (samplecnt_t) nframes);
		assert (cnt > 0);

		connect_and_run (bufs, start, start + cnt * speed, speed, cnt, offset, true);

		nframes -= cnt;
		offset += cnt;
		start += cnt * speed;

		map_loop_range (start, end);

		if (!find_next_event (timepos_t (start), timepos_t (end), next_event)) {
			break;
		}
	}

	/* cleanup anything that is left to do */

	if (nframes) {
		connect_and_run (bufs, start, start + nframes * speed, speed, nframes, offset, true);
	}
}

float
PluginInsert::default_parameter_value (const Evoral::Parameter& param)
{
	if (param.type() != PluginAutomation)
		return 1.0;

	if (_plugins.empty()) {
		fatal << _("programming error: ") << X_("PluginInsert::default_parameter_value() called with no plugin")
		      << endmsg;
		abort(); /*NOTREACHED*/
	}

	return _plugins[0]->default_value (param.id());
}


bool
PluginInsert::can_reset_all_parameters ()
{
	bool all = true;
	uint32_t params = 0;
	for (uint32_t par = 0; par < _plugins[0]->parameter_count(); ++par) {
		bool ok=false;
		const uint32_t cid = _plugins[0]->nth_parameter (par, ok);

		if (!ok || !_plugins[0]->parameter_is_input(cid)) {
			continue;
		}

		boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter(PluginAutomation, 0, cid));
		if (!ac) {
			continue;
		}

		++params;
		if (ac->automation_state() & Play) {
			all = false;
			break;
		}
	}
	return all && (params > 0);
}

bool
PluginInsert::reset_parameters_to_default ()
{
	bool all = true;

	for (uint32_t par = 0; par < _plugins[0]->parameter_count(); ++par) {
		bool ok=false;
		const uint32_t cid = _plugins[0]->nth_parameter (par, ok);

		if (!ok || !_plugins[0]->parameter_is_input(cid)) {
			continue;
		}

		const float dflt = _plugins[0]->default_value (cid);
		const float curr = _plugins[0]->get_parameter (cid);

		if (dflt == curr) {
			continue;
		}

		boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter(PluginAutomation, 0, cid));
		if (!ac) {
			continue;
		}

		if (ac->automation_state() & Play) {
			all = false;
			continue;
		}

		ac->set_value (dflt, Controllable::NoGroup);
	}
	return all;
}

boost::shared_ptr<Plugin>
PluginInsert::plugin_factory (boost::shared_ptr<Plugin> other)
{
	boost::shared_ptr<LadspaPlugin> lp;
	boost::shared_ptr<LuaProc> lua;
	boost::shared_ptr<LV2Plugin> lv2p;
#ifdef WINDOWS_VST_SUPPORT
	boost::shared_ptr<WindowsVSTPlugin> vp;
#endif
#ifdef LXVST_SUPPORT
	boost::shared_ptr<LXVSTPlugin> lxvp;
#endif
#ifdef MACVST_SUPPORT
	boost::shared_ptr<MacVSTPlugin> mvp;
#endif
#ifdef VST3_SUPPORT
	boost::shared_ptr<VST3Plugin> vst3;
#endif
#ifdef AUDIOUNIT_SUPPORT
	boost::shared_ptr<AUPlugin> ap;
#endif

	if ((lp = boost::dynamic_pointer_cast<LadspaPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LadspaPlugin (*lp));
	} else if ((lua = boost::dynamic_pointer_cast<LuaProc> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LuaProc (*lua));
	} else if ((lv2p = boost::dynamic_pointer_cast<LV2Plugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LV2Plugin (*lv2p));
#ifdef WINDOWS_VST_SUPPORT
	} else if ((vp = boost::dynamic_pointer_cast<WindowsVSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new WindowsVSTPlugin (*vp));
#endif
#ifdef LXVST_SUPPORT
	} else if ((lxvp = boost::dynamic_pointer_cast<LXVSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LXVSTPlugin (*lxvp));
#endif
#ifdef MACVST_SUPPORT
	} else if ((mvp = boost::dynamic_pointer_cast<MacVSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new MacVSTPlugin (*mvp));
#endif
#ifdef VST3_SUPPORT
	} else if ((vst3 = boost::dynamic_pointer_cast<VST3Plugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new VST3Plugin (*vst3));
#endif
#ifdef AUDIOUNIT_SUPPORT
	} else if ((ap = boost::dynamic_pointer_cast<AUPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new AUPlugin (*ap));
#endif
	}

	fatal << string_compose (_("programming error: %1"),
			  X_("unknown plugin type in PluginInsert::plugin_factory"))
	      << endmsg;
	abort(); /*NOTREACHED*/
	return boost::shared_ptr<Plugin> ((Plugin*) 0);
}

void
PluginInsert::set_input_map (uint32_t num, ChanMapping m) {
	if (num < _in_map.size()) {
		bool changed = _in_map[num] != m;
		_in_map[num] = m;
		changed |= sanitize_maps ();
		if (changed) {
			mapping_changed ();
		}
	}
}

void
PluginInsert::set_output_map (uint32_t num, ChanMapping m) {
	if (num < _out_map.size()) {
		bool changed = _out_map[num] != m;
		_out_map[num] = m;
		changed |= sanitize_maps ();
		if (changed) {
			mapping_changed ();
		}
	}
}

void
PluginInsert::set_thru_map (ChanMapping m) {
	bool changed = _thru_map != m;
	_thru_map = m;
	changed |= sanitize_maps ();
	if (changed) {
		mapping_changed ();
	}
}

bool
PluginInsert::pre_seed (const ChanCount& in, const ChanCount& out,
		const ChanMapping& im, const ChanMapping& om, const ChanMapping& tm)
{
	if (_configured) { return false; }
	_configured_in = in;
	_configured_out = out;
	_in_map[0] = im;
	_out_map[0] = om;
	_thru_map = tm;
	_maps_from_state = in.n_total () > 0 && out.n_total () > 0;
	return true;
}

ChanMapping
PluginInsert::input_map () const
{
	ChanMapping rv;
	uint32_t pc = 0;
	for (PinMappings::const_iterator i = _in_map.begin (); i != _in_map.end (); ++i, ++pc) {
		ChanMapping m (i->second);
		const ChanMapping::Mappings& mp ((*i).second.mappings());
		for (ChanMapping::Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
			for (ChanMapping::TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
				rv.set (tm->first, i->first + pc * natural_input_streams().get(tm->first), i->second);
			}
		}
	}
	return rv;
}


ChanMapping
PluginInsert::no_sc_input_map () const
{
	ChanMapping rv;
	uint32_t pc = 0;
	for (PinMappings::const_iterator i = _in_map.begin (); i != _in_map.end (); ++i, ++pc) {
		ChanMapping m (i->second);
		const ChanMapping::Mappings& mp ((*i).second.mappings());
		for (ChanMapping::Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
			uint32_t ins = natural_input_streams().get(tm->first) - _cached_sidechain_pins.get(tm->first);
			for (ChanMapping::TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
				if (i->first < ins) {
					rv.set (tm->first, i->first + pc * ins, i->second);
				}
			}
		}
	}
	if (has_midi_thru ()) {
		rv.set (DataType::MIDI, 0, 0);
	}
	return rv;
}

ChanMapping
PluginInsert::output_map () const
{
	ChanMapping rv;
	uint32_t pc = 0;
	for (PinMappings::const_iterator i = _out_map.begin (); i != _out_map.end (); ++i, ++pc) {
		ChanMapping m (i->second);
		const ChanMapping::Mappings& mp ((*i).second.mappings());
		for (ChanMapping::Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
			for (ChanMapping::TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
				rv.set (tm->first, i->first + pc * natural_output_streams().get(tm->first), i->second);
			}
		}
	}
	if (has_midi_bypass ()) {
		rv.set (DataType::MIDI, 0, 0);
	}

	return rv;
}

bool
PluginInsert::has_midi_bypass () const
{
	if (_configured_in.n_midi () == 1 && _configured_out.n_midi () == 1
			&& natural_output_streams ().n_midi () == 0) {
		return true;
	}
	return false;
}

bool
PluginInsert::has_midi_thru () const
{
	if (_configured_in.n_midi () == 1 && _configured_out.n_midi () == 1
			&& natural_input_streams ().n_midi () == 0 && natural_output_streams ().n_midi () == 0) {
		return true;
	}
	return false;
}

bool
PluginInsert::is_channelstrip () const
{
	return false;
}

void
PluginInsert::mapping_changed ()
{
	PluginMapChanged (); /* EMIT SIGNAL */
	_no_inplace = check_inplace ();
	_session.set_dirty();
}

bool
PluginInsert::check_inplace ()
{
	bool inplace_ok = !_plugins.front()->inplace_broken ();

	if (_thru_map.n_total () > 0) {
		// TODO once midi-bypass is part of the mapping, ignore it
		inplace_ok = false;
	}

	if (_match.method == Split && inplace_ok) {
		assert (get_count() == 1);
		assert (_in_map.size () == 1);
		if (!_out_map[0].is_monotonic ()) {
			inplace_ok = false;
		}
		if (_configured_internal != _configured_in) {
			/* no sidechain -- TODO we could allow this with
			 * some more logic in PluginInsert::connect_and_run().
			 *
			 * PluginInsert::reset_map() already maps it.
			 */
			inplace_ok = false;
		}
		/* check mapping */
		for (DataType::iterator t = DataType::begin(); t != DataType::end() && inplace_ok; ++t) {
			if (_configured_internal.get (*t) == 0) {
				continue;
			}
			bool valid;
			uint32_t first_idx = _in_map[0].get (*t, 0, &valid);
			if (!valid || first_idx != 0) {
				// so far only allow to copy the *first* stream's buffer to others
				inplace_ok = false;
			} else {
				for (uint32_t i = 1; i < natural_input_streams ().get (*t); ++i) {
					uint32_t idx = _in_map[0].get (*t, i, &valid);
					if (valid && idx != first_idx) {
						inplace_ok = false;
						break;
					}
				}
			}
		}

		if (inplace_ok) {
			DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: In Place Split Map\n", name()));
			return false;
		}
	}

	for (uint32_t pc = 0; pc < get_count() && inplace_ok ; ++pc) {
		if (!_in_map[pc].is_monotonic ()) {
			inplace_ok = false;
		}
		if (!_out_map[pc].is_monotonic ()) {
			inplace_ok = false;
		}
	}

	if (inplace_ok) {
		/* check if every output is fed by the corresponding input
		 *
		 * this prevents  in-port 1 -> sink-pin 2  ||  source-pin 1 -> out port 1, source-pin 2 -> out port 2
		 * (with in-place,  source-pin 1 -> out port 1 overwrites in-port 1)
		 *
		 * but allows     in-port 1 -> sink-pin 2  ||  source-pin 2 -> out port 1
		 */
		ChanMapping const& in_map (input_map ());
		const ChanMapping::Mappings out_m (output_map ().mappings ());
		for (ChanMapping::Mappings::const_iterator t = out_m.begin (); t != out_m.end () && inplace_ok; ++t) {
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				/* src-pin: c->first, out-port: c->second */
				bool valid;
				uint32_t in_port = in_map.get (t->first, c->first, &valid);
				if (valid && in_port != c->second) {
					inplace_ok = false;
					break;
				}
			}
		}
	}

	DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: %2\n", name(), inplace_ok ? "In-Place" : "No Inplace Processing"));
	return !inplace_ok; // no-inplace
}

bool
PluginInsert::sanitize_maps ()
{
	bool changed = false;
	/* strip dead wood */
	PinMappings new_ins;
	PinMappings new_outs;
	ChanMapping new_thru;

	for (uint32_t pc = 0; pc < get_count(); ++pc) {
		ChanMapping new_in;
		ChanMapping new_out;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t i = 0; i < natural_input_streams().get (*t); ++i) {
				bool valid;
				uint32_t idx = _in_map[pc].get (*t, i, &valid);
				if (valid && idx < _configured_internal.get (*t)) {
					new_in.set (*t, i, idx);
				}
			}
			for (uint32_t o = 0; o < natural_output_streams().get (*t); ++o) {
				bool valid;
				uint32_t idx = _out_map[pc].get (*t, o, &valid);
				if (valid && idx < _configured_out.get (*t)) {
					new_out.set (*t, o, idx);
				}
			}
		}
		if (_in_map[pc] != new_in || _out_map[pc] != new_out) {
			changed = true;
		}
		new_ins[pc] = new_in;
		new_outs[pc] = new_out;
	}

	/* prevent dup output assignments */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (uint32_t o = 0; o < _configured_out.get (*t); ++o) {
			bool mapped = false;
			for (uint32_t pc = 0; pc < get_count(); ++pc) {
				bool valid;
				uint32_t idx = new_outs[pc].get_src (*t, o, &valid);
				if (valid && mapped) {
					new_outs[pc].unset (*t, idx);
				} else if (valid) {
					mapped = true;
				}
			}
		}
	}

	/* remove excess thru */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (uint32_t o = 0; o < _configured_out.get (*t); ++o) {
			bool valid;
			uint32_t idx = _thru_map.get (*t, o, &valid);
			if (valid && idx < _configured_internal.get (*t)) {
				new_thru.set (*t, o, idx);
			}
		}
	}

	/* prevent out + thru,  existing plugin outputs override thru */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (uint32_t o = 0; o < _configured_out.get (*t); ++o) {
			bool mapped = false;
			bool valid;
			for (uint32_t pc = 0; pc < get_count(); ++pc) {
				new_outs[pc].get_src (*t, o, &mapped);
				if (mapped) { break; }
			}
			if (!mapped) { continue; }
			uint32_t idx = new_thru.get (*t, o, &valid);
			if (mapped) {
				new_thru.unset (*t, idx);
			}
		}
	}

	if (has_midi_bypass ()) {
		// TODO: include midi-bypass in the thru set,
		// remove dedicated handling.
		new_thru.unset (DataType::MIDI, 0);
	}

	if (_in_map != new_ins || _out_map != new_outs || _thru_map != new_thru) {
		changed = true;
	}
	_in_map = new_ins;
	_out_map = new_outs;
	_thru_map = new_thru;

	return changed;
}

bool
PluginInsert::reset_map (bool emit)
{
	const PinMappings old_in (_in_map);
	const PinMappings old_out (_out_map);

	_in_map.clear ();
	_out_map.clear ();
	_thru_map = ChanMapping ();

	/* build input map */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t sc = 0; // side-chain round-robin (all instances)
		uint32_t pc = 0;
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			const uint32_t nis = natural_input_streams ().get(*t);
			const uint32_t stride = nis - sidechain_input_pins().get (*t);

			/* SC inputs are last in the plugin-insert.. */
			const uint32_t sc_start = _configured_in.get (*t);
			const uint32_t sc_len = _configured_internal.get (*t) - sc_start;
			/* ...but may not be at the end of the plugin ports.
			 * in case the side-chain is not the last port, shift connections back.
			 * and connect to side-chain
			 */
			uint32_t shift = 0;
			uint32_t ic = 0; // split inputs
			const uint32_t cend = _configured_in.get (*t);

			for (uint32_t in = 0; in < nis; ++in) {
				const Plugin::IOPortDescription& iod (_plugins[pc]->describe_io_port (*t, true, in));
				if (iod.is_sidechain) {
					/* connect sidechain sinks to sidechain inputs in round-robin fashion */
					if (sc_len > 0) {// side-chain may be hidden
						_in_map[pc].set (*t, in, sc_start + sc);
						sc = (sc + 1) % sc_len;
					}
					++shift;
				} else {
					if (_match.method == Split) {
						if (cend == 0) { continue; }
						if (_strict_io && ic + stride * pc >= cend) {
							break;
						}
						/* connect *no* sidechain sinks in round-robin fashion */
						_in_map[pc].set (*t, in, ic + stride * pc);
						if (_strict_io && (ic + 1) == cend) {
							break;
						}
						ic = (ic + 1) % cend;
					} else {
						uint32_t s = in - shift;
						if (stride * pc + s < cend) {
							_in_map[pc].set (*t, in, s + stride * pc);
						}
					}
				}
			}
		}
	}

	/* build output map */
	uint32_t pc = 0;
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
		_out_map[pc] = ChanMapping (ChanCount::min (natural_output_streams(), _configured_out));
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			_out_map[pc].offset_to(*t, pc * natural_output_streams().get(*t));
		}
	}

	sanitize_maps ();
	if (old_in == _in_map && old_out == _out_map) {
		return false;
	}
	if (emit) {
		mapping_changed ();
	}
	return true;
}

bool
PluginInsert::configure_io (ChanCount in, ChanCount out)
{
	Match old_match = _match;
	ChanCount old_in;
	ChanCount old_internal;
	ChanCount old_out;
	ChanCount old_pins;

	old_pins = natural_input_streams();
	old_in = _configured_in;
	old_out = _configured_out;
	old_internal = _configured_internal;

	_configured_in = in;
	_configured_internal = in;
	_configured_out = out;

	ChanCount aux_in;

	if (_sidechain) {
		/* TODO hide midi-bypass, and custom outs. Best /fake/ "out" here.
		 * (currently _sidechain->configure_io always succeeds
		 *  since Processor::configure_io() succeeds)
		 */
		if (!_sidechain->configure_io (in, out)) {
			DEBUG_TRACE (DEBUG::ChanMapping, "Sidechain configuration failed\n");
			return false;
		}
		_configured_internal += _sidechain->input()->n_ports();
		aux_in = _sidechain->input()->n_ports();

		// include (static_cast<Route*>owner())->name() ??
		_sidechain->input ()-> set_pretty_name (string_compose (_("SC %1"), name ()));
	}

	/* get plugin configuration */
	_match = private_can_support_io_configuration (in, out); // sets out
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::ChanMapping)) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, string_compose ("%1: ",  name()));
		DEBUG_STR_APPEND(a, _match);
		DEBUG_TRACE (DEBUG::ChanMapping, DEBUG_STR(a).str());
	}
#endif

	/* set the matching method and number of plugins that we will use to meet this configuration */
	if (set_count (_match.plugins) == false) {
		PluginIoReConfigure (); /* EMIT SIGNAL */
		_configured = false;
		return false;
	}

	/* configure plugins */
	switch (_match.method) {
	case Split:
	case Hide:
		if (_plugins.front()->reconfigure_io (natural_input_streams(), ChanCount (), out) == false) {
			PluginIoReConfigure (); /* EMIT SIGNAL */
			_configured = false;
			return false;
		}
		break;
	case Delegate:
		{
			ChanCount din (in);
			ChanCount daux (aux_in);
			ChanCount dout (_configured_out);
			if (_custom_cfg) {
				if (_custom_sinks.n_total () > 0) {
					din = std::min (natural_input_streams(), _custom_sinks);
					if (_custom_sinks > natural_input_streams()) {
						daux = _custom_sinks - din;
					}
				}
				dout = _custom_out;
			} else if (_preset_out.n_audio () > 0) {
				dout.set (DataType::AUDIO, _preset_out.n_audio ());
			} else if (dout.n_midi () > 0 && dout.n_audio () == 0) {
				dout.set (DataType::AUDIO, 2);
			}
			//if (dout.n_audio () == 0) { dout.set (DataType::AUDIO, 1); } // XXX why?
			DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: Delegate lookup: %2 %3 %4\n", name(), din, daux, dout));
			bool const r = _plugins.front()->match_variable_io (din, daux, dout);
			assert (r);
			DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: Delegate configuration: %2 %3 %4\n", name(), din, daux, dout));
			if (_plugins.front()->reconfigure_io (din, daux, dout) == false) {
				PluginIoReConfigure (); /* EMIT SIGNAL */
				_configured = false;
				return false;
			}
			if (!_custom_cfg) {
				_custom_sinks = din;
			}
		}
		break;
	default:
		if (_plugins.front()->reconfigure_io (in, aux_in, out) == false) {
			PluginIoReConfigure (); /* EMIT SIGNAL */
			_configured = false;
			return false;
		}
		break;
	}

	DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: cfg:%2 state:%3 chn-in:%4 chn-out:%5 inpin:%6 match:%7 cust:%8 size-in:%9 size-out:%10\n",
				name (),
				_configured ? "Y" : "N",
				_maps_from_state ? "Y" : "N",
				old_in == in ? "==" : "!=",
				old_out == out ? "==" : "!=",
				old_pins == natural_input_streams () ? "==" : "!=",
				old_match.method == _match.method ? "==" : "!=",
				old_match.custom_cfg == _match.custom_cfg ? "==" : "!=",
				_in_map.size() == get_count () ? "==" : "!=",
				_out_map.size() == get_count () ? "==" : "!="
				));

	bool mapping_changed = false;
	if (old_in == in && old_out == out
			&& _configured
			&& old_pins == natural_input_streams ()
			&& old_match.method == _match.method
			&& old_match.custom_cfg == _match.custom_cfg
			&& _in_map.size() == _out_map.size()
			&& _in_map.size() == get_count ()
		 ) {
		/* If the configuration has not changed, keep the mapping */
		mapping_changed = sanitize_maps ();
	} else if (_match.custom_cfg && _configured) {
		/* don't touch the map in manual mode */
		mapping_changed = sanitize_maps ();
	} else {
#ifdef MIXBUS
		if (is_channelstrip ()) {
			/* fake channel map - for wire display */
			_in_map.clear ();
			_out_map.clear ();
			_thru_map = ChanMapping ();
			_in_map[0] = ChanMapping (ChanCount::min (_configured_in, ChanCount (DataType::AUDIO, 2)));
			_out_map[0] = ChanMapping (ChanCount::min (_configured_out, ChanCount (DataType::AUDIO, 2)));
			/* set "thru" map for in-place forward of audio */
			for (uint32_t i = 2; i < _configured_in.n_audio(); ++i) {
				_thru_map.set (DataType::AUDIO, i, i);
			}
			/* and midi (after implicit 1st channel bypass) */
			for (uint32_t i = 1; i < _configured_in.n_midi(); ++i) {
				_thru_map.set (DataType::MIDI, i, i);
			}
		} else
#endif
		if (_maps_from_state && old_in == in && old_out == out) {
			mapping_changed = true;
			sanitize_maps ();
		} else {
			/* generate a new mapping */
			mapping_changed = reset_map (false);
		}
		_maps_from_state = false;
	}

	if (mapping_changed) {
		PluginMapChanged (); /* EMIT SIGNAL */

#ifndef NDEBUG
		if (DEBUG_ENABLED(DEBUG::ChanMapping)) {
			uint32_t pc = 0;
			DEBUG_STR_DECL(a);
			DEBUG_STR_APPEND(a, "\n--------<<--------\n");
			for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
				if (pc > 0) {
			DEBUG_STR_APPEND(a, "----><----\n");
				}
				DEBUG_STR_APPEND(a, string_compose ("Channel Map for %1 plugin %2\n", name(), pc));
				DEBUG_STR_APPEND(a, " * Inputs:\n");
				DEBUG_STR_APPEND(a, _in_map[pc]);
				DEBUG_STR_APPEND(a, " * Outputs:\n");
				DEBUG_STR_APPEND(a, _out_map[pc]);
			}
			DEBUG_STR_APPEND(a, " * Thru:\n");
			DEBUG_STR_APPEND(a, _thru_map);
			DEBUG_STR_APPEND(a, "-------->>--------\n");
			DEBUG_TRACE (DEBUG::ChanMapping, DEBUG_STR(a).str());
		}
#endif
	}

	_no_inplace = check_inplace ();

	/* only the "noinplace_buffers" thread buffers need to be this large,
	 * this can be optimized. other buffers are fine with
	 * ChanCount::max (natural_input_streams (), natural_output_streams())
	 * and route.cc's max (configured_in, configured_out)
	 *
	 * no-inplace copies "thru" outputs (to emulate in-place) for
	 * all outputs (to prevent overwrite) into a temporary space
	 * which also holds input buffers (in case the plugin does process
	 * in-place and overwrites those).
	 *
	 * this buffers need to be at least as
	 *   natural_input_streams () + possible outputs.
	 *
	 * sidechain inputs add a constraint on the input:
	 * configured input + sidechain (=_configured_internal)
	 *
	 * NB. this also satisfies
	 * max (natural_input_streams(), natural_output_streams())
	 * which is needed for silence runs
	 */
	_required_buffers = ChanCount::max (_configured_internal,
			natural_input_streams () + ChanCount::max (_configured_out, natural_output_streams () * get_count ()));

	if (old_in != in || old_out != out || old_internal != _configured_internal
			|| old_pins != natural_input_streams ()
			|| (old_match.method != _match.method && (old_match.method == Split || _match.method == Split))
		 ) {
		PluginIoReConfigure (); /* EMIT SIGNAL */
	}

	_delaybuffers.configure (_configured_out, _plugins.front ()->max_latency ());
	_latency_changed = true;

	/* we don't know the analysis window size, so we must work with the
	 * current buffer size here. each request for data fills in these
	 * buffers and the analyser makes sure it gets enough data for the
	 * analysis window. We also only analyze audio, so we can ignore
	 * MIDI buffers.
	 */
	ChanCount cc_analysis_in (DataType::AUDIO, in.n_audio());
	ChanCount cc_analysis_out (DataType::AUDIO, out.n_audio());

	session().ensure_buffer_set (_signal_analysis_inputs, cc_analysis_in);
	_signal_analysis_inputs.set_count (cc_analysis_in);

	session().ensure_buffer_set (_signal_analysis_outputs, cc_analysis_out);
	_signal_analysis_outputs.set_count (cc_analysis_out);

	// std::cerr << "set counts to i" << in.n_audio() << "/o" << out.n_audio() << std::endl;

	_configured = true;
	return Processor::configure_io (in, out);
}

/** Decide whether this PluginInsert can support a given IO configuration.
 *  To do this, we run through a set of possible solutions in rough order of
 *  preference.
 *
 *  @param in Required input channel count.
 *  @param out Filled in with the output channel count if we return true.
 *  @return true if the given IO configuration can be supported.
 */
bool
PluginInsert::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (_sidechain) {
		_sidechain->can_support_io_configuration (in, out); // never fails, sets "out"
	}
	return private_can_support_io_configuration (in, out).method != Impossible;
}

PluginInsert::Match
PluginInsert::private_can_support_io_configuration (ChanCount const& in, ChanCount& out) const
{
	if (!_custom_cfg && _preset_out.n_audio () > 0) {
		// preseed hint (for variable i/o)
		out.set (DataType::AUDIO, _preset_out.n_audio ());
	}

	Match rv = internal_can_support_io_configuration (in, out);

	if (!_custom_cfg && _preset_out.n_audio () > 0) {
		DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: using output preset: %2\n", name(), _preset_out));
		out.set (DataType::AUDIO, _preset_out.n_audio ());
	}
	return rv;
}

/** A private version of can_support_io_configuration which returns the method
 *  by which the configuration can be matched, rather than just whether or not
 *  it can be.
 */
PluginInsert::Match
PluginInsert::internal_can_support_io_configuration (ChanCount const & inx, ChanCount& out) const
{
	if (_plugins.empty()) {
		return Match();
	}

#ifdef MIXBUS
	if (is_channelstrip ()) {
		out = inx;
		return Match (ExactMatch, 1);
	}
#endif

	/* if a user specified a custom cfg, so be it. */
	if (_custom_cfg) {
		PluginInfoPtr info = _plugins.front()->get_info();
		out = _custom_out;
		if (info->reconfigurable_io()) {
			return Match (Delegate, 1, _strict_io, true);
		} else {
			return Match (ExactMatch, get_count(), _strict_io, true);
		}
	}

	/* try automatic configuration */
	Match m = PluginInsert::automatic_can_support_io_configuration (inx, out);

	PluginInfoPtr info = _plugins.front()->get_info();
	ChanCount inputs  = info->n_inputs;
	ChanCount outputs = info->n_outputs;

	/* handle case strict-i/o */
	if (_strict_io && m.method != Impossible) {
		m.strict_io = true;

		/* special case MIDI instruments */
		if (is_instrument ()) {
			// output = midi-bypass + at most master-out channels.
			ChanCount max_out (DataType::AUDIO, 2); // TODO use master-out
			max_out.set (DataType::MIDI, out.get(DataType::MIDI));
			out = ChanCount::min (out, max_out);
			DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: special case strict-i/o instrument\n", name()));
			return m;
		}

		switch (m.method) {
			case NoInputs:
				if (inx.n_audio () != out.n_audio ()) { // ignore midi bypass
					/* replicate processor to match output count (generators and such)
					 * at least enough to feed every output port. */
					uint32_t f = 1; // at least one. e.g. control data filters, no in, no out.
					for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
						uint32_t nout = outputs.get (*t);
						if (nout == 0 || inx.get(*t) == 0) { continue; }
						f = max (f, (uint32_t) ceil (inx.get(*t) / (float)nout));
					}
					out = inx;
					DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: special case strict-i/o for generator\n", name()));
					return Match (Replicate, f, _strict_io);
				}
				break;
			default:
				break;
		}

		out = inx;
		return m;
	}

	if (m.method != Impossible) {
		return m;
	}

	ChanCount ns_inputs  = inputs - sidechain_input_pins ();

	DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: resolving 'Impossible' match...\n", name()));

	if (info->reconfigurable_io()) {
		//out = inx; // hint
		ChanCount main_in = inx;
		ChanCount aux_in = sidechain_input_pins ();
		if (out.n_midi () > 0 && out.n_audio () == 0) {
			out.set (DataType::AUDIO, 2);
		}
		//if (out.n_audio () == 0) { out.set (DataType::AUDIO, 1); } // why?
		bool const r = _plugins.front()->match_variable_io (main_in, aux_in, out);
		if (!r) {
			// houston, we have a problem.
			return Match (Impossible, 0);
		}
		// midi bypass
		if (inx.n_midi () > 0 && out.n_midi () == 0) { out.set (DataType::MIDI, 1); }
		return Match (Delegate, 1, _strict_io);
	}

	ChanCount midi_bypass;
	if (inx.get(DataType::MIDI) == 1 && outputs.get(DataType::MIDI) == 0) {
		midi_bypass.set (DataType::MIDI, 1);
	}

	// add at least as many plugins so that output count matches input count (w/o sidechain pins)
	uint32_t f = 0;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t nin = ns_inputs.get (*t);
		uint32_t nout = outputs.get (*t);
		if (nin == 0 || inx.get(*t) == 0) { continue; }
		// prefer floor() so the count won't overly increase IFF (nin < nout)
		f = max (f, (uint32_t) floor (inx.get(*t) / (float)nout));
	}
	if (f > 0 && outputs * f >= _configured_out) {
		out = outputs * f + midi_bypass;
		return Match (Replicate, f, _strict_io);
	}

	// add at least as many plugins needed to connect all inputs (w/o sidechain pins)
	f = 0;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t nin = ns_inputs.get (*t);
		if (nin == 0 || inx.get(*t) == 0) { continue; }
		f = max (f, (uint32_t) ceil (inx.get(*t) / (float)nin));
	}
	if (f > 0) {
		out = outputs * f + midi_bypass;
		return Match (Replicate, f, _strict_io);
	}

	// add at least as many plugins needed to connect all inputs
	f = 1;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t nin = inputs.get (*t);
		if (nin == 0 || inx.get(*t) == 0) { continue; }
		f = max (f, (uint32_t) ceil (inx.get(*t) / (float)nin));
	}
	out = outputs * f + midi_bypass;
	return Match (Replicate, f, _strict_io);
}

/* this is the original Ardour 3/4 behavior, mainly for backwards compatibility */
PluginInsert::Match
PluginInsert::automatic_can_support_io_configuration (ChanCount const& inx, ChanCount& out) const
{
	if (_plugins.empty()) {
		return Match();
	}

	PluginInfoPtr info = _plugins.front()->get_info();
	ChanCount in; in += inx;
	ChanCount midi_bypass;

	if (info->reconfigurable_io()) {
		/* Plugin has flexible I/O, so delegate to it
		 * pre-seed outputs, plugin tries closest match
		 */
		//out = in; // hint
		ChanCount aux_in = sidechain_input_pins ();
		if (out.n_midi () > 0 && out.n_audio () == 0) { out.set (DataType::AUDIO, 2); }
		if (out.n_audio () == 0) { out.set (DataType::AUDIO, 1); }
		bool const r = _plugins.front()->match_variable_io (in, aux_in, out);
		if (!r) {
			return Match (Impossible, 0);
		}
		// midi bypass
		if (in.n_midi () > 0 && out.n_midi () == 0) { out.set (DataType::MIDI, 1); }
		return Match (Delegate, 1);
	}

	ChanCount inputs  = info->n_inputs;
	ChanCount outputs = info->n_outputs;
	ChanCount ns_inputs  = inputs - sidechain_input_pins ();

	if (in.get(DataType::MIDI) == 1 && outputs.get(DataType::MIDI) == 0) {
		DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: bypassing midi-data\n", name()));
		midi_bypass.set (DataType::MIDI, 1);
	}
	if (in.get(DataType::MIDI) == 1 && inputs.get(DataType::MIDI) == 0) {
		DEBUG_TRACE (DEBUG::ChanMapping, string_compose ("%1: hiding midi-port from plugin\n", name()));
		in.set(DataType::MIDI, 0);
	}

	// add internally provided sidechain ports
	ChanCount insc = in + sidechain_input_ports ();

	bool no_inputs = true;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (inputs.get (*t) != 0) {
			no_inputs = false;
			break;
		}
	}

	if (no_inputs) {
		/* no inputs so we can take any input configuration since we throw it away */
		out = outputs + midi_bypass;
		return Match (NoInputs, 1);
	}

	/* Plugin inputs match requested inputs + side-chain-ports exactly */
	if (inputs == insc) {
		out = outputs + midi_bypass;
		return Match (ExactMatch, 1);
	}

	/* Plugin inputs matches without side-chain-pins */
	if (ns_inputs == in) {
		out = outputs + midi_bypass;
		return Match (ExactMatch, 1);
	}

	/* We may be able to run more than one copy of the plugin within this insert
	   to cope with the insert having more inputs than the plugin.
	   We allow replication only for plugins with either zero or 1 inputs and outputs
	   for every valid data type.
	*/

	uint32_t f             = 0;
	bool     can_replicate = true;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

		// ignore side-chains
		uint32_t nin = ns_inputs.get (*t);

		// No inputs of this type
		if (nin == 0 && in.get(*t) == 0) {
			continue;
		}

		if (nin != 1 || outputs.get (*t) != 1) {
			can_replicate = false;
			break;
		}

		// Potential factor not set yet
		if (f == 0) {
			f = in.get(*t) / nin;
		}

		// Factor for this type does not match another type, can not replicate
		if (f != (in.get(*t) / nin)) {
			can_replicate = false;
			break;
		}
	}

	if (can_replicate && f > 0) {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			out.set (*t, outputs.get(*t) * f);
		}
		out += midi_bypass;
		return Match (Replicate, f);
	}

	/* If the processor has exactly one input of a given type, and
	   the plugin has more, we can feed the single processor input
	   to some or all of the plugin inputs.  This is rather
	   special-case-y, but the 1-to-many case is by far the
	   simplest.  How do I split thy 2 processor inputs to 3
	   plugin inputs?  Let me count the ways ...
	*/

	bool can_split = true;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

		bool const can_split_type = (in.get (*t) == 1 && ns_inputs.get (*t) > 1);
		bool const nothing_to_do_for_type = (in.get (*t) == 0 && inputs.get (*t) == 0);

		if (!can_split_type && !nothing_to_do_for_type) {
			can_split = false;
		}
	}

	if (can_split) {
		out = outputs + midi_bypass;
		return Match (Split, 1);
	}

	/* If the plugin has more inputs than we want, we can `hide' some of them
	   by feeding them silence.
	*/

	bool could_hide = false;
	bool cannot_hide = false;
	ChanCount hide_channels;

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (inputs.get(*t) > in.get(*t)) {
			/* there is potential to hide, since the plugin has more inputs of type t than the insert */
			hide_channels.set (*t, inputs.get(*t) - in.get(*t));
			could_hide = true;
		} else if (inputs.get(*t) < in.get(*t)) {
			/* we definitely cannot hide, since the plugin has fewer inputs of type t than the insert */
			cannot_hide = true;
		}
	}

	if (could_hide && !cannot_hide) {
		out = outputs + midi_bypass;
		return Match (Hide, 1, false, false, hide_channels);
	}

	return Match (Impossible, 0);
}


XMLNode&
PluginInsert::state ()
{
	XMLNode& node = Processor::state ();

	node.set_property("type", _plugins[0]->state_node_name());
	node.set_property("unique-id", _plugins[0]->unique_id());
	node.set_property("count", (uint32_t)_plugins.size());

	/* remember actual i/o configuration (for later placeholder
	 * in case the plugin goes missing) */
	node.add_child_nocopy (* _configured_in.state (X_("ConfiguredInput")));
	node.add_child_nocopy (* _custom_sinks.state (X_("CustomSinks")));
	node.add_child_nocopy (* _configured_out.state (X_("ConfiguredOutput")));
	node.add_child_nocopy (* _preset_out.state (X_("PresetOutput")));

	/* save custom i/o config */
	node.set_property("custom", _custom_cfg);
	for (uint32_t pc = 0; pc < get_count(); ++pc) {
		char tmp[128];
		snprintf (tmp, sizeof(tmp), "InputMap-%d", pc);
		node.add_child_nocopy (* _in_map[pc].state (tmp));
		snprintf (tmp, sizeof(tmp), "OutputMap-%d", pc);
		node.add_child_nocopy (* _out_map[pc].state (tmp));
	}
	node.add_child_nocopy (* _thru_map.state ("ThruMap"));

	if (_sidechain) {
		node.add_child_nocopy (_sidechain->get_state ());
	}

	_plugins[0]->set_insert_id(this->id());
	node.add_child_nocopy (_plugins[0]->get_state());

	for (Controls::iterator c = controls().begin(); c != controls().end(); ++c) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> ((*c).second);
		if (ac) {
			node.add_child_nocopy (ac->get_state());
		}
	}

	return node;
}

void
PluginInsert::set_control_ids (const XMLNode& node, int version)
{
	const XMLNodeList& nlist = node.children();
	for (XMLNodeConstIterator iter = nlist.begin(); iter != nlist.end(); ++iter) {
		if ((*iter)->name() != Controllable::xml_node_name) {
			continue;
		}

		uint32_t p = (uint32_t)-1;
		std::string str;
		if ((*iter)->get_property (X_("symbol"), str)) {
			boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (_plugins[0]);
			if (lv2plugin) {
				p = lv2plugin->port_index(str.c_str());
			}
		}
		if (p == (uint32_t)-1) {
			(*iter)->get_property (X_("parameter"), p);
		}

		if (p == (uint32_t)-1) {
			continue;
		}

		/* this may create the new controllable */
		boost::shared_ptr<Evoral::Control> c = control (Evoral::Parameter (PluginAutomation, 0, p));

		if (!c) {
			continue;
		}
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c);
		if (ac) {
			ac->set_state (**iter, version);
		}
	}
}

void
PluginInsert::update_control_values (const XMLNode& node, int version)
{
	const XMLNodeList& nlist = node.children();
	for (XMLNodeConstIterator iter = nlist.begin(); iter != nlist.end(); ++iter) {
		if ((*iter)->name() != Controllable::xml_node_name) {
			continue;
		}

		float val;
		if (!(*iter)->get_property (X_("value"), val)) {
			continue;
		}

		uint32_t p = (uint32_t)-1;

		std::string str;
		if ((*iter)->get_property (X_("symbol"), str)) {
			boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (_plugins[0]);
			if (lv2plugin) {
				p = lv2plugin->port_index(str.c_str());
			}
		}

		if (p == (uint32_t)-1) {
			(*iter)->get_property (X_("parameter"), p);
		}

		if (p == (uint32_t)-1) {
			continue;
		}

		/* lookup controllable */
		boost::shared_ptr<Evoral::Control> c = control (Evoral::Parameter (PluginAutomation, 0, p), false);
		if (!c) {
			continue;
		}
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c);
		if (ac) {
			ac->set_value (val, Controllable::NoGroup);
		}
	}
}

int
PluginInsert::set_state(const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	ARDOUR::PluginType type;

	std::string str;
	if (!node.get_property ("type", str)) {
		error << _("XML node describing plugin is missing the `type' field") << endmsg;
		return -1;
	}

	if (str == X_("ladspa") || str == X_("Ladspa")) { /* handle old school sessions */
		type = ARDOUR::LADSPA;
	} else if (str == X_("lv2")) {
		type = ARDOUR::LV2;
	} else if (str == X_("windows-vst")) {
		type = ARDOUR::Windows_VST;
	} else if (str == X_("lxvst")) {
		type = ARDOUR::LXVST;
	} else if (str == X_("mac-vst")) {
		type = ARDOUR::MacVST;
	} else if (str == X_("audiounit")) {
		type = ARDOUR::AudioUnit;
	} else if (str == X_("luaproc")) {
		type = ARDOUR::Lua;
	} else if (str == X_("vst3")) {
		type = ARDOUR::VST3;
	} else {
		error << string_compose (_("unknown plugin type %1 in plugin insert state"), str) << endmsg;
		return -1;
	}

	XMLProperty const* prop = node.property ("unique-id");

	if (prop == 0) {
#ifdef WINDOWS_VST_SUPPORT
		/* older sessions contain VST plugins with only an "id" field.  */
		if (type == ARDOUR::Windows_VST) {
			prop = node.property ("id");
		}
#endif

#ifdef LXVST_SUPPORT
		/*There shouldn't be any older sessions with linuxVST support.. but anyway..*/
		if (type == ARDOUR::LXVST) {
			prop = node.property ("id");
		}
#endif

		/* recheck  */
		if (prop == 0) {
			error << _("Plugin has no unique ID field") << endmsg;
			return -1;
		}
	}

	bool any_vst = false;
	uint32_t count = 1;
	node.get_property ("count", count);

	if (_plugins.empty()) {
		/* Find and load plugin module */
		boost::shared_ptr<Plugin> plugin = find_plugin (_session, prop->value(), type);

		/* treat VST plugins equivalent if they have the same uniqueID
		 * allow to move sessions windows <> linux */
#ifdef LXVST_SUPPORT
		if (plugin == 0 && (type == ARDOUR::Windows_VST || type == ARDOUR::MacVST)) {
			type = ARDOUR::LXVST;
			plugin = find_plugin (_session, prop->value(), type);
			if (plugin) { any_vst = true; }
		}
#endif

#ifdef WINDOWS_VST_SUPPORT
		if (plugin == 0 && (type == ARDOUR::LXVST || type == ARDOUR::MacVST)) {
			type = ARDOUR::Windows_VST;
			plugin = find_plugin (_session, prop->value(), type);
			if (plugin) { any_vst = true; }
		}
#endif

#ifdef MACVST_SUPPORT
		if (plugin == 0 && (type == ARDOUR::Windows_VST || type == ARDOUR::LXVST)) {
			type = ARDOUR::MacVST;
			plugin = find_plugin (_session, prop->value(), type);
			if (plugin) { any_vst = true; }
		}
#endif

		if (plugin == 0 && type == ARDOUR::Lua) {
			/* unique ID (sha1 of script) was not found,
			 * load the plugin from the serialized version in the
			 * session-file instead.
			 */
			boost::shared_ptr<LuaProc> lp (new LuaProc (_session.engine(), _session, ""));
			XMLNode *ls = node.child (lp->state_node_name().c_str());
			if (ls && lp) {
				if (0 == lp->set_script_from_state (*ls)) {
					plugin = lp;
				}
			}
		}

		if (plugin == 0) {
			error << string_compose(
					_("Found a reference to a plugin (\"%1\") that is unknown.\n"
						"Perhaps it was removed or moved since it was last used."),
					prop->value())
				<< endmsg;
			return -1;
		}

		/* The name of the PluginInsert comes from the plugin */
		_name = plugin->get_info()->name;

		/* Processor::set_state() will set this, but too late
		 * for it to be available when setting up plugin
		 * state. We can't call Processor::set_state() until
		 * the plugins themselves are created and added.
		 */

		set_id (node);

		/* if we are adding the first plugin, we will need to set
		 * up automatable controls.
		 */
		add_plugin (plugin);
		create_automatable_parameters ();
		set_control_ids (node, version);

		if (_plugins.size() != count) {
			for (uint32_t n = 1; n < count; ++n) {
				add_plugin (plugin_factory (plugin));
			}
		}
	} else {
		assert (_plugins[0]->unique_id() == prop->value());
		/* update controllable value only (copy plugin state) */
		set_id (node);
		update_control_values (node, version);
	}

	Processor::set_state (node, version);

	PBD::ID new_id = this->id();
	PBD::ID old_id = this->id();

	node.get_property ("id", old_id);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		/* find the node with the type-specific node name ("lv2", "ladspa", etc)
		 * and set all plugins to the same state.
		 */
		if ((*niter)->name() == _plugins[0]->state_node_name ()
		    || (any_vst && ((*niter)->name() == "lxvst" || (*niter)->name() == "windows-vst" || (*niter)->name() == "mac-vst"))
		   ) {

			for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
				/* Plugin state can include external files which are named after the ID.
				 *
				 * If regenerate_xml_or_string_ids() is set, the ID will already have
				 * been changed, so we need to use the old ID from the XML to load the
				 * state and then update the ID.
				 *
				 * When copying a plugin-state, route_ui takes care of of updating the ID,
				 * but we need to call set_insert_id() to clear the cached plugin-state
				 * and force a change.
				 */
				if (!regenerate_xml_or_string_ids ()) {
					(*i)->set_insert_id (new_id);
				} else {
					(*i)->set_insert_id (old_id);
				}

				(*i)->set_state (**niter, version);

				if (regenerate_xml_or_string_ids ()) {
					(*i)->set_insert_id (new_id);
				}
			}

			/* when copying plugin state, notify UI */
			for (Controls::const_iterator li = controls().begin(); li != controls().end(); ++li) {
				boost::shared_ptr<PBD::Controllable> c = boost::dynamic_pointer_cast<PBD::Controllable> (li->second);
				if (c) {
					c->Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
				}
			}

			break;
		}
	}

	if (version < 3000) {

		/* Only 2.X sessions need a call to set_parameter_state() - in 3.X and above
		   this is all handled by Automatable
		*/

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			if ((*niter)->name() == "Redirect") {
				/* XXX do we need to tackle placement? i think not (pd; oct 16 2009) */
				Processor::set_state (**niter, version);
				break;
			}
		}

		set_parameter_state_2X (node, version);
	}

	node.get_property (X_("custom"), _custom_cfg);

	uint32_t in_maps = 0;
	uint32_t out_maps = 0;
	XMLNodeList kids = node.children ();
	for (XMLNodeIterator i = kids.begin(); i != kids.end(); ++i) {
		if ((*i)->name() == X_("ConfiguredInput")) {
			_configured_in = ChanCount(**i);
		}
		if ((*i)->name() == X_("CustomSinks")) {
			_custom_sinks = ChanCount(**i);
		}
		if ((*i)->name() == X_("ConfiguredOutput")) {
			_custom_out = ChanCount(**i);
			_configured_out = ChanCount(**i);
		}
		if ((*i)->name() == X_("PresetOutput")) {
			_preset_out = ChanCount(**i);
		}
		if (strncmp ((*i)->name ().c_str(), X_("InputMap-"), 9) == 0) {
			long pc = atol (&((*i)->name().c_str()[9]));
			if (pc >= 0 && pc <= (long) get_count()) {
				_in_map[pc] = ChanMapping (**i);
				++in_maps;
			}
		}
		if (strncmp ((*i)->name ().c_str(), X_("OutputMap-"), 10) == 0) {
			long pc = atol (&((*i)->name().c_str()[10]));
			if (pc >= 0 && pc <= (long) get_count()) {
				_out_map[pc] = ChanMapping (**i);
				++out_maps;
			}
		}
		if ((*i)->name () ==  "ThruMap") {
				_thru_map = ChanMapping (**i);
		}

		// sidechain is a Processor (IO)
		if ((*i)->name () ==  Processor::state_node_name) {
			if (!_sidechain) {
				if (regenerate_xml_or_string_ids ()) {
					add_sidechain_from_xml (**i, version);
				} else {
					add_sidechain (0);
				}
			}
			if (!regenerate_xml_or_string_ids ()) {
				_sidechain->set_state (**i, version);
			} else {
				update_sidechain_name ();
			}
		}
	}

	if (in_maps == out_maps && out_maps >0 && out_maps == get_count()) {
		_maps_from_state = true;
	}

	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		if (active()) {
			(*i)->activate ();
		} else {
			(*i)->deactivate ();
		}
	}

	PluginConfigChanged (); /* EMIT SIGNAL */
	return 0;
}

void
PluginInsert::update_id (PBD::ID id)
{
	set_id (id.to_s());
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->set_insert_id (id);
	}
}

void
PluginInsert::set_owner (SessionObject* o)
{
	Processor::set_owner (o);
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->set_owner (o);
	}
}

void
PluginInsert::set_state_dir (const std::string& d)
{
	// state() only saves the state of the first plugin
	_plugins[0]->set_state_dir (d);
}

void
PluginInsert::set_parameter_state_2X (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	/* look for port automation node */

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() != port_automation_node_name) {
			continue;
		}

		XMLNodeList cnodes;
		XMLNodeConstIterator iter;
		XMLNode *child;
		uint32_t port_id;

		cnodes = (*niter)->children ("port");

		for (iter = cnodes.begin(); iter != cnodes.end(); ++iter){

			child = *iter;

			if (!child->get_property("number", port_id)) {
				warning << _("PluginInsert: Auto: no ladspa port number") << endmsg;
				continue;
			}

			if (port_id >= _plugins[0]->parameter_count()) {
				warning << _("PluginInsert: Auto: port id out of range") << endmsg;
				continue;
			}

			boost::shared_ptr<AutomationControl> c = boost::dynamic_pointer_cast<AutomationControl>(
					control(Evoral::Parameter(PluginAutomation, 0, port_id), true));

			if (c && c->alist()) {
				if (!child->children().empty()) {
					c->alist()->set_state (*child->children().front(), version);
				}
			} else {
				error << string_compose (_("PluginInsert: automatable control %1 not found - ignored"), port_id) << endmsg;
			}
		}

		/* done */

		break;
	}
}

boost::shared_ptr<ReadOnlyControl>
PluginInsert::control_output (uint32_t num) const
{
	CtrlOutMap::const_iterator i = _control_outputs.find (num);
	if (i == _control_outputs.end ()) {
		return boost::shared_ptr<ReadOnlyControl> ();
	} else {
		return (*i).second;
	}
}

string
PluginInsert::describe_parameter (Evoral::Parameter param)
{
	if (param.type() == PluginAutomation) {
		return _plugins[0]->describe_parameter (param);
	} else if (param.type() == PluginPropertyAutomation) {
		boost::shared_ptr<AutomationControl> c(automation_control(param));
		if (c && !c->desc().label.empty()) {
			return c->desc().label;
		}
	}
	return Automatable::describe_parameter(param);
}

ARDOUR::samplecnt_t
PluginInsert::signal_latency() const
{
	if (!_pending_active) {
		return 0;
	}
	return plugin_latency ();
}

ARDOUR::PluginType
PluginInsert::type () const
{
	return plugin()->get_info()->type;
}

PluginInsert::PluginControl::PluginControl (PluginInsert*                     p,
                                            const Evoral::Parameter&          param,
                                            const ParameterDescriptor&        desc,
                                            boost::shared_ptr<AutomationList> list)
	: AutomationControl (p->session(), param, desc, list, p->describe_parameter(param))
	, _plugin (p)
{
	if (alist()) {
		if (desc.toggled) {
			list->set_interpolation(Evoral::ControlList::Discrete);
		}
	}
}

/** @param val `user' value */

void
PluginInsert::PluginControl::actually_set_value (double user_val, PBD::Controllable::GroupControlDisposition group_override)
{
	/* FIXME: probably should be taking out some lock here.. */

	for (Plugins::iterator i = _plugin->_plugins.begin(); i != _plugin->_plugins.end(); ++i) {
		(*i)->set_parameter (_list->parameter().id(), user_val, 0);
	}

	boost::shared_ptr<Plugin> iasp = _plugin->_impulseAnalysisPlugin.lock();
	if (iasp) {
		iasp->set_parameter (_list->parameter().id(), user_val, 0);
	}

	AutomationControl::actually_set_value (user_val, group_override);
}

void
PluginInsert::PluginControl::catch_up_with_external_value (double user_val)
{
	AutomationControl::actually_set_value (user_val, Controllable::NoGroup);
}

XMLNode&
PluginInsert::PluginControl::get_state ()
{
	XMLNode& node (AutomationControl::get_state());
	node.set_property (X_("parameter"), parameter().id());

	boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (_plugin->_plugins[0]);
	if (lv2plugin) {
		node.set_property (X_("symbol"), lv2plugin->port_symbol (parameter().id()));
	}

	return node;
}

/** @return `user' val */
double
PluginInsert::PluginControl::get_value () const
{
	boost::shared_ptr<Plugin> plugin = _plugin->plugin (0);

	if (!plugin) {
		return 0.0;
	}

	return plugin->get_parameter (_list->parameter().id());
}

std::string
PluginInsert::PluginControl::get_user_string () const
{
	boost::shared_ptr<Plugin> plugin = _plugin->plugin (0);
	if (plugin) {
		std::string pp;
		if (plugin->print_parameter (parameter().id(), pp) && pp.size () > 0) {
			return pp;
		}
	}
	return AutomationControl::get_user_string ();
}

PluginInsert::PluginPropertyControl::PluginPropertyControl (PluginInsert*                     p,
                                                            const Evoral::Parameter&          param,
                                                            const ParameterDescriptor&        desc,
                                                            boost::shared_ptr<AutomationList> list)
	: AutomationControl (p->session(), param, desc, list)
	, _plugin (p)
{
}

void
PluginInsert::PluginPropertyControl::actually_set_value (double user_val, Controllable::GroupControlDisposition gcd)
{
	/* Old numeric set_value(), coerce to appropriate datatype if possible.
	   This is lossy, but better than nothing until Ardour's automation system
	   can handle various datatypes all the way down. */
	const Variant value(_desc.datatype, user_val);
	if (value.type() == Variant::NOTHING) {
		error << "set_value(double) called for non-numeric property" << endmsg;
		return;
	}

	for (Plugins::iterator i = _plugin->_plugins.begin(); i != _plugin->_plugins.end(); ++i) {
		(*i)->set_property(_list->parameter().id(), value);
	}

	_value = value;

	AutomationControl::actually_set_value (user_val, gcd);
}

XMLNode&
PluginInsert::PluginPropertyControl::get_state ()
{
	XMLNode& node (AutomationControl::get_state());
	node.set_property (X_("property"), parameter().id());
	node.remove_property (X_("value"));

	return node;
}

double
PluginInsert::PluginPropertyControl::get_value () const
{
	return _value.to_double();
}

boost::shared_ptr<Plugin>
PluginInsert::get_impulse_analysis_plugin()
{
	boost::shared_ptr<Plugin> ret;
	if (_impulseAnalysisPlugin.expired()) {
		// LV2 in particular uses various _session params
		// during init() -- most notably block_size..
		// not great.
		ret = plugin_factory(_plugins[0]);
		ret->use_for_impulse_analysis ();
		ChanCount ins = internal_input_streams ();
		ChanCount out (internal_output_streams ());
		ChanCount aux_in;
		if (ret->get_info ()->reconfigurable_io ()) {
			// populate get_info ()->n_inputs and ->n_outputs
			ret->match_variable_io (ins, aux_in, out);
			assert (out == internal_output_streams ());
		}
		ret->reconfigure_io (ins, aux_in, out);
		ret->set_owner (_owner);
		_impulseAnalysisPlugin = ret;

		_plugins[0]->add_slave (ret, false);
		ret->DropReferences.connect_same_thread (*this, boost::bind (&PluginInsert::plugin_removed, this, _impulseAnalysisPlugin));
	} else {
		ret = _impulseAnalysisPlugin.lock();
	}

	return ret;
}

void
PluginInsert::collect_signal_for_analysis (samplecnt_t nframes)
{
	if (_signal_analysis_collect_nsamples_max != 0
			|| _signal_analysis_collect_nsamples  != 0) {
		return;
	}

	// called from outside the audio thread, so this should be safe
	// only do audio as analysis is (currently) only for audio plugins
	_signal_analysis_inputs.ensure_buffers (DataType::AUDIO, input_streams().n_audio(),  nframes);
	_signal_analysis_outputs.ensure_buffers (DataType::AUDIO, output_streams().n_audio(), nframes);

	/* these however should not be set while processing,
	 * however in the given order, this should be fine.
	 */
	_signal_analysis_collect_nsamples     = 0;
	_signal_analysis_collect_nsamples_max = nframes;
}

/** Add a plugin to our list */
void
PluginInsert::add_plugin (boost::shared_ptr<Plugin> plugin)
{
	plugin->set_insert_id (this->id());
	plugin->set_owner (_owner);

	if (_plugins.empty()) {
		/* first (and probably only) plugin instance - connect to relevant signals */

		plugin->ParameterChangedExternally.connect_same_thread (*this, boost::bind (&PluginInsert::parameter_changed_externally, this, _1, _2));
		plugin->StartTouch.connect_same_thread (*this, boost::bind (&PluginInsert::start_touch, this, _1));
		plugin->EndTouch.connect_same_thread (*this, boost::bind (&PluginInsert::end_touch, this, _1));
		_custom_sinks = plugin->get_info()->n_inputs;
		// cache sidechain port count
		_cached_sidechain_pins.reset ();
		const ChanCount& nis (plugin->get_info()->n_inputs);
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t in = 0; in < nis.get (*t); ++in) {
				const Plugin::IOPortDescription& iod (plugin->describe_io_port (*t, true, in));
				if (iod.is_sidechain) {
					_cached_sidechain_pins.set (*t, 1 + _cached_sidechain_pins.n(*t));
				}
			}
		}
	}
#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	boost::shared_ptr<VSTPlugin> vst = boost::dynamic_pointer_cast<VSTPlugin> (plugin);
	if (vst) {
		vst->set_insert (this, _plugins.size ());
	}
#endif

	_plugins.push_back (plugin);

	if (_plugins.size() > 1) {
		_plugins[0]->add_slave (plugin, true);
		plugin->DropReferences.connect_same_thread (*this, boost::bind (&PluginInsert::plugin_removed, this, boost::weak_ptr<Plugin> (plugin)));
	}
}

void
PluginInsert::plugin_removed (boost::weak_ptr<Plugin> wp)
{
	boost::shared_ptr<Plugin> plugin = wp.lock();
	if (_plugins.size () == 0 || !plugin) {
		return;
	}
	_plugins[0]->remove_slave (plugin);
}

void
PluginInsert::add_sidechain_from_xml (const XMLNode& node, int version)
{
	if (version < 3000) {
		return;
	}

	XMLNodeList nlist = node.children();

	if (nlist.size() == 0) {
		return;
	}

	uint32_t audio = 0;
	uint32_t midi = 0;

	XMLNodeConstIterator it = nlist.front()->children().begin();
	for ( ; it != nlist.front()->children().end(); ++ it) {
		if ((*it)->name() == "Port") {
			DataType type(DataType::NIL);
			(*it)->get_property ("type", type);
			if (type == DataType::AUDIO) {
				++audio;
			} else if (type == DataType::MIDI) {
				++midi;
			}
		}
	}

	ChanCount in_cc = ChanCount();
	in_cc.set (DataType::AUDIO, audio);
	in_cc.set (DataType::MIDI, midi);

	add_sidechain (audio, midi);
}

bool
PluginInsert::load_preset (ARDOUR::Plugin::PresetRecord pr)
{
	bool ok = true;
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		if (! (*i)->load_preset (pr)) {
			ok = false;
		}
	}

	boost::shared_ptr<Plugin> iasp = _impulseAnalysisPlugin.lock();
	if (iasp) {
		iasp->load_preset (pr);
	}

	return ok;
}

void
PluginInsert::realtime_handle_transport_stopped ()
{
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
	}
}

void
PluginInsert::realtime_locate (bool for_loop_end)
{
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->realtime_locate (for_loop_end);
	}
}

void
PluginInsert::monitoring_changed ()
{
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->monitoring_changed ();
	}
}

void
PluginInsert::latency_changed ()
{
	// this is called in RT context, LatencyChanged is emitted after run()
	_latency_changed = true;
	LatencyChanged (); /* EMIT SIGNAL */
	// XXX This needs a proper API not an owner() hack:
	// TODO Route should subscribe to LatencyChanged() and forward it
	// to the session as processor_latency_changed.
	assert (owner ());
	static_cast<Route*>(owner ())->processor_latency_changed (); /* EMIT SIGNAL */
}

void
PluginInsert::start_touch (uint32_t param_id)
{
	boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, param_id));
	if (ac) {
		// ToDo subtract _plugin_signal_latency  from audible_sample() when rolling, assert > 0
		ac->start_touch (timepos_t (session().audible_sample()));
	}
}

void
PluginInsert::end_touch (uint32_t param_id)
{
	boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, param_id));
	if (ac) {
		// ToDo subtract _plugin_signal_latency  from audible_sample() when rolling, assert > 0
		ac->stop_touch (timepos_t (session().audible_sample()));
	}
}

bool
PluginInsert::provides_stats () const
{
#if defined MIXBUS && defined NDEBUG
	if (is_channelstrip () || !display_to_user ()) {
		return false;
	}
#endif
	return true;
}

bool
PluginInsert::get_stats (PBD::microseconds_t& min, PBD::microseconds_t& max, double& avg, double& dev) const
{
	/* TODO: consider taking a try/lock: Don't run concurrently with
	 * TimingStats::update, TimingStats::reset.
	 */
	return _timing_stats.get_stats (min, max, avg, dev);
}

void
PluginInsert::clear_stats ()
{
	g_atomic_int_set (&_stat_reset, 1);
}

std::ostream& operator<<(std::ostream& o, const ARDOUR::PluginInsert::Match& m)
{
	switch (m.method) {
		case PluginInsert::Impossible: o << "Impossible"; break;
		case PluginInsert::Delegate:   o << "Delegate"; break;
		case PluginInsert::NoInputs:   o << "NoInputs"; break;
		case PluginInsert::ExactMatch: o << "ExactMatch"; break;
		case PluginInsert::Replicate:  o << "Replicate"; break;
		case PluginInsert::Split:      o << "Split"; break;
		case PluginInsert::Hide:       o << "Hide"; break;
	}
	o << " cnt: " << m.plugins
		<< (m.strict_io ? " strict-io" : "")
		<< (m.custom_cfg ? " custom-cfg" : "");
	if (m.method == PluginInsert::Hide) {
		o << " hide: " << m.hide;
	}
	o << "\n";
	return o;
}
