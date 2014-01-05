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
{
	/* the first is the master */

	if (plug) {
		add_plugin (plug);
		create_automatable_parameters ();
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

	} else if (num < _plugins.size()) {
		uint32_t diff = _plugins.size() - num;
		for (uint32_t n= 0; n < diff; ++n) {
			_plugins.pop_back();
		}
	}

	return true;
}

PluginInsert::~PluginInsert ()
{
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
		out.set_midi (out.n_midi() * _plugins.size() + midi_bypass.n_midi());
		return out;
	}
}

ChanCount
PluginInsert::input_streams() const
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

	Plugin::ParameterDescriptor desc;

	for (set<Evoral::Parameter>::iterator i = a.begin(); i != a.end(); ++i) {
		if (i->type() == PluginAutomation) {

			Evoral::Parameter param(*i);

			_plugins.front()->get_parameter_descriptor(i->id(), desc);

			/* the Parameter belonging to the actual plugin doesn't have its range set
			   but we want the Controllable related to this Parameter to have those limits.
			*/

			param.set_range (desc.lower, desc.upper, _plugins.front()->default_value(i->id()), desc.toggled);
			can_automate (param);
			boost::shared_ptr<AutomationList> list(new AutomationList(param));
			add_control (boost::shared_ptr<AutomationControl> (new PluginControl(this, param, list)));
		}
	}
}

void
PluginInsert::parameter_changed (uint32_t which, float val)
{
	boost::shared_ptr<AutomationControl> ac = automation_control (Evoral::Parameter (PluginAutomation, 0, which));

	if (ac) {
		ac->set_value (val);
                
                Plugins::iterator i = _plugins.begin();
                
                /* don't set the first plugin, just all the slaves */
                
                if (i != _plugins.end()) {
                        ++i;
                        for (; i != _plugins.end(); ++i) {
                                (*i)->set_parameter (which, val);
                        }
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
	// Calculate if, and how many frames we need to collect for analysis
	framecnt_t collect_signal_nframes = (_signal_analysis_collect_nframes_max -
					     _signal_analysis_collected_nframes);
	if (nframes < collect_signal_nframes) { // we might not get all frames now
		collect_signal_nframes = nframes;
	}

	ChanCount const in_streams = input_streams ();
	ChanCount const out_streams = output_streams ();

	ChanMapping in_map (in_streams);
	ChanMapping out_map (out_streams);
	bool valid;
	if (_match.method == Split) {
		/* fix the input mapping so that we have maps for each of the plugin's inputs */
		in_map = ChanMapping (natural_input_streams ());

		/* copy the first stream's buffer contents to the others */
		/* XXX: audio only */
		uint32_t first_idx = in_map.get (DataType::AUDIO, 0, &valid);
		if (valid) {
			for (uint32_t i = in_streams.n_audio(); i < natural_input_streams().n_audio(); ++i) {
				bufs.get_audio(in_map.get (DataType::AUDIO, i, &valid)).read_from(bufs.get_audio(first_idx), nframes, offset, offset);
			}
		}
	}

	/* Note that we've already required that plugins
	   be able to handle in-place processing.
	*/

	if (with_auto) {

		uint32_t n = 0;

		for (Controls::iterator li = controls().begin(); li != controls().end(); ++li, ++n) {

			boost::shared_ptr<AutomationControl> c
				= boost::dynamic_pointer_cast<AutomationControl>(li->second);

			if (c->parameter().type() == PluginAutomation && c->automation_playback()) {
				bool valid;

				const float val = c->list()->rt_safe_eval (now, valid);

				if (valid) {
					c->set_value(val);
				}

			}
		}
	}

	if (collect_signal_nframes > 0) {
		// collect input
		//std::cerr << "collect input, bufs " << bufs.count().n_audio() << " count,  " << bufs.available().n_audio() << " available" << std::endl;
		//std::cerr << "               streams " << input_streams().n_audio() << std::endl;
		//std::cerr << "filling buffer with " << collect_signal_nframes << " frames at " << _signal_analysis_collected_nframes << std::endl;

		_signal_analysis_inputs.set_count(input_streams());

		for (uint32_t i = 0; i < input_streams().n_audio(); ++i) {
			_signal_analysis_inputs.get_audio(i).read_from(
				bufs.get_audio(i),
				collect_signal_nframes,
				_signal_analysis_collected_nframes); // offset is for target buffer
		}

	}

	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->connect_and_run(bufs, in_map, out_map, nframes, offset);
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			in_map.offset_to(*t, natural_input_streams().get(*t));
			out_map.offset_to(*t, natural_output_streams().get(*t));
		}
	}

	if (collect_signal_nframes > 0) {
		// collect output
		//std::cerr << "       output, bufs " << bufs.count().n_audio() << " count,  " << bufs.available().n_audio() << " available" << std::endl;
		//std::cerr << "               streams " << output_streams().n_audio() << std::endl;

		_signal_analysis_outputs.set_count(output_streams());

		for (uint32_t i = 0; i < output_streams().n_audio(); ++i) {
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
	/* leave remaining channel buffers alone */
}

void
PluginInsert::silence (framecnt_t nframes)
{
	if (!active ()) {
		return;
	}

	ChanMapping in_map(input_streams());
	ChanMapping out_map(output_streams());

	if (_match.method == Split) {
		/* fix the input mapping so that we have maps for each of the plugin's inputs */
		in_map = ChanMapping (natural_input_streams ());
	}

	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->connect_and_run (_session.get_scratch_buffers ((*i)->get_info()->n_inputs, true), in_map, out_map, nframes, 0);
	}
}

void
PluginInsert::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (_pending_active) {
		/* run as normal if we are active or moving from inactive to active */

		if (_session.transport_rolling()) {
			automation_run (bufs, nframes);
		} else {
			connect_and_run (bufs, nframes, 0, false);
		}

	} else {
		if (has_no_audio_inputs()) {

			/* silence all (audio) outputs. Should really declick
			 * at the transitions of "active"
			 */

			uint32_t out = output_streams().n_audio ();

			for (uint32_t n = 0; n < out; ++n) {
				bufs.get_audio (n).silence (nframes);
			}

			bufs.count().set_audio (out);

		} else {

			/* does this need to be done with MIDI? it appears not */

			uint32_t in = input_streams ().n_audio ();
			uint32_t out = output_streams().n_audio ();

			if (out > in) {

				/* not active, but something has make up for any channel count increase */
				
				// TODO: option round-robin (n % in) or silence additional buffers ??
				for (uint32_t n = in; n < out; ++n) {
					bufs.get_audio(n).read_from(bufs.get_audio(in - 1), nframes);
				}
			}

			bufs.count().set_audio (out);
		}
	}

	_active = _pending_active;

	/* we have no idea whether the plugin generated silence or not, so mark
	 * all buffers appropriately.
	 */

}

void
PluginInsert::set_parameter (Evoral::Parameter param, float val)
{
	if (param.type() != PluginAutomation) {
		return;
	}

	/* the others will be set from the event triggered by this */

	_plugins[0]->set_parameter (param.id(), val);

	boost::shared_ptr<AutomationControl> ac
			= boost::dynamic_pointer_cast<AutomationControl>(control(param));

	if (ac) {
		ac->set_value(val);
	} else {
		warning << "set_parameter called for nonexistant parameter "
			<< EventTypeMap::instance().to_symbol(param) << endmsg;
	}

	_session.set_dirty();
}

float
PluginInsert::get_parameter (Evoral::Parameter param)
{
	if (param.type() != PluginAutomation) {
		return 0.0;
	} else {
		assert (!_plugins.empty ());
		return _plugins[0]->get_parameter (param.id());
	}
}

void
PluginInsert::automation_run (BufferSet& bufs, pframes_t nframes)
{
	Evoral::ControlEvent next_event (0, 0.0f);
	framepos_t now = _session.transport_frame ();
	framepos_t end = now + nframes;
	framecnt_t offset = 0;

	Glib::Threads::Mutex::Lock lm (control_lock(), Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		connect_and_run (bufs, nframes, offset, false);
		return;
	}

	if (!find_next_event (now, end, next_event) || requires_fixed_sized_buffers()) {

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
		/*NOTREACHED*/
	}

	return _plugins[0]->default_value (param.id());
}

boost::shared_ptr<Plugin>
PluginInsert::plugin_factory (boost::shared_ptr<Plugin> other)
{
	boost::shared_ptr<LadspaPlugin> lp;
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
	/*NOTREACHED*/
	return boost::shared_ptr<Plugin> ((Plugin*) 0);
}

bool
PluginInsert::configure_io (ChanCount in, ChanCount out)
{
	Match old_match = _match;
	ChanCount old_in = input_streams ();
	ChanCount old_out = output_streams ();

	/* set the matching method and number of plugins that we will use to meet this configuration */
	_match = private_can_support_io_configuration (in, out);
	if (set_count (_match.plugins) == false) {
		return false;
	}

	if (  (old_match.method != _match.method && (old_match.method == Split || _match.method == Split))
			|| old_in != in
			|| old_out != out
			)
	{
		PluginIoReConfigure (); /* EMIT SIGNAL */
	}

	/* configure plugins */
	switch (_match.method) {
	case Split:
	case Hide:
		if (_plugins.front()->configure_io (_plugins.front()->get_info()->n_inputs, out)) {
			return false;
		}
		break;

	default:
		if (_plugins.front()->configure_io (in, out) == false) {
			return false;
		}
		break;
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
PluginInsert::private_can_support_io_configuration (ChanCount const & inx, ChanCount& out)
{
	PluginInfoPtr info = _plugins.front()->get_info();
	ChanCount in; in += inx;
	midi_bypass.reset();

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
		midi_bypass.set(DataType::MIDI, 1);
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

	if (can_replicate) {
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
		return Match (Hide, 1, hide_channels);
	}

	midi_bypass.reset();
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

			if ((prop = (*iter)->property (X_("parameter"))) != 0) {
				uint32_t p = atoi (prop->value());

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

	if (plugin == 0) {
		error << string_compose(
			_("Found a reference to a plugin (\"%1\") that is unknown.\n"
			  "Perhaps it was removed or moved since it was last used."),
			prop->value())
		      << endmsg;
		return -1;
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

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		/* find the node with the type-specific node name ("lv2", "ladspa", etc)
		   and set all plugins to the same state.
		*/

		if ((*niter)->name() == plugin->state_node_name()) {

			plugin->set_state (**niter, version);

			for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
				(*i)->set_state (**niter, version);
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

			if (c) {
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

					Plugin::ParameterDescriptor desc;
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
	if (param.type() != PluginAutomation) {
		return Automatable::describe_parameter(param);
	}

	return _plugins[0]->describe_parameter (param);
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

PluginInsert::PluginControl::PluginControl (PluginInsert* p, const Evoral::Parameter &param, boost::shared_ptr<AutomationList> list)
	: AutomationControl (p->session(), param, list, p->describe_parameter(param))
	, _plugin (p)
{
	Plugin::ParameterDescriptor desc;
	boost::shared_ptr<Plugin> plugin = p->plugin (0);
	
	alist()->reset_default (plugin->default_value (param.id()));

	plugin->get_parameter_descriptor (param.id(), desc);
	_logarithmic = desc.logarithmic;
	_sr_dependent = desc.sr_dependent;
	_toggled = desc.toggled;
}

/** @param val `user' value */
void
PluginInsert::PluginControl::set_value (double user_val)
{
	/* FIXME: probably should be taking out some lock here.. */

	for (Plugins::iterator i = _plugin->_plugins.begin(); i != _plugin->_plugins.end(); ++i) {
		(*i)->set_parameter (_list->parameter().id(), user_val);
	}

	boost::shared_ptr<Plugin> iasp = _plugin->_impulseAnalysisPlugin.lock();
	if (iasp) {
		iasp->set_parameter (_list->parameter().id(), user_val);
	}

	AutomationControl::set_value (user_val);
}

double
PluginInsert::PluginControl::internal_to_interface (double val) const
{
	if (_logarithmic) {
		/* some plugins have a log-scale range "0.."
		 * ideally we'd map the range down to infinity somehow :)
		 *
		 * one solution could be to use
		 *   val = exp(lower + log(range) * value);
		 *   (log(val) - lower) / range)
		 * This approach would require access to the actual range (ie
		 * Plugin::ParameterDescriptor) and also require handling
		 * of unbound ranges..
		 *
		 * currently an arbitrarly low number is assumed to represnt
		 * log(0) as hot-fix solution.
		 */
		if (val > 0) {
			val = log (val);
		} else {
			val = -8; // ~ -70dB = 20 * log10(exp(-8))
		}
	}

	return val;
}

double
PluginInsert::PluginControl::interface_to_internal (double val) const
{
	if (_logarithmic) {
		if (val <= -8) {
			/* see note in PluginInsert::PluginControl::internal_to_interface() */
			val= 0;
		} else {
			val = exp (val);
		}
	}

	return val;
}

XMLNode&
PluginInsert::PluginControl::get_state ()
{
	stringstream ss;

	XMLNode& node (AutomationControl::get_state());
	ss << parameter().id();
	node.add_property (X_("parameter"), ss.str());

	return node;
}

/** @return `user' val */
double
PluginInsert::PluginControl::get_value () const
{
	/* FIXME: probably should be taking out some lock here.. */
	return _plugin->get_parameter (_list->parameter());
}

boost::shared_ptr<Plugin>
PluginInsert::get_impulse_analysis_plugin()
{
	boost::shared_ptr<Plugin> ret;
	if (_impulseAnalysisPlugin.expired()) {
		ret = plugin_factory(_plugins[0]);
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
	_signal_analysis_inputs.ensure_buffers(  DataType::AUDIO, input_streams().n_audio(),  nframes);
	_signal_analysis_outputs.ensure_buffers( DataType::AUDIO, output_streams().n_audio(), nframes);

	_signal_analysis_collected_nframes   = 0;
	_signal_analysis_collect_nframes_max = nframes;
}

/** Add a plugin to our list */
void
PluginInsert::add_plugin (boost::shared_ptr<Plugin> plugin)
{
	plugin->set_insert_info (this);
	
	if (_plugins.empty()) {
                /* first (and probably only) plugin instance - connect to relevant signals 
                 */

		plugin->ParameterChanged.connect_same_thread (*this, boost::bind (&PluginInsert::parameter_changed, this, _1, _2));
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
