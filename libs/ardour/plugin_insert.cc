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

#include <string>

#include <sigc++/bind.h>

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>

#include <ardour/plugin_insert.h>
#include <ardour/plugin.h>
#include <ardour/port.h>
#include <ardour/route.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/buffer_set.h>
#include <ardour/automation_event.h>

#ifdef HAVE_SLV2
#include <ardour/lv2_plugin.h>
#endif

#ifdef VST_SUPPORT
#include <ardour/vst_plugin.h>
#endif

#ifdef HAVE_AUDIOUNITS
#include <ardour/audio_unit.h>
#endif

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/types.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

const string PluginInsert::port_automation_node_name = "PortAutomation";

PluginInsert::PluginInsert (Session& s, boost::shared_ptr<Plugin> plug, Placement placement)
	: Processor (s, plug->name(), placement)
{
	/* the first is the master */

	_plugins.push_back (plug);

	init ();

	{
		Glib::Mutex::Lock em (_session.engine().process_lock());
		IO::PortCountChanged (max(input_streams(), output_streams()));
	}

	ProcessorCreated (this); /* EMIT SIGNAL */
}

PluginInsert::PluginInsert (Session& s, const XMLNode& node)
	: Processor (s, "unnamed plugin insert", PreFader)
{
	if (set_state (node)) {
		throw failed_constructor();
	}

	// set_automatable ();

	{
		Glib::Mutex::Lock em (_session.engine().process_lock());
		IO::PortCountChanged (max(input_streams(), output_streams()));
	}
}

PluginInsert::PluginInsert (const PluginInsert& other)
	: Processor (other._session, other._name, other.placement())
{
	uint32_t count = other._plugins.size();

	/* make as many copies as requested */
	for (uint32_t n = 0; n < count; ++n) {
		_plugins.push_back (plugin_factory (other.plugin (n)));
	}

	init ();

	ProcessorCreated (this); /* EMIT SIGNAL */
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
			_plugins.push_back (plugin_factory (_plugins[0]));

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

void
PluginInsert::init ()
{
	set_automatable ();
}

PluginInsert::~PluginInsert ()
{
	GoingAway (); /* EMIT SIGNAL */
}

void
PluginInsert::auto_state_changed (Parameter which)
{
	if (which.type() != PluginAutomation)
		return;

	boost::shared_ptr<AutomationControl> c = control (which);

	if (c && c->list()->automation_state() != Off) {
		_plugins[0]->set_parameter (which.id(), c->list()->eval (_session.transport_frame()));
	}
}

ChanCount
PluginInsert::output_streams() const
{
	ChanCount out = _plugins.front()->get_info()->n_outputs;

	if (out == ChanCount::INFINITE) {

		return _plugins.front()->output_streams ();

	} else {

		out.set_audio (out.n_audio() * _plugins.size());
		out.set_midi (out.n_midi() * _plugins.size());

		return out;
	}
}

ChanCount
PluginInsert::input_streams() const
{
	ChanCount in = _plugins[0]->get_info()->n_inputs;
	
	if (in == ChanCount::INFINITE) {
		return _plugins[0]->input_streams ();
	} else {
		in.set_audio (in.n_audio() * _plugins.size());
		in.set_midi (in.n_midi() * _plugins.size());

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
PluginInsert::is_generator() const
{
	/* XXX more finesse is possible here. VST plugins have a
	   a specific "instrument" flag, for example.
	 */

	return _plugins[0]->get_info()->n_inputs.n_audio() == 0;
}

void
PluginInsert::set_automatable ()
{
	set<Parameter> a = _plugins.front()->automatable ();

	Plugin::ParameterDescriptor desc;

	for (set<Parameter>::iterator i = a.begin(); i != a.end(); ++i) {
		if (i->type() == PluginAutomation) {
			can_automate (*i);
			_plugins.front()->get_parameter_descriptor(i->id(), desc);
			boost::shared_ptr<AutomationList> list(new AutomationList(
					*i,
					//(desc.min_unbound ? FLT_MIN : desc.lower),
					//(desc.max_unbound ? FLT_MAX : desc.upper),
					desc.lower, desc.upper,
					_plugins.front()->default_value(i->id())));

			add_control(boost::shared_ptr<AutomationControl>(
					new PluginControl(*this, list)));
		}
	}
}

void
PluginInsert::parameter_changed (Parameter which, float val)
{
	if (which.type() != PluginAutomation)
		return;

	vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin();

	/* don't set the first plugin, just all the slaves */

	if (i != _plugins.end()) {
		++i;
		for (; i != _plugins.end(); ++i) {
			(*i)->set_parameter (which, val);
		}
	}
}

void
PluginInsert::set_block_size (nframes_t nframes)
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->set_block_size (nframes);
	}
}

void
PluginInsert::activate ()
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->activate ();
	}
}

void
PluginInsert::deactivate ()
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->deactivate ();
	}
}

void
PluginInsert::connect_and_run (BufferSet& bufs, nframes_t nframes, nframes_t offset, bool with_auto, nframes_t now)
{
	uint32_t in_index = 0;
	uint32_t out_index = 0;

	/* Note that we've already required that plugins
	   be able to handle in-place processing.
	*/

	if (with_auto) {

		uint32_t n = 0;
		
		for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li, ++n) {
			
			boost::shared_ptr<AutomationControl> c = li->second;

			if (c->parameter().type() == PluginAutomation && c->list()->automation_playback()) {
				bool valid;

				const float val = c->list()->rt_safe_eval (now, valid);				

				if (valid) {
					c->set_value(val);
				}

			} 
		}
	}

	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->connect_and_run (bufs, in_index, out_index, nframes, offset);
	}

	/* leave remaining channel buffers alone */
}

void
PluginInsert::silence (nframes_t nframes, nframes_t offset)
{
	uint32_t in_index = 0;
	uint32_t out_index = 0;

	if (active()) {
		for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
			(*i)->connect_and_run (_session.get_silent_buffers ((*i)->get_info()->n_inputs), in_index, out_index, nframes, offset);
		}
	}
}
	
void
PluginInsert::run_in_place (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset)
{
	if (active()) {

		if (_session.transport_rolling()) {
			automation_run (bufs, nframes, offset);
		} else {
			connect_and_run (bufs, nframes, offset, false);
		}
	} else {

		/* FIXME: type, audio only */

		uint32_t in = _plugins[0]->get_info()->n_inputs.n_audio();
		uint32_t out = _plugins[0]->get_info()->n_outputs.n_audio();

		if (out > in) {

			/* not active, but something has make up for any channel count increase */
			
			for (uint32_t n = out - in; n < out; ++n) {
				memcpy (bufs.get_audio(n).data(nframes, offset), bufs.get_audio(in - 1).data(nframes, offset), sizeof (Sample) * nframes);
			}
		}

		bufs.count().set_audio(out);
	}
}

void
PluginInsert::set_parameter (Parameter param, float val)
{
	if (param.type() != PluginAutomation)
		return;

	/* the others will be set from the event triggered by this */

	_plugins[0]->set_parameter (param.id(), val);
	
	boost::shared_ptr<AutomationControl> c = control (param);
	
	if (c)
		c->set_value(val);

	_session.set_dirty();
}

float
PluginInsert::get_parameter (Parameter param)
{
	if (param.type() != PluginAutomation)
		return 0.0;
	else
		return
		_plugins[0]->get_parameter (param.id());
}

void
PluginInsert::automation_run (BufferSet& bufs, nframes_t nframes, nframes_t offset)
{
	ControlEvent next_event (0, 0.0f);
	nframes_t now = _session.transport_frame ();
	nframes_t end = now + nframes;

	Glib::Mutex::Lock lm (_automation_lock, Glib::TRY_LOCK);

	if (!lm.locked()) {
		connect_and_run (bufs, nframes, offset, false);
		return;
	}
	
	if (!find_next_event (now, end, next_event)) {
		
 		/* no events have a time within the relevant range */
		
 		connect_and_run (bufs, nframes, offset, true, now);
 		return;
 	}
	
 	while (nframes) {

		nframes_t cnt = min (((nframes_t) ceil (next_event.when) - now), nframes);
  
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
PluginInsert::default_parameter_value (Parameter param)
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
#ifdef HAVE_SLV2
	boost::shared_ptr<LV2Plugin> lv2p;
#endif
#ifdef VST_SUPPORT
	boost::shared_ptr<VSTPlugin> vp;
#endif
#ifdef HAVE_AUDIOUNITS
	boost::shared_ptr<AUPlugin> ap;
#endif

	if ((lp = boost::dynamic_pointer_cast<LadspaPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LadspaPlugin (*lp));
#ifdef HAVE_SLV2
	} else if ((lv2p = boost::dynamic_pointer_cast<LV2Plugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LV2Plugin (*lv2p));
#endif
#ifdef VST_SUPPORT
	} else if ((vp = boost::dynamic_pointer_cast<VSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new VSTPlugin (*vp));
#endif
#ifdef HAVE_AUDIOUNITS
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
	if (set_count (count_for_configuration (in, out)) < 0) {
		return false;
	}

	/* if we're running replicated plugins, each plugin has
	   the same i/o configuration and we may need to announce how many
	   output streams there are.

	   if we running a single plugin, we need to configure it.
	*/

	if (_plugins.front()->configure_io (in, out) < 0) {
		return false;
	}

	return Processor::configure_io (in, out);
}

bool
PluginInsert::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	if (_plugins.front()->reconfigurable_io()) {
		/* plugin has flexible I/O, so delegate to it */
		return _plugins.front()->can_support_io_configuration (in, out);
	}

	ChanCount outputs = _plugins[0]->get_info()->n_outputs;
	ChanCount inputs = _plugins[0]->get_info()->n_inputs;

	if ((inputs.n_total() == 0)
			|| (inputs.n_total() == 1 && outputs == inputs)
			|| (inputs.n_total() == 1 && outputs == inputs
				&& ((inputs.n_audio() == 0 && in.n_audio() == 0)
					|| (inputs.n_midi() == 0 && in.n_midi() == 0)))
			|| (inputs == in)) {
		out = outputs;
		return true;
	}

	bool can_replicate = true;

	/* if number of inputs is a factor of the requested input
	   configuration for every type, we can replicate.
	*/
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (inputs.get(*t) >= in.get(*t) || (inputs.get(*t) % in.get(*t) != 0)) {
			can_replicate = false;
			break;
		}
	}

	if (!can_replicate || (in.n_total() % inputs.n_total() != 0)) {
		return false;
	}

	if (inputs.n_total() == 0) {
		/* instrument plugin, always legal, but throws away any existing streams */
		out = outputs;
	} else if (inputs.n_total() == 1 && outputs == inputs
			&& ((inputs.n_audio() == 0 && in.n_audio() == 0)
			    || (inputs.n_midi() == 0 && in.n_midi() == 0))) {
		/* mono, single-typed plugin, replicate as needed to match in */
		out = in;
	} else if (inputs == in) {
		/* exact match */
		out = outputs;
	} else {
		/* replicate - note that we've already verified that
		   the replication count is constant across all data types.
		*/
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			out.set (*t, outputs.get(*t) * (in.get(*t) / inputs.get(*t)));
		}
	}
		
	return true;
}

/* Number of plugin instances required to support a given channel configuration.
 * (private helper)
 */
int32_t
PluginInsert::count_for_configuration (ChanCount in, ChanCount out) const
{
	if (_plugins.front()->reconfigurable_io()) {
		/* plugin has flexible I/O, so the answer is always 1 */
		/* this could change if we ever decide to replicate AU's */
		return 1;
	}

	// FIXME: take 'out' into consideration
	
	ChanCount outputs = _plugins[0]->get_info()->n_outputs;
	ChanCount inputs = _plugins[0]->get_info()->n_inputs;

	if (inputs.n_total() == 0) {
		/* instrument plugin, always legal, but throws away any existing streams */
		return 1;
	}

	if (inputs.n_total() == 1 && outputs == inputs
			&& ((inputs.n_audio() == 0 && in.n_audio() == 0)
				|| (inputs.n_midi() == 0 && in.n_midi() == 0))) {
		/* mono plugin, replicate as needed to match in */
		return in.n_total();
	}

	if (inputs == in) {
		/* exact match */
		return 1;
	}

	// assumes in is valid, so we must be replicating
	if (inputs.n_total() < in.n_total()
			&& (in.n_total() % inputs.n_total() == 0)) {

		return in.n_total() / inputs.n_total();
	}

	/* err... */
	return 0;
}

XMLNode&
PluginInsert::get_state(void)
{
	return state (true);
}

XMLNode&
PluginInsert::state (bool full)
{
	XMLNode& node = Processor::state (full);

	node.add_property ("type", _plugins[0]->state_node_name());
	node.add_property("unique-id", _plugins[0]->unique_id());
	node.add_property("count", string_compose("%1", _plugins.size()));
	node.add_child_nocopy (_plugins[0]->get_state());

	/* add port automation state */
	XMLNode *autonode = new XMLNode(port_automation_node_name);
	set<Parameter> automatable = _plugins[0]->automatable();
	
	for (set<Parameter>::iterator x = automatable.begin(); x != automatable.end(); ++x) {
		
		/*XMLNode* child = new XMLNode("port");
		snprintf(buf, sizeof(buf), "%" PRIu32, *x);
		child->add_property("number", string(buf));
		
		child->add_child_nocopy (automation_list (*x).state (full));
		autonode->add_child_nocopy (*child);
		*/
		autonode->add_child_nocopy (control(*x)->list()->state (full));
	}

	node.add_child_nocopy (*autonode);
	
	return node;
}

int
PluginInsert::set_state(const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	const XMLProperty *prop;
	ARDOUR::PluginType type;

	if ((prop = node.property ("type")) == 0) {
		error << _("XML node describing insert is missing the `type' field") << endmsg;
		return -1;
	}

	if (prop->value() == X_("ladspa") || prop->value() == X_("Ladspa")) { /* handle old school sessions */
		type = ARDOUR::LADSPA;
	} else if (prop->value() == X_("lv2")) {
		type = ARDOUR::LV2;
	} else if (prop->value() == X_("vst")) {
		type = ARDOUR::VST;
	} else {
		error << string_compose (_("unknown plugin type %1 in plugin insert state"),
				  prop->value())
		      << endmsg;
		return -1;
	}
	
	prop = node.property ("unique-id");
	if (prop == 0) {
		error << _("Plugin has no unique ID field") << endmsg;
		return -1;
	}

	boost::shared_ptr<Plugin> plugin;
	
	plugin = find_plugin (_session, prop->value(), type);	

	if (plugin == 0) {
		error << string_compose(_("Found a reference to a plugin (\"%1\") that is unknown.\n"
				   "Perhaps it was removed or moved since it was last used."), prop->value()) 
		      << endmsg;
		return -1;
	}

	uint32_t count = 1;

	if ((prop = node.property ("count")) != 0) {
		sscanf (prop->value().c_str(), "%u", &count);
	}

	if (_plugins.size() != count) {
		
		_plugins.push_back (plugin);
		
		for (uint32_t n=1; n < count; ++n) {
			_plugins.push_back (plugin_factory (plugin));
		}
	}
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == plugin->state_node_name()) {
			for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
				(*i)->set_state (**niter);
			}
			break;
		}
	} 

	const XMLNode* insert_node = &node;

	// legacy sessions: search for child IOProcessor node
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "IOProcessor") {
			insert_node = *niter;
			break;
		}
	}
	
	Processor::set_state (*insert_node);

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
		
		for(iter = cnodes.begin(); iter != cnodes.end(); ++iter){
			
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

			if (!child->children().empty()) {
				control (Parameter(PluginAutomation, port_id), true)->list()->set_state (*child->children().front());
			} else {
				if ((cprop = child->property("auto")) != 0) {
					
					/* old school */

					int x;
					sscanf (cprop->value().c_str(), "0x%x", &x);
					control (Parameter(PluginAutomation, port_id), true)->list()->set_automation_state (AutoState (x));

				} else {
					
					/* missing */
					
					control (Parameter(PluginAutomation, port_id), true)->list()->set_automation_state (Off);
				}
			}

		}

		/* done */

		break;
	} 

	if (niter == nlist.end()) {
		warning << string_compose(_("XML node describing a port automation is missing the `%1' information"), port_automation_node_name) << endmsg;
	}
	
	// The name of the PluginInsert comes from the plugin, nothing else
	_name = plugin->get_info()->name;
	
	return 0;
}

string
PluginInsert::describe_parameter (Parameter param)
{
	if (param.type() != PluginAutomation)
		return Automatable::describe_parameter(param);

	return _plugins[0]->describe_parameter (param);
}

ARDOUR::nframes_t 
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
	boost::shared_ptr<LadspaPlugin> lp;
#ifdef VST_SUPPORT
	boost::shared_ptr<VSTPlugin> vp;
#endif
#ifdef HAVE_AUDIOUNITS
	boost::shared_ptr<AUPlugin> ap;
#endif
	
	PluginPtr other = plugin ();

	if ((lp = boost::dynamic_pointer_cast<LadspaPlugin> (other)) != 0) {
		return ARDOUR::LADSPA;
#ifdef VST_SUPPORT
	} else if ((vp = boost::dynamic_pointer_cast<VSTPlugin> (other)) != 0) {
		return ARDOUR::VST;
#endif
#ifdef HAVE_AUDIOUNITS
	} else if ((ap = boost::dynamic_pointer_cast<AUPlugin> (other)) != 0) {
		return ARDOUR::AudioUnit;
#endif
	} else {
		/* NOT REACHED */
		return (ARDOUR::PluginType) 0;
	}
}

PluginInsert::PluginControl::PluginControl (PluginInsert& p, boost::shared_ptr<AutomationList> list)
	: AutomationControl (p.session(), list, p.describe_parameter(list->parameter()))
	, _plugin (p)
	, _list (list)
{
	Plugin::ParameterDescriptor desc;
	p.plugin(0)->get_parameter_descriptor (list->parameter().id(), desc);
	_logarithmic = desc.logarithmic;
	_toggled = desc.toggled;
}
	 
void
PluginInsert::PluginControl::set_value (float val)
{
	/* FIXME: probably should be taking out some lock here.. */
	
	if (_toggled) {
		if (val > 0.5) {
			val = 1.0;
		} else {
			val = 0.0;
		}
	} else {
			
		/*const float range = _list->get_max_y() - _list->get_min_y();
		const float lower = _list->get_min_y();

		if (!_logarithmic) {
			val = lower + (range * val);
		} else {
			float log_lower = 0.0f;
			if (lower > 0.0f) {
				log_lower = log(lower);
			}

			val = exp(log_lower + log(range) * val);
		}*/

	}

	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugin._plugins.begin();
			i != _plugin._plugins.end(); ++i) {
		(*i)->set_parameter (_list->parameter().id(), val);
	}

	AutomationControl::set_value(val);
}

float
PluginInsert::PluginControl::get_value (void) const
{
	/* FIXME: probably should be taking out some lock here.. */
	
	float val = _plugin.get_parameter (_list->parameter());

	return val;

	/*if (_toggled) {
		
		return val;
		
	} else {
		
		if (_logarithmic) {
			val = log(val);
		}
		
		return ((val - lower) / range);
	}*/
}

