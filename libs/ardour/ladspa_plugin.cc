/*
    Copyright (C) 2000-2006 Paul Davis 

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

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include <vector>
#include <string>

#include <cstdlib>
#include <cstdio> // so libraptor doesn't complain
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>

#include <lrdf.h>

#include <pbd/compose.h>
#include <pbd/error.h>
#include <pbd/xml++.h>

#include <midi++/manager.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/buffer_set.h>

#include <pbd/stl_delete.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

LadspaPlugin::LadspaPlugin (void *mod, AudioEngine& e, Session& session, uint32_t index, nframes_t rate)
	: Plugin (e, session)
{
	init (mod, index, rate);
}

LadspaPlugin::LadspaPlugin (const LadspaPlugin &other)
	: Plugin (other)
{
	init (other._module, other._index, other._sample_rate);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		_control_data[i] = other._shadow_data[i];
		_shadow_data[i] = other._shadow_data[i];
	}
}

void
LadspaPlugin::init (void *mod, uint32_t index, nframes_t rate)
{
	LADSPA_Descriptor_Function dfunc;
	uint32_t i, port_cnt;
	const char *errstr;

	_module = mod;
	_control_data = 0;
	_shadow_data = 0;
	_latency_control_port = 0;
	_was_activated = false;

	dfunc = (LADSPA_Descriptor_Function) dlsym (_module, "ladspa_descriptor");

	if ((errstr = dlerror()) != NULL) {
		error << _("LADSPA: module has no descriptor function.") << endmsg;
		throw failed_constructor();
	}

	if ((_descriptor = dfunc (index)) == 0) {
		error << _("LADSPA: plugin has gone away since discovery!") << endmsg;
		throw failed_constructor();
	}

	_index = index;

	if (LADSPA_IS_INPLACE_BROKEN(_descriptor->Properties)) {
		error << string_compose(_("LADSPA: \"%1\" cannot be used, since it cannot do inplace processing"), _descriptor->Name) << endmsg;
		throw failed_constructor();
	}
	
	_sample_rate = rate;

	if (_descriptor->instantiate == 0) {
		throw failed_constructor();
	}

	if ((_handle = _descriptor->instantiate (_descriptor, rate)) == 0) {
		throw failed_constructor();
	}

	port_cnt = parameter_count();

	_control_data = new LADSPA_Data[port_cnt];
	_shadow_data = new LADSPA_Data[port_cnt];

	for (i = 0; i < port_cnt; ++i) {
		if (LADSPA_IS_PORT_CONTROL(port_descriptor (i))) {
			connect_port (i, &_control_data[i]);
			
			if (LADSPA_IS_PORT_OUTPUT(port_descriptor (i)) &&
			    strcmp (port_names()[i], X_("latency")) == 0) {
				_latency_control_port = &_control_data[i];
				*_latency_control_port = 0;
			}

			if (!LADSPA_IS_PORT_INPUT(port_descriptor (i))) {
				continue;
			}
		
			_shadow_data[i] = default_value (i);
		}
	}

	latency_compute_run ();
}

LadspaPlugin::~LadspaPlugin ()
{
	deactivate ();
	cleanup ();

	GoingAway (); /* EMIT SIGNAL */
	
	/* XXX who should close a plugin? */

        // dlclose (module);

	if (_control_data) {
		delete [] _control_data;
	}

	if (_shadow_data) {
		delete [] _shadow_data;
	}
}

string
LadspaPlugin::unique_id() const
{
	char buf[32];
	snprintf (buf, sizeof (buf), "%lu", _descriptor->UniqueID);
	return string (buf);
}

float
LadspaPlugin::default_value (uint32_t port)
{
	const LADSPA_PortRangeHint *prh = port_range_hints();
	float ret = 0.0f;
	bool bounds_given = false;
	bool sr_scaling = false;
	bool earlier_hint = false;

	/* defaults - case 1 */
	
	if (LADSPA_IS_HINT_HAS_DEFAULT(prh[port].HintDescriptor)) {
		if (LADSPA_IS_HINT_DEFAULT_MINIMUM(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound;
			bounds_given = true;
			sr_scaling = true;
			earlier_hint = true;
		}
		
		/* FIXME: add support for logarithmic defaults */
		
		else if (LADSPA_IS_HINT_DEFAULT_LOW(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound * 0.75f + prh[port].UpperBound * 0.25f;
			bounds_given = true;
			sr_scaling = true;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound * 0.50f + prh[port].UpperBound * 0.50f;
			bounds_given = true;
			sr_scaling = true;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_HIGH(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound * 0.25f + prh[port].UpperBound * 0.75f;
			bounds_given = true;
			sr_scaling = true;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(prh[port].HintDescriptor)) {
			ret = prh[port].UpperBound;
			bounds_given = true;
			sr_scaling = true;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_0(prh[port].HintDescriptor)) {
			ret = 0.0f;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_1(prh[port].HintDescriptor)) {
			ret = 1.0f;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_100(prh[port].HintDescriptor)) {
			ret = 100.0f;
			earlier_hint = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_440(prh[port].HintDescriptor)) {
			ret = 440.0f;
			earlier_hint = true;
		}
		else {
			/* no hint found */
			ret = 0.0f;
		}
	}
	
	/* defaults - case 2 */
	else if (LADSPA_IS_HINT_BOUNDED_BELOW(prh[port].HintDescriptor) &&
		 !LADSPA_IS_HINT_BOUNDED_ABOVE(prh[port].HintDescriptor)) {
		
		if (prh[port].LowerBound < 0) {
			ret = 0.0f;
		} else {
			ret = prh[port].LowerBound;
		}

		bounds_given = true;
		sr_scaling = true;
	}
	
	/* defaults - case 3 */
	else if (!LADSPA_IS_HINT_BOUNDED_BELOW(prh[port].HintDescriptor) &&
		 LADSPA_IS_HINT_BOUNDED_ABOVE(prh[port].HintDescriptor)) {
		
		if (prh[port].UpperBound > 0) {
			ret = 0.0f;
		} else {
			ret = prh[port].UpperBound;
		}

		bounds_given = true;
		sr_scaling = true;
	}
	
	/* defaults - case 4 */
	else if (LADSPA_IS_HINT_BOUNDED_BELOW(prh[port].HintDescriptor) &&
		 LADSPA_IS_HINT_BOUNDED_ABOVE(prh[port].HintDescriptor)) {
		
		if (prh[port].LowerBound < 0 && prh[port].UpperBound > 0) {
			ret = 0.0f;
		} else if (prh[port].LowerBound < 0 && prh[port].UpperBound < 0) {
			ret = prh[port].UpperBound;
		} else {
			ret = prh[port].LowerBound;
		}
		bounds_given = true;	
		sr_scaling = true;
	}
	
	/* defaults - case 5 */
		
	if (LADSPA_IS_HINT_SAMPLE_RATE(prh[port].HintDescriptor) && !earlier_hint) {
		if (bounds_given) {
			if (sr_scaling) {
				ret *= _sample_rate;
			}
		} else {
			ret = _sample_rate;
		}
	}

	return ret;
}	

void
LadspaPlugin::set_parameter (uint32_t which, float val)
{
	if (which < _descriptor->PortCount) {
		_shadow_data[which] = (LADSPA_Data) val;
#if 0
		ParameterChanged (Parameter(PluginAutomation, 0, which), val); /* EMIT SIGNAL */

		if (which < parameter_count() && controls[which]) {
			controls[which]->Changed ();
		}
#endif
		
	} else {
		warning << string_compose (_("illegal parameter number used with plugin \"%1\". This may"
					     "indicate a change in the plugin design, and presets may be"
					     "invalid"), name())
			<< endmsg;
	}
}

float
LadspaPlugin::get_parameter (uint32_t which) const
{
	if (LADSPA_IS_PORT_INPUT(port_descriptor (which))) {
		return (float) _shadow_data[which];
	} else {
		return (float) _control_data[which];
	}
}

uint32_t
LadspaPlugin::nth_parameter (uint32_t n, bool& ok) const
{
	uint32_t x, c;

	ok = false;

	for (c = 0, x = 0; x < _descriptor->PortCount; ++x) {
		if (LADSPA_IS_PORT_CONTROL (port_descriptor (x))) {
			if (c++ == n) {
				ok = true;
				return x;
			}
		}
	}
	return 0;
}

XMLNode&
LadspaPlugin::get_state()
{
	XMLNode *root = new XMLNode(state_node_name());
	XMLNode *child;
	char buf[16];
	LocaleGuard lg (X_("POSIX"));

	for (uint32_t i = 0; i < parameter_count(); ++i){

		if (LADSPA_IS_PORT_INPUT(port_descriptor (i)) && 
		    LADSPA_IS_PORT_CONTROL(port_descriptor (i))){

			child = new XMLNode("port");
			snprintf(buf, sizeof(buf), "%u", i);
			child->add_property("number", string(buf));
			snprintf(buf, sizeof(buf), "%+f", _shadow_data[i]);
			child->add_property("value", string(buf));
			root->add_child_nocopy (*child);
		}
	}

	return *root;
}

bool
LadspaPlugin::save_preset (string name)
{
	return Plugin::save_preset (name, "ladspa");
}

int
LadspaPlugin::set_state(const XMLNode& node)
{
	XMLNodeList nodes;
	XMLProperty *prop;
	XMLNodeConstIterator iter;
	XMLNode *child;
	const char *port;
	const char *data;
	uint32_t port_id;
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LadspaPlugin::set_state") << endmsg;
		return -1;
	}

	nodes = node.children ("port");

	for(iter = nodes.begin(); iter != nodes.end(); ++iter){

		child = *iter;

		if ((prop = child->property("number")) != 0) {
			port = prop->value().c_str();
		} else {
			warning << _("LADSPA: no ladspa port number") << endmsg;
			continue;
		}
		if ((prop = child->property("value")) != 0) {
			data = prop->value().c_str();
		} else {
			warning << _("LADSPA: no ladspa port data") << endmsg;
			continue;
		}

		sscanf (port, "%" PRIu32, &port_id);
		set_parameter (port_id, atof(data));
	}

	latency_compute_run ();

	return 0;
}

int
LadspaPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	LADSPA_PortRangeHint prh;

	prh  = port_range_hints()[which];
	

	if (LADSPA_IS_HINT_BOUNDED_BELOW(prh.HintDescriptor)) {
		desc.min_unbound = false;
		if (LADSPA_IS_HINT_SAMPLE_RATE(prh.HintDescriptor)) {
			desc.lower = prh.LowerBound * _session.frame_rate();
		} else {
			desc.lower = prh.LowerBound;
		}
	} else {
		desc.min_unbound = true;
		desc.lower = 0;
	}
	

	if (LADSPA_IS_HINT_BOUNDED_ABOVE(prh.HintDescriptor)) {
		desc.max_unbound = false;
		if (LADSPA_IS_HINT_SAMPLE_RATE(prh.HintDescriptor)) {
			desc.upper = prh.UpperBound * _session.frame_rate();
		} else {
			desc.upper = prh.UpperBound;
		}
	} else {
		desc.max_unbound = true;
		desc.upper = 4; /* completely arbitrary */
	}
	
	if (LADSPA_IS_HINT_INTEGER (prh.HintDescriptor)) {
		desc.step = 1.0;
		desc.smallstep = 0.1;
		desc.largestep = 10.0;
	} else {
		float delta = desc.upper - desc.lower;
		desc.step = delta / 1000.0f;
		desc.smallstep = delta / 10000.0f;
		desc.largestep = delta/10.0f;
	}
	
	desc.toggled = LADSPA_IS_HINT_TOGGLED (prh.HintDescriptor);
	desc.logarithmic = LADSPA_IS_HINT_LOGARITHMIC (prh.HintDescriptor);
	desc.sr_dependent = LADSPA_IS_HINT_SAMPLE_RATE (prh.HintDescriptor);
	desc.integer_step = LADSPA_IS_HINT_INTEGER (prh.HintDescriptor);

	desc.label = port_names()[which];

	return 0;
}

string
LadspaPlugin::describe_parameter (Evoral::Parameter which)
{
	if (which.type() == PluginAutomation && which.id() < parameter_count()) {
		return port_names()[which.id()];
	} else {
		return "??";
	}
}

ARDOUR::nframes_t
LadspaPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

	if (_latency_control_port) {
		return (nframes_t) floor (*_latency_control_port);
	} else {
		return 0;
	}
}

set<Evoral::Parameter>
LadspaPlugin::automatable () const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i){
		if (LADSPA_IS_PORT_INPUT(port_descriptor (i)) && 
		    LADSPA_IS_PORT_CONTROL(port_descriptor (i))){
			
			ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
		}
	}

	return ret;
}

int
LadspaPlugin::connect_and_run (BufferSet& bufs, uint32_t& in_index, uint32_t& out_index, nframes_t nframes, nframes_t offset)
{
	uint32_t port_index = 0;
	cycles_t then, now;

	then = get_cycles ();

	const uint32_t nbufs = bufs.count().n_audio();

	while (port_index < parameter_count()) {
		if (LADSPA_IS_PORT_AUDIO (port_descriptor(port_index))) {
			if (LADSPA_IS_PORT_INPUT (port_descriptor(port_index))) {
				const size_t index = min(in_index, nbufs - 1);
				connect_port (port_index, bufs.get_audio(index).data(nframes, offset));
				//cerr << this << ' ' << name() << " @ " << offset << " inport " << in_index << " = buf " 
				//     << min((uint32_t)in_index,nbufs) << " = " << &bufs[min((uint32_t)in_index,nbufs)][offset] << endl;
				in_index++;


			} else if (LADSPA_IS_PORT_OUTPUT (port_descriptor (port_index))) {
				const size_t index = min(out_index,nbufs - 1);
				connect_port (port_index, bufs.get_audio(index).data(nframes, offset));
				// cerr << this << ' ' << name() << " @ " << offset << " outport " << out_index << " = buf " 
				//     << min((uint32_t)out_index,nbufs) << " = " << &bufs[min((uint32_t)out_index,nbufs)][offset] << endl;
				out_index++;
			}
		}
		port_index++;
	}
	
	run_in_place (nframes);
	now = get_cycles ();
	set_cycles ((uint32_t) (now - then));

	return 0;
}

bool
LadspaPlugin::parameter_is_control (uint32_t param) const
{
	return LADSPA_IS_PORT_CONTROL(port_descriptor (param));
}

bool
LadspaPlugin::parameter_is_audio (uint32_t param) const
{
	return LADSPA_IS_PORT_AUDIO(port_descriptor (param));
}

bool
LadspaPlugin::parameter_is_output (uint32_t param) const
{
	return LADSPA_IS_PORT_OUTPUT(port_descriptor (param));
}

bool
LadspaPlugin::parameter_is_input (uint32_t param) const
{
	return LADSPA_IS_PORT_INPUT(port_descriptor (param));
}

void
LadspaPlugin::print_parameter (uint32_t param, char *buf, uint32_t len) const
{
	if (buf && len) {
		if (param < parameter_count()) {
			snprintf (buf, len, "%.3f", get_parameter (param));
		} else {
			strcat (buf, "0");
		}
	}
}

void
LadspaPlugin::run_in_place (nframes_t nframes)
{
	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (LADSPA_IS_PORT_INPUT(port_descriptor (i)) && LADSPA_IS_PORT_CONTROL(port_descriptor (i))) {
			_control_data[i] = _shadow_data[i];
		}
	}
	_descriptor->run (_handle, nframes);
}

void
LadspaPlugin::latency_compute_run ()
{
	if (!_latency_control_port) {
		return;
	}

	/* we need to run the plugin so that it can set its latency
	   parameter.
	*/
	
	activate ();
	
	uint32_t port_index = 0;
	uint32_t in_index = 0;
	uint32_t out_index = 0;
	const nframes_t bufsize = 1024;
	LADSPA_Data buffer[bufsize];

	memset(buffer,0,sizeof(LADSPA_Data)*bufsize);
		
	/* Note that we've already required that plugins
	   be able to handle in-place processing.
	*/
	
	port_index = 0;
	
	while (port_index < parameter_count()) {
		if (LADSPA_IS_PORT_AUDIO (port_descriptor (port_index))) {
			if (LADSPA_IS_PORT_INPUT (port_descriptor (port_index))) {
				connect_port (port_index, buffer);
				in_index++;
			} else if (LADSPA_IS_PORT_OUTPUT (port_descriptor (port_index))) {
				connect_port (port_index, buffer);
				out_index++;
			}
		}
		port_index++;
	}
	
	run_in_place (bufsize);
	deactivate ();
}

PluginPtr
LadspaPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;
		void *module;

		if ((module = dlopen (path.c_str(), RTLD_NOW)) == 0) {
			error << string_compose(_("LADSPA: cannot load module from \"%1\""), path) << endmsg;
			error << dlerror() << endmsg;
		} else {
			plugin.reset (new LadspaPlugin (module, session.engine(), session, index, session.frame_rate()));
		}

		plugin->set_info(PluginInfoPtr(new LadspaPluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}	
}
