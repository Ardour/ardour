
/*
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

#ifndef __ardour_lv2_plugin_h__
#define __ardour_lv2_plugin_h__

#include <set>
#include <string>
#include <vector>

#include <dlfcn.h>

#include "pbd/stateful.h"

#include <jack/types.h>

#include "ardour/plugin.h"
#include "ardour/uri_map.h"

namespace ARDOUR {

class AudioEngine;
class Session;

class LV2Plugin : public ARDOUR::Plugin
{
  public:
	LV2Plugin (ARDOUR::AudioEngine& engine,
	           ARDOUR::Session&     session,
	           void*                c_plugin,
	           framecnt_t           sample_rate);
	LV2Plugin (const LV2Plugin &);
	~LV2Plugin ();

	std::string unique_id () const;
	const char* uri () const;
	const char* label () const;
	const char* name () const;
	const char* maker () const;

	uint32_t   num_ports () const;
	uint32_t   parameter_count () const;
	float      default_value (uint32_t port);
	framecnt_t signal_latency () const;
	void       set_parameter (uint32_t port, float val);
	float      get_parameter (uint32_t port) const;
	int        get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t   nth_parameter (uint32_t port, bool& ok) const;

	const void* extension_data (const char* uri) const;

	void* c_plugin();
	void* c_ui();
	void* c_ui_type();

	bool is_external_ui () const;

	const char* port_symbol (uint32_t port) const;

	const LV2_Feature* const* features () { return _features; }

	std::set<Evoral::Parameter> automatable () const;

	void activate ();
	void deactivate ();
	void cleanup ();

	int set_block_size (pframes_t /*nframes*/) { return 0; }

	int connect_and_run (BufferSet& bufs,
	                     ChanMapping in, ChanMapping out,
	                     pframes_t nframes, framecnt_t offset);

	std::string describe_parameter (Evoral::Parameter);
	std::string state_node_name () const { return "lv2"; }

	void print_parameter (uint32_t param,
	                      char*    buf,
	                      uint32_t len) const;

	bool parameter_is_audio (uint32_t) const;
	bool parameter_is_control (uint32_t) const;
	bool parameter_is_midi (uint32_t) const;
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;
	bool parameter_is_toggled (uint32_t) const;

	boost::shared_ptr<Plugin::ScalePoints>
	get_scale_points(uint32_t port_index) const;

	static uint32_t midi_event_type () { return _midi_event_type; }

	void set_insert_info(const PluginInsert* insert);

	int      set_state (const XMLNode& node, int version);
	bool     save_preset (std::string uri);
	void     remove_preset (std::string uri);
	bool     load_preset (PresetRecord);
	std::string current_preset () const;

	bool has_editor () const;

  private:
	struct Impl;
	Impl*             _impl;
	void*             _module;
	LV2_Feature**     _features;
	framecnt_t        _sample_rate;
	float*            _control_data;
	float*            _shadow_data;
	float*            _defaults;
	float*            _latency_control_port;
	bool              _was_activated;
	bool              _has_state_interface;
	std::vector<bool> _port_is_input;

	std::map<std::string,uint32_t> _port_indices;

	PBD::ID _insert_id;

	typedef struct {
		const void* (*extension_data) (const char* uri);
	} LV2_DataAccess;

	LV2_DataAccess _data_access_extension_data;
	LV2_Feature    _data_access_feature;
	LV2_Feature    _instance_access_feature;
	LV2_Feature    _map_path_feature;
	LV2_Feature    _make_path_feature;

	static URIMap   _uri_map;
	static uint32_t _midi_event_type;
	static uint32_t _state_path_type;

	const std::string state_dir () const;

	static int
	lv2_state_store_callback (void*       handle,
	                          uint32_t    key,
	                          const void* value,
	                          size_t      size,
	                          uint32_t    type,
	                          uint32_t    flags);

	static const void*
	lv2_state_retrieve_callback (void*     handle,
	                             uint32_t  key,
	                             size_t*   size,
	                             uint32_t* type,
	                             uint32_t* flags);

	static char* lv2_state_abstract_path (void*       host_data,
	                                      const char* absolute_path);
	static char* lv2_state_absolute_path (void*       host_data,
	                                      const char* abstract_path);
	static char* lv2_state_make_path (void*       host_data,
	                                  const char* path);

	void init (void* c_plugin, framecnt_t rate);
	void run (pframes_t nsamples);

	void latency_compute_run ();
	std::string do_save_preset (std::string);
	void do_remove_preset (std::string);
	void find_presets ();
	void add_state (XMLNode *) const;
};


class LV2PluginInfo : public PluginInfo {
public:
	LV2PluginInfo (void* c_plugin);
	~LV2PluginInfo ();

	static PluginInfoList* discover ();

	PluginPtr load (Session& session);

	void* _c_plugin;
};

typedef boost::shared_ptr<LV2PluginInfo> LV2PluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_lv2_plugin_h__ */
