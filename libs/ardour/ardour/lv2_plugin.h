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

#ifndef __ardour_lv2_plugin_h__
#define __ardour_lv2_plugin_h__

#include <set>
#include <vector>
#include <string>
#include <dlfcn.h>

#include <sigc++/signal.h>

#include <pbd/stateful.h> 

#include <jack/types.h>
#include <slv2/slv2.h>
#include <ardour/plugin.h>

namespace ARDOUR {
class AudioEngine;
class Session;
struct LV2World;

class LV2Plugin : public ARDOUR::Plugin
{
  public:
	LV2Plugin (ARDOUR::AudioEngine&, ARDOUR::Session&, ARDOUR::LV2World&, SLV2Plugin plugin, nframes_t sample_rate);
	LV2Plugin (const LV2Plugin &);
	~LV2Plugin ();

	/* Plugin interface */
	
	std::string unique_id() const;
	const char* label() const           { return slv2_value_as_string(_name); }
	const char* name() const            { return slv2_value_as_string(_name); }
	const char* maker() const           { return _author ? slv2_value_as_string(_author) : "Unknown"; }
	uint32_t    parameter_count() const { return slv2_plugin_get_num_ports(_plugin); }
	float       default_value (uint32_t port);
	nframes_t   signal_latency() const;
	void        set_parameter (uint32_t port, float val);
	float       get_parameter (uint32_t port) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;

	const void* extension_data(const char* uri) { return _instance->lv2_descriptor->extension_data(uri); }

	SLV2Plugin slv2_plugin()         { return _plugin; }
	SLV2UI     slv2_ui()             { return _ui; }
	SLV2Port   slv2_port(uint32_t i) { return slv2_plugin_get_port_by_index(_plugin, i); }

	const char* port_symbol(uint32_t port);
	
	const LV2_Feature* const* features() { return _features; }

	std::set<Evoral::Parameter> automatable() const;

	void activate () { 
		if (!_was_activated) {
			slv2_instance_activate(_instance);
			_was_activated = true;
		}
	}

	void deactivate () {
		if (_was_activated) {
			slv2_instance_deactivate(_instance);
			_was_activated = false;
		}
	}

	void cleanup () {
		activate();
		deactivate();
		slv2_instance_free(_instance);
		_instance = NULL;
	}

	void set_block_size (nframes_t nframes) {}
	
	int         connect_and_run (BufferSet& bufs, uint32_t& in, uint32_t& out, nframes_t nframes, nframes_t offset);
	std::string describe_parameter (Evoral::Parameter);
	std::string state_node_name() const { return "lv2"; }
	void        print_parameter (uint32_t, char*, uint32_t len) const;

	bool parameter_is_audio(uint32_t) const;
	bool parameter_is_control(uint32_t) const;
	bool parameter_is_midi(uint32_t) const;
	bool parameter_is_input(uint32_t) const;
	bool parameter_is_output(uint32_t) const;
	bool parameter_is_toggled(uint32_t) const;

	XMLNode& get_state();
	int      set_state(const XMLNode& node);
	bool     save_preset(std::string uri);
	bool     load_preset(const string uri);
	virtual std::vector<Plugin::PresetRecord> get_presets();

	bool has_editor() const;

  private:
	void*                    _module;
	LV2World&                _world;
	LV2_Feature**            _features;
	SLV2Plugin               _plugin;
	SLV2UI                   _ui;
	SLV2Value                _name;
	SLV2Value                _author;
	SLV2Instance             _instance;
	nframes_t                _sample_rate;
	float*                   _control_data;
	float*                   _shadow_data;
	float*                   _defaults;
	float*                   _latency_control_port;
	bool                     _was_activated;
	vector<bool>             _port_is_input;
	map<string,uint32_t>     _port_indices;

	typedef struct { const void* (*extension_data)(const char* uri); } LV2_DataAccess;
	LV2_DataAccess _data_access_extension_data;
	LV2_Feature _data_access_feature;
	LV2_Feature _instance_access_feature;

	void init (LV2World& world, SLV2Plugin plugin, nframes_t rate);
	void run (nframes_t nsamples);
	void latency_compute_run ();
};


/** The SLV2World, and various cached (as symbols, fast) URIs.
 *
 * This object represents everything ardour 'knows' about LV2
 * (ie understood extensions/features/etc)
 */
struct LV2World {
	LV2World();
	~LV2World();

	SLV2World world;
	SLV2Value input_class; ///< Input port
	SLV2Value output_class; ///< Output port
	SLV2Value audio_class; ///< Audio port
	SLV2Value control_class; ///< Control port
	SLV2Value event_class; ///< Event port
	SLV2Value midi_class; ///< MIDI event
	SLV2Value in_place_broken;
	SLV2Value integer;
	SLV2Value toggled;
	SLV2Value srate;
	SLV2Value gtk_gui;
};


class LV2PluginInfo : public PluginInfo {
public:	
	LV2PluginInfo (void* slv2_world, void* slv2_plugin);;
	~LV2PluginInfo ();;
	static PluginInfoList discover (void* slv2_world);

	PluginPtr load (Session& session);

	void* _lv2_world;
	void* _slv2_plugin;
};

typedef boost::shared_ptr<LV2PluginInfo> LV2PluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_lv2_plugin_h__ */
