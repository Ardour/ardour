/*
    Copyright (C) 2008 Paul Davis
    Author: Dave Robillard

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
#include <string>

#include <cstdlib>
#include <cmath>
#include <cstring>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"
#include "pbd/xml++.h"

#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_buffer.h"
#include "ardour/lv2_event_buffer.h"
#include "ardour/lv2_plugin.h"

#include "pbd/stl_delete.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

URIMap   LV2Plugin::_uri_map;
uint32_t LV2Plugin::_midi_event_type = _uri_map.uri_to_id(
		"http://lv2plug.in/ns/ext/event",
		"http://lv2plug.in/ns/ext/midi#MidiEvent");

LV2Plugin::LV2Plugin (AudioEngine& e, Session& session, LV2World& world, SLV2Plugin plugin, nframes_t rate)
	: Plugin (e, session)
	, _world(world)
	, _features(NULL)
{
	init (world, plugin, rate);
}

LV2Plugin::LV2Plugin (const LV2Plugin &other)
	: Plugin (other)
	, _world(other._world)
	, _features(NULL)
{
	init (other._world, other._plugin, other._sample_rate);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		_control_data[i] = other._shadow_data[i];
		_shadow_data[i] = other._shadow_data[i];
	}
}

void
LV2Plugin::init (LV2World& world, SLV2Plugin plugin, nframes_t rate)
{
	_world = world;
	_plugin = plugin;
	_ui = NULL;
	_control_data = 0;
	_shadow_data = 0;
	_latency_control_port = 0;
	_was_activated = false;

	_instance = slv2_plugin_instantiate(plugin, rate, _features);
	_name = slv2_plugin_get_name(plugin);
	assert(_name);
	_author = slv2_plugin_get_author_name(plugin);

	if (_instance == 0) {
		error << _("LV2: Failed to instantiate plugin ") << slv2_plugin_get_uri(plugin) << endl;
		throw failed_constructor();
	}

	if (slv2_plugin_has_feature(plugin, world.in_place_broken)) {
		error << string_compose(
				_("LV2: \"%1\" cannot be used, since it cannot do inplace processing"),
				slv2_value_as_string(_name));
		slv2_value_free(_name);
		slv2_value_free(_author);
		throw failed_constructor();
	}

	_instance_access_feature.URI = "http://lv2plug.in/ns/ext/instance-access";
	_instance_access_feature.data = (void*)_instance->lv2_handle;

	_data_access_extension_data.extension_data = _instance->lv2_descriptor->extension_data;
	_data_access_feature.URI = "http://lv2plug.in/ns/ext/data-access";
	_data_access_feature.data = &_data_access_extension_data;

	_features = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * 4);
	_features[0] = &_instance_access_feature;
	_features[1] = &_data_access_feature;
	_features[2] = _uri_map.feature();
	_features[3] = NULL;

	_sample_rate = rate;

	const uint32_t num_ports = slv2_plugin_get_num_ports(plugin);

	_control_data = new float[num_ports];
	_shadow_data = new float[num_ports];
	_defaults = new float[num_ports];

	const bool latent = slv2_plugin_has_latency(plugin);
	uint32_t latency_port = (latent ? slv2_plugin_get_latency_port_index(plugin) : 0);

	for (uint32_t i = 0; i < num_ports; ++i) {
		SLV2Port port = slv2_plugin_get_port_by_index(plugin, i);
		SLV2Value sym = slv2_port_get_symbol(_plugin, port);
		_port_indices.insert(std::make_pair(slv2_value_as_string(sym), i));
		if (parameter_is_control(i)) {
			SLV2Value def;
			slv2_port_get_range(plugin, port, &def, NULL, NULL);
			_defaults[i] = def ? slv2_value_as_float(def) : 0.0f;
			slv2_value_free(def);

			slv2_instance_connect_port (_instance, i, &_control_data[i]);

			if (latent && i == latency_port) {
				_latency_control_port = &_control_data[i];
				*_latency_control_port = 0;
			}

			if (parameter_is_input(i)) {
				_shadow_data[i] = default_value (i);
			}
		} else {
			_defaults[i] = 0.0f;
		}
	}

	SLV2UIs uis = slv2_plugin_get_uis(_plugin);
	if (slv2_uis_size(uis) > 0) {
		for (unsigned i=0; i < slv2_uis_size(uis); ++i) {
			SLV2UI ui = slv2_uis_get_at(uis, i);
			if (slv2_ui_is_a(ui, _world.gtk_gui)) {
				_ui = ui;
				break;
			}
		}

		// if gtk gui is not available, try to find external gui
		if (!_ui) {
			for (unsigned i=0; i < slv2_uis_size(uis); ++i) {
				SLV2UI ui = slv2_uis_get_at(uis, i);
				if (slv2_ui_is_a(ui, _world.external_gui)) {
					_ui = ui;
					break;
				}
			}
		}
	}

	latency_compute_run ();
}

LV2Plugin::~LV2Plugin ()
{
	deactivate ();
	cleanup ();

	GoingAway (); /* EMIT SIGNAL */

	slv2_instance_free(_instance);
	slv2_value_free(_name);
	slv2_value_free(_author);

	delete [] _control_data;
	delete [] _shadow_data;
}

bool
LV2Plugin::is_external_ui() const
{
	return slv2_ui_is_a(_ui, _world.external_gui);
}

string
LV2Plugin::unique_id() const
{
	return slv2_value_as_uri(slv2_plugin_get_uri(_plugin));
}


float
LV2Plugin::default_value (uint32_t port)
{
	return _defaults[port];
}

const char*
LV2Plugin::port_symbol (uint32_t index)
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, index);
	if (!port) {
		error << name() << ": Invalid port index " << index << endmsg;
	}

	SLV2Value sym = slv2_port_get_symbol(_plugin, port);
	return slv2_value_as_string(sym);
}


void
LV2Plugin::set_parameter (uint32_t which, float val)
{
	if (which < slv2_plugin_get_num_ports(_plugin)) {
		_shadow_data[which] = val;
#if 0
		ParameterChanged (which, val); /* EMIT SIGNAL */

		if (which < parameter_count() && controls[which]) {
			controls[which]->Changed ();
		}
#endif

	} else {
		warning << string_compose (_("Illegal parameter number used with plugin \"%1\"."
				"This is a bug in either Ardour or the LV2 plugin (%2)"),
				name(), unique_id()) << endmsg;
	}
}

float
LV2Plugin::get_parameter (uint32_t which) const
{
	if (parameter_is_input(which)) {
		return (float) _shadow_data[which];
	} else {
		return (float) _control_data[which];
	}
	return 0.0f;
}

uint32_t
LV2Plugin::nth_parameter (uint32_t n, bool& ok) const
{
	uint32_t x, c;

	ok = false;

	for (c = 0, x = 0; x < slv2_plugin_get_num_ports(_plugin); ++x) {
		if (parameter_is_control (x)) {
			if (c++ == n) {
				ok = true;
				return x;
			}
		}
	}

	return 0;
}

XMLNode&
LV2Plugin::get_state()
{
	XMLNode *root = new XMLNode(state_node_name());
	XMLNode *child;
	char buf[16];
	LocaleGuard lg (X_("POSIX"));

	for (uint32_t i = 0; i < parameter_count(); ++i){

		if (parameter_is_input(i) && parameter_is_control(i)) {
			child = new XMLNode("Port");
			/*snprintf(buf, sizeof(buf), "%u", i);
			child->add_property("number", string(buf));*/
			child->add_property("symbol", port_symbol(i));
			snprintf(buf, sizeof(buf), "%+f", _shadow_data[i]);
			child->add_property("value", string(buf));
			root->add_child_nocopy (*child);

			/*if (i < controls.size() && controls[i]) {
				root->add_child_nocopy (controls[i]->get_state());
			}*/
		}
	}

	return *root;
}

vector<Plugin::PresetRecord>
LV2Plugin::get_presets()
{
	vector<PresetRecord> result;
	SLV2Results presets = slv2_plugin_query_sparql(_plugin,
			"PREFIX lv2p: <http://lv2plug.in/ns/dev/presets#>\n"
			"PREFIX dc:  <http://dublincore.org/documents/dcmi-namespace/>\n"
			"SELECT ?p ?name WHERE { <> lv2p:hasPreset ?p . ?p dc:title ?name }\n");
	for (; !slv2_results_finished(presets); slv2_results_next(presets)) {
		SLV2Value uri  = slv2_results_get_binding_value(presets, 0);
		SLV2Value name = slv2_results_get_binding_value(presets, 1);
		PresetRecord rec(slv2_value_as_string(uri), slv2_value_as_string(name));
		result.push_back(rec);
		this->presets.insert(std::make_pair(slv2_value_as_string(uri), rec));
	}
	slv2_results_free(presets);
	return result;
}

bool
LV2Plugin::load_preset(const string uri)
{
	const string query = string(
			"PREFIX lv2p: <http://lv2plug.in/ns/dev/presets#>\n"
			"PREFIX dc:  <http://dublincore.org/documents/dcmi-namespace/>\n"
			"SELECT ?sym ?val WHERE { <") + uri + "> lv2:port ?port . "
				" ?port lv2:symbol ?sym ; lv2p:value ?val . }";
	SLV2Results values = slv2_plugin_query_sparql(_plugin, query.c_str());
	for (; !slv2_results_finished(values); slv2_results_next(values)) {
		SLV2Value sym  = slv2_results_get_binding_value(values, 0);
		SLV2Value val = slv2_results_get_binding_value(values, 1);
		if (slv2_value_is_float(val)) {
			uint32_t index = _port_indices[slv2_value_as_string(sym)];
			set_parameter(index, slv2_value_as_float(val));
		}
	}
	slv2_results_free(values);
	return true;
}

bool
LV2Plugin::save_preset (string /*name*/)
{
	return false;
}

bool
LV2Plugin::has_editor() const
{
	return (_ui != NULL);
}

int
LV2Plugin::set_state(const XMLNode& node)
{
	XMLNodeList nodes;
	XMLProperty *prop;
	XMLNodeConstIterator iter;
	XMLNode *child;
	const char *sym;
	const char *value;
	uint32_t port_id;
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LV2Plugin::set_state") << endmsg;
		return -1;
	}

	nodes = node.children ("Port");

	for (iter = nodes.begin(); iter != nodes.end(); ++iter){

		child = *iter;

		if ((prop = child->property("symbol")) != 0) {
			sym = prop->value().c_str();
		} else {
			warning << _("LV2: port has no symbol, ignored") << endmsg;
			continue;
		}

		map<string,uint32_t>::iterator i = _port_indices.find(sym);
		if (i != _port_indices.end()) {
			port_id = i->second;
		} else {
			warning << _("LV2: port has unknown index, ignored") << endmsg;
			continue;
		}

		if ((prop = child->property("value")) != 0) {
			value = prop->value().c_str();
		} else {
			warning << _("LV2: port has no value, ignored") << endmsg;
			continue;
		}

		set_parameter (port_id, atof(value));
	}

	latency_compute_run ();

	return 0;
}

int
LV2Plugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, which);

	SLV2Value def, min, max;
	slv2_port_get_range(_plugin, port, &def, &min, &max);

    desc.integer_step = slv2_port_has_property(_plugin, port, _world.integer);
    desc.toggled = slv2_port_has_property(_plugin, port, _world.toggled);
    desc.logarithmic = slv2_port_has_property(_plugin, port, _world.logarithmic);
    desc.sr_dependent = slv2_port_has_property(_plugin, port, _world.srate);
    desc.label = slv2_value_as_string(slv2_port_get_name(_plugin, port));
    desc.lower = min ? slv2_value_as_float(min) : 0.0f;
    desc.upper = max ? slv2_value_as_float(max) : 1.0f;
    desc.min_unbound = false; // TODO (LV2 extension)
    desc.max_unbound = false; // TODO (LV2 extension)

	if (desc.integer_step) {
		desc.step = 1.0;
		desc.smallstep = 0.1;
		desc.largestep = 10.0;
	} else {
		const float delta = desc.upper - desc.lower;
		desc.step = delta / 1000.0f;
		desc.smallstep = delta / 10000.0f;
		desc.largestep = delta/10.0f;
	}

	slv2_value_free(def);
	slv2_value_free(min);
	slv2_value_free(max);

	return 0;
}


string
LV2Plugin::describe_parameter (Evoral::Parameter which)
{
	if (which.type() == PluginAutomation && which.id() < parameter_count()) {
		SLV2Value name = slv2_port_get_name(_plugin,
			slv2_plugin_get_port_by_index(_plugin, which));
		string ret(slv2_value_as_string(name));
		slv2_value_free(name);
		return ret;
	} else {
		return "??";
	}
}

nframes_t
LV2Plugin::signal_latency () const
{
	if (_latency_control_port) {
		return (nframes_t) floor (*_latency_control_port);
	} else {
		return 0;
	}
}

set<Evoral::Parameter>
LV2Plugin::automatable () const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i){
		if (parameter_is_input(i) && parameter_is_control(i)) {
			ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
		}
	}

	return ret;
}

int
LV2Plugin::connect_and_run (BufferSet& bufs,
		ChanMapping in_map, ChanMapping out_map,
		nframes_t nframes, nframes_t offset)
{
	cycles_t then = get_cycles ();

	uint32_t audio_in_index  = 0;
	uint32_t audio_out_index = 0;
	uint32_t midi_in_index   = 0;
	uint32_t midi_out_index  = 0;
	for (uint32_t port_index = 0; port_index < parameter_count(); ++port_index) {
		if (parameter_is_audio(port_index)) {
			if (parameter_is_input(port_index)) {
				const uint32_t buf_index = in_map.get(DataType::AUDIO, audio_in_index++);
				//cerr << port_index << " : " << " AUDIO IN " << buf_index << endl;
				slv2_instance_connect_port(_instance, port_index,
						bufs.get_audio(buf_index).data(offset));
			} else if (parameter_is_output(port_index)) {
				const uint32_t buf_index = out_map.get(DataType::AUDIO, audio_out_index++);
				//cerr << port_index << " : " << " AUDIO OUT " << buf_index << endl;
				slv2_instance_connect_port(_instance, port_index,
						bufs.get_audio(buf_index).data(offset));
			}
		} else if (parameter_is_midi(port_index)) {
			if (parameter_is_input(port_index)) {
				const uint32_t buf_index = in_map.get(DataType::MIDI, midi_in_index++);
				//cerr << port_index << " : " << " MIDI IN " << buf_index << endl;
				slv2_instance_connect_port(_instance, port_index,
						bufs.get_lv2_midi(true, buf_index).data());
			} else if (parameter_is_output(port_index)) {
				const uint32_t buf_index = out_map.get(DataType::MIDI, midi_out_index++);
				//cerr << port_index << " : " << " MIDI OUT " << buf_index << endl;
				slv2_instance_connect_port(_instance, port_index,
						bufs.get_lv2_midi(false, buf_index).data());
			}
		} else if (!parameter_is_control(port_index)) {
			// Optional port (it'd better be if we've made it this far...)
			slv2_instance_connect_port(_instance, port_index, NULL);
		}
	}

	run (nframes);

	midi_out_index = 0;
	for (uint32_t port_index = 0; port_index < parameter_count(); ++port_index) {
		if (parameter_is_midi(port_index) && parameter_is_output(port_index)) {
			const uint32_t buf_index = out_map.get(DataType::MIDI, midi_out_index++);
			bufs.flush_lv2_midi(true, buf_index);
		}
	}

	cycles_t now = get_cycles ();
	set_cycles ((uint32_t) (now - then));

	return 0;
}

bool
LV2Plugin::parameter_is_control (uint32_t param) const
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, param);
	return slv2_port_is_a(_plugin, port, _world.control_class);
}

bool
LV2Plugin::parameter_is_audio (uint32_t param) const
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, param);
	return slv2_port_is_a(_plugin, port, _world.audio_class);
}

bool
LV2Plugin::parameter_is_midi (uint32_t param) const
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, param);
	return slv2_port_is_a(_plugin, port, _world.event_class);
	//	&& slv2_port_supports_event(_plugin, port, _world.midi_class);
}

bool
LV2Plugin::parameter_is_output (uint32_t param) const
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, param);
	return slv2_port_is_a(_plugin, port, _world.output_class);
}

bool
LV2Plugin::parameter_is_input (uint32_t param) const
{
	SLV2Port port = slv2_plugin_get_port_by_index(_plugin, param);
	return slv2_port_is_a(_plugin, port, _world.input_class);
}

void
LV2Plugin::print_parameter (uint32_t param, char *buf, uint32_t len) const
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
LV2Plugin::run (nframes_t nframes)
{
	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_control(i) && parameter_is_input(i))  {
			_control_data[i] = _shadow_data[i];
		}
	}

	slv2_instance_run(_instance, nframes);
}

void
LV2Plugin::latency_compute_run ()
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
	float buffer[bufsize];

	memset(buffer,0,sizeof(float)*bufsize);

	/* Note that we've already required that plugins
	   be able to handle in-place processing.
	*/

	port_index = 0;

	while (port_index < parameter_count()) {
		if (parameter_is_audio (port_index)) {
			if (parameter_is_input (port_index)) {
				slv2_instance_connect_port (_instance, port_index, buffer);
				in_index++;
			} else if (parameter_is_output (port_index)) {
				slv2_instance_connect_port (_instance, port_index, buffer);
				out_index++;
			}
		}
		port_index++;
	}

	run (bufsize);
	deactivate ();
}

LV2World::LV2World()
	: world(slv2_world_new())
{
	slv2_world_load_all(world);
	input_class = slv2_value_new_uri(world, SLV2_PORT_CLASS_INPUT);
	output_class = slv2_value_new_uri(world, SLV2_PORT_CLASS_OUTPUT);
	control_class = slv2_value_new_uri(world, SLV2_PORT_CLASS_CONTROL);
	audio_class = slv2_value_new_uri(world, SLV2_PORT_CLASS_AUDIO);
	event_class = slv2_value_new_uri(world, SLV2_PORT_CLASS_EVENT);
	midi_class = slv2_value_new_uri(world, SLV2_EVENT_CLASS_MIDI);
	in_place_broken = slv2_value_new_uri(world, SLV2_NAMESPACE_LV2 "inPlaceBroken");
	integer = slv2_value_new_uri(world, SLV2_NAMESPACE_LV2 "integer");
	toggled = slv2_value_new_uri(world, SLV2_NAMESPACE_LV2 "toggled");
	srate = slv2_value_new_uri(world, SLV2_NAMESPACE_LV2 "sampleRate");
	gtk_gui = slv2_value_new_uri(world, "http://lv2plug.in/ns/extensions/ui#GtkUI");
	external_gui = slv2_value_new_uri(world, "http://lv2plug.in/ns/extensions/ui#external");
	logarithmic = slv2_value_new_uri(world, "http://lv2plug.in/ns/dev/extportinfo#logarithmic");
}

LV2World::~LV2World()
{
	slv2_value_free(input_class);
	slv2_value_free(output_class);
	slv2_value_free(control_class);
	slv2_value_free(audio_class);
	slv2_value_free(event_class);
	slv2_value_free(midi_class);
	slv2_value_free(in_place_broken);
}

LV2PluginInfo::LV2PluginInfo (void* lv2_world, void* slv2_plugin)
	: _lv2_world(lv2_world)
	, _slv2_plugin(slv2_plugin)
{
}

LV2PluginInfo::~LV2PluginInfo()
{
}

PluginPtr
LV2PluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		plugin.reset (new LV2Plugin (session.engine(), session,
				*(LV2World*)_lv2_world, (SLV2Plugin)_slv2_plugin, session.frame_rate()));

		plugin->set_info(PluginInfoPtr(new LV2PluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}

	return PluginPtr();
}

PluginInfoList
LV2PluginInfo::discover (void* lv2_world)
{
	PluginInfoList plugs;

	LV2World* world = (LV2World*)lv2_world;
	SLV2Plugins plugins = slv2_world_get_all_plugins(world->world);

	cerr << "LV2: Discovered " << slv2_plugins_size (plugins) << " plugins\n";

	for (unsigned i=0; i < slv2_plugins_size(plugins); ++i) {
		SLV2Plugin p = slv2_plugins_get_at(plugins, i);
		LV2PluginInfoPtr info (new LV2PluginInfo(lv2_world, p));

		SLV2Value name = slv2_plugin_get_name(p);

		if (!name) {
			cerr << "LV2: invalid plugin\n";
			continue;
		}

		info->name = string(slv2_value_as_string(name));
		slv2_value_free(name);

		SLV2PluginClass pclass = slv2_plugin_get_class(p);
		SLV2Value label = slv2_plugin_class_get_label(pclass);
		info->category = slv2_value_as_string(label);

		SLV2Value author_name = slv2_plugin_get_author_name(p);
		info->creator = author_name ? string(slv2_value_as_string(author_name)) : "Unknown";
		slv2_value_free(author_name);

		info->path = "/NOPATH"; // Meaningless for LV2

		info->n_inputs.set_audio(slv2_plugin_get_num_ports_of_class(p,
				world->input_class, world->audio_class, NULL));
		info->n_inputs.set_midi(slv2_plugin_get_num_ports_of_class(p,
				world->input_class, world->event_class, NULL));

		info->n_outputs.set_audio(slv2_plugin_get_num_ports_of_class(p,
				world->output_class, world->audio_class, NULL));
		info->n_outputs.set_midi(slv2_plugin_get_num_ports_of_class(p,
				world->output_class, world->event_class, NULL));

		info->unique_id = slv2_value_as_uri(slv2_plugin_get_uri(p));
		info->index = 0; // Meaningless for LV2

		plugs.push_back (info);
	}

	return plugs;
}

