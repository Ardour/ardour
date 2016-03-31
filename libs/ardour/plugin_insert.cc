/*
    Copyright (C) 2000 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <string>

#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/convert.h"

#include "ardour/audio_buffer.h"
#include "ardour/automation_list.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/event_type_map.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/luaproc.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"

#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#endif

#ifdef LXVST_SUPPORT
#include "ardour/lxvst_plugin.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#endif

#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

const string PluginInsert::port_automation_node_name = "PortAutomation";

PluginInsert::PluginInsert (Session& s, boost::shared_ptr<Plugin> plug)
	: Processor (s, (plug ? plug->name() : string ("toBeRenamed")))
	, _signal_analysis_collected_nframes(0)
	, _signal_analysis_collect_nframes_max(0)
	, _configured (false)
	, _no_inplace (false)
	, _strict_io (false)
	, _custom_cfg (false)
	, _pending_no_inplace (false)
{
	/* the first is the master */

	if (plug) {
		add_plugin (plug);
		create_automatable_parameters ();
	}
}

PluginInsert::~PluginInsert ()
{
}

void
PluginInsert::set_strict_io (bool b)
{
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

	/* this is a bad idea.... we shouldn't do this while active.
	   only a route holding their redirect_lock should be calling this
	*/

	if (num == 0) {
		return false;
	} else if (num > _plugins.size()) {
		uint32_t diff = num - _plugins.size();

		for (uint32_t n = 0; n < diff; ++n) {
			boost::shared_ptr<Plugin> p = plugin_factory (_plugins[0]);
			add_plugin (p);
			if (active ()) {
				p->activate ();
			}

			if (require_state) {
				/* XXX do something */
			}
		}
		PluginConfigChanged (); /* EMIT SIGNAL */

	} else if (num < _plugins.size()) {
		uint32_t diff = _plugins.size() - num;
		for (uint32_t n= 0; n < diff; ++n) {
			_plugins.pop_back();
		}
		PluginConfigChanged (); /* EMIT SIGNAL */
	}

	return true;
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

void
PluginInsert::control_list_automation_state_changed (Evoral::Parameter which, AutoState s)
{
	if (which.type() != PluginAutomation)
		return;

	boost::shared_ptr<AutomationControl> c
			= boost::dynamic_pointer_cast<AutomationControl>(control (which));

	if (c && s != Off) {
		_plugins[0]->set_parameter (which.id(), c->list()->eval (_session.transport_frame()));
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
		assert (_plugins.size() == 1);
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
	return _plugins[0]->get_info()->n_outputs;
}

ChanCount
PluginInsert::natural_input_streams() const
{
	return _plugins[0]->get_info()->n_inputs;
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

bool
PluginInsert::is_midi_instrument() const
{
	/* XXX more finesse is possible here. VST plugins have a
	   a specific "instrument" flag, for example.
	 */
	PluginInfoPtr pi = _plugins[0]->get_info();

	return pi->n_inputs.n_midi() != 0 &&
		pi->n_outputs.n_audio() > 0;
}

void
PluginInsert::create_automatable_parameters ()
{
	assert (!_plugins.empty());

	set<Evoral::Parameter> a = _plugins.front()->automatable ();

	for (set<Evoral::Parameter>::iterator i = a.begin(); i != a.end(); ++i) {
		if (i->type() == PluginAutomation) {

			Evoral::Parameter param(*i);

			ParameterDescriptor desc;
			_plugins.front()->get_parameter_descriptor(i->id(), desc);

			can_automate (param);
			boost::shared_ptr<AutomationList> list(new AutomationList(param, desc));
			boost::shared_ptr<AutomationControl> c (new PluginControl(this, param, desc, list));
			add_control (c);
			_plugins.front()->set_automation_control (i->id(), c);
		} else if (i->type() == PluginPropertyAutomation) {
			Evoral::Parameter param(*i);
			const ParameterDescriptor& desc = _plugins.front()->get_property_descriptor(param.id());
			if (desc.datatype != Variant::NOTHING) {
				boost::shared_ptr<AutomationList> list;
				if (Variant::type_is_numeric(desc.datatype)) {
					list = boost::shared_ptr<AutomationList>(new AutomationList(param, desc));
				}
				add_control (boost::shared_ptr<AutomationControl> (new PluginPropertyControl(this, param, desc, list)));
			}
		}
	}
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
			(*i)->set_parameter (which, val);
		}
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
PluginInsert::activate ()
{
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->activate ();
	}

	Processor::activate ();
}

void
PluginInsert::deactivate ()
{
	Processor::deactivate ();

	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->deactivate ();
	}
}

void
PluginInsert::flush ()
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->flush ();
	}
}

void
PluginInsert::connect_and_run (BufferSet& bufs, pframes_t nframes, framecnt_t offset, bool with_auto, framepos_t now)
{
	PinMappings in_map (_in_map);
	PinMappings out_map (_out_map);

#if 1
	// auto-detect if inplace processing is possible
	// TODO: do this once. during configure_io and every time the
	// plugin-count or mapping changes.
	bool inplace_ok = true;
	for (uint32_t pc = 0; pc < get_count() && inplace_ok ; ++pc) {
		if (!in_map[pc].is_monotonic ()) {
			inplace_ok = false;
		}
		if (!out_map[pc].is_monotonic ()) {
			inplace_ok = false;
		}
	}

	if (_pending_no_inplace != !inplace_ok) {
#ifndef NDEBUG // this 'cerr' needs to go ASAP.
		cerr << name () << " automatically set : " << (inplace_ok ? "Use Inplace" : "No Inplace") << "\n"; // XXX
#endif
		_pending_no_inplace = !inplace_ok;
	}
#endif

	_no_inplace = _pending_no_inplace || _plugins.front()->inplace_broken ();


#if 1
	// TODO optimize special case.
	// Currently this never triggers because the in_map for "Split" triggeres no_inplace.
	if (_match.method == Split && !_no_inplace) {
		assert (in_map.size () == 1);
		in_map[0] = ChanMapping (ChanCount::max (natural_input_streams (), _configured_in));
		ChanCount const in_streams = internal_input_streams ();
		/* copy the first stream's audio buffer contents to the others */
		bool valid;
		uint32_t first_idx = in_map[0].get (DataType::AUDIO, 0, &valid);
		if (valid) {
			for (uint32_t i = in_streams.n_audio(); i < natural_input_streams().n_audio(); ++i) {
				uint32_t idx = in_map[0].get (DataType::AUDIO, i, &valid);
				if (valid) {
					bufs.get_audio(idx).read_from(bufs.get_audio(first_idx), nframes, offset, offset);
				}
			}
		}
	}
#endif

	bufs.set_count(ChanCount::max(bufs.count(), _configured_in));
	bufs.set_count(ChanCount::max(bufs.count(), _configured_out));

	if (with_auto) {

		uint32_t n = 0;

		for (Controls::iterator li = controls().begin(); li != controls().end(); ++li, ++n) {

			boost::shared_ptr<AutomationControl> c
				= boost::dynamic_pointer_cast<AutomationControl>(li->second);

			if (c->list() && c->automation_playback()) {
				bool valid;

				const float val = c->list()->rt_safe_eval (now, valid);

				if (valid) {
					/* This is the ONLY place where we are
					 *  allowed to call
					 *  AutomationControl::set_value_unchecked(). We
					 *  know that the control is in
					 *  automation playback mode, so no
					 *  check on writable() is required
					 *  (which must be done in AutomationControl::set_value()
					 *
					 */
					c->set_value_unchecked(val);
				}

			}
		}
	}

	/* Calculate if, and how many frames we need to collect for analysis */
	framecnt_t collect_signal_nframes = (_signal_analysis_collect_nframes_max -
					     _signal_analysis_collected_nframes);
	if (nframes < collect_signal_nframes) { // we might not get all frames now
		collect_signal_nframes = nframes;
	}

	if (collect_signal_nframes > 0) {
		// collect input
		//std::cerr << "collect input, bufs " << bufs.count().n_audio() << " count,  " << bufs.available().n_audio() << " available" << std::endl;
		//std::cerr << "               streams " << internal_input_streams().n_audio() << std::endl;
		//std::cerr << "filling buffer with " << collect_signal_nframes << " frames at " << _signal_analysis_collected_nframes << std::endl;

		_signal_analysis_inputs.set_count(internal_input_streams());

		for (uint32_t i = 0; i < internal_input_streams().n_audio(); ++i) {
			_signal_analysis_inputs.get_audio(i).read_from(
				bufs.get_audio(i),
				collect_signal_nframes,
				_signal_analysis_collected_nframes); // offset is for target buffer
		}

	}
#ifdef MIXBUS
	if (_plugins.front()->is_channelstrip() ) {
		if (_configured_in.n_audio() > 0) {
			ChanMapping mb_in_map (ChanCount::min (_configured_in, ChanCount (DataType::AUDIO, 2)));
			ChanMapping mb_out_map (ChanCount::min (_configured_out, ChanCount (DataType::AUDIO, 2)));

			_plugins.front()->connect_and_run (bufs, mb_in_map, mb_out_map, nframes, offset);

			for (uint32_t out = _configured_in.n_audio (); out < bufs.count().get (DataType::AUDIO); ++out) {
				bufs.get (DataType::AUDIO, out).silence (nframes, offset);
			}
		}
	} else
#endif
	if (_no_inplace) {
		BufferSet& inplace_bufs  = _session.get_noinplace_buffers();
		ARDOUR::ChanMapping used_outputs;

		uint32_t pc = 0;
		// TODO optimize this flow. prepare during configure_io()
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {

			ARDOUR::ChanMapping i_in_map (natural_input_streams());
			ARDOUR::ChanMapping i_out_map;
			ARDOUR::ChanCount mapped;
			ARDOUR::ChanCount backmap;

			// map inputs sequentially
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				for (uint32_t in = 0; in < natural_input_streams().get (*t); ++in) {
					bool valid;
					uint32_t in_idx = in_map[pc].get (*t, in, &valid);
					uint32_t m = mapped.get (*t);
					if (valid) {
						inplace_bufs.get (*t, m).read_from (bufs.get (*t, in_idx), nframes, offset, offset);
					} else {
						inplace_bufs.get (*t, m).silence (nframes, offset);
					}
					mapped.set (*t, m + 1);
				}
			}

			// TODO use map_offset_to()  instead ??
			backmap = mapped;

			// map outputs
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				for (uint32_t out = 0; out < natural_output_streams().get (*t); ++out) {
					uint32_t m = mapped.get (*t);
					inplace_bufs.get (*t, m).silence (nframes, offset);
					i_out_map.set (*t, out, m);
					mapped.set (*t, m + 1);
				}
			}

			if ((*i)->connect_and_run(inplace_bufs, i_in_map, i_out_map, nframes, offset)) {
				deactivate ();
			}

			// copy back outputs
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				for (uint32_t out = 0; out < natural_output_streams().get (*t); ++out) {
					uint32_t m = backmap.get (*t);
					bool valid;
					uint32_t out_idx = out_map[pc].get (*t, out, &valid);
					if (valid) {
						bufs.get (*t, out_idx).read_from (inplace_bufs.get (*t, m), nframes, offset, offset);
						used_outputs.set (*t, out_idx, 1); // mark as used
					}
					backmap.set (*t, m + 1);
				}
			}
		}
		/* all instances have completed, now clear outputs that have not been written to.
		 * (except midi bypass)
		 */
		if (has_midi_bypass ()) {
			used_outputs.set (DataType::MIDI, 0, 1); // Midi bypass.
		}
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < bufs.count().get (*t); ++out) {
				bool valid;
				used_outputs.get (*t, out, &valid);
				if (valid) { continue; }
				bufs.get (*t, out).silence (nframes, offset);
			}
		}

	} else {
		uint32_t pc = 0;
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			if ((*i)->connect_and_run(bufs, in_map[pc], out_map[pc], nframes, offset)) {
				deactivate ();
			}
		}

		// TODO optimize: store "unconnected" in a fixed set.
		// it only changes on reconfiguration.
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t out = 0; out < bufs.count().get (*t); ++out) {
				bool mapped = false;
				if (*t == DataType::MIDI && out == 0 && has_midi_bypass ()) {
					mapped = true; // in-place Midi bypass
				}
				for (uint32_t pc = 0; pc < get_count() && !mapped; ++pc) {
					for (uint32_t o = 0; o < natural_output_streams().get (*t); ++o) {
						bool valid;
						uint32_t idx = out_map[pc].get (*t, o, &valid);
						if (valid && idx == out) {
							mapped = true;
							break;
						}
					}
				}
				if (!mapped) {
					bufs.get (*t, out).silence (nframes, offset);
				}
			}
		}
	}

	if (collect_signal_nframes > 0) {
		// collect output
		//std::cerr << "       output, bufs " << bufs.count().n_audio() << " count,  " << bufs.available().n_audio() << " available" << std::endl;
		//std::cerr << "               streams " << internal_output_streams().n_audio() << std::endl;

		_signal_analysis_outputs.set_count(internal_output_streams());

		for (uint32_t i = 0; i < internal_output_streams().n_audio(); ++i) {
			_signal_analysis_outputs.get_audio(i).read_from(
				bufs.get_audio(i),
				collect_signal_nframes,
				_signal_analysis_collected_nframes); // offset is for target buffer
		}

		_signal_analysis_collected_nframes += collect_signal_nframes;
		assert(_signal_analysis_collected_nframes <= _signal_analysis_collect_nframes_max);

		if (_signal_analysis_collected_nframes == _signal_analysis_collect_nframes_max) {
			_signal_analysis_collect_nframes_max = 0;
			_signal_analysis_collected_nframes   = 0;

			AnalysisDataGathered(&_signal_analysis_inputs,
					     &_signal_analysis_outputs);
		}
	}
}

void
PluginInsert::silence (framecnt_t nframes)
{
	if (!active ()) {
		return;
	}

	ChanMapping in_map (natural_input_streams ());
	ChanMapping out_map (natural_output_streams ());

	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->connect_and_run (_session.get_scratch_buffers ((*i)->get_info()->n_inputs, true), in_map, out_map, nframes, 0);
	}
}

void
PluginInsert::run (BufferSet& bufs, framepos_t start_frame, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (_pending_active) {
		/* run as normal if we are active or moving from inactive to active */

		if (_session.transport_rolling() || _session.bounce_processing()) {
			automation_run (bufs, start_frame, nframes);
		} else {
			connect_and_run (bufs, nframes, 0, false);
		}

	} else {
		// TODO use mapping in bypassed mode ?!
		// -> do we bypass the processor or the plugin

		uint32_t in = input_streams ().n_audio ();
		uint32_t out = output_streams().n_audio ();

		if (has_no_audio_inputs() || in == 0) {

			/* silence all (audio) outputs. Should really declick
			 * at the transitions of "active"
			 */

			for (uint32_t n = 0; n < out; ++n) {
				bufs.get_audio (n).silence (nframes);
			}

		} else if (out > in) {

			/* not active, but something has make up for any channel count increase
			 * for now , simply replicate last buffer
			 */
			for (uint32_t n = in; n < out; ++n) {
				bufs.get_audio(n).read_from(bufs.get_audio(in - 1), nframes);
			}
		}

		bufs.count().set_audio (out);
	}

	_active = _pending_active;

	/* we have no idea whether the plugin generated silence or not, so mark
	 * all buffers appropriately.
	 */
}

void
PluginInsert::automation_run (BufferSet& bufs, framepos_t start, pframes_t nframes)
{
	Evoral::ControlEvent next_event (0, 0.0f);
	framepos_t now = start;
	framepos_t end = now + nframes;
	framecnt_t offset = 0;

	Glib::Threads::Mutex::Lock lm (control_lock(), Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		connect_and_run (bufs, nframes, offset, false);
		return;
	}

	if (!find_next_event (now, end, next_event) || _plugins.front()->requires_fixed_sized_buffers()) {

		/* no events have a time within the relevant range */

		connect_and_run (bufs, nframes, offset, true, now);
		return;
	}

	while (nframes) {

		framecnt_t cnt = min (((framecnt_t) ceil (next_event.when) - now), (framecnt_t) nframes);

		connect_and_run (bufs, cnt, offset, true, now);

		nframes -= cnt;
		offset += cnt;
		now += cnt;

		if (!find_next_event (now, end, next_event)) {
			break;
		}
	}

	/* cleanup anything that is left to do */

	if (nframes) {
		connect_and_run (bufs, nframes, offset, true, now);
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
#ifdef LV2_SUPPORT
	boost::shared_ptr<LV2Plugin> lv2p;
#endif
#ifdef WINDOWS_VST_SUPPORT
	boost::shared_ptr<WindowsVSTPlugin> vp;
#endif
#ifdef LXVST_SUPPORT
	boost::shared_ptr<LXVSTPlugin> lxvp;
#endif
#ifdef AUDIOUNIT_SUPPORT
	boost::shared_ptr<AUPlugin> ap;
#endif

	if ((lp = boost::dynamic_pointer_cast<LadspaPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LadspaPlugin (*lp));
	} else if ((lua = boost::dynamic_pointer_cast<LuaProc> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LuaProc (*lua));
#ifdef LV2_SUPPORT
	} else if ((lv2p = boost::dynamic_pointer_cast<LV2Plugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LV2Plugin (*lv2p));
#endif
#ifdef WINDOWS_VST_SUPPORT
	} else if ((vp = boost::dynamic_pointer_cast<WindowsVSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new WindowsVSTPlugin (*vp));
#endif
#ifdef LXVST_SUPPORT
	} else if ((lxvp = boost::dynamic_pointer_cast<LXVSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LXVSTPlugin (*lxvp));
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
		if (changed) {
			PluginMapChanged (); /* EMIT SIGNAL */
		}
	}
}

void
PluginInsert::set_output_map (uint32_t num, ChanMapping m) {
	if (num < _out_map.size()) {
		bool changed = _out_map[num] != m;
		_out_map[num] = m;
		if (changed) {
			PluginMapChanged (); /* EMIT SIGNAL */
		}
	}
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
	if (_configured_in.n_midi () == 1 && _configured_out.n_midi () == 1 && natural_output_streams ().n_midi () == 0) {
		return true;
	}
	return false;
}

bool
PluginInsert::configure_io (ChanCount in, ChanCount out)
{
	Match old_match = _match;
	ChanCount old_in;
	ChanCount old_out;

	if (_configured) {
		old_in = _configured_in;
		old_out = _configured_out;
	}

	_configured_in = in;
	_configured_out = out;

	/* get plugin configuration */
	_match = private_can_support_io_configuration (in, out);
#ifndef NDEBUG // XXX
	cout << "Match '" << name() << "': " << _match;
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
		if (_plugins.front()->configure_io (natural_input_streams(), out) == false) {
			PluginIoReConfigure (); /* EMIT SIGNAL */
			_configured = false;
			return false;
		}
		break;
	case Delegate:
		{
			ChanCount dout;
			ChanCount useins;
			bool const r = _plugins.front()->can_support_io_configuration (in, dout, &useins);
			assert (r);
			assert (_match.strict_io || dout.n_audio() == out.n_audio()); // sans midi bypass
			if (useins.n_audio() == 0) {
				useins = in;
			}
			if (_plugins.front()->configure_io (useins, dout) == false) {
				PluginIoReConfigure (); /* EMIT SIGNAL */
				_configured = false;
				return false;
			}
		}
		break;
	default:
		if (_plugins.front()->configure_io (in, out) == false) {
			PluginIoReConfigure (); /* EMIT SIGNAL */
			_configured = false;
			return false;
		}
		break;
	}

	bool mapping_changed = false;
	if (old_in == in && old_out == out && _configured
			&& old_match.method == _match.method
			&& _in_map.size() == _out_map.size()
			&& _in_map.size() == get_count ()
		 ) {
		/* If the configuraton has not changed, keep the mapping */
	} else if (_match.custom_cfg && _configured) {
		/* strip dead wood */
		for (uint32_t pc = 0; pc < get_count(); ++pc) {
			ChanMapping new_in;
			ChanMapping new_out;
			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				for (uint32_t i = 0; i < natural_input_streams().get (*t); ++i) {
					bool valid;
					uint32_t idx = _in_map[pc].get (*t, i, &valid);
					if (valid && idx <= in.get (*t)) {
						new_in.set (*t, i, idx);
					}
				}
				for (uint32_t o = 0; o < natural_output_streams().get (*t); ++o) {
					bool valid;
					uint32_t idx = _out_map[pc].get (*t, o, &valid);
					if (valid && idx <= out.get (*t)) {
						new_out.set (*t, o, idx);
					}
				}
			}
			if (_in_map[pc] != new_in || _out_map[pc] != new_out) {
				mapping_changed = true;
			}
			_in_map[pc] = new_in;
			_out_map[pc] = new_out;
		}
	} else {
		/* generate a new mapping */
		uint32_t pc = 0;
		_in_map.clear ();
		_out_map.clear ();
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			if (_match.method == Split) {
				_in_map[pc] = ChanMapping ();
				/* connect inputs in round-robin fashion */
				for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
					const uint32_t cend = _configured_in.get (*t);
					if (cend == 0) { continue; }
					uint32_t c = 0;
					for (uint32_t in = 0; in < natural_input_streams().get (*t); ++in) {
						_in_map[pc].set (*t, in, c);
						c = c + 1 % cend;
					}
				}
			} else {
				_in_map[pc] = ChanMapping (ChanCount::min (natural_input_streams (), in));
			}
			_out_map[pc] = ChanMapping (ChanCount::min (natural_output_streams(), out));

			for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
				_in_map[pc].offset_to(*t, pc * natural_input_streams().get(*t));
				_out_map[pc].offset_to(*t, pc * natural_output_streams().get(*t));
			}
			mapping_changed = true;
		}
	}

	if (mapping_changed) {
		PluginMapChanged (); /* EMIT SIGNAL */
#ifndef NDEBUG // XXX
		uint32_t pc = 0;
		cout << "----<<----\n";
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			cout << "Channel Map for " << name() << " plugin " << pc << "\n";
			cout << " * Inputs:\n" << _in_map[pc];
			cout << " * Outputs:\n" << _out_map[pc];
		}
		cout << "---->>----\n";
#endif
	}

	if (old_in != in || old_out != out
			|| (old_match.method != _match.method && (old_match.method == Split || _match.method == Split))
		 ) {
		PluginIoReConfigure (); /* EMIT SIGNAL */
	}

	// we don't know the analysis window size, so we must work with the
	// current buffer size here. each request for data fills in these
	// buffers and the analyser makes sure it gets enough data for the
	// analysis window
	session().ensure_buffer_set (_signal_analysis_inputs, in);
	//_signal_analysis_inputs.set_count (in);

	session().ensure_buffer_set (_signal_analysis_outputs, out);
	//_signal_analysis_outputs.set_count (out);

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
	return private_can_support_io_configuration (in, out).method != Impossible;
}

/** A private version of can_support_io_configuration which returns the method
 *  by which the configuration can be matched, rather than just whether or not
 *  it can be.
 */
PluginInsert::Match
PluginInsert::private_can_support_io_configuration (ChanCount const & inx, ChanCount& out) const
{
	if (_plugins.empty()) {
		return Match();
	}

	/* if a user specified a custom cfg, so be it. */
	if (_custom_cfg) {
		out = _custom_out;
		return Match (ExactMatch, get_count(), false, true); // XXX
	}

	/* try automatic configuration */
	Match m = PluginInsert::automatic_can_support_io_configuration (inx, out);

	PluginInfoPtr info = _plugins.front()->get_info();
	ChanCount inputs  = info->n_inputs;
	ChanCount outputs = info->n_outputs;
	ChanCount midi_bypass;
	if (inx.get(DataType::MIDI) == 1 && outputs.get(DataType::MIDI) == 0) {
		midi_bypass.set (DataType::MIDI, 1);
	}

	/* handle case strict-i/o */
	if (_strict_io && m.method != Impossible) {
		m.strict_io = true;

		/* special case MIDI instruments */
		if (is_midi_instrument()) {
			// output = midi-bypass + at most master-out channels.
			ChanCount max_out (DataType::AUDIO, 2); // TODO use master-out
			max_out.set (DataType::MIDI, out.get(DataType::MIDI));
			out = ChanCount::min (out, max_out);
			return m;
		}

		switch (m.method) {
			case NoInputs:
				if (inx.n_audio () != out.n_audio ()) { // ignore midi bypass
					/* replicate processor to match output count (generators and such)
					 * at least enough to feed every output port. */
					uint32_t f = 1; // at least one. e.g. control data filters, no in, no out.
					for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
						uint32_t nin = inputs.get (*t);
						if (nin == 0 || inx.get(*t) == 0) { continue; }
						f = max (f, (uint32_t) ceil (inx.get(*t) / (float)nin));
					}
					out = inx + midi_bypass;
					return Match (Replicate, f);
				}
				break;
			case Split:
				break;
			default:
				break;
		}

		out = inx + midi_bypass;
		if (inx.get(DataType::MIDI) == 1
				&& out.get (DataType::MIDI) == 0
				&& outputs.get(DataType::MIDI) == 0) {
			out += ChanCount (DataType::MIDI, 1);
		}
		return m;
	}

	if (m.method != Impossible) {
		return m;
	}

	if (info->reconfigurable_io()) {
		ChanCount useins;
		bool const r = _plugins.front()->can_support_io_configuration (inx, out, &useins);
		if (!r) {
			// houston, we have a problem.
			return Match (Impossible, 0);
		}
		return Match (Delegate, 1);
	}

	// add at least as many plugins so that output count matches input count
	uint32_t f = 0;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t nin = inputs.get (*t);
		uint32_t nout = outputs.get (*t);
		if (nin == 0 || inx.get(*t) == 0) { continue; }
		// prefer floor() so the count won't overly increase IFF (nin < nout)
		f = max (f, (uint32_t) floor (inx.get(*t) / (float)nout));
	}
	if (f > 0 && outputs * f >= _configured_out) {
		out = outputs * f + midi_bypass;
		return Match (Replicate, f);
	}

	// add at least as many plugins needed to connect all inputs
	f = 1;
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t nin = inputs.get (*t);
		if (nin == 0 || inx.get(*t) == 0) { continue; }
		f = max (f, (uint32_t) ceil (inx.get(*t) / (float)nin));
	}
	out = outputs * f + midi_bypass;
	return Match (Replicate, f);
}

/* this is the original Ardour 3/4 behavior, mainly for backwards compatibility */
PluginInsert::Match
PluginInsert::automatic_can_support_io_configuration (ChanCount const & inx, ChanCount& out) const
{
	if (_plugins.empty()) {
		return Match();
	}

	PluginInfoPtr info = _plugins.front()->get_info();
	ChanCount in; in += inx;
	ChanCount midi_bypass;

	if (info->reconfigurable_io()) {
		/* Plugin has flexible I/O, so delegate to it */
		bool const r = _plugins.front()->can_support_io_configuration (in, out);
		if (!r) {
			return Match (Impossible, 0);
		}
		return Match (Delegate, 1);
	}

	ChanCount inputs  = info->n_inputs;
	ChanCount outputs = info->n_outputs;

	if (in.get(DataType::MIDI) == 1 && outputs.get(DataType::MIDI) == 0) {
		DEBUG_TRACE ( DEBUG::Processors, string_compose ("bypassing midi-data around %1\n", name()));
		midi_bypass.set (DataType::MIDI, 1);
	}
	if (in.get(DataType::MIDI) == 1 && inputs.get(DataType::MIDI) == 0) {
		DEBUG_TRACE ( DEBUG::Processors, string_compose ("hiding midi-port from plugin %1\n", name()));
		in.set(DataType::MIDI, 0);
	}

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

	/* Plugin inputs match requested inputs exactly */
	if (inputs == in) {
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

		uint32_t nin = inputs.get (*t);

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

		bool const can_split_type = (in.get (*t) == 1 && inputs.get (*t) > 1);
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
PluginInsert::get_state ()
{
	return state (true);
}

XMLNode&
PluginInsert::state (bool full)
{
	XMLNode& node = Processor::state (full);

	node.add_property("type", _plugins[0]->state_node_name());
	node.add_property("unique-id", _plugins[0]->unique_id());
	node.add_property("count", string_compose("%1", _plugins.size()));

	/* remember actual i/o configuration (for later placeholder
	 * in case the plugin goes missing) */
	node.add_child_nocopy (* _configured_in.state (X_("ConfiguredInput")));
	node.add_child_nocopy (* _configured_out.state (X_("ConfiguredOutput")));

	/* save custom i/o config */
	node.add_property("custom", _custom_cfg ? "yes" : "no");
	if (_custom_cfg) {
		assert (_custom_out == _configured_out); // redundant
		for (uint32_t pc = 0; pc < get_count(); ++pc) {
			// TODO save _in_map[pc], _out_map[pc]
		}
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
	XMLNodeConstIterator iter;
	set<Evoral::Parameter>::const_iterator p;

	for (iter = nlist.begin(); iter != nlist.end(); ++iter) {
		if ((*iter)->name() == Controllable::xml_node_name) {
			const XMLProperty* prop;

			uint32_t p = (uint32_t)-1;
#ifdef LV2_SUPPORT
			if ((prop = (*iter)->property (X_("symbol"))) != 0) {
				boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (_plugins[0]);
				if (lv2plugin) {
					p = lv2plugin->port_index(prop->value().c_str());
				}
			}
#endif
			if (p == (uint32_t)-1 && (prop = (*iter)->property (X_("parameter"))) != 0) {
				p = atoi (prop->value());
			}

			if (p != (uint32_t)-1) {

				/* this may create the new controllable */

				boost::shared_ptr<Evoral::Control> c = control (Evoral::Parameter (PluginAutomation, 0, p));

#ifndef NO_PLUGIN_STATE
				if (!c) {
					continue;
				}
				boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c);
				if (ac) {
					ac->set_state (**iter, version);
				}
#endif
			}
		}
	}
}

int
PluginInsert::set_state(const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	const XMLProperty *prop;
	ARDOUR::PluginType type;

	if ((prop = node.property ("type")) == 0) {
		error << _("XML node describing plugin is missing the `type' field") << endmsg;
		return -1;
	}

	if (prop->value() == X_("ladspa") || prop->value() == X_("Ladspa")) { /* handle old school sessions */
		type = ARDOUR::LADSPA;
	} else if (prop->value() == X_("lv2")) {
		type = ARDOUR::LV2;
	} else if (prop->value() == X_("windows-vst")) {
		type = ARDOUR::Windows_VST;
	} else if (prop->value() == X_("lxvst")) {
		type = ARDOUR::LXVST;
	} else if (prop->value() == X_("audiounit")) {
		type = ARDOUR::AudioUnit;
	} else if (prop->value() == X_("luaproc")) {
		type = ARDOUR::Lua;
	} else {
		error << string_compose (_("unknown plugin type %1 in plugin insert state"),
				  prop->value())
		      << endmsg;
		return -1;
	}

	prop = node.property ("unique-id");

	if (prop == 0) {
#ifdef WINDOWS_VST_SUPPORT
		/* older sessions contain VST plugins with only an "id" field.
		 */

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

	boost::shared_ptr<Plugin> plugin = find_plugin (_session, prop->value(), type);

	/* treat linux and windows VST plugins equivalent if they have the same uniqueID
	 * allow to move sessions windows <> linux */
#ifdef LXVST_SUPPORT
	if (plugin == 0 && type == ARDOUR::Windows_VST) {
		type = ARDOUR::LXVST;
		plugin = find_plugin (_session, prop->value(), type);
	}
#endif

#ifdef WINDOWS_VST_SUPPORT
	if (plugin == 0 && type == ARDOUR::LXVST) {
		type = ARDOUR::Windows_VST;
		plugin = find_plugin (_session, prop->value(), type);
	}
#endif

	if (plugin == 0) {
		error << string_compose(
			_("Found a reference to a plugin (\"%1\") that is unknown.\n"
			  "Perhaps it was removed or moved since it was last used."),
			prop->value())
		      << endmsg;
		return -1;
	}

	if (type == ARDOUR::Lua) {
		XMLNode *ls = node.child (plugin->state_node_name().c_str());
		// we need to load the script to set the name and parameters.
		boost::shared_ptr<LuaProc> lp = boost::dynamic_pointer_cast<LuaProc>(plugin);
		if (ls && lp) {
			lp->set_script_from_state (*ls);
		}
	}

	// The name of the PluginInsert comes from the plugin, nothing else
	_name = plugin->get_info()->name;

	uint32_t count = 1;

	// Processor::set_state() will set this, but too late
	// for it to be available when setting up plugin
	// state. We can't call Processor::set_state() until
	// the plugins themselves are created and added.

	set_id (node);

	if (_plugins.empty()) {
		/* if we are adding the first plugin, we will need to set
		   up automatable controls.
		*/
		add_plugin (plugin);
		create_automatable_parameters ();
		set_control_ids (node, version);
	}

	if ((prop = node.property ("count")) != 0) {
		sscanf (prop->value().c_str(), "%u", &count);
	}

	if (_plugins.size() != count) {
		for (uint32_t n = 1; n < count; ++n) {
			add_plugin (plugin_factory (plugin));
		}
	}

	Processor::set_state (node, version);

	PBD::ID new_id = this->id();
	PBD::ID old_id = this->id();

	if ((prop = node.property ("id")) != 0) {
		old_id = prop->value ();
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		/* find the node with the type-specific node name ("lv2", "ladspa", etc)
		   and set all plugins to the same state.
		*/

		if ((*niter)->name() == plugin->state_node_name()) {

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

	if ((prop = node.property (X_("custom"))) != 0) {
		_custom_cfg = string_is_affirmative (prop->value());
	}

	XMLNodeList kids = node.children ();
	for (XMLNodeIterator i = kids.begin(); i != kids.end(); ++i) {
		if ((*i)->name() == X_("ConfiguredOutput")) {
			_custom_out = ChanCount(**i);
		}
		// TODO restore mappings for all 0 .. count.
	}



	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		if (active()) {
			(*i)->activate ();
		} else {
			(*i)->deactivate ();
		}
	}

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
		XMLProperty *cprop;
		XMLNodeConstIterator iter;
		XMLNode *child;
		const char *port;
		uint32_t port_id;

		cnodes = (*niter)->children ("port");

		for (iter = cnodes.begin(); iter != cnodes.end(); ++iter){

			child = *iter;

			if ((cprop = child->property("number")) != 0) {
				port = cprop->value().c_str();
			} else {
				warning << _("PluginInsert: Auto: no ladspa port number") << endmsg;
				continue;
			}

			sscanf (port, "%" PRIu32, &port_id);

			if (port_id >= _plugins[0]->parameter_count()) {
				warning << _("PluginInsert: Auto: port id out of range") << endmsg;
				continue;
			}

			boost::shared_ptr<AutomationControl> c = boost::dynamic_pointer_cast<AutomationControl>(
					control(Evoral::Parameter(PluginAutomation, 0, port_id), true));

			if (c && c->alist()) {
				if (!child->children().empty()) {
					c->alist()->set_state (*child->children().front(), version);

					/* In some cases 2.X saves lists with min_yval and max_yval
					   being FLT_MIN and FLT_MAX respectively.  This causes problems
					   in A3 because these min/max values are used to compute
					   where GUI control points should be drawn.  If we see such
					   values, `correct' them to the min/max of the appropriate
					   parameter.
					*/

					float min_y = c->alist()->get_min_y ();
					float max_y = c->alist()->get_max_y ();

					ParameterDescriptor desc;
					_plugins.front()->get_parameter_descriptor (port_id, desc);

					if (min_y == FLT_MIN) {
						min_y = desc.lower;
					}

					if (max_y == FLT_MAX) {
						max_y = desc.upper;
					}

					c->alist()->set_yrange (min_y, max_y);
				}
			} else {
				error << string_compose (_("PluginInsert: automatable control %1 not found - ignored"), port_id) << endmsg;
			}
		}

		/* done */

		break;
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

ARDOUR::framecnt_t
PluginInsert::signal_latency() const
{
	if (_user_latency) {
		return _user_latency;
	}

	return _plugins[0]->signal_latency ();
}

ARDOUR::PluginType
PluginInsert::type ()
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
		alist()->reset_default (desc.normal);
		if (desc.toggled) {
			list->set_interpolation(Evoral::ControlList::Discrete);
		}
	}

	if (desc.toggled) {
		set_flags(Controllable::Toggle);
	}
}

/** @param val `user' value */
void
PluginInsert::PluginControl::set_value (double user_val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (writable()) {
		_set_value (user_val, group_override);
	}
}
void
PluginInsert::PluginControl::set_value_unchecked (double user_val)
{
	/* used only by automation playback */
	_set_value (user_val, Controllable::NoGroup);
}

void
PluginInsert::PluginControl::_set_value (double user_val, PBD::Controllable::GroupControlDisposition group_override)
{
	/* FIXME: probably should be taking out some lock here.. */

	for (Plugins::iterator i = _plugin->_plugins.begin(); i != _plugin->_plugins.end(); ++i) {
		(*i)->set_parameter (_list->parameter().id(), user_val);
	}

	boost::shared_ptr<Plugin> iasp = _plugin->_impulseAnalysisPlugin.lock();
	if (iasp) {
		iasp->set_parameter (_list->parameter().id(), user_val);
	}

	AutomationControl::set_value (user_val, group_override);
}

void
PluginInsert::PluginControl::catch_up_with_external_value (double user_val)
{
	AutomationControl::set_value (user_val, Controllable::NoGroup);
}

XMLNode&
PluginInsert::PluginControl::get_state ()
{
	stringstream ss;

	XMLNode& node (AutomationControl::get_state());
	ss << parameter().id();
	node.add_property (X_("parameter"), ss.str());
#ifdef LV2_SUPPORT
	boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (_plugin->_plugins[0]);
	if (lv2plugin) {
		node.add_property (X_("symbol"), lv2plugin->port_symbol (parameter().id()));
	}
#endif

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

PluginInsert::PluginPropertyControl::PluginPropertyControl (PluginInsert*                     p,
                                                            const Evoral::Parameter&          param,
                                                            const ParameterDescriptor&        desc,
                                                            boost::shared_ptr<AutomationList> list)
	: AutomationControl (p->session(), param, desc, list)
	, _plugin (p)
{
	if (alist()) {
		alist()->set_yrange (desc.lower, desc.upper);
		alist()->reset_default (desc.normal);
	}

	if (desc.toggled) {
		set_flags(Controllable::Toggle);
	}
}

void
PluginInsert::PluginPropertyControl::set_value (double user_val, PBD::Controllable::GroupControlDisposition /* group_override*/)
{
	if (writable()) {
		set_value_unchecked (user_val);
	}
}

void
PluginInsert::PluginPropertyControl::set_value_unchecked (double user_val)
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
	AutomationControl::set_value (user_val, Controllable::NoGroup);
}

XMLNode&
PluginInsert::PluginPropertyControl::get_state ()
{
	stringstream ss;

	XMLNode& node (AutomationControl::get_state());
	ss << parameter().id();
	node.add_property (X_("property"), ss.str());
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
		ret = plugin_factory(_plugins[0]);
		ret->configure_io (internal_input_streams (), internal_output_streams ());
		_impulseAnalysisPlugin = ret;
	} else {
		ret = _impulseAnalysisPlugin.lock();
	}

	return ret;
}

void
PluginInsert::collect_signal_for_analysis (framecnt_t nframes)
{
	// called from outside the audio thread, so this should be safe
	// only do audio as analysis is (currently) only for audio plugins
	_signal_analysis_inputs.ensure_buffers(  DataType::AUDIO, internal_input_streams().n_audio(),  nframes);
	_signal_analysis_outputs.ensure_buffers( DataType::AUDIO, internal_output_streams().n_audio(), nframes);

	_signal_analysis_collected_nframes   = 0;
	_signal_analysis_collect_nframes_max = nframes;
}

/** Add a plugin to our list */
void
PluginInsert::add_plugin (boost::shared_ptr<Plugin> plugin)
{
	plugin->set_insert_id (this->id());

	if (_plugins.empty()) {
                /* first (and probably only) plugin instance - connect to relevant signals
                 */

		plugin->ParameterChangedExternally.connect_same_thread (*this, boost::bind (&PluginInsert::parameter_changed_externally, this, _1, _2));
                plugin->StartTouch.connect_same_thread (*this, boost::bind (&PluginInsert::start_touch, this, _1));
                plugin->EndTouch.connect_same_thread (*this, boost::bind (&PluginInsert::end_touch, this, _1));
	}

	_plugins.push_back (plugin);
}

void
PluginInsert::realtime_handle_transport_stopped ()
{
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
	}
}

void
PluginInsert::realtime_locate ()
{
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->realtime_locate ();
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
PluginInsert::start_touch (uint32_t param_id)
{
        boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, param_id));
        if (ac) {
                ac->start_touch (session().audible_frame());
        }
}

void
PluginInsert::end_touch (uint32_t param_id)
{
        boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, param_id));
        if (ac) {
                ac->stop_touch (true, session().audible_frame());
        }
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
