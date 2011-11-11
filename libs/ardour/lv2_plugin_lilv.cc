;/*
    Copyright (C) 2008-2011 Paul Davis
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

#include <string>
#include <vector>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <glibmm.h>

#include <boost/utility.hpp>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"
#include "pbd/xml++.h"

#include "libardour-config.h"

#include "ardour/ardour.h"
#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/lv2_event_buffer.h"
#include "ardour/lv2_plugin.h"
#include "ardour/lv2_state.h"
#include "ardour/session.h"

#include "i18n.h"
#include <locale.h>

#include <lilv/lilv.h>

#include "lv2/lv2plug.in/ns/ext/files/files.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "rdff.h"
#ifdef HAVE_SUIL
#include <suil/suil.h>
#endif

#define NS_DC    "http://dublincore.org/documents/dcmi-namespace/"
#define NS_LV2   "http://lv2plug.in/ns/lv2core#"
#define NS_STATE "http://lv2plug.in/ns/ext/state#"
#define NS_PSET  "http://lv2plug.in/ns/dev/presets#"
#define NS_UI    "http://lv2plug.in/ns/extensions/ui#"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

URIMap LV2Plugin::_uri_map;
uint32_t LV2Plugin::_midi_event_type = _uri_map.uri_to_id(
        "http://lv2plug.in/ns/ext/event",
        "http://lv2plug.in/ns/ext/midi#MidiEvent");

class LV2World : boost::noncopyable {
public:
	LV2World ();
	~LV2World ();

	LilvWorld* world;
	LilvNode*  input_class;      ///< Input port
	LilvNode*  output_class;     ///< Output port
	LilvNode*  audio_class;      ///< Audio port
	LilvNode*  control_class;    ///< Control port
	LilvNode*  event_class;      ///< Event port
	LilvNode*  midi_class;       ///< MIDI event
	LilvNode*  in_place_broken;
	LilvNode*  integer;
	LilvNode*  toggled;
	LilvNode*  srate;
	LilvNode*  gtk_gui;
	LilvNode*  external_gui;
	LilvNode*  logarithmic;
};

static LV2World _world;

struct LV2Plugin::Impl {
	Impl() : plugin(0), ui(0), ui_type(0), name(0), author(0), instance(0) {}
	LilvPlugin*     plugin;
	const LilvUI*   ui;
	const LilvNode* ui_type;
	LilvNode*       name;
	LilvNode*       author;
	LilvInstance*   instance;
};

LV2Plugin::LV2Plugin (AudioEngine& engine,
                      Session&     session,
                      void*        c_plugin,
                      framecnt_t   rate)
	: Plugin(engine, session)
	, _impl(new Impl())
	, _features(NULL)
	, _insert_id("0")
{
	init(c_plugin, rate);
}

LV2Plugin::LV2Plugin (const LV2Plugin& other)
	: Plugin(other)
	, _impl(new Impl())
	, _features(NULL)
	, _insert_id(other._insert_id)
{
	init(other._impl->plugin, other._sample_rate);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		_control_data[i] = other._shadow_data[i];
		_shadow_data[i]  = other._shadow_data[i];
	}
}

void
LV2Plugin::init(void* c_plugin, framecnt_t rate)
{
	DEBUG_TRACE(DEBUG::LV2, "init\n");

	_impl->plugin          = (LilvPlugin*)c_plugin;
	_impl->ui              = NULL;
	_impl->ui_type         = NULL;
	_control_data         = 0;
	_shadow_data          = 0;
	_latency_control_port = 0;
	_was_activated        = false;

	_instance_access_feature.URI  = "http://lv2plug.in/ns/ext/instance-access";
	_data_access_feature.URI      = "http://lv2plug.in/ns/ext/data-access";
	_path_support_feature.URI     = LV2_FILES_PATH_SUPPORT_URI;
	_new_file_support_feature.URI = LV2_FILES_NEW_FILE_SUPPORT_URI;

	LilvPlugin* plugin = _impl->plugin;

	LilvNode* state_iface_uri = lilv_new_uri(_world.world, NS_STATE "Interface");
#ifdef lilv_plugin_has_extension_data
	_has_state_interface = lilv_plugin_has_extension_data(plugin, state_iface_uri);
	lilv_node_free(state_iface_uri);
#else
	LilvNode* lv2_extensionData = lilv_new_uri(_world.world, NS_LV2 "extensionData");
	LilvNodes* extdatas = lilv_plugin_get_value(plugin, lv2_extensionData);
	LILV_FOREACH(nodes, i, extdatas) {
		if (lilv_node_equals(lilv_nodes_get(extdatas, i), state_iface_uri)) {
			_has_state_interface = true;
			break;
		}
	}
#endif

	_features    = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * 6);
	_features[0] = &_instance_access_feature;
	_features[1] = &_data_access_feature;
	_features[2] = &_path_support_feature;
	_features[3] = &_new_file_support_feature;
	_features[4] = _uri_map.feature();
	_features[5] = NULL;

	LV2_Files_Path_Support* path_support = (LV2_Files_Path_Support*)malloc(
		sizeof(LV2_Files_Path_Support));
	path_support->host_data = this;
	path_support->abstract_path = &lv2_files_abstract_path;
	path_support->absolute_path = &lv2_files_absolute_path;
	_path_support_feature.data = path_support;

	LV2_Files_New_File_Support* new_file_support = (LV2_Files_New_File_Support*)malloc(
		sizeof(LV2_Files_New_File_Support));
	new_file_support->host_data = this;
	new_file_support->new_file_path = &lv2_files_new_file_path;
	_new_file_support_feature.data = new_file_support;

	_impl->instance = lilv_plugin_instantiate(plugin, rate, _features);
	_impl->name     = lilv_plugin_get_name(plugin);
	_impl->author   = lilv_plugin_get_author_name(plugin);

	if (_impl->instance == 0) {
		error << _("LV2: Failed to instantiate plugin ") << uri() << endmsg;
		throw failed_constructor();
	}

	_instance_access_feature.data              = (void*)_impl->instance->lv2_handle;
	_data_access_extension_data.extension_data = _impl->instance->lv2_descriptor->extension_data;
	_data_access_feature.data                  = &_data_access_extension_data;

	if (lilv_plugin_has_feature(plugin, _world.in_place_broken)) {
		error << string_compose(
		    _("LV2: \"%1\" cannot be used, since it cannot do inplace processing"),
		    lilv_node_as_string(_impl->name)) << endmsg;
		lilv_node_free(_impl->name);
		lilv_node_free(_impl->author);
		throw failed_constructor();
	}

	_sample_rate = rate;

	const uint32_t num_ports    = this->num_ports();
	const bool     latent       = lilv_plugin_has_latency(plugin);
	const uint32_t latency_port = (latent)
	    ? lilv_plugin_get_latency_port_index(plugin)
		: 0;

	_control_data = new float[num_ports];
	_shadow_data  = new float[num_ports];
	_defaults     = new float[num_ports];

	for (uint32_t i = 0; i < num_ports; ++i) {
		const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
		const LilvNode* sym  = lilv_port_get_symbol(plugin, port);

		// Store index in map so we can look up index by symbol
		_port_indices.insert(std::make_pair(lilv_node_as_string(sym), i));

		// Get range and default value if applicable
		if (parameter_is_control(i)) {
			LilvNode* def;
			lilv_port_get_range(plugin, port, &def, NULL, NULL);
			_defaults[i] = def ? lilv_node_as_float(def) : 0.0f;
			if (lilv_port_has_property (plugin, port, _world.srate)) {
				_defaults[i] *= _session.frame_rate ();
			}
			lilv_node_free(def);

			lilv_instance_connect_port(_impl->instance, i, &_control_data[i]);

			if (latent && ( i == latency_port) ) {
				_latency_control_port  = &_control_data[i];
				*_latency_control_port = 0;
			}

			if (parameter_is_input(i)) {
				_shadow_data[i] = default_value(i);
			}
		} else {
			_defaults[i] = 0.0f;
		}
	}

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
				_impl->ui      = this_ui;
				_impl->ui_type = this_ui_type;
				break;
			}
		}
#else
		// Look for Gtk native UI
		LILV_FOREACH(uis, i, uis) {
			const LilvUI* ui = lilv_uis_get(uis, i);
			if (lilv_ui_is_a(ui, _world.gtk_gui)) {
				_impl->ui      = ui;
				_impl->ui_type = _world.gtk_gui;
				break;
			}
		}
#endif

		// If Gtk UI is not available, try to find external UI
		if (!_impl->ui) {
			LILV_FOREACH(uis, i, uis) {
				const LilvUI* ui = lilv_uis_get(uis, i);
				if (lilv_ui_is_a(ui, _world.external_gui)) {
					_impl->ui      = ui;
					_impl->ui_type = _world.external_gui;
					break;
				}
			}
		}
	}

	latency_compute_run();
}

LV2Plugin::~LV2Plugin ()
{
	DEBUG_TRACE(DEBUG::LV2, string_compose("%1 destroy\n", name()));

	deactivate();
	cleanup();

	lilv_instance_free(_impl->instance);
	lilv_node_free(_impl->name);
	lilv_node_free(_impl->author);

	delete [] _control_data;
	delete [] _shadow_data;
}

bool
LV2Plugin::is_external_ui() const
{
	if (!_impl->ui) {
		return false;
	}
	return lilv_ui_is_a(_impl->ui, _world.external_gui);
}

string
LV2Plugin::unique_id() const
{
	return lilv_node_as_uri(lilv_plugin_get_uri(_impl->plugin));
}

const char*
LV2Plugin::uri() const
{
	return lilv_node_as_uri(lilv_plugin_get_uri(_impl->plugin));
}

const char*
LV2Plugin::label() const
{
	return lilv_node_as_string(_impl->name);
}

const char*
LV2Plugin::name() const
{
	return lilv_node_as_string(_impl->name);
}

const char*
LV2Plugin::maker() const
{
	return _impl->author ? lilv_node_as_string (_impl->author) : "Unknown";
}

uint32_t
LV2Plugin::num_ports() const
{
	return lilv_plugin_get_num_ports(_impl->plugin);
}

uint32_t
LV2Plugin::parameter_count() const
{
	return lilv_plugin_get_num_ports(_impl->plugin);
}

float
LV2Plugin::default_value(uint32_t port)
{
	return _defaults[port];
}

const char*
LV2Plugin::port_symbol(uint32_t index) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, index);
	if (!port) {
		error << name() << ": Invalid port index " << index << endmsg;
	}

	const LilvNode* sym = lilv_port_get_symbol(_impl->plugin, port);
	return lilv_node_as_string(sym);
}

void
LV2Plugin::set_parameter(uint32_t which, float val)
{
	DEBUG_TRACE(DEBUG::LV2, string_compose(
		            "%1 set parameter %2 to %3\n", name(), which, val));

	if (which < lilv_plugin_get_num_ports(_impl->plugin)) {
		_shadow_data[which] = val;
	} else {
		warning << string_compose(
		    _("Illegal parameter number used with plugin \"%1\". "
		      "This is a bug in either %2 or the LV2 plugin <%3>"),
		    name(), PROGRAM_NAME, unique_id()) << endmsg;
	}

	Plugin::set_parameter(which, val);
}

float
LV2Plugin::get_parameter(uint32_t which) const
{
	if (parameter_is_input(which)) {
		return (float)_shadow_data[which];
	} else {
		return (float)_control_data[which];
	}
	return 0.0f;
}

uint32_t
LV2Plugin::nth_parameter(uint32_t n, bool& ok) const
{
	ok = false;
	for (uint32_t c = 0, x = 0; x < lilv_plugin_get_num_ports(_impl->plugin); ++x) {
		if (parameter_is_control(x)) {
			if (c++ == n) {
				ok = true;
				return x;
			}
		}
	}

	return 0;
}

const void*
LV2Plugin::extension_data (const char* uri) const
{
	return lilv_instance_get_extension_data(_impl->instance, uri);
}

void*
LV2Plugin::c_plugin ()
{
	return _impl->plugin;
}

void*
LV2Plugin::c_ui ()
{
	return (void*)_impl->ui;
}

void*
LV2Plugin::c_ui_type ()
{
	return (void*)_impl->ui_type;
}

int
LV2Plugin::lv2_state_store_callback(LV2_State_Handle handle,
                                    uint32_t         key,
                                    const void*      value,
                                    size_t           size,
                                    uint32_t         type,
                                    uint32_t         flags)
{
	DEBUG_TRACE(DEBUG::LV2, string_compose(
		            "state store %1 (size: %2, type: %3)\n",
		            _uri_map.id_to_uri(NULL, key),
		            size,
		            _uri_map.id_to_uri(NULL, type)));

	LV2State* state = (LV2State*)handle;
	state->add_uri(key,  _uri_map.id_to_uri(NULL, key));
	state->add_uri(type, _uri_map.id_to_uri(NULL, type));
	return state->add_value(key, value, size, type, flags);
}

const void*
LV2Plugin::lv2_state_retrieve_callback(LV2_State_Handle host_data,
                                       uint32_t         key,
                                       size_t*          size,
                                       uint32_t*        type,
                                       uint32_t*        flags)
{
	LV2State* state = (LV2State*)host_data;
	LV2State::Values::const_iterator i = state->values.find(key);
	if (i == state->values.end()) {
		warning << "LV2 plugin attempted to retrieve nonexistent key: "
		        << _uri_map.id_to_uri(NULL, key) << endmsg;
		return NULL;
	}
	*size = i->second.size;
	*type = i->second.type;
	*flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE; // FIXME
	DEBUG_TRACE(DEBUG::LV2, string_compose(
		            "state retrieve %1 = %2 (size: %3, type: %4)\n",
		            _uri_map.id_to_uri(NULL, key),
		            i->second.value, *size, *type));
	return i->second.value;
}

char*
LV2Plugin::lv2_files_abstract_path(LV2_Files_Host_Data host_data,
                                   const char*         absolute_path)
{
	LV2Plugin* me = (LV2Plugin*)host_data;
	if (me->_insert_id == PBD::ID("0")) {
		return g_strdup(absolute_path);
	}

	const std::string state_dir = Glib::build_filename(me->_session.plugins_dir(),
	                                                   me->_insert_id.to_s());

	char* ret = NULL;
	if (strncmp(absolute_path, state_dir.c_str(), state_dir.length())) {
		ret = g_strdup(absolute_path);
	} else {
		const std::string path(absolute_path + state_dir.length() + 1);
		ret = g_strndup(path.c_str(), path.length());
	}

	DEBUG_TRACE(DEBUG::LV2, string_compose("abstract path %1 => %2\n",
	                                       absolute_path, ret));

	return ret;
}

char*
LV2Plugin::lv2_files_absolute_path(LV2_Files_Host_Data host_data,
                                   const char*         abstract_path)
{
	LV2Plugin* me = (LV2Plugin*)host_data;
	if (me->_insert_id == PBD::ID("0")) {
		return g_strdup(abstract_path);
	}

	char* ret = NULL;
	if (g_path_is_absolute(abstract_path)) {
		ret = g_strdup(abstract_path);
	} else {
		const std::string apath(abstract_path);
		const std::string state_dir = Glib::build_filename(me->_session.plugins_dir(),
		                                                   me->_insert_id.to_s());
		const std::string path = Glib::build_filename(state_dir,
		                                              apath);
		ret = g_strndup(path.c_str(), path.length());
	}

	DEBUG_TRACE(DEBUG::LV2, string_compose("absolute path %1 => %2\n",
	                                       abstract_path, ret));

	return ret;
}

char*
LV2Plugin::lv2_files_new_file_path(LV2_Files_Host_Data host_data,
                                   const char*         relative_path)
{
	LV2Plugin* me = (LV2Plugin*)host_data;
	if (me->_insert_id == PBD::ID("0")) {
		return g_strdup(relative_path);
	}

	const std::string state_dir = Glib::build_filename(me->_session.plugins_dir(),
	                                                   me->_insert_id.to_s());
	const std::string path = Glib::build_filename(state_dir,
	                                              relative_path);

	char* dirname = g_path_get_dirname(path.c_str());
	g_mkdir_with_parents(dirname, 0744);
	free(dirname);

	DEBUG_TRACE(DEBUG::LV2, string_compose("new file path %1 => %2\n",
	                                       relative_path, path));

	return g_strndup(path.c_str(), path.length());
}

void
LV2Plugin::add_state(XMLNode* root) const
{
	assert(_insert_id != PBD::ID("0"));

	XMLNode*    child;
	char        buf[16];
	LocaleGuard lg(X_("POSIX"));

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input(i) && parameter_is_control(i)) {
			child = new XMLNode("Port");
			child->add_property("symbol", port_symbol(i));
			snprintf(buf, sizeof(buf), "%+f", _shadow_data[i]);
			child->add_property("value", string(buf));
			root->add_child_nocopy(*child);
		}
	}

	if (_has_state_interface) {
		// Create state directory for this plugin instance
		cerr << "Create statefile name from ID " << _insert_id << endl;
		const std::string state_filename = _insert_id.to_s() + ".rdff";
		const std::string state_path     = Glib::build_filename(
		        _session.plugins_dir(), state_filename);

		cout << "Saving LV2 plugin state to " << state_path << endl;

		// Get LV2 State extension data from plugin instance
		LV2_State_Interface* state_iface = (LV2_State_Interface*)extension_data(
			"http://lv2plug.in/ns/ext/state#Interface");
		if (!state_iface) {
			warning << string_compose(
			    _("Plugin \"%1\% failed to return LV2 state interface"),
			    unique_id());
			return;
		}

		// Save plugin state to state object
		LV2State state(_uri_map);
		state_iface->save(_impl->instance->lv2_handle,
		                  &LV2Plugin::lv2_state_store_callback,
		                  &state,
		                  LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE,
		                  NULL);

		// Write state object to RDFF file
		RDFF file = rdff_open(state_path.c_str(), true);
		state.write(file);
		rdff_close(file);

		root->add_property("state-file", state_filename);
	}
}

static inline const LilvNode*
get_value(LilvWorld* world, const LilvNode* subject, const LilvNode* predicate)
{
	LilvNodes* vs = lilv_world_find_nodes(world, subject, predicate, NULL);
	return vs ? lilv_nodes_get_first(vs) : NULL;
}

void
LV2Plugin::find_presets()
{
	LilvNode* dc_title       = lilv_new_uri(_world.world, NS_DC   "title");
	LilvNode* pset_hasPreset = lilv_new_uri(_world.world, NS_PSET "hasPreset");

	LilvNodes* presets = lilv_plugin_get_value(_impl->plugin, pset_hasPreset);
	LILV_FOREACH(nodes, i, presets) {
		const LilvNode* preset = lilv_nodes_get(presets, i);
		const LilvNode* name   = get_value(_world.world, preset, dc_title);
		if (name) {
			_presets.insert(std::make_pair(lilv_node_as_string(preset),
			                               PresetRecord(
			                                   lilv_node_as_string(preset),
			                                   lilv_node_as_string(name))));
		} else {
			warning << string_compose(
			    _("Plugin \"%1\% preset \"%2%\" is missing a dc:title\n"),
			    unique_id(), lilv_node_as_string(preset));
		}
	}
	lilv_nodes_free(presets);

	lilv_node_free(pset_hasPreset);
	lilv_node_free(dc_title);
}

bool
LV2Plugin::load_preset(PresetRecord r)
{
	Plugin::load_preset(r);

	LilvNode* lv2_port   = lilv_new_uri(_world.world, NS_LV2 "port");
	LilvNode* lv2_symbol = lilv_new_uri(_world.world, NS_LV2 "symbol");
	LilvNode* pset_value = lilv_new_uri(_world.world, NS_PSET "value");
	LilvNode* preset     = lilv_new_uri(_world.world, r.uri.c_str());

	LilvNodes* ports = lilv_world_find_nodes(_world.world, preset, lv2_port, NULL);
	LILV_FOREACH(nodes, i, ports) {
		const LilvNode* port   = lilv_nodes_get(ports, i);
		const LilvNode* symbol = get_value(_world.world, port, lv2_symbol);
		const LilvNode* value  = get_value(_world.world, port, pset_value);
		if (value && lilv_node_is_float(value)) {
			set_parameter(_port_indices[lilv_node_as_string(symbol)],
			              lilv_node_as_float(value));
		}
	}
	lilv_nodes_free(ports);

	lilv_node_free(preset);
	lilv_node_free(pset_value);
	lilv_node_free(lv2_symbol);
	lilv_node_free(lv2_port);

	return true;
}

std::string
LV2Plugin::do_save_preset(string /*name*/)
{
	return "";
}

void
LV2Plugin::do_remove_preset(string /*name*/)
{}

bool
LV2Plugin::has_editor() const
{
	return _impl->ui != NULL;
}

void
LV2Plugin::set_insert_info(const PluginInsert* insert)
{
	_insert_id = insert->id();
}

int
LV2Plugin::set_state(const XMLNode& node, int version)
{
	XMLNodeList          nodes;
	const XMLProperty*   prop;
	XMLNodeConstIterator iter;
	XMLNode*             child;
	const char*          sym;
	const char*          value;
	uint32_t             port_id;
	LocaleGuard          lg(X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LV2Plugin::set_state") << endmsg;
		return -1;
	}

	if (version < 3000) {
		nodes = node.children("port");
	} else {
		nodes = node.children("Port");
	}

	for (iter = nodes.begin(); iter != nodes.end(); ++iter) {

		child = *iter;

		if ((prop = child->property("symbol")) != 0) {
			sym = prop->value().c_str();
		} else {
			warning << _("LV2: port has no symbol, ignored") << endmsg;
			continue;
		}

		map<string, uint32_t>::iterator i = _port_indices.find(sym);

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

		set_parameter(port_id, atof(value));
	}

	if ((prop = node.property("state-file")) != 0) {
		std::string state_path = Glib::build_filename(_session.plugins_dir(),
		                                              prop->value());

		cout << "LV2 state path " << state_path << endl;

		// Get LV2 State extension data from plugin instance
		LV2_State_Interface* state_iface = (LV2_State_Interface*)extension_data(
			"http://lv2plug.in/ns/ext/state#Interface");
		if (state_iface) {
			cout << "Loading LV2 state from " << state_path << endl;
			RDFF     file = rdff_open(state_path.c_str(), false);
			LV2State state(_uri_map);
			state.read(file);
			state_iface->restore(_impl->instance->lv2_handle,
			                     &LV2Plugin::lv2_state_retrieve_callback,
			                     &state,
			                     LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE,
			                     NULL);
			rdff_close(file);
		} else {
			warning << string_compose(
			    _("Plugin \"%1\% failed to return LV2 state interface"),
			    unique_id());
		}
	}

	latency_compute_run();

	return Plugin::set_state(node, version);
}

int
LV2Plugin::get_parameter_descriptor(uint32_t which, ParameterDescriptor& desc) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, which);

	LilvNode *def, *min, *max;
	lilv_port_get_range(_impl->plugin, port, &def, &min, &max);

	desc.integer_step = lilv_port_has_property(_impl->plugin, port, _world.integer);
	desc.toggled      = lilv_port_has_property(_impl->plugin, port, _world.toggled);
	desc.logarithmic  = lilv_port_has_property(_impl->plugin, port, _world.logarithmic);
	desc.sr_dependent = lilv_port_has_property(_impl->plugin, port, _world.srate);
	desc.label        = lilv_node_as_string(lilv_port_get_name(_impl->plugin, port));
	desc.lower        = min ? lilv_node_as_float(min) : 0.0f;
	desc.upper        = max ? lilv_node_as_float(max) : 1.0f;
	if (desc.sr_dependent) {
		desc.lower *= _session.frame_rate ();
		desc.upper *= _session.frame_rate ();
	}
		
	desc.min_unbound  = false; // TODO: LV2 extension required
	desc.max_unbound  = false; // TODO: LV2 extension required

	if (desc.integer_step) {
		desc.step      = 1.0;
		desc.smallstep = 0.1;
		desc.largestep = 10.0;
	} else {
		const float delta = desc.upper - desc.lower;
		desc.step      = delta / 1000.0f;
		desc.smallstep = delta / 10000.0f;
		desc.largestep = delta / 10.0f;
	}

	lilv_node_free(def);
	lilv_node_free(min);
	lilv_node_free(max);

	return 0;
}

string
LV2Plugin::describe_parameter(Evoral::Parameter which)
{
	if (( which.type() == PluginAutomation) && ( which.id() < parameter_count()) ) {
		LilvNode* name = lilv_port_get_name(_impl->plugin,
		                                    lilv_plugin_get_port_by_index(_impl->plugin, which.id()));
		string ret(lilv_node_as_string(name));
		lilv_node_free(name);
		return ret;
	} else {
		return "??";
	}
}

framecnt_t
LV2Plugin::signal_latency() const
{
	if (_latency_control_port) {
		return (framecnt_t)floor(*_latency_control_port);
	} else {
		return 0;
	}
}

set<Evoral::Parameter>
LV2Plugin::automatable() const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input(i) && parameter_is_control(i)) {
			ret.insert(ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
		}
	}

	return ret;
}

void
LV2Plugin::activate()
{
	DEBUG_TRACE(DEBUG::LV2, string_compose("%1 activate\n", name()));

	if (!_was_activated) {
		lilv_instance_activate(_impl->instance);
		_was_activated = true;
	}
}

void
LV2Plugin::deactivate()
{
	DEBUG_TRACE(DEBUG::LV2, string_compose("%1 deactivate\n", name()));

	if (_was_activated) {
		lilv_instance_deactivate(_impl->instance);
		_was_activated = false;
	}
}

void
LV2Plugin::cleanup()
{
	DEBUG_TRACE(DEBUG::LV2, string_compose("%1 cleanup\n", name()));

	activate();
	deactivate();
	lilv_instance_free(_impl->instance);
	_impl->instance = NULL;
}

int
LV2Plugin::connect_and_run(BufferSet& bufs,
	ChanMapping in_map, ChanMapping out_map,
	pframes_t nframes, framecnt_t offset)
{
	DEBUG_TRACE(DEBUG::LV2, string_compose("%1 run %2 offset %3\n", name(), nframes, offset));
	Plugin::connect_and_run(bufs, in_map, out_map, nframes, offset);

	cycles_t then = get_cycles();

	ChanCount bufs_count;
	bufs_count.set(DataType::AUDIO, 1);
	bufs_count.set(DataType::MIDI, 1);
	BufferSet& silent_bufs  = _session.get_silent_buffers(bufs_count);
	BufferSet& scratch_bufs = _session.get_silent_buffers(bufs_count);

	uint32_t audio_in_index  = 0;
	uint32_t audio_out_index = 0;
	uint32_t midi_in_index   = 0;
	uint32_t midi_out_index  = 0;
	bool valid;
	for (uint32_t port_index = 0; port_index < parameter_count(); ++port_index) {
		if (parameter_is_audio(port_index)) {
			if (parameter_is_input(port_index)) {
				const uint32_t buf_index = in_map.get(DataType::AUDIO, audio_in_index++, &valid);
				lilv_instance_connect_port(_impl->instance, port_index,
				                           valid ? bufs.get_audio(buf_index).data(offset)
				                                 : silent_bufs.get_audio(0).data(offset));
			} else if (parameter_is_output(port_index)) {
				const uint32_t buf_index = out_map.get(DataType::AUDIO, audio_out_index++, &valid);
				//cerr << port_index << " : " << " AUDIO OUT " << buf_index << endl;
				lilv_instance_connect_port(_impl->instance, port_index,
				                           valid ? bufs.get_audio(buf_index).data(offset)
				                                 : scratch_bufs.get_audio(0).data(offset));
			}
		} else if (parameter_is_midi(port_index)) {
			/* FIXME: The checks here for bufs.count().n_midi() > buf_index shouldn't
			   be necessary, but the mapping is illegal in some cases.  Ideally
			   that should be fixed, but this is easier...
			*/
			if (parameter_is_input(port_index)) {
				const uint32_t buf_index = in_map.get(DataType::MIDI, midi_in_index++, &valid);
				if (valid && bufs.count().n_midi() > buf_index) {
					lilv_instance_connect_port(_impl->instance, port_index,
					                           bufs.get_lv2_midi(true, buf_index).data());
				} else {
					lilv_instance_connect_port(_impl->instance, port_index,
					                           silent_bufs.get_lv2_midi(true, 0).data());
				}
			} else if (parameter_is_output(port_index)) {
				const uint32_t buf_index = out_map.get(DataType::MIDI, midi_out_index++, &valid);
				if (valid && bufs.count().n_midi() > buf_index) {
					lilv_instance_connect_port(_impl->instance, port_index,
					                           bufs.get_lv2_midi(false, buf_index).data());
				} else {
					lilv_instance_connect_port(_impl->instance, port_index,
					                           scratch_bufs.get_lv2_midi(true, 0).data());
				}
			}
		} else if (!parameter_is_control(port_index)) {
			// Optional port (it'd better be if we've made it this far...)
			lilv_instance_connect_port(_impl->instance, port_index, NULL);
		}
	}

	run(nframes);

	midi_out_index = 0;
	for (uint32_t port_index = 0; port_index < parameter_count(); ++port_index) {
		if (parameter_is_midi(port_index) && parameter_is_output(port_index)) {
			const uint32_t buf_index = out_map.get(DataType::MIDI, midi_out_index++, &valid);
			if (valid) {
				bufs.flush_lv2_midi(true, buf_index);
			}
		}
	}

	cycles_t now = get_cycles();
	set_cycles((uint32_t)(now - then));

	return 0;
}

bool
LV2Plugin::parameter_is_control(uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, param);
	return lilv_port_is_a(_impl->plugin, port, _world.control_class);
}

bool
LV2Plugin::parameter_is_audio(uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, param);
	return lilv_port_is_a(_impl->plugin, port, _world.audio_class);
}

bool
LV2Plugin::parameter_is_midi(uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, param);
	return lilv_port_is_a(_impl->plugin, port, _world.event_class);
	//	&& lilv_port_supports_event(_impl->plugin, port, _world.midi_class);
}

bool
LV2Plugin::parameter_is_output(uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, param);
	return lilv_port_is_a(_impl->plugin, port, _world.output_class);
}

bool
LV2Plugin::parameter_is_input(uint32_t param) const
{
	const LilvPort* port = lilv_plugin_get_port_by_index(_impl->plugin, param);
	return lilv_port_is_a(_impl->plugin, port, _world.input_class);
}

void
LV2Plugin::print_parameter(uint32_t param, char* buf, uint32_t len) const
{
	if (buf && len) {
		if (param < parameter_count()) {
			snprintf(buf, len, "%.3f", get_parameter(param));
		} else {
			strcat(buf, "0");
		}
	}
}

boost::shared_ptr<Plugin::ScalePoints>
LV2Plugin::get_scale_points(uint32_t port_index) const
{
	const LilvPort*  port   = lilv_plugin_get_port_by_index(_impl->plugin, port_index);
	LilvScalePoints* points = lilv_port_get_scale_points(_impl->plugin, port);

	boost::shared_ptr<Plugin::ScalePoints> ret;
	if (!points) {
		return ret;
	}

	ret = boost::shared_ptr<Plugin::ScalePoints>(new ScalePoints());

	LILV_FOREACH(scale_points, i, points) {
		const LilvScalePoint* p     = lilv_scale_points_get(points, i);
		const LilvNode*       label = lilv_scale_point_get_label(p);
		const LilvNode*       value = lilv_scale_point_get_value(p);
		if (label && (lilv_node_is_float(value) || lilv_node_is_int(value))) {
			ret->insert(make_pair(lilv_node_as_string(label),
			                      lilv_node_as_float(value)));
		}
	}

	lilv_scale_points_free(points);
	return ret;
}

void
LV2Plugin::run(pframes_t nframes)
{
	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_control(i) && parameter_is_input(i)) {
			_control_data[i] = _shadow_data[i];
		}
	}

	lilv_instance_run(_impl->instance, nframes);
}

void
LV2Plugin::latency_compute_run()
{
	if (!_latency_control_port) {
		return;
	}

	// Run the plugin so that it can set its latency parameter

	activate();

	uint32_t port_index = 0;
	uint32_t in_index   = 0;
	uint32_t out_index  = 0;

	const framecnt_t bufsize = 1024;
	float            buffer[bufsize];

	memset(buffer, 0, sizeof(float) * bufsize);

	// FIXME: Ensure plugins can handle in-place processing

	port_index = 0;

	while (port_index < parameter_count()) {
		if (parameter_is_audio(port_index)) {
			if (parameter_is_input(port_index)) {
				lilv_instance_connect_port(_impl->instance, port_index, buffer);
				in_index++;
			} else if (parameter_is_output(port_index)) {
				lilv_instance_connect_port(_impl->instance, port_index, buffer);
				out_index++;
			}
		}
		port_index++;
	}

	run(bufsize);
	deactivate();
}

LV2World::LV2World()
	: world(lilv_world_new())
{
	lilv_world_load_all(world);
	input_class     = lilv_new_uri(world, LILV_URI_INPUT_PORT);
	output_class    = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
	control_class   = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
	audio_class     = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
	event_class     = lilv_new_uri(world, LILV_URI_EVENT_PORT);
	midi_class      = lilv_new_uri(world, LILV_URI_MIDI_EVENT);
	in_place_broken = lilv_new_uri(world, LILV_NS_LV2 "inPlaceBroken");
	integer         = lilv_new_uri(world, LILV_NS_LV2 "integer");
	toggled         = lilv_new_uri(world, LILV_NS_LV2 "toggled");
	srate           = lilv_new_uri(world, LILV_NS_LV2 "sampleRate");
	gtk_gui         = lilv_new_uri(world, NS_UI "GtkUI");
	external_gui    = lilv_new_uri(world, NS_UI "external");
	logarithmic     = lilv_new_uri(world, "http://lv2plug.in/ns/dev/extportinfo#logarithmic");
}

LV2World::~LV2World()
{
	lilv_node_free(input_class);
	lilv_node_free(output_class);
	lilv_node_free(control_class);
	lilv_node_free(audio_class);
	lilv_node_free(event_class);
	lilv_node_free(midi_class);
	lilv_node_free(in_place_broken);
}

LV2PluginInfo::LV2PluginInfo (void* c_plugin)
	: _c_plugin(c_plugin)
{
	type = ARDOUR::LV2;
}

LV2PluginInfo::~LV2PluginInfo()
{}

PluginPtr
LV2PluginInfo::load(Session& session)
{
	try {
		PluginPtr plugin;

		plugin.reset(new LV2Plugin(session.engine(), session,
		                           (LilvPlugin*)_c_plugin,
		                           session.frame_rate()));

		plugin->set_info(PluginInfoPtr(new LV2PluginInfo(*this)));
		return plugin;
	} catch (failed_constructor& err) {
		return PluginPtr((Plugin*)0);
	}

	return PluginPtr();
}

PluginInfoList*
LV2PluginInfo::discover()
{
	PluginInfoList*    plugs   = new PluginInfoList;
	const LilvPlugins* plugins = lilv_world_get_all_plugins(_world.world);

	cerr << "LV2: Discovering " << lilv_plugins_size(plugins) << " plugins" << endl;

	LILV_FOREACH(plugins, i, plugins) {
		const LilvPlugin* p = lilv_plugins_get(plugins, i);
		LV2PluginInfoPtr  info(new LV2PluginInfo((void*)p));

		LilvNode* name = lilv_plugin_get_name(p);
		if (!name) {
			cerr << "LV2: invalid plugin\n";
			continue;
		}

		info->type = LV2;

		info->name = string(lilv_node_as_string(name));
		lilv_node_free(name);

		const LilvPluginClass* pclass = lilv_plugin_get_class(p);
		const LilvNode*        label  = lilv_plugin_class_get_label(pclass);
		info->category = lilv_node_as_string(label);

		LilvNode* author_name = lilv_plugin_get_author_name(p);
		info->creator = author_name ? string(lilv_node_as_string(author_name)) : "Unknown";
		lilv_node_free(author_name);

		info->path = "/NOPATH"; // Meaningless for LV2

		info->n_inputs.set_audio(
			lilv_plugin_get_num_ports_of_class(
				p, _world.input_class, _world.audio_class, NULL));
		info->n_inputs.set_midi(
			lilv_plugin_get_num_ports_of_class(
				p, _world.input_class, _world.event_class, NULL));

		info->n_outputs.set_audio(
			lilv_plugin_get_num_ports_of_class(
				p, _world.output_class, _world.audio_class, NULL));
		info->n_outputs.set_midi(
			lilv_plugin_get_num_ports_of_class(
				p, _world.output_class, _world.event_class, NULL));

		info->unique_id = lilv_node_as_uri(lilv_plugin_get_uri(p));
		info->index     = 0; // Meaningless for LV2

		plugs->push_back(info);
	}

	cerr << "Done LV2 discovery" << endl;

	return plugs;
}
