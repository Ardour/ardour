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

#ifndef __ardour_lv2_plugin_h__
#define __ardour_lv2_plugin_h__

#include <set>
#include <vector>
#include <string>
#include <dlfcn.h>

#include <sigc++/signal.h>

#include <pbd/stateful.h> 

#include <jack/types.h>
#include <lilv/lilv.h>
#include <ardour/plugin.h>

#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

namespace ARDOUR {
class AudioEngine;
class Session;
struct LV2World;

class LV2Plugin : public ARDOUR::Plugin
{
  public:
	LV2Plugin (ARDOUR::AudioEngine&, ARDOUR::Session&, ARDOUR::LV2World&, LilvPlugin* plugin, nframes_t sample_rate);
	LV2Plugin (const LV2Plugin &);
	~LV2Plugin ();

	/* Plugin interface */
	
	std::string unique_id() const;
	const char* uri() const             { return lilv_node_as_string(lilv_plugin_get_uri(_plugin)); }
	const char* label() const           { return lilv_node_as_string(_name); }
	const char* name() const            { return lilv_node_as_string(_name); }
	const char* maker() const           { return _author ? lilv_node_as_string(_author) : "Unknown"; }
	uint32_t    parameter_count() const { return lilv_plugin_get_num_ports(_plugin); }
	float       default_value (uint32_t port);
	nframes_t   latency() const;
	void        set_parameter (uint32_t port, float val);
	float       get_parameter (uint32_t port) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;

	const void* extension_data(const char* uri) { return _instance->lv2_descriptor->extension_data(uri); }

	LilvPlugin*     lilv_plugin()         { return _plugin; }
	const LilvUI*   lilv_ui()             { return _ui; }
	const LilvNode* ui_type()             { return _ui_type; }
	bool            is_external_ui() const;
	const LilvPort* lilv_port(uint32_t i) { return lilv_plugin_get_port_by_index(_plugin, i); }

	const char* port_symbol(uint32_t port);
	
	const LV2_Feature* const* features() { return _features; }
	
	std::set<uint32_t> automatable() const;

	void activate () { 
		if (!_was_activated) {
			lilv_instance_activate(_instance);
			_was_activated = true;
		}
	}

	void deactivate () {
		if (_was_activated) {
			lilv_instance_deactivate(_instance);
			_was_activated = false;
		}
	}

	void cleanup () {
		activate();
		deactivate();
		lilv_instance_free(_instance);
		_instance = NULL;
	}

	int set_block_size (nframes_t nframes) { return 0; }
	
	int         connect_and_run (std::vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, nframes_t nframes, nframes_t offset);
	std::string describe_parameter (uint32_t);
	std::string state_node_name() const { return "lv2"; }
	void        print_parameter (uint32_t, char*, uint32_t len) const;

	bool parameter_is_audio(uint32_t) const;
	bool parameter_is_control(uint32_t) const;
	bool parameter_is_input(uint32_t) const;
	bool parameter_is_output(uint32_t) const;
	bool parameter_is_toggled(uint32_t) const;

	XMLNode& get_state();
	int      set_state(const XMLNode& node);
	bool     save_preset(std::string name);

	bool has_editor() const;

	static LV2_Feature    _urid_map_feature;
	static LV2_URID_Map   _urid_map;
	static LV2_Feature    _urid_unmap_feature;
	static LV2_URID_Unmap _urid_unmap;

  private:
	void*           _module;
	LV2World&       _world;
	LV2_Feature**   _features;
	LilvPlugin*     _plugin;
	const LilvUI*   _ui;
	const LilvNode* _ui_type;
	LilvNode*       _name;
	LilvNode*       _author;
	LilvInstance*   _instance;
	nframes_t       _sample_rate;
	float*          _control_data;
	float*          _shadow_data;
	float*          _defaults;
	float*          _bpm_control_port;  ///< Special input set by ardour
	float*          _freewheel_control_port;  ///< Special input set by ardour
	float*          _latency_control_port;  ///< Special output set by plugin
	bool            _was_activated;
	vector<bool>    _port_is_input;

	typedef struct { const void* (*extension_data)(const char* uri); } LV2_DataAccess;
	LV2_DataAccess _data_access_extension_data;
	LV2_Feature _data_access_feature;
	LV2_Feature _instance_access_feature;

	void init (LV2World& world, LilvPlugin* plugin, nframes_t rate);
	void run (nframes_t nsamples);
	void latency_compute_run ();

	/** Find the LV2 input port with the given designation.
	 * If found, bufptrs[port_index] will be set to bufptr.
	 */
        const LilvPort* designated_input (const char* uri, void** bufptrs[], void** bufptr);
};


/** The LilvWorld, and various cached (as symbols, fast) URIs.
 *
 * This object represents everything ardour 'knows' about LV2
 * (ie understood extensions/features/etc)
 */
struct LV2World {
	LV2World();
	~LV2World();

	LilvWorld* world;
	LilvNode* input_class;
	LilvNode* output_class;
	LilvNode* audio_class;
	LilvNode* control_class;
	LilvNode* in_place_broken;
	LilvNode* integer;
	LilvNode* toggled;
	LilvNode* srate;
	LilvNode* gtk_gui;
	LilvNode* external_gui;
	LilvNode* logarithmic;
};


class LV2PluginInfo : public PluginInfo {
public:	
	LV2PluginInfo (void* lilv_world, const void* lilv_plugin);;
	~LV2PluginInfo ();;
	static PluginInfoList discover (void* lilv_world);

	PluginPtr load (Session& session);

	void*       _lv2_world;
	const void* _lilv_plugin;
};

typedef boost::shared_ptr<LV2PluginInfo> LV2PluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_lv2_plugin_h__ */
