/*
    Copyright (C) 2008 Paul Davis 
    Author: David Robillard

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

#include <glib.h>

#include <pbd/compose.h>
#include <pbd/error.h>
#include <pbd/pathscanner.h>
#include <pbd/xml++.h>
#include <pbd/localeguard.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/lv2_plugin.h>
#include <ardour/tempo.h>

#include <pbd/stl_delete.h>

#ifdef HAVE_SUIL
#include <suil/suil.h>
#endif

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static LV2_URID
urid_map(LV2_URID_Map_Handle handle, const char* uri)
{
	return g_quark_from_string(uri);
}

const char*
urid_unmap(LV2_URID_Unmap_Handle handle, LV2_URID urid)
{
	return g_quark_to_string(urid);
}

LV2_URID_Map   LV2Plugin::_urid_map           = { NULL, urid_map };
LV2_Feature    LV2Plugin::_urid_map_feature   = { LV2_URID_MAP_URI, &LV2Plugin::_urid_map };
LV2_URID_Unmap LV2Plugin::_urid_unmap         = { NULL, urid_unmap };
LV2_Feature    LV2Plugin::_urid_unmap_feature = { LV2_URID_UNMAP_URI, &LV2Plugin::_urid_unmap };

LV2Plugin::LV2Plugin (AudioEngine& e, Session& session, LV2World& world, LilvPlugin* plugin, nframes_t rate)
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
LV2Plugin::init (LV2World& world, LilvPlugin* plugin, nframes_t rate)
{
	_world = world;
	_plugin = plugin;
	_ui = NULL;
	_control_data = 0;
	_shadow_data = 0;
	_bpm_control_port = NULL;
	_freewheel_control_port = NULL;
	_latency_control_port = NULL;
	_was_activated = false;
	
	_instance_access_feature.URI = "http://lv2plug.in/ns/ext/instance-access";
	_data_access_feature.URI = "http://lv2plug.in/ns/ext/data-access";

	_features = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * 5);
	_features[0] = &_instance_access_feature;
	_features[1] = &_data_access_feature;
	_features[2] = &_urid_map_feature;
	_features[3] = &_urid_unmap_feature;
	_features[4] = NULL;

	_instance = lilv_plugin_instantiate(plugin, rate, _features);
	_name = lilv_plugin_get_name(plugin);
	_author = lilv_plugin_get_author_name(plugin);

	_instance_access_feature.data = (void*)_instance->lv2_handle;

	_data_access_extension_data.extension_data = _instance->lv2_descriptor->extension_data;
	_data_access_feature.data = &_data_access_extension_data;
	
	if (_instance == 0) {
		error << _("LV2: Failed to instantiate plugin ") << lilv_plugin_get_uri(plugin) << endl;
		throw failed_constructor();
	}

	if (lilv_plugin_has_feature(plugin, world.in_place_broken)) {
		error << string_compose(_("LV2: \"%1\" cannot be used, since it cannot do inplace processing"),
				lilv_node_as_string(_name));
		lilv_node_free(_name);
		lilv_node_free(_author);
		throw failed_constructor();
	}
	
	_instance_access_feature.URI = "http://lv2plug.in/ns/ext/instance-access";
	_instance_access_feature.data = (void*)_instance->lv2_handle;

	_data_access_extension_data.extension_data = _instance->lv2_descriptor->extension_data;
	_data_access_feature.URI = "http://lv2plug.in/ns/ext/data-access";
	_data_access_feature.data = &_data_access_extension_data;
	
	_features = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * 5);
	_features[0] = &_instance_access_feature;
	_features[1] = &_data_access_feature;
	_features[2] = &_urid_map_feature;
	_features[3] = &_urid_unmap_feature;
	_features[4] = NULL;

	_sample_rate = rate;

	const uint32_t num_ports = lilv_plugin_get_num_ports(plugin);

	_control_data = new float[num_ports];
	_shadow_data = new float[num_ports];
	_defaults = new float[num_ports];

	const bool latent = lilv_plugin_has_latency(plugin);
	uint32_t latency_index = (latent ? lilv_plugin_get_latency_port_index(plugin) : 0);

	#define NS_TIME "http://lv2plug.in/ns/ext/time#"
	
	// Build an array of pointers to special parameter buffers
	void*** params = new void**[num_ports];
	for (uint32_t i = 0; i < num_ports; ++i) {
		params[i] = NULL;
	}
	designated_input (NS_TIME "beatsPerMinute", params, (void**)&_bpm_control_port);
	designated_input (LILV_NS_LV2 "freeWheeling", params, (void**)&_freewheel_control_port);

	for (uint32_t i = 0; i < num_ports; ++i) {
		if (parameter_is_control(i)) {
			const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
			LilvNode* def;
			lilv_port_get_range(plugin, port, &def, NULL, NULL);
			_defaults[i] = def ? lilv_node_as_float(def) : 0.0f;
			lilv_node_free(def);

			lilv_instance_connect_port (_instance, i, &_control_data[i]);

			if (latent && i == latency_index) {
				_latency_control_port = &_control_data[i];
				*_latency_control_port = 0;
			}

			if (parameter_is_input(i)) {
				_shadow_data[i] = default_value (i);
				if (params[i]) {
					*params[i] = (void*)&_shadow_data[i];
				}
			}
		} else {
			_defaults[i] = 0.0f;
		}
	}

	delete[] params;

	LilvUIs* uis = lilv_plugin_get_uis(plugin);
	if (lilv_uis_size(uis) > 0) {
#ifdef HAVE_SUIL
		// Look for embeddable UI
		LILV_FOREACH(uis, u, uis) {
			const LilvUI*   this_ui      = lilv_uis_get(uis, u);
			const LilvNode* this_ui_type = NULL;
			if (lilv_ui_is_supported(this_ui,
			                         suil_ui_supported,
			                         _world.gtk_gui,
			                         &this_ui_type)) {
				// TODO: Multiple UI support
				_ui      = this_ui;
				_ui_type = this_ui_type;
				break;
			}
		}
#else
		// Look for Gtk native UI
		LILV_FOREACH(uis, i, uis) {
			const LilvUI* ui = lilv_uis_get(uis, i);
			if (lilv_ui_is_a(ui, _world.gtk_gui)) {
				_ui      = ui;
				_ui_type = _world.gtk_gui;
				break;
			}
		}
#endif
		
		// If Gtk UI is not available, try to find external UI
		if (!_ui) {
			LILV_FOREACH(uis, i, uis) {
				const LilvUI* ui = lilv_uis_get(uis, i);
				if (lilv_ui_is_a(ui, _world.external_gui)) {
					_ui      = ui;
					_ui_type = _world.external_gui;
					break;
				}
			}
		}
	}

	Plugin::setup_controls ();

	latency_compute_run ();
}

LV2Plugin::~LV2Plugin ()
{
	deactivate ();
	cleanup ();

	GoingAway (); /* EMIT SIGNAL */
	
	lilv_instance_free(_instance);
	lilv_node_free(_name);
	lilv_node_free(_author);

	if (_control_data) {
		delete [] _control_data;
	}

	if (_shadow_data) {
		delete [] _shadow_data;
	}
}

bool
LV2Plugin::is_external_ui() const
{
	return lilv_ui_is_a(_ui, _world.external_gui);
}

string
LV2Plugin::unique_id() const
{
	return lilv_node_as_uri(lilv_plugin_get_uri(_plugin));
}


float
LV2Plugin::default_value (uint32_t port)
{
	return _defaults[port];
}	

const char*
LV2Plugin::port_symbol (uint32_t index)
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_plugin, index);
	if (!port) {
		error << name() << ": Invalid port index " << index << endmsg;
	}

	const LilvNode* sym = lilv_port_get_symbol(_plugin, port);
	return lilv_node_as_string(sym);
}


void
LV2Plugin::set_parameter (uint32_t which, float val)
{
	if (which < lilv_plugin_get_num_ports(_plugin)) {
		_shadow_data[which] = val;
		ParameterChanged (which, val); /* EMIT SIGNAL */

		if (which < parameter_count() && controls[which]) {
			controls[which]->Changed ();
		}
		
	} else {
		warning << string_compose (_("Illegal parameter number used with plugin \"%1\"."
				"This is a bug in either %2 or the LV2 plugin (%3)"),
				name(), PROGRAM_NAME, unique_id()) << endmsg;
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

	for (c = 0, x = 0; x < lilv_plugin_get_num_ports(_plugin); ++x) {
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
			child = new XMLNode("port");
			snprintf(buf, sizeof(buf), "%u", i);
			child->add_property("number", string(buf));
			child->add_property("symbol", port_symbol(i));
			snprintf(buf, sizeof(buf), "%+f", _shadow_data[i]);
			child->add_property("value", string(buf));
			root->add_child_nocopy (*child);

			if (i < controls.size() && controls[i]) {
				root->add_child_nocopy (controls[i]->get_state());
			}
		}
	}

	return *root;
}

bool
LV2Plugin::save_preset (string name)
{
	return Plugin::save_preset (name, "lv2");
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
	const char *port;
	const char *data;
	uint32_t port_id;
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LV2Plugin::set_state") << endmsg;
		return -1;
	}

	nodes = node.children ("port");

	for(iter = nodes.begin(); iter != nodes.end(); ++iter){

		child = *iter;

		if ((prop = child->property("number")) != 0) {
			port = prop->value().c_str();
		} else {
			warning << _("LV2: no lv2 port number") << endmsg;
			continue;
		}

		if ((prop = child->property("value")) != 0) {
			data = prop->value().c_str();
		} else {
			warning << _("LV2: no lv2 port data") << endmsg;
			continue;
		}

		sscanf (port, "%" PRIu32, &port_id);
		set_parameter (port_id, atof(data));
	}
	
	latency_compute_run ();

	return 0;
}

int
LV2Plugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_plugin, which);

	LilvNode *def, *min, *max;
	lilv_port_get_range(_plugin, port, &def, &min, &max);
	
    desc.integer_step = lilv_port_has_property(_plugin, port, _world.integer);
    desc.toggled = lilv_port_has_property(_plugin, port, _world.toggled);
    desc.logarithmic = lilv_port_has_property(_plugin, port, _world.logarithmic);
    desc.sr_dependent = lilv_port_has_property(_plugin, port, _world.srate);
    desc.label = lilv_node_as_string(lilv_port_get_name(_plugin, port));
    desc.lower = min ? lilv_node_as_float(min) : 0.0f;
    desc.upper = max ? lilv_node_as_float(max) : 1.0f;
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

	lilv_node_free(def);
	lilv_node_free(min);
	lilv_node_free(max);

	return 0;
}


string
LV2Plugin::describe_parameter (uint32_t which)
{
	if (which < parameter_count()) {
		LilvNode* name = lilv_port_get_name(_plugin,
			lilv_plugin_get_port_by_index(_plugin, which));
		string ret(lilv_node_as_string(name));
		lilv_node_free(name);
		return ret;
	} else {
		return "??";
	}
}

nframes_t
LV2Plugin::latency () const
{
	if (_latency_control_port) {
		return (nframes_t) floor (*_latency_control_port);
	} else {
		return 0;
	}
}

set<uint32_t>
LV2Plugin::automatable () const
{
	set<uint32_t> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i){
		if (parameter_is_input(i) && parameter_is_control(i)) {
			ret.insert (ret.end(), i);
		}
	}

	return ret;
}

int
LV2Plugin::connect_and_run (vector<Sample*>& bufs, uint32_t nbufs, int32_t& in_index, int32_t& out_index, nframes_t nframes, nframes_t offset)
{
	uint32_t port_index;
	cycles_t then, now;

	port_index = 0;

	then = get_cycles ();

	if (_freewheel_control_port) {
		*_freewheel_control_port = _session.engine().freewheeling ();
	}

	if (_bpm_control_port) {
		TempoMap& tmap (_session.tempo_map ());
		TempoMap::Metric metric = tmap.metric_at (_session.transport_frame () + offset);
		*_bpm_control_port = metric.tempo().beats_per_minute ();
	}
		
	while (port_index < parameter_count()) {
		if (parameter_is_audio(port_index)) {
			if (parameter_is_input(port_index)) {
				lilv_instance_connect_port(_instance, port_index,
							   bufs[min((uint32_t)in_index, nbufs - 1)] + offset);
				in_index++;
			} else if (parameter_is_output(port_index)) {
				lilv_instance_connect_port(_instance, port_index,
							   bufs[min((uint32_t)out_index, nbufs - 1)] + offset);
				out_index++;
			}
		}
		port_index++;
	}
	
	run (nframes);
	now = get_cycles ();
	set_cycles ((uint32_t) (now - then));

	return 0;
}

bool
LV2Plugin::parameter_is_control (uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_plugin, param);
	return lilv_port_is_a(_plugin, port, _world.control_class);
}

bool
LV2Plugin::parameter_is_audio (uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_plugin, param);
	return lilv_port_is_a(_plugin, port, _world.audio_class);
}

bool
LV2Plugin::parameter_is_output (uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_plugin, param);
	return lilv_port_is_a(_plugin, port, _world.output_class);
}

bool
LV2Plugin::parameter_is_input (uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_plugin, param);
	return lilv_port_is_a(_plugin, port, _world.input_class);
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

	lilv_instance_run(_instance, nframes);
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
				lilv_instance_connect_port (_instance, port_index, buffer);
				in_index++;
			} else if (parameter_is_output (port_index)) {
				lilv_instance_connect_port (_instance, port_index, buffer);
				out_index++;
			}
		}
		port_index++;
	}
	
	run (bufsize);
	deactivate ();
}

LilvPort*
LV2Plugin::designated_input (const char* uri, void** bufptrs[], void** bufptr)
{
	LilvPort* port = NULL;
#ifdef HAVE_NEW_LILV
	LilvNode* designation = lilv_new_uri(_world.world, uri);
	port = lilv_plugin_get_port_by_designation(
		_plugin, _world.input_class, designation);
	lilv_node_free(designation);
	if (port) {
		bufptrs[lilv_port_get_index(_plugin, port)] = bufptr;
	}
#endif
	return port;
}

LV2World::LV2World()
	: world(lilv_world_new())
{
	lilv_world_load_all(world);
	input_class = lilv_new_uri(world, LILV_URI_INPUT_PORT);
	output_class = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
	control_class = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
	audio_class = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
	in_place_broken = lilv_new_uri(world, LILV_NS_LV2 "inPlaceBroken");
	integer = lilv_new_uri(world, LILV_NS_LV2 "integer");
	toggled = lilv_new_uri(world, LILV_NS_LV2 "toggled");
	srate = lilv_new_uri(world, LILV_NS_LV2 "sampleRate");
	gtk_gui = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#GtkUI");
	external_gui = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#external");
 	logarithmic = lilv_new_uri(world, "http://lv2plug.in/ns/dev/extportinfo#logarithmic");
}

LV2World::~LV2World()
{
	lilv_node_free(input_class);
	lilv_node_free(output_class);
	lilv_node_free(control_class);
	lilv_node_free(audio_class);
	lilv_node_free(in_place_broken);
}

LV2PluginInfo::LV2PluginInfo (void* lv2_world, const void* lilv_plugin)
	: _lv2_world(lv2_world)
	, _lilv_plugin(lilv_plugin)
{
	type = ARDOUR::LV2;
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
				*(LV2World*)_lv2_world, (LilvPlugin*)_lilv_plugin, session.frame_rate()));

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
	const LilvPlugins* plugins = lilv_world_get_all_plugins(world->world);

	LILV_FOREACH(plugins, i, plugins) {
		const LilvPlugin* p = lilv_plugins_get(plugins, i);
		LV2PluginInfoPtr info (new LV2PluginInfo(lv2_world, p));

		LilvNode* name = lilv_plugin_get_name(p);

		if (!name) {
			cerr << "LV2: invalid plugin\n";
			continue;
		}
		
		info->name = string(lilv_node_as_string(name));
		lilv_node_free(name);

		const LilvPluginClass* pclass = lilv_plugin_get_class(p);
		const LilvNode* label = lilv_plugin_class_get_label(pclass);
		info->category = lilv_node_as_string(label);

		LilvNode* author_name = lilv_plugin_get_author_name(p);
		info->creator = author_name ? string(lilv_node_as_string(author_name)) : "Unknown";
		lilv_node_free(author_name);

		info->path = "/NOPATH"; // Meaningless for LV2

		info->n_inputs = lilv_plugin_get_num_ports_of_class(p,
				world->input_class, world->audio_class, NULL);
		
		info->n_outputs = lilv_plugin_get_num_ports_of_class(p,
				world->output_class, world->audio_class, NULL);

		info->unique_id = lilv_node_as_uri(lilv_plugin_get_uri(p));
		info->index = 0; // Meaningless for LV2
		
		plugs.push_back (info);
	}

	return plugs;
}

