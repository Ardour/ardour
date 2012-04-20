/*
    Copyright (C) 2008-2012 Paul Davis
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
#include <giomm/file.h>

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
#include "ardour/lv2_plugin.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/worker.h"

#include "i18n.h"
#include <locale.h>

#include <lilv/lilv.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/worker/worker.h"

#include "lv2_evbuf.h"

#ifdef HAVE_SUIL
#include <suil/suil.h>
#endif

#define NS_PSET "http://lv2plug.in/ns/ext/presets#"
#define NS_UI   "http://lv2plug.in/ns/extensions/ui#"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

URIMap LV2Plugin::_uri_map;
uint32_t LV2Plugin::_midi_event_type_ev = _uri_map.uri_to_id(
	"http://lv2plug.in/ns/ext/event",
	"http://lv2plug.in/ns/ext/midi#MidiEvent");
uint32_t LV2Plugin::_midi_event_type = _uri_map.uri_to_id(
	NULL,
	"http://lv2plug.in/ns/ext/midi#MidiEvent");
uint32_t LV2Plugin::_chunk_type = _uri_map.uri_to_id(
	NULL, LV2_ATOM__Chunk);
uint32_t LV2Plugin::_sequence_type = _uri_map.uri_to_id(
	NULL, LV2_ATOM__Sequence);
uint32_t LV2Plugin::_event_transfer_type = _uri_map.uri_to_id(
	NULL, LV2_ATOM__eventTransfer);
uint32_t LV2Plugin::_path_type = _uri_map.uri_to_id(
	NULL, LV2_ATOM__Path);

class LV2World : boost::noncopyable {
public:
	LV2World ();
	~LV2World ();

	LilvWorld* world;

	LilvNode* atom_AtomPort;
	LilvNode* atom_Chunk;
	LilvNode* atom_Sequence;
	LilvNode* atom_bufferType;
	LilvNode* atom_eventTransfer;
	LilvNode* ev_EventPort;
	LilvNode* ext_logarithmic;
	LilvNode* lv2_AudioPort;
	LilvNode* lv2_ControlPort;
	LilvNode* lv2_InputPort;
	LilvNode* lv2_OutputPort;
	LilvNode* lv2_enumeration;
	LilvNode* lv2_inPlaceBroken;
	LilvNode* lv2_integer;
	LilvNode* lv2_sampleRate;
	LilvNode* lv2_toggled;
	LilvNode* midi_MidiEvent;
	LilvNode* ui_GtkUI;
	LilvNode* ui_external;
};

static LV2World _world;

/** Called by the plugin to schedule non-RT work. */
static LV2_Worker_Status
work_schedule(LV2_Worker_Schedule_Handle handle,
              uint32_t                   size,
              const void*                data)
{
	LV2Plugin* plugin = (LV2Plugin*)handle;
	if (plugin->session().engine().freewheeling()) {
		// Freewheeling, do the work immediately in this (audio) thread
		return (LV2_Worker_Status)plugin->work(size, data);
	} else {
		// Enqueue message for the worker thread
		return plugin->worker()->schedule(size, data) ?
			LV2_WORKER_SUCCESS : LV2_WORKER_ERR_UNKNOWN;
	}
}

/** Called by the plugin to respond to non-RT work. */
static LV2_Worker_Status
work_respond(LV2_Worker_Respond_Handle handle,
             uint32_t                  size,
             const void*               data)
{
	LV2Plugin* plugin = (LV2Plugin*)handle;
	if (plugin->session().engine().freewheeling()) {
		// Freewheeling, respond immediately in this (audio) thread
		return (LV2_Worker_Status)plugin->work_response(size, data);
	} else {
		// Enqueue response for the worker
		return plugin->worker()->respond(size, data) ?
			LV2_WORKER_SUCCESS : LV2_WORKER_ERR_UNKNOWN;
	}
}

struct LV2Plugin::Impl {
	Impl() : plugin(0), ui(0), ui_type(0), name(0), author(0), instance(0)
	       , work_iface(0)
	       , state(0)
	{}
	
	/** Find the LV2 input port with the given designation.
	 * If found, bufptrs[port_index] will be set to bufptr.
	 */
	LilvPort* designated_input (const char* uri, void** bufptrs[], void** bufptr);

	LilvPlugin*           plugin;
	const LilvUI*         ui;
	const LilvNode*       ui_type;
	LilvNode*             name;
	LilvNode*             author;
	LilvInstance*         instance;
	LV2_Worker_Interface* work_iface;
	LilvState*            state;
};

LV2Plugin::LV2Plugin (AudioEngine& engine,
                      Session&     session,
                      void*        c_plugin,
                      framecnt_t   rate)
	: Plugin(engine, session)
	, _impl(new Impl())
	, _features(NULL)
	, _worker(NULL)
	, _insert_id("0")
{
	init(c_plugin, rate);
}

LV2Plugin::LV2Plugin (const LV2Plugin& other)
	: Plugin(other)
	, _impl(new Impl())
	, _features(NULL)
	, _worker(NULL)
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

	_impl->plugin           = (LilvPlugin*)c_plugin;
	_impl->ui               = NULL;
	_impl->ui_type          = NULL;
	_to_ui                  = NULL;
	_from_ui                = NULL;
	_control_data           = 0;
	_shadow_data            = 0;
	_ev_buffers             = 0;
	_bpm_control_port       = 0;
	_freewheel_control_port = 0;
	_latency_control_port   = 0;
	_state_version          = 0;
	_was_activated          = false;
	_has_state_interface    = false;

	_instance_access_feature.URI = "http://lv2plug.in/ns/ext/instance-access";
	_data_access_feature.URI     = "http://lv2plug.in/ns/ext/data-access";
	_make_path_feature.URI       = LV2_STATE__makePath;
	_work_schedule_feature.URI   = LV2_WORKER__schedule;

	LilvPlugin* plugin = _impl->plugin;

	LilvNode* state_iface_uri = lilv_new_uri(_world.world, LV2_STATE__interface);
	LilvNode* state_uri       = lilv_new_uri(_world.world, LV2_STATE_URI);
	_has_state_interface =
		// What plugins should have (lv2:extensionData state:Interface)
		lilv_plugin_has_extension_data(plugin, state_iface_uri)
		// What some outdated/incorrect ones have
		|| lilv_plugin_has_feature(plugin, state_uri);
	lilv_node_free(state_uri);
	lilv_node_free(state_iface_uri);

	_features    = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * 8);
	_features[0] = &_instance_access_feature;
	_features[1] = &_data_access_feature;
	_features[2] = &_make_path_feature;
	_features[3] = _uri_map.uri_map_feature();
	_features[4] = _uri_map.urid_map_feature();
	_features[5] = _uri_map.urid_unmap_feature();
	_features[6] = NULL;
	_features[7] = NULL;

	LV2_State_Make_Path* make_path = (LV2_State_Make_Path*)malloc(
		sizeof(LV2_State_Make_Path));
	make_path->handle = this;
	make_path->path = &lv2_state_make_path;
	_make_path_feature.data = make_path;

	LilvNode* worker_schedule = lilv_new_uri(_world.world, LV2_WORKER__schedule);
	if (lilv_plugin_has_feature(plugin, worker_schedule)) {
		LV2_Worker_Schedule* schedule = (LV2_Worker_Schedule*)malloc(
			sizeof(LV2_Worker_Schedule));
		_worker                     = new Worker(this, 4096);
		schedule->handle            = this;
		schedule->schedule_work     = work_schedule;
		_work_schedule_feature.data = schedule;
		_features[6]                = &_work_schedule_feature;
	}
	lilv_node_free(worker_schedule);

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

	_impl->work_iface = (LV2_Worker_Interface*)extension_data(LV2_WORKER__interface);

	if (lilv_plugin_has_feature(plugin, _world.lv2_inPlaceBroken)) {
		error << string_compose(
		    _("LV2: \"%1\" cannot be used, since it cannot do inplace processing"),
		    lilv_node_as_string(_impl->name)) << endmsg;
		lilv_node_free(_impl->name);
		lilv_node_free(_impl->author);
		throw failed_constructor();
	}

	_sample_rate = rate;

	const uint32_t num_ports = this->num_ports();
	for (uint32_t i = 0; i < num_ports; ++i) {
		const LilvPort* port  = lilv_plugin_get_port_by_index(_impl->plugin, i);
		PortFlags       flags = 0;

		if (lilv_port_is_a(_impl->plugin, port, _world.lv2_OutputPort)) {
			flags |= PORT_OUTPUT;
		} else if (lilv_port_is_a(_impl->plugin, port, _world.lv2_InputPort)) {
			flags |= PORT_INPUT;
		} else {
			error << string_compose(
				"LV2: \"%1\" port %2 is neither input nor output",
				lilv_node_as_string(_impl->name), i) << endmsg;
			throw failed_constructor();
		}

		if (lilv_port_is_a(_impl->plugin, port, _world.lv2_ControlPort)) {
			flags |= PORT_CONTROL;
		} else if (lilv_port_is_a(_impl->plugin, port, _world.lv2_AudioPort)) {
			flags |= PORT_AUDIO;
		} else if (lilv_port_is_a(_impl->plugin, port, _world.ev_EventPort)) {
			flags |= PORT_EVENT;
		} else if (lilv_port_is_a(_impl->plugin, port, _world.atom_AtomPort)) {
			LilvNodes* buffer_types = lilv_port_get_value(
				_impl->plugin, port, _world.atom_bufferType);
				if (lilv_nodes_contains(buffer_types, _world.atom_Sequence)) {
					flags |= PORT_MESSAGE;
				}
			lilv_nodes_free(buffer_types);
		} else {
			error << string_compose(
				"LV2: \"%1\" port %2 has no known data type",
				lilv_node_as_string(_impl->name), i) << endmsg;
			throw failed_constructor();
		}

		_port_flags.push_back(flags);
	}

	_control_data = new float[num_ports];
	_shadow_data  = new float[num_ports];
	_defaults     = new float[num_ports];
	_ev_buffers   = new LV2_Evbuf*[num_ports];
	memset(_ev_buffers, 0, sizeof(LV2_Evbuf*) * num_ports);

	const bool     latent        = lilv_plugin_has_latency(plugin);
	const uint32_t latency_index = (latent)
		? lilv_plugin_get_latency_port_index(plugin)
		: 0;

#define NS_TIME "http://lv2plug.in/ns/ext/time#"
	
	// Build an array of pointers to special parameter buffers
	void*** params = new void**[num_ports];
	for (uint32_t i = 0; i < num_ports; ++i) {
		params[i] = NULL;
	}
	_impl->designated_input (NS_TIME "beatsPerMinute", params, (void**)&_bpm_control_port);
	_impl->designated_input (LILV_NS_LV2 "freeWheeling", params, (void**)&_freewheel_control_port);

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
			if (lilv_port_has_property (plugin, port, _world.lv2_sampleRate)) {
				_defaults[i] *= _session.frame_rate ();
			}
			lilv_node_free(def);

			lilv_instance_connect_port(_impl->instance, i, &_control_data[i]);

			if (latent && i == latency_index) {
				_latency_control_port  = &_control_data[i];
				*_latency_control_port = 0;
			}

			if (parameter_is_input(i)) {
				_shadow_data[i] = default_value(i);
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
			                         _world.ui_GtkUI,
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
			if (lilv_ui_is_a(ui, _world.ui_GtkUI)) {
				_impl->ui      = ui;
				_impl->ui_type = _world.ui_GtkUI;
				break;
			}
		}
#endif

		// If Gtk UI is not available, try to find external UI
		if (!_impl->ui) {
			LILV_FOREACH(uis, i, uis) {
				const LilvUI* ui = lilv_uis_get(uis, i);
				if (lilv_ui_is_a(ui, _world.ui_external)) {
					_impl->ui      = ui;
					_impl->ui_type = _world.ui_external;
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

	free(_features);
	free(_make_path_feature.data);
	free(_work_schedule_feature.data);

	delete _to_ui;
	delete _from_ui;

	delete [] _control_data;
	delete [] _shadow_data;
	delete [] _ev_buffers;
}

bool
LV2Plugin::is_external_ui() const
{
	if (!_impl->ui) {
		return false;
	}
	return lilv_ui_is_a(_impl->ui, _world.ui_external);
}

bool
LV2Plugin::ui_is_resizable () const
{
	const LilvNode* s   = lilv_ui_get_uri(_impl->ui);
	LilvNode*       p   = lilv_new_uri(_world.world, NS_UI "optionalFeature");
	LilvNode*       fs  = lilv_new_uri(_world.world, NS_UI "fixedSize");
	LilvNode*       nrs = lilv_new_uri(_world.world, NS_UI "noUserResize");

	LilvNodes* fs_matches = lilv_world_find_nodes(_world.world, s, p, fs);
	LilvNodes* nrs_matches = lilv_world_find_nodes(_world.world, s, p, nrs);

	lilv_nodes_free(nrs_matches);
	lilv_nodes_free(fs_matches);
	lilv_node_free(nrs);
	lilv_node_free(fs);
	lilv_node_free(p);

	return !fs_matches && !nrs_matches;
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

uint32_t
LV2Plugin::port_index (const char* symbol) const
{
	const map<string, uint32_t>::const_iterator i = _port_indices.find(symbol);
	if (i != _port_indices.end()) {
		return  i->second;
	} else {
		warning << string_compose(_("LV2: Unknown port %1"), symbol) << endmsg;
		return (uint32_t)-1;
	}
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

/** Directory for all plugin state. */
const std::string
LV2Plugin::plugin_dir() const
{
	return Glib::build_filename(_session.plugins_dir(), _insert_id.to_s());
}

/** Directory for files created by the plugin (except during save). */
const std::string
LV2Plugin::scratch_dir() const
{
	return Glib::build_filename(plugin_dir(), "scratch");
}

/** Directory for snapshots of files in the scratch directory. */
const std::string
LV2Plugin::file_dir() const
{
	return Glib::build_filename(plugin_dir(), "files");
}

/** Directory to save state snapshot version @c num into. */
const std::string
LV2Plugin::state_dir(unsigned num) const
{
	return Glib::build_filename(plugin_dir(), string_compose("state%1", num));
}

/** Implementation of state:makePath for files created at instantiation time.
 * Note this is not used for files created at save time (Lilv deals with that).
 */
char*
LV2Plugin::lv2_state_make_path(LV2_State_Make_Path_Handle handle,
                               const char*                path)
{
	LV2Plugin* me = (LV2Plugin*)handle;
	if (me->_insert_id == PBD::ID("0")) {
		warning << string_compose(
			"File path \"%1\" requested but LV2 %2 has no insert ID",
			path, me->name()) << endmsg;
		return g_strdup(path);
	}

	const std::string abs_path = Glib::build_filename(me->scratch_dir(), path);
	const std::string dirname  = Glib::path_get_dirname(abs_path);
	g_mkdir_with_parents(dirname.c_str(), 0744);

	DEBUG_TRACE(DEBUG::LV2, string_compose("new file path %1 => %2\n",
	                                       path, abs_path));

	std::cerr << "MAKE PATH " << path
	          << " => " << g_strndup(abs_path.c_str(), abs_path.length())
	          << std::endl;
	return g_strndup(abs_path.c_str(), abs_path.length());
}

static void
remove_directory(const std::string& path)
{
	if (!Glib::file_test(path, Glib::FILE_TEST_IS_DIR)) {
		warning << string_compose("\"%1\" is not a directory", path) << endmsg;
		return;
	}

	Glib::RefPtr<Gio::File>           dir = Gio::File::create_for_path(path);
	Glib::RefPtr<Gio::FileEnumerator> e   = dir->enumerate_children();
	Glib::RefPtr<Gio::FileInfo>       fi;
	while ((fi = e->next_file())) {
		if (fi->get_type() == Gio::FILE_TYPE_DIRECTORY) {
			remove_directory(fi->get_name());
		} else {
			dir->get_child(fi->get_name())->remove();
		}
	}
	dir->remove();
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
		cout << "LV2 " << name() << " has state interface" << endl;
		// Provisionally increment state version and create directory
		const std::string new_dir = state_dir(++_state_version);
		g_mkdir_with_parents(new_dir.c_str(), 0744);

		cout << "NEW DIR: " << new_dir << endl;

		LilvState* state = lilv_state_new_from_instance(
			_impl->plugin,
			_impl->instance,
			_uri_map.urid_map(),
			scratch_dir().c_str(),
			file_dir().c_str(),
			_session.externals_dir().c_str(),
			new_dir.c_str(),
			NULL,
			(void*)this,
			0,
			NULL);

		if (!_impl->state || !lilv_state_equals(state, _impl->state)) {
			lilv_state_save(_world.world,
			                _uri_map.urid_map(),
			                _uri_map.urid_unmap(),
			                state,
			                NULL,
			                new_dir.c_str(),
			                "state.ttl");

			lilv_state_free(_impl->state);
			_impl->state = state;

			cout << "Saved LV2 state to " << state_dir(_state_version) << endl;
		} else {
			// State is identical, decrement version and nuke directory
			cout << "LV2 state identical, not saving" << endl;
			lilv_state_free(state);
			remove_directory(new_dir);
			--_state_version;
		}

		root->add_property("state-dir", string_compose("state%1", _state_version));
	} else {
		cout << "LV2 " << name() << " has no state interface." << endl;
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
	LilvNode* lv2_appliesTo = lilv_new_uri(_world.world, LILV_NS_LV2  "appliesTo");
	LilvNode* pset_Preset   = lilv_new_uri(_world.world, NS_PSET "Preset");
	LilvNode* rdfs_label    = lilv_new_uri(_world.world, LILV_NS_RDFS "label");

	LilvNodes* presets = lilv_plugin_get_related(_impl->plugin, pset_Preset);
	LILV_FOREACH(nodes, i, presets) {
		const LilvNode* preset = lilv_nodes_get(presets, i);
		lilv_world_load_resource(_world.world, preset);
		const LilvNode* name = get_value(_world.world, preset, rdfs_label);
		if (name) {
			_presets.insert(std::make_pair(lilv_node_as_string(preset),
			                               Plugin::PresetRecord(
				                               lilv_node_as_string(preset),
				                               lilv_node_as_string(name))));
		} else {
			warning << string_compose(
			    _("Plugin \"%1\% preset \"%2%\" is missing a label\n"),
			    lilv_node_as_string(lilv_plugin_get_uri(_impl->plugin)),
			    lilv_node_as_string(preset)) << endmsg;
		}
	}
	lilv_nodes_free(presets);

	lilv_node_free(rdfs_label);
	lilv_node_free(pset_Preset);
	lilv_node_free(lv2_appliesTo);
}

bool
LV2Plugin::load_preset(PresetRecord r)
{
	Plugin::load_preset(r);

	std::map<std::string,uint32_t>::iterator it;

	LilvNode* lv2_port   = lilv_new_uri(_world.world, LILV_NS_LV2 "port");
	LilvNode* lv2_symbol = lilv_new_uri(_world.world, LILV_NS_LV2 "symbol");
	LilvNode* preset     = lilv_new_uri(_world.world, r.uri.c_str());
	LilvNode* pset_value = lilv_new_uri(_world.world, NS_PSET "value");

	LilvNodes* ports = lilv_world_find_nodes(_world.world, preset, lv2_port, NULL);
	LILV_FOREACH(nodes, i, ports) {
		const LilvNode* port   = lilv_nodes_get(ports, i);
		const LilvNode* symbol = get_value(_world.world, port, lv2_symbol);
		const LilvNode* value  = get_value(_world.world, port, pset_value);
		if (value && lilv_node_is_float(value)) {
		        it = _port_indices.find(lilv_node_as_string(symbol));
			if (it != _port_indices.end())
			        set_parameter(it->second,lilv_node_as_float(value));
		}
	}
	lilv_nodes_free(ports);

	lilv_node_free(pset_value);
	lilv_node_free(preset);
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

bool
LV2Plugin::has_message_output() const
{
	for (uint32_t i = 0; i < num_ports(); ++i) {
		if ((_port_flags[i] & PORT_MESSAGE) && _port_flags[i] & PORT_OUTPUT) {
			return true;
		}
	}
	return false;
}

uint32_t
LV2Plugin::atom_eventTransfer() const
{
	return _event_transfer_type;
}

void
LV2Plugin::write_to(RingBuffer<uint8_t>* dest,
                    uint32_t             index,
                    uint32_t             protocol,
                    uint32_t             size,
                    uint8_t*             body)
{
	const uint32_t buf_size = sizeof(UIMessage) + size;
	uint8_t        buf[buf_size];

	UIMessage* msg = (UIMessage*)buf;
	msg->index    = index;
	msg->protocol = protocol;
	msg->size     = size;
	memcpy(msg + 1, body, size);

	if (dest->write(buf, buf_size) != buf_size) {
		error << "Error writing to UI=>Plugin RingBuffer" << endmsg;
	}
}

void
LV2Plugin::write_from_ui(uint32_t index,
                         uint32_t protocol,
                         uint32_t size,
                         uint8_t* body)
{
	if (!_from_ui) {
		_from_ui = new RingBuffer<uint8_t>(4096);
	}

	write_to(_from_ui, index, protocol, size, body);
}

void
LV2Plugin::write_to_ui(uint32_t index,
                       uint32_t protocol,
                       uint32_t size,
                       uint8_t* body)
{
	std::cerr << "WRITE TO UI" << std::endl;
	write_to(_to_ui, index, protocol, size, body);
}

void
LV2Plugin::enable_ui_emmission()
{
	if (!_to_ui) {
		_to_ui = new RingBuffer<uint8_t>(4096);
	}
}

void
LV2Plugin::emit_to_ui(void* controller, UIMessageSink sink)
{
	if (!_to_ui) {
		return;
	}

	uint32_t read_space = _to_ui->read_space();
	while (read_space > sizeof(UIMessage)) {
		UIMessage msg;
		if (_to_ui->read((uint8_t*)&msg, sizeof(msg)) != sizeof(msg)) {
			error << "Error reading from Plugin=>UI RingBuffer" << endmsg;
			break;
		}
		uint8_t body[msg.size];
		if (_to_ui->read(body, msg.size) != msg.size) {
			error << "Error reading from Plugin=>UI RingBuffer" << endmsg;
			break;
		}

		sink(controller, msg.index, msg.size, msg.protocol, body);

		read_space -= sizeof(msg) + msg.size;
	}
}

int
LV2Plugin::work(uint32_t size, const void* data)
{
	return _impl->work_iface->work(
		_impl->instance->lv2_handle, work_respond, this, size, data);
}

int
LV2Plugin::work_response(uint32_t size, const void* data)
{
	return _impl->work_iface->work_response(
		_impl->instance->lv2_handle, size, data);
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

	_state_version = 0;
	if ((prop = node.property("state-dir")) != 0) {
		if (sscanf(prop->value().c_str(), "state%u", &_state_version) != 1) {
			error << string_compose(
				"LV2: failed to parse state version from \"%1\"",
				prop->value()) << endmsg;
		}

		std::string state_file = Glib::build_filename(
			plugin_dir(),
			Glib::build_filename(prop->value(), "state.ttl"));

		cout << "Loading LV2 state from " << state_file << endl;
		LilvState* state = lilv_state_new_from_file(
			_world.world, _uri_map.urid_map(), NULL, state_file.c_str());

		lilv_state_restore(state, _impl->instance, NULL, NULL, 0, NULL);
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

	desc.integer_step = lilv_port_has_property(_impl->plugin, port, _world.lv2_integer);
	desc.toggled      = lilv_port_has_property(_impl->plugin, port, _world.lv2_toggled);
	desc.logarithmic  = lilv_port_has_property(_impl->plugin, port, _world.ext_logarithmic);
	desc.sr_dependent = lilv_port_has_property(_impl->plugin, port, _world.lv2_sampleRate);
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

	desc.enumeration = lilv_port_has_property(_impl->plugin, port, _world.lv2_enumeration);

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

	if (_freewheel_control_port) {
		*_freewheel_control_port = _session.engine().freewheeling ();
	}

	if (_bpm_control_port) {
		TempoMap& tmap (_session.tempo_map ());
		Tempo tempo = tmap.tempo_at (_session.transport_frame () + offset);
		*_bpm_control_port = tempo.beats_per_minute ();
	}

	ChanCount bufs_count;
	bufs_count.set(DataType::AUDIO, 1);
	bufs_count.set(DataType::MIDI, 1);
	BufferSet& silent_bufs  = _session.get_silent_buffers(bufs_count);
	BufferSet& scratch_bufs = _session.get_silent_buffers(bufs_count);
	uint32_t const num_ports = parameter_count();

	uint32_t audio_in_index  = 0;
	uint32_t audio_out_index = 0;
	uint32_t midi_in_index   = 0;
	uint32_t midi_out_index  = 0;
	bool valid;
	for (uint32_t port_index = 0; port_index < num_ports; ++port_index) {
		void*     buf   = NULL;
		uint32_t  index = 0;
		PortFlags flags = _port_flags[port_index];
		if (flags & PORT_AUDIO) {
			if (flags & PORT_INPUT) {
				index = in_map.get(DataType::AUDIO, audio_in_index++, &valid);
				buf = (valid)
					? bufs.get_audio(index).data(offset)
					: silent_bufs.get_audio(0).data(offset);
			} else {
				index = out_map.get(DataType::AUDIO, audio_out_index++, &valid);
				buf = (valid)
					? bufs.get_audio(index).data(offset)
					: scratch_bufs.get_audio(0).data(offset);
			}
		} else if (flags & (PORT_EVENT|PORT_MESSAGE)) {
			/* FIXME: The checks here for bufs.count().n_midi() > index shouldn't
			   be necessary, but the mapping is illegal in some cases.  Ideally
			   that should be fixed, but this is easier...
			*/
			if (flags & PORT_INPUT) {
				index = in_map.get(DataType::MIDI, midi_in_index++, &valid);
				_ev_buffers[port_index] = (valid && bufs.count().n_midi() > index)
					? bufs.get_lv2_midi(true, index, flags & PORT_EVENT)
					: silent_bufs.get_lv2_midi(true, 0, flags & PORT_EVENT);
				buf = lv2_evbuf_get_buffer(_ev_buffers[port_index]);
			} else {
				index = out_map.get(DataType::MIDI, midi_out_index++, &valid);
				_ev_buffers[port_index] = (valid && bufs.count().n_midi() > index)
					? bufs.get_lv2_midi(false, index, flags & PORT_EVENT)
					: scratch_bufs.get_lv2_midi(false, 0, flags & PORT_EVENT);
				buf = lv2_evbuf_get_buffer(_ev_buffers[port_index]);
			}
		} else {
			continue;  // Control port, leave buffer alone
		}
		lilv_instance_connect_port(_impl->instance, port_index, buf);
	}

	// Read messages from UI and push into appropriate buffers
	if (_from_ui) {
		uint32_t read_space = _from_ui->read_space();
		while (read_space > sizeof(UIMessage)) {
			UIMessage msg;
			if (_from_ui->read((uint8_t*)&msg, sizeof(msg)) != sizeof(msg)) {
				error << "Error reading from UI=>Plugin RingBuffer" << endmsg;
				break;
			}
			uint8_t body[msg.size];
			if (_from_ui->read(body, msg.size) != msg.size) {
				error << "Error reading from UI=>Plugin RingBuffer" << endmsg;
				break;
			}
			if (msg.protocol == _event_transfer_type) {
				LV2_Evbuf*            buf = _ev_buffers[msg.index];
				LV2_Evbuf_Iterator    i    = lv2_evbuf_end(buf);
				const LV2_Atom* const atom = (const LV2_Atom*)body;
				lv2_evbuf_write(&i, nframes, 0, atom->type, atom->size,
				                (const uint8_t*)LV2_ATOM_BODY(atom));
			} else {
				error << "Received unknown message type from UI" << endmsg;
			}
			read_space -= sizeof(UIMessage) + msg.size;
		}
	}

	run(nframes);

	midi_out_index = 0;
	for (uint32_t port_index = 0; port_index < num_ports; ++port_index) {
		PortFlags flags = _port_flags[port_index];

		// Flush MIDI (write back to Ardour MIDI buffers)
		if ((flags & PORT_OUTPUT) && (flags & (PORT_EVENT|PORT_MESSAGE))) {
			const uint32_t buf_index = out_map.get(
				DataType::MIDI, midi_out_index++, &valid);
			if (valid) {
				bufs.flush_lv2_midi(true, buf_index);
			}
		}

		// Write messages to UI
		if (_to_ui && (flags & PORT_OUTPUT) && (flags & PORT_MESSAGE)) {
			LV2_Evbuf* buf = _ev_buffers[port_index];
			for (LV2_Evbuf_Iterator i = lv2_evbuf_begin(buf);
			     lv2_evbuf_is_valid(i);
			     i = lv2_evbuf_next(i)) {
				uint32_t frames, subframes, type, size;
				uint8_t* data;
				lv2_evbuf_get(i, &frames, &subframes, &type, &size, &data);
				write_to_ui(port_index, _event_transfer_type,
				            size + sizeof(LV2_Atom),
				            data - sizeof(LV2_Atom));
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
	assert(param < _port_flags.size());
	return _port_flags[param] & PORT_CONTROL;
}

bool
LV2Plugin::parameter_is_audio(uint32_t param) const
{
	assert(param < _port_flags.size());
	return _port_flags[param] & PORT_AUDIO;
}

bool
LV2Plugin::parameter_is_event(uint32_t param) const
{
	assert(param < _port_flags.size());
	return _port_flags[param] & PORT_EVENT;
}

bool
LV2Plugin::parameter_is_output(uint32_t param) const
{
	assert(param < _port_flags.size());
	return _port_flags[param] & PORT_OUTPUT;
}

bool
LV2Plugin::parameter_is_input(uint32_t param) const
{
	assert(param < _port_flags.size());
	return _port_flags[param] & PORT_INPUT;
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
	uint32_t const N = parameter_count();
	for (uint32_t i = 0; i < N; ++i) {
		if (parameter_is_control(i) && parameter_is_input(i)) {
			_control_data[i] = _shadow_data[i];
		}
	}

	lilv_instance_run(_impl->instance, nframes);

	if (_impl->work_iface) {
		_worker->emit_responses();
		if (_impl->work_iface->end_run) {
			_impl->work_iface->end_run(_impl->instance->lv2_handle);
		}
	}
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

LilvPort*
LV2Plugin::Impl::designated_input (const char* uri, void** bufptrs[], void** bufptr)
{
	LilvPort* port        = NULL;
	LilvNode* designation = lilv_new_uri(_world.world, uri);
	port = lilv_plugin_get_port_by_designation(
		plugin, _world.lv2_InputPort, designation);
	lilv_node_free(designation);
	if (port) {
		bufptrs[lilv_port_get_index(plugin, port)] = bufptr;
	}
	return port;
}

LV2World::LV2World()
	: world(lilv_world_new())
{
	lilv_world_load_all(world);
	atom_AtomPort      = lilv_new_uri(world, LV2_ATOM__AtomPort);
	atom_Chunk         = lilv_new_uri(world, LV2_ATOM__Chunk);
	atom_Sequence      = lilv_new_uri(world, LV2_ATOM__Sequence);
	atom_bufferType    = lilv_new_uri(world, LV2_ATOM__bufferType);
	atom_eventTransfer = lilv_new_uri(world, LV2_ATOM__eventTransfer);
	ev_EventPort       = lilv_new_uri(world, LILV_URI_EVENT_PORT);
	ext_logarithmic    = lilv_new_uri(world, "http://lv2plug.in/ns/dev/extportinfo#logarithmic");
	lv2_AudioPort      = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
	lv2_ControlPort    = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
	lv2_InputPort      = lilv_new_uri(world, LILV_URI_INPUT_PORT);
	lv2_OutputPort     = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
	lv2_inPlaceBroken  = lilv_new_uri(world, LILV_NS_LV2 "inPlaceBroken");
	lv2_integer        = lilv_new_uri(world, LILV_NS_LV2 "integer");
	lv2_sampleRate     = lilv_new_uri(world, LILV_NS_LV2 "sampleRate");
	lv2_toggled        = lilv_new_uri(world, LILV_NS_LV2 "toggled");
	lv2_enumeration    = lilv_new_uri(world, LILV_NS_LV2 "enumeration");
	midi_MidiEvent     = lilv_new_uri(world, LILV_URI_MIDI_EVENT);
	ui_GtkUI           = lilv_new_uri(world, NS_UI "GtkUI");
	ui_external        = lilv_new_uri(world, NS_UI "external");
}

LV2World::~LV2World()
{
	lilv_node_free(ui_external);
	lilv_node_free(ui_GtkUI);
	lilv_node_free(midi_MidiEvent);
	lilv_node_free(lv2_toggled);
	lilv_node_free(lv2_sampleRate);
	lilv_node_free(lv2_integer);
	lilv_node_free(lv2_inPlaceBroken);
	lilv_node_free(lv2_OutputPort);
	lilv_node_free(lv2_InputPort);
	lilv_node_free(lv2_ControlPort);
	lilv_node_free(lv2_AudioPort);
	lilv_node_free(ext_logarithmic);
	lilv_node_free(ev_EventPort);
	lilv_node_free(atom_eventTransfer);
	lilv_node_free(atom_bufferType);
	lilv_node_free(atom_Sequence);
	lilv_node_free(atom_Chunk);
	lilv_node_free(atom_AtomPort);
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
				p, _world.lv2_InputPort, _world.lv2_AudioPort, NULL));
		info->n_inputs.set_midi(
			lilv_plugin_get_num_ports_of_class(
				p, _world.lv2_InputPort, _world.ev_EventPort, NULL)
			+ lilv_plugin_get_num_ports_of_class(
				p, _world.lv2_InputPort, _world.atom_AtomPort, NULL));

		info->n_outputs.set_audio(
			lilv_plugin_get_num_ports_of_class(
				p, _world.lv2_OutputPort, _world.lv2_AudioPort, NULL));
		info->n_outputs.set_midi(
			lilv_plugin_get_num_ports_of_class(
				p, _world.lv2_OutputPort, _world.ev_EventPort, NULL)
			+ lilv_plugin_get_num_ports_of_class(
				p, _world.lv2_OutputPort, _world.atom_AtomPort, NULL));

		info->unique_id = lilv_node_as_uri(lilv_plugin_get_uri(p));
		info->index     = 0; // Meaningless for LV2

		plugs->push_back(info);
	}

	cerr << "Done LV2 discovery" << endl;

	return plugs;
}
