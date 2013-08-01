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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <inttypes.h>

#include <vector>
#include <string>

#include <cstdlib>
#include <cstdio> // so libraptor doesn't complain
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/xml++.h"
#include "pbd/stacktrace.h"

#include "midi++/manager.h"

#include "ardour/session.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"

#include "pbd/stl_delete.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

LadspaPlugin::LadspaPlugin (string module_path, AudioEngine& e, Session& session, uint32_t index, framecnt_t rate)
	: Plugin (e, session)
{
	init (module_path, index, rate);
}

LadspaPlugin::LadspaPlugin (const LadspaPlugin &other)
	: Plugin (other)
{
	init (other._module_path, other._index, other._sample_rate);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		_control_data[i] = other._shadow_data[i];
		_shadow_data[i] = other._shadow_data[i];
	}
}

void
LadspaPlugin::init (string module_path, uint32_t index, framecnt_t rate)
{
	void* func;
	LADSPA_Descriptor_Function dfunc;
	uint32_t i, port_cnt;

	_module_path = module_path;
	_module = new Glib::Module(_module_path);
	_control_data = 0;
	_shadow_data = 0;
	_latency_control_port = 0;
	_was_activated = false;

	if (!(*_module)) {
		error << _("LADSPA: Unable to open module: ") << Glib::Module::get_last_error() << endmsg;
		delete _module;
		throw failed_constructor();
	}

	if (!_module->get_symbol("ladspa_descriptor", func)) {
		error << _("LADSPA: module has no descriptor function.") << endmsg;
		throw failed_constructor();
	}

	dfunc = (LADSPA_Descriptor_Function)func;

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

	// glib has internal reference counting on modules so this is ok
	delete _module;

	delete [] _control_data;
	delete [] _shadow_data;
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
		}

		else if (LADSPA_IS_HINT_DEFAULT_LOW(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound * 0.75f + prh[port].UpperBound * 0.25f;
			bounds_given = true;
			sr_scaling = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound * 0.5f + prh[port].UpperBound * 0.5f;
			bounds_given = true;
			sr_scaling = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_HIGH(prh[port].HintDescriptor)) {
			ret = prh[port].LowerBound * 0.25f + prh[port].UpperBound * 0.75f;
			bounds_given = true;
			sr_scaling = true;
		}
		else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(prh[port].HintDescriptor)) {
			ret = prh[port].UpperBound;
			bounds_given = true;
			sr_scaling = true;
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

		if (get_parameter (which) == val) {
			return;
		}

		_shadow_data[which] = (LADSPA_Data) val;

#if 0
		if (which < parameter_count() && controls[which]) {
			controls[which]->Changed ();
		}
#endif

	} else {
		warning << string_compose (_("illegal parameter number used with plugin \"%1\". This may "
					     "indicate a change in the plugin design, and presets may be "
					     "invalid"), name())
			<< endmsg;
	}

	Plugin::set_parameter (which, val);
}

/** @return `plugin' value */
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

void
LadspaPlugin::add_state (XMLNode* root) const
{
	XMLNode *child;
	char buf[16];
	LocaleGuard lg (X_("POSIX"));

	for (uint32_t i = 0; i < parameter_count(); ++i){

		if (LADSPA_IS_PORT_INPUT(port_descriptor (i)) &&
		    LADSPA_IS_PORT_CONTROL(port_descriptor (i))){

			child = new XMLNode("Port");
			snprintf(buf, sizeof(buf), "%u", i);
			child->add_property("number", string(buf));
			snprintf(buf, sizeof(buf), "%+f", _shadow_data[i]);
			child->add_property("value", string(buf));
			root->add_child_nocopy (*child);
		}
	}
}

int
LadspaPlugin::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

#ifndef NO_PLUGIN_STATE
	XMLNodeList nodes;
	XMLProperty *prop;
	XMLNodeConstIterator iter;
	XMLNode *child;
	const char *port;
	const char *data;
	uint32_t port_id;
#endif
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LadspaPlugin::set_state") << endmsg;
		return -1;
	}

#ifndef NO_PLUGIN_STATE

	nodes = node.children ("Port");

	for (iter = nodes.begin(); iter != nodes.end(); ++iter) {

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
#endif

	latency_compute_run ();

	return Plugin::set_state (node, version);
}

int
LadspaPlugin::set_state_2X (const XMLNode& node, int /* version */)
{
#ifndef NO_PLUGIN_STATE
	XMLNodeList nodes;
	XMLProperty *prop;
	XMLNodeConstIterator iter;
	XMLNode *child;
	const char *port;
	const char *data;
	uint32_t port_id;
#endif
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LadspaPlugin::set_state") << endmsg;
		return -1;
	}

#ifndef NO_PLUGIN_STATE
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
#endif

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

ARDOUR::framecnt_t
LadspaPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

	if (_latency_control_port) {
		return (framecnt_t) floor (*_latency_control_port);
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
LadspaPlugin::connect_and_run (BufferSet& bufs,
		ChanMapping in_map, ChanMapping out_map,
		pframes_t nframes, framecnt_t offset)
{
	Plugin::connect_and_run (bufs, in_map, out_map, nframes, offset);

	cycles_t now;
	cycles_t then = get_cycles ();

	BufferSet& silent_bufs  = _session.get_silent_buffers(ChanCount(DataType::AUDIO, 1));
	BufferSet& scratch_bufs = _session.get_scratch_buffers(ChanCount(DataType::AUDIO, 1));

	uint32_t audio_in_index  = 0;
	uint32_t audio_out_index = 0;
	bool valid;
	for (uint32_t port_index = 0; port_index < parameter_count(); ++port_index) {
		if (LADSPA_IS_PORT_AUDIO(port_descriptor(port_index))) {
			if (LADSPA_IS_PORT_INPUT(port_descriptor(port_index))) {
				const uint32_t buf_index = in_map.get(DataType::AUDIO, audio_in_index++, &valid);
				connect_port(port_index,
				             valid ? bufs.get_audio(buf_index).data(offset)
				                   : silent_bufs.get_audio(0).data(offset));
			} else if (LADSPA_IS_PORT_OUTPUT(port_descriptor(port_index))) {
				const uint32_t buf_index = out_map.get(DataType::AUDIO, audio_out_index++, &valid);
				connect_port(port_index,
				             valid ? bufs.get_audio(buf_index).data(offset)
				                   : scratch_bufs.get_audio(0).data(offset));
			}
		}
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

boost::shared_ptr<Plugin::ScalePoints>
LadspaPlugin::get_scale_points(uint32_t port_index) const
{
	boost::shared_ptr<Plugin::ScalePoints> ret;
#ifdef HAVE_LRDF
	const uint32_t id     = atol(unique_id().c_str());
	lrdf_defaults* points = lrdf_get_scale_values(id, port_index);

	if (!points) {
		return ret;
	}

	ret = boost::shared_ptr<Plugin::ScalePoints>(new ScalePoints());

	for (uint32_t i = 0; i < points->count; ++i) {
		ret->insert(make_pair(points->items[i].label,
		                      points->items[i].value));
	}

	lrdf_free_setting_values(points);
#endif
	return ret;
}

void
LadspaPlugin::run_in_place (pframes_t nframes)
{
	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (LADSPA_IS_PORT_INPUT(port_descriptor (i)) && LADSPA_IS_PORT_CONTROL(port_descriptor (i))) {
			_control_data[i] = _shadow_data[i];
		}
	}

	assert (_was_activated);

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
	const framecnt_t bufsize = 1024;
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
		PluginPtr plugin (new LadspaPlugin (path, session.engine(), session, index, session.frame_rate()));
		plugin->set_info(PluginInfoPtr(new LadspaPluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

LadspaPluginInfo::LadspaPluginInfo()
{
       type = ARDOUR::LADSPA;
}


void
LadspaPlugin::find_presets ()
{
#ifdef HAVE_LRDF
	uint32_t id;
	std::string unique (unique_id());

	if (!isdigit (unique[0])) {
		return;
	}

	id = atol (unique.c_str());

	lrdf_uris* set_uris = lrdf_get_setting_uris(id);

	if (set_uris) {
		for (uint32_t i = 0; i < (uint32_t) set_uris->count; ++i) {
			if (char* label = lrdf_get_label(set_uris->items[i])) {
				PresetRecord rec (set_uris->items[i], label);
				_presets.insert (make_pair (set_uris->items[i], rec));
			}
		}
		lrdf_free_uris(set_uris);
	}
#endif
}


bool
LadspaPlugin::load_preset (PresetRecord r)
{
#ifdef HAVE_LRDF
	lrdf_defaults* defs = lrdf_get_setting_values (r.uri.c_str());

	if (defs) {
		for (uint32_t i = 0; i < (uint32_t) defs->count; ++i) {
			if (parameter_is_input (defs->items[i].pid)) {
				set_parameter(defs->items[i].pid, defs->items[i].value);
			}
		}
		lrdf_free_setting_values(defs);
	}

	Plugin::load_preset (r);
#endif
	return true;
}

/* XXX: should be in liblrdf */
static void
lrdf_remove_preset (const char* /*source*/, const char *setting_uri)
{
#ifdef HAVE_LRDF
	lrdf_statement p;
	lrdf_statement *q;
	lrdf_statement *i;
	char setting_uri_copy[64];
	char buf[64];

	strncpy(setting_uri_copy, setting_uri, sizeof(setting_uri_copy));

	p.subject = setting_uri_copy;
	strncpy(buf, LADSPA_BASE "hasPortValue", sizeof(buf));
	p.predicate = buf;
	p.object = NULL;
	q = lrdf_matches(&p);

	p.predicate = NULL;
	p.object = NULL;
	for (i = q; i; i = i->next) {
		p.subject = i->object;
		lrdf_remove_matches(&p);
	}

	lrdf_free_statements(q);

	p.subject = NULL;
	strncpy(buf, LADSPA_BASE "hasSetting", sizeof(buf));
	p.predicate = buf;
	p.object = setting_uri_copy;
	lrdf_remove_matches(&p);

	p.subject = setting_uri_copy;
	p.predicate = NULL;
	p.object = NULL;
	lrdf_remove_matches (&p);
#endif
}

void
LadspaPlugin::do_remove_preset (string name)
{
#ifdef HAVE_LRDF
	string const envvar = preset_envvar ();
	if (envvar.empty()) {
		warning << _("Could not locate HOME.  Preset not removed.") << endmsg;
		return;
	}

	Plugin::PresetRecord const * p = preset_by_label (name);
	if (!p) {
		return;
	}

	string const source = preset_source (envvar);
	lrdf_remove_preset (source.c_str(), p->uri.c_str ());

	write_preset_file (envvar);
#endif
}

string
LadspaPlugin::preset_envvar () const
{
	char* envvar;
	if ((envvar = getenv ("HOME")) == 0) {
		return "";
	}

	return envvar;
}

string
LadspaPlugin::preset_source (string envvar) const
{
	return string_compose ("file:%1/.ladspa/rdf/ardour-presets.n3", envvar);
}

bool
LadspaPlugin::write_preset_file (string envvar)
{
#ifdef HAVE_LRDF
	string path = string_compose("%1/.ladspa", envvar);
	if (g_mkdir_with_parents (path.c_str(), 0775)) {
		warning << string_compose(_("Could not create %1.  Preset not saved. (%2)"), path, strerror(errno)) << endmsg;
		return false;
	}

	path += "/rdf";
	if (g_mkdir_with_parents (path.c_str(), 0775)) {
		warning << string_compose(_("Could not create %1.  Preset not saved. (%2)"), path, strerror(errno)) << endmsg;
		return false;
	}

	string const source = preset_source (envvar);

	if (lrdf_export_by_source (source.c_str(), source.substr(5).c_str())) {
		warning << string_compose(_("Error saving presets file %1."), source) << endmsg;
		return false;
	}

	return true;
#else
	return false;
#endif
}

string
LadspaPlugin::do_save_preset (string name)
{
#ifdef HAVE_LRDF
	/* make a vector of pids that are input parameters */
	vector<int> input_parameter_pids;
	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input (i)) {
			input_parameter_pids.push_back (i);
		}
	}

	std::string unique (unique_id());

	if (!isdigit (unique[0])) {
		return "";
	}

	uint32_t const id = atol (unique.c_str());

	lrdf_defaults defaults;
	defaults.count = input_parameter_pids.size ();
	lrdf_portvalue portvalues[input_parameter_pids.size()];
	defaults.items = portvalues;

	for (vector<int>::size_type i = 0; i < input_parameter_pids.size(); ++i) {
		portvalues[i].pid = input_parameter_pids[i];
		portvalues[i].value = get_parameter (input_parameter_pids[i]);
	}

	string const envvar = preset_envvar ();
	if (envvar.empty()) {
		warning << _("Could not locate HOME.  Preset not saved.") << endmsg;
		return "";
	}

	string const source = preset_source (envvar);

	char* uri_char = lrdf_add_preset (source.c_str(), name.c_str(), id, &defaults);
	string uri (uri_char);
	free (uri_char);

	if (!write_preset_file (envvar)) {
		return "";
	}

	return uri;
#else
	return string();
#endif
}

LADSPA_PortDescriptor
LadspaPlugin::port_descriptor (uint32_t i) const
{
	if (i < _descriptor->PortCount) {		
		return _descriptor->PortDescriptors[i];
	}
	
	warning << "LADSPA plugin port index " << i << " out of range." << endmsg;
	return 0;
}

		
	
