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

#ifndef __ardour_lv2_plugin_h__
#define __ardour_lv2_plugin_h__

#include <set>
#include <string>
#include <vector>

#include "ardour/plugin.h"
#include "ardour/uri_map.h"
#include "ardour/worker.h"
#include "pbd/ringbuffer.h"

typedef struct LV2_Evbuf_Impl LV2_Evbuf;

namespace ARDOUR {

// a callback function for lilv_state_new_from_instance(). friend of LV2Plugin
// so we can pass an LV2Plugin* in user_data and access its private members.
const void* lv2plugin_get_port_value(const char* port_symbol,
                                     void*       user_data,
                                     uint32_t*   size,
                                     uint32_t*   type);

class AudioEngine;
class Session;

class LIBARDOUR_API LV2Plugin : public ARDOUR::Plugin, public ARDOUR::Workee
{
  public:
	LV2Plugin (ARDOUR::AudioEngine& engine,
	           ARDOUR::Session&     session,
	           const void*          c_plugin,
	           framecnt_t           sample_rate);
	LV2Plugin (const LV2Plugin &);
	~LV2Plugin ();

	std::string unique_id () const;
	const char* uri () const;
	const char* label () const;
	const char* name () const;
	const char* maker () const;

	uint32_t    num_ports () const;
	uint32_t    parameter_count () const;
	float       default_value (uint32_t port);
	framecnt_t  signal_latency () const;
	void        set_parameter (uint32_t port, float val);
	float       get_parameter (uint32_t port) const;
	std::string get_docs() const;
	std::string get_parameter_docs(uint32_t which) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;

	const void* extension_data (const char* uri) const;

	const void* c_plugin();
	const void* c_ui();
	const void* c_ui_type();

	bool is_external_ui () const;
	bool is_external_kx () const;
	bool ui_is_resizable () const;

	const char* port_symbol (uint32_t port) const;
	uint32_t    port_index (const char* symbol) const;

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
	bool parameter_is_event (uint32_t) const;
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;
	bool parameter_is_toggled (uint32_t) const;

	boost::shared_ptr<Plugin::ScalePoints>
	get_scale_points(uint32_t port_index) const;

	void set_insert_info(const PluginInsert* insert);

	int      set_state (const XMLNode& node, int version);
	bool     save_preset (std::string uri);
	void     remove_preset (std::string uri);
	bool     load_preset (PresetRecord);
	std::string current_preset () const;

	bool has_editor () const;
	bool has_message_output () const;

	bool write_from_ui(uint32_t       index,
	                   uint32_t       protocol,
	                   uint32_t       size,
	                   const uint8_t* body);

	typedef void UIMessageSink(void*       controller,
	                           uint32_t    index,
	                           uint32_t    size,
	                           uint32_t    format,
	                           const void* buffer);

	void enable_ui_emmission();
	void emit_to_ui(void* controller, UIMessageSink sink);

	Worker* worker() { return _worker; }

	int work(uint32_t size, const void* data);
	int work_response(uint32_t size, const void* data);

	static URIMap _uri_map;

	struct URIDs {
		uint32_t atom_Chunk;
		uint32_t atom_Path;
		uint32_t atom_Sequence;
		uint32_t atom_eventTransfer;
		uint32_t log_Error;
		uint32_t log_Note;
		uint32_t log_Warning;
		uint32_t midi_MidiEvent;
		uint32_t time_Position;
		uint32_t time_bar;
		uint32_t time_barBeat;
		uint32_t time_beatUnit;
		uint32_t time_beatsPerBar;
		uint32_t time_beatsPerMinute;
		uint32_t time_frame;
		uint32_t time_speed;
	};

	static URIDs urids;

  private:
	struct Impl;
	Impl*         _impl;
	void*         _module;
	LV2_Feature** _features;
	Worker*       _worker;
	framecnt_t    _sample_rate;
	float*        _control_data;
	float*        _shadow_data;
	float*        _defaults;
	LV2_Evbuf**   _ev_buffers;
	LV2_Evbuf**   _atom_ev_buffers;
	float*        _bpm_control_port;  ///< Special input set by ardour
	float*        _freewheel_control_port;  ///< Special input set by ardour
	float*        _latency_control_port;  ///< Special output set by ardour
	framepos_t    _next_cycle_start;  ///< Expected start frame of next run cycle
	double        _next_cycle_speed;  ///< Expected start frame of next run cycle
	PBD::ID       _insert_id;

	friend const void* lv2plugin_get_port_value(const char* port_symbol,
	                                            void*       user_data,
	                                            uint32_t*   size,
	                                            uint32_t*   type);

	typedef enum {
		PORT_INPUT    = 1,       ///< Input port
		PORT_OUTPUT   = 1 << 1,  ///< Output port
		PORT_AUDIO    = 1 << 2,  ///< Audio (buffer of float)
		PORT_CONTROL  = 1 << 3,  ///< Control (single float)
		PORT_EVENT    = 1 << 4,  ///< Old event API event port
		PORT_SEQUENCE = 1 << 5,  ///< New atom API event port
		PORT_MIDI     = 1 << 6,  ///< Event port understands MIDI
		PORT_POSITION = 1 << 7   ///< Event port understands position
	} PortFlag;

	typedef unsigned PortFlags;

	std::vector<PortFlags>         _port_flags;
	std::vector<size_t>            _port_minimumSize;
	std::map<std::string,uint32_t> _port_indices;

	/// Message send to/from UI via ports
	struct UIMessage {
		uint32_t index;
		uint32_t protocol;
		uint32_t size;
	};

	bool write_to_ui(uint32_t       index,
	                 uint32_t       protocol,
	                 uint32_t       size,
	                 const uint8_t* body);

	bool write_to(RingBuffer<uint8_t>* dest,
	              uint32_t             index,
	              uint32_t             protocol,
	              uint32_t             size,
	              const uint8_t*       body);

	// Created on demand so the space is only consumed if necessary
	RingBuffer<uint8_t>* _to_ui;
	RingBuffer<uint8_t>* _from_ui;

	typedef struct {
		const void* (*extension_data) (const char* uri);
	} LV2_DataAccess;

	LV2_DataAccess _data_access_extension_data;
	LV2_Feature    _data_access_feature;
	LV2_Feature    _instance_access_feature;
	LV2_Feature    _make_path_feature;
	LV2_Feature    _log_feature;
	LV2_Feature    _work_schedule_feature;
	LV2_Feature    _options_feature;
	LV2_Feature    _def_state_feature;

	// Options passed to plugin
	int32_t _block_length;
	int32_t _seq_size;

	mutable unsigned _state_version;

	bool _was_activated;
	bool _has_state_interface;

	const std::string plugin_dir () const;
	const std::string scratch_dir () const;
	const std::string file_dir () const;
	const std::string state_dir (unsigned num) const;

	static char* lv2_state_make_path (void*       host_data,
	                                  const char* path);

	void init (const void* c_plugin, framecnt_t rate);
	void allocate_atom_event_buffers ();
	void run (pframes_t nsamples);

	void latency_compute_run ();
	std::string do_save_preset (std::string);
	void do_remove_preset (std::string);
	void find_presets ();
	void add_state (XMLNode *) const;
};


class LIBARDOUR_API LV2PluginInfo : public PluginInfo {
public:
	LV2PluginInfo (const void* c_plugin);
	~LV2PluginInfo ();

	static PluginInfoList* discover ();

	PluginPtr load (Session& session);

	const void* _c_plugin;
};

typedef boost::shared_ptr<LV2PluginInfo> LV2PluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_lv2_plugin_h__ */
