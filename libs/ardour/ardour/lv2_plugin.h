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

#include <glibmm/threads.h>
#include <set>
#include <string>
#include <vector>
#include <boost/enable_shared_from_this.hpp>

#include "ardour/plugin.h"
#include "ardour/uri_map.h"
#include "ardour/worker.h"
#include "pbd/ringbuffer.h"

#ifdef LV2_EXTENDED // -> needs to eventually go upstream to lv2plug.in
#include "ardour/lv2_extensions.h"
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

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
	framecnt_t  max_latency () const;
	framecnt_t  signal_latency () const;
	void        set_parameter (uint32_t port, float val);
	float       get_parameter (uint32_t port) const;
	std::string get_docs() const;
	std::string get_parameter_docs(uint32_t which) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;
	bool        get_layout (uint32_t which, UILayoutHint&) const;

	IOPortDescription describe_io_port (DataType dt, bool input, uint32_t id) const;

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
	virtual void set_automation_control (uint32_t, boost::shared_ptr<AutomationControl>);

	void activate ();
	void deactivate ();
	void cleanup ();

	int set_block_size (pframes_t);
	bool requires_fixed_sized_buffers () const;

	int connect_and_run (BufferSet& bufs,
	                     framepos_t start, framepos_t end, double speed,
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

	uint32_t designated_bypass_port ();

	boost::shared_ptr<ScalePoints>
	get_scale_points(uint32_t port_index) const;

	void set_insert_id(PBD::ID id);
	void set_state_dir (const std::string& d = "");

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

	void enable_ui_emission();
	void emit_to_ui(void* controller, UIMessageSink sink);

	Worker* worker() { return _worker; }

	URIMap&       uri_map()       { return _uri_map; }
	const URIMap& uri_map() const { return _uri_map; }

	int work(Worker& worker, uint32_t size, const void* data);
	int work_response(uint32_t size, const void* data);

	void                       set_property(uint32_t key, const Variant& value);
	const PropertyDescriptors& get_supported_properties() const { return _property_descriptors; }
	const ParameterDescriptor& get_property_descriptor(uint32_t id) const;
	void                       announce_property_values();

  private:
	struct Impl;
	Impl*         _impl;
	void*         _module;
	LV2_Feature** _features;
	Worker*       _worker;
	Worker*       _state_worker;
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
	double        _next_cycle_beat;  ///< Expected bar_beat of next run cycle
	double        _current_bpm;
	PBD::ID       _insert_id;
	std::string   _plugin_state_dir;
	uint32_t      _patch_port_in_index;
	uint32_t      _patch_port_out_index;
	URIMap&       _uri_map;
	bool          _no_sample_accurate_ctrl;
	bool          _can_write_automation;
	framecnt_t    _max_latency;
	framecnt_t    _current_latency;

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
		PORT_POSITION = 1 << 7,  ///< Event port understands position
		PORT_PATCHMSG = 1 << 8,  ///< Event port supports patch:Message
		PORT_AUTOCTRL = 1 << 9,  ///< Event port supports auto:AutomationControl
		PORT_CTRLED   = 1 << 10, ///< Port prop auto:AutomationControlled (can be self controlled)
		PORT_CTRLER   = 1 << 11, ///< Port prop auto:AutomationController (can be self set)
		PORT_NOAUTO   = 1 << 12  ///< Port don't allow to automate
	} PortFlag;

	typedef unsigned PortFlags;

	std::vector<PortFlags>         _port_flags;
	std::vector<size_t>            _port_minimumSize;
	std::map<std::string,uint32_t> _port_indices;

	PropertyDescriptors _property_descriptors;

	struct AutomationCtrl {
		AutomationCtrl (const AutomationCtrl &other)
			: ac (other.ac)
			, guard (other.guard)
		{ }

		AutomationCtrl (boost::shared_ptr<ARDOUR::AutomationControl> c)
			: ac (c)
			, guard (false)
		{ }
		boost::shared_ptr<ARDOUR::AutomationControl> ac;
		bool guard;
	};

	typedef boost::shared_ptr<AutomationCtrl> AutomationCtrlPtr;
	typedef std::map<uint32_t, AutomationCtrlPtr> AutomationCtrlMap;
	AutomationCtrlMap _ctrl_map;
	AutomationCtrlPtr get_automation_control (uint32_t);

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

	Glib::Threads::Mutex _work_mutex;

#ifdef LV2_EXTENDED
	const LV2_Inline_Display_Interface* _display_interface;
#endif

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
#ifdef LV2_EXTENDED
	LV2_Feature    _queue_draw_feature;
#endif

	// Options passed to plugin
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
	void run (pframes_t nsamples, bool sync_work = false);

	void load_supported_properties(PropertyDescriptors& descs);

#ifdef LV2_EXTENDED
	bool has_inline_display ();
	Plugin::Display_Image_Surface* render_inline_display (uint32_t, uint32_t);
#endif

	void latency_compute_run ();
	std::string do_save_preset (std::string);
	void do_remove_preset (std::string);
	void find_presets ();
	void add_state (XMLNode *) const;
};


class LIBARDOUR_API LV2PluginInfo : public PluginInfo , public boost::enable_shared_from_this<ARDOUR::LV2PluginInfo> {
public:
	LV2PluginInfo (const char* plugin_uri);
	~LV2PluginInfo ();

	static PluginInfoList* discover ();

	PluginPtr load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
	virtual bool in_category (const std::string &c) const;
	virtual bool is_instrument() const;

	char * _plugin_uri;
};

typedef boost::shared_ptr<LV2PluginInfo> LV2PluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_lv2_plugin_h__ */
