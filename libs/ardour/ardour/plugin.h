/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_plugin_h__
#define __ardour_plugin_h__

#include <boost/shared_ptr.hpp>
#include <set>
#include <string>

#include "pbd/controllable.h"
#include "pbd/statefuldestructible.h"

#include "ardour/buffer_set.h"
#include "ardour/chan_count.h"
#include "ardour/chan_mapping.h"
#include "ardour/cycles.h"
#include "ardour/latent.h"
#include "ardour/libardour_visibility.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/types.h"
#include "ardour/variant.h"

#include <map>
#include <set>
#include <vector>

namespace ARDOUR
{
class AudioEngine;
class Session;
class BufferSet;
class PluginInsert;
class Plugin;
class PluginInfo;
class AutomationControl;
class SessionObject;

typedef boost::shared_ptr<Plugin>     PluginPtr;
typedef boost::shared_ptr<PluginInfo> PluginInfoPtr;
typedef std::list<PluginInfoPtr>      PluginInfoList;
typedef std::set<uint32_t>            PluginOutputConfiguration;

/** A plugin is an external module (usually 3rd party provided) loaded into Ardour
 * for the purpose of digital signal processing.
 *
 * This class provides an abstraction for methords provided by
 * all supported plugin standards such as presets, name, parameters etc.
 *
 * Plugins are not used directly in Ardour but always wrapped by a PluginInsert.
 */
class LIBARDOUR_API Plugin : public PBD::StatefulDestructible, public HasLatency
{
public:
	Plugin (ARDOUR::AudioEngine&, ARDOUR::Session&);
	Plugin (const Plugin&);
	virtual ~Plugin ();

	XMLNode&    get_state ();
	virtual int set_state (const XMLNode&, int version);

	virtual void set_insert_id (PBD::ID id) {}
	virtual void set_state_dir (const std::string& d = "") {}

	virtual std::string unique_id () const                   = 0;
	virtual const char* label () const                       = 0;
	virtual const char* name () const                        = 0;
	virtual const char* maker () const                       = 0;
	virtual uint32_t    parameter_count () const             = 0;
	virtual float       default_value (uint32_t port)        = 0;
	virtual float       get_parameter (uint32_t which) const = 0;

	virtual std::string get_docs () const { return ""; }
	virtual std::string get_parameter_docs (uint32_t /*which*/) const { return ""; }

	virtual int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const = 0;
	virtual uint32_t    nth_parameter (uint32_t which, bool& ok) const                        = 0;
	virtual std::string parameter_label (uint32_t which) const;

	virtual void activate ()   = 0;
	virtual void deactivate () = 0;
	virtual void flush ()
	{
		deactivate ();
		activate ();
	}

	virtual std::set<Evoral::Parameter> automatable () const                   = 0;
	virtual std::string                 describe_parameter (Evoral::Parameter) = 0;
	virtual std::string                 state_node_name () const               = 0;

	virtual bool print_parameter (uint32_t, std::string&) const { return false; }

	virtual bool parameter_is_audio (uint32_t) const   = 0;
	virtual bool parameter_is_control (uint32_t) const = 0;
	virtual bool parameter_is_input (uint32_t) const   = 0;
	virtual bool parameter_is_output (uint32_t) const  = 0;

	virtual uint32_t designated_bypass_port () { return UINT32_MAX; }

	struct LIBARDOUR_API IOPortDescription {
	public:
		IOPortDescription (const std::string& n, bool sc = false, std::string gn = "", uint32_t gc = 0)
		        : name (n)
		        , is_sidechain (sc)
		        , group_name (gn.empty () ? n : gn)
		        , group_channel (gc)
		{ }

		IOPortDescription (const IOPortDescription& other)
		        : name (other.name)
		        , is_sidechain (other.is_sidechain)
		        , group_name (other.group_name)
		        , group_channel (other.group_channel)
		{ }

		std::string name;
		bool        is_sidechain;

		std::string group_name;
		uint32_t    group_channel;
	};

	virtual IOPortDescription         describe_io_port (DataType dt, bool input, uint32_t id) const;
	virtual PluginOutputConfiguration possible_output () const;

	virtual void set_automation_control (uint32_t /*port_index*/, boost::shared_ptr<ARDOUR::AutomationControl>) {}

	virtual boost::shared_ptr<ScalePoints> get_scale_points (uint32_t /*port_index*/) const
	{
		return boost::shared_ptr<ScalePoints> ();
	}

	samplecnt_t signal_latency () const
	{
		return plugin_latency ();
	}

	/** the max possible latency a plugin will have */
	virtual samplecnt_t max_latency () const { return 0; }

	virtual int  set_block_size (pframes_t nframes) = 0;
	virtual bool requires_fixed_sized_buffers () const { return false; }
	virtual bool inplace_broken () const { return false; }
	virtual bool connect_all_audio_outputs () const { return false; }

	virtual int connect_and_run (BufferSet&  bufs,
	                             samplepos_t start, samplepos_t end, double speed,
	                             ChanMapping const& in, ChanMapping const& out,
	                             pframes_t nframes, samplecnt_t offset);


	bool write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf);

	void realtime_handle_transport_stopped ();
	void realtime_locate (bool);
	void monitoring_changed ();

	virtual void add_slave (boost::shared_ptr<Plugin>, bool realtime) {}
	virtual void remove_slave (boost::shared_ptr<Plugin>) {}

	typedef struct {
		unsigned char* data;
		int            width;
		int            height;
		int            stride;
	} Display_Image_Surface;

	virtual bool has_inline_display () { return false; }
	virtual bool inline_display_in_gui () { return false; }
	virtual Display_Image_Surface* render_inline_display (uint32_t, uint32_t) { return NULL; }
	PBD::Signal0<void> QueueDraw;

	virtual bool has_midnam () { return false; }
	virtual bool read_midnam () { return false; }
	virtual std::string midnam_model () { return ""; }
	PBD::Signal0<void> UpdateMidnam;
	PBD::Signal0<void> UpdatedMidnam;

	virtual bool knows_bank_patch () { return false; }
	virtual uint32_t bank_patch (uint8_t chn) { return UINT32_MAX; }
	PBD::Signal1<void, uint8_t> BankPatchChange;

	struct PresetRecord {
		PresetRecord () : valid (false) { }

		PresetRecord (const std::string& u, const std::string& l, bool s = true, const std::string& d = "")
		        : uri (u)
		        , label (l)
		        , description (d)
		        , user (s)
		        , valid (true)
		{ }

		bool operator!= (PresetRecord const& a) const
		{
			return uri != a.uri || label != a.label;
		}

		std::string uri;
		std::string label;
		std::string description;
		bool        user;
		bool        valid;
	};

	/** Create a new plugin-preset from the current state
	 *
	 * @param name label to use for new preset (needs to be unique)
	 * @return PresetRecord with empty URI on failure
	 */
	PresetRecord save_preset (std::string name);
	void         remove_preset (std::string);

	/** Set parameters using a preset */
	virtual bool load_preset (PresetRecord);
	void         clear_preset ();

	const PresetRecord* preset_by_label (const std::string&);
	const PresetRecord* preset_by_uri (const std::string&);

	std::vector<PresetRecord> get_presets ();

	/** @return Last preset to be requested; the settings may have
	 * been changed since; find out with parameter_changed_since_last_preset.
	 */
	PresetRecord last_preset () const
	{
		return _last_preset;
	}

	bool parameter_changed_since_last_preset () const
	{
		return _parameter_changed_since_last_preset;
	}

	virtual int first_user_preset_index () const { return 0; }

	/** Emitted when a preset is added or removed, respectively */
	PBD::Signal0<void> PresetAdded;
	PBD::Signal0<void> PresetRemoved;

	/** Emitted when any preset has been changed */
	static PBD::Signal3<void, std::string, Plugin*, bool> PresetsChanged;

	/** Emitted when a preset has been loaded */
	PBD::Signal0<void> PresetLoaded;

	/** Emitted when a parameter is altered in a way that may have
	 *  changed the settings with respect to any loaded preset.
	 */
	PBD::Signal0<void> PresetDirty;

	/** Emitted for preset-load to set a control-port */
	PBD::Signal2<void, uint32_t, float> PresetPortSetValue;

	/** @return true if plugin has a custom plugin GUI */
	virtual bool has_editor () const = 0;

	/** Emitted when a parameter is altered by something outside of our
	 * control, most typically a Plugin GUI/editor
	 */
	PBD::Signal2<void, uint32_t, float> ParameterChangedExternally;

	virtual bool reconfigure_io (ChanCount /*in*/, ChanCount /*aux_in*/, ChanCount /*out*/) { return true; }
	virtual bool match_variable_io (ChanCount& /*in*/, ChanCount& /*aux_in*/, ChanCount& /*out*/) { return false; }

	virtual ChanCount output_streams () const;
	virtual ChanCount input_streams () const;

	virtual void set_info (const PluginInfoPtr info) { _info = info; }
	PluginInfoPtr get_info () const { return _info; }

	virtual void set_owner (SessionObject* o) { _owner = o; }
	SessionObject* owner () const { return _owner; }

	void set_cycles (uint32_t c) { _cycles = c; }
	cycles_t cycles () const { return _cycles; }

	void use_for_impulse_analysis ()
	{
		_for_impulse_analysis = true;
	}

	ARDOUR::AudioEngine& engine () const { return _engine; }
	ARDOUR::Session& session () const { return _session; }

	typedef std::map<uint32_t, ParameterDescriptor> PropertyDescriptors;

	/** Get a descrption of all properties supported by this plugin.
	 *
	 * Properties are distinct from parameters in that they are potentially
	 * dynamic, referred to by key, and do not correspond 1:1 with ports.
	 *
	 * For LV2 plugins, properties are implemented by sending/receiving set/get
	 * messages to/from the plugin via event ports.
	 */
	virtual const PropertyDescriptors& get_supported_properties () const
	{
		static const PropertyDescriptors nothing;
		return nothing;
	}

	virtual const ParameterDescriptor& get_property_descriptor (uint32_t id) const
	{
		static const ParameterDescriptor nothing;
		return nothing;
	}

	/** Set a property from the UI.
	 *
	 * This is not UI-specific, but may only be used by one thread.  If the
	 * Ardour UI is present, that is the UI thread, but otherwise, any thread
	 * except the audio thread may call this function as long as it is not
	 * called concurrently.
	 */
	virtual void set_property (uint32_t key, const Variant& value) {}

	/** Emit PropertyChanged for all current property values. */
	virtual void announce_property_values () {}

	/** Emitted when a property is changed in the plugin. */
	PBD::Signal2<void, uint32_t, Variant> PropertyChanged;

	PBD::Signal1<void, uint32_t> StartTouch;
	PBD::Signal1<void, uint32_t> EndTouch;

protected:
	friend class PluginInsert;
	friend class Session;

	/* Called when a parameter of the plugin is changed outside of this
	 * host's control (typical via a plugin's own GUI/editor)
	 */
	virtual void parameter_changed_externally (uint32_t which, float val);

	/* should be overridden by plugin API specific derived types to
	 * actually implement changing the parameter. The derived type should
	 * call this after the change is made.
	 *
	 * @param which parameter-id
	 * @param val the raw value (plugin internal)
	 * @param when time offset of samples in current cycle (0 .. n_samples)
	 *             when the event is effective.
	 */
	virtual void set_parameter (uint32_t which, float val, sampleoffset_t when);

	/** Do the actual saving of the current plugin settings to a preset of the provided name.
	 *  Should return a URI on success, or an empty string on failure.
	 */
	virtual std::string do_save_preset (std::string) = 0;
	/** Do the actual removal of a preset of the provided name */
	virtual void do_remove_preset (std::string) = 0;


	/** Plugin's [internal] state changed, mark preset and session
	 * as modified.
	 */
	void state_changed ();

	ARDOUR::AudioEngine& _engine;
	ARDOUR::Session&     _session;
	PluginInfoPtr        _info;
	uint32_t             _cycles;
	SessionObject*       _owner;
	bool                 _for_impulse_analysis;

	std::map<std::string, PresetRecord> _presets;

private:
	virtual samplecnt_t plugin_latency () const = 0;

	/** Fill _presets with our presets */
	virtual void find_presets () = 0;

	/** Add state to an existing XMLNode */
	virtual void add_state (XMLNode*) const = 0;

	bool             _have_presets;
	MidiNoteTracker _tracker;
	BufferSet        _pending_stop_events;
	bool             _have_pending_stop_events;
	PresetRecord     _last_preset;
	bool             _parameter_changed_since_last_preset;

	PBD::ScopedConnection _preset_connection;

	MidiRingBuffer<samplepos_t> _immediate_events;

	void invalidate_preset_cache (std::string const&, Plugin*, bool);
	void resolve_midi ();
};

struct PluginPreset {
	PluginInfoPtr        _pip;
	Plugin::PresetRecord _preset;

	PluginPreset (PluginInfoPtr pip, const Plugin::PresetRecord* preset = NULL)
	        : _pip (pip)
	{
		if (preset) {
			_preset.uri         = preset->uri;
			_preset.label       = preset->label;
			_preset.user        = preset->user;
			_preset.description = preset->description;
			_preset.valid       = preset->valid;
		}
	}
};

typedef boost::shared_ptr<PluginPreset> PluginPresetPtr;
typedef std::list<PluginPresetPtr>      PluginPresetList;

PluginPtr
find_plugin (ARDOUR::Session&, std::string unique_id, ARDOUR::PluginType);

class LIBARDOUR_API PluginInfo
{
public:
	PluginInfo ()
	        : multichannel_name_ambiguity (false)
	        , plugintype_name_ambiguity (false)
	        , index (0)
	{}

	virtual ~PluginInfo () {}

	std::string        name;
	std::string        category;
	std::string        creator;
	std::string        path;
	ChanCount          n_inputs;
	ChanCount          n_outputs;
	ARDOUR::PluginType type;

	bool multichannel_name_ambiguity;
	bool plugintype_name_ambiguity;

	std::string unique_id;

	virtual PluginPtr load (Session& session) = 0;

	/* NOTE: it is possible for a plugin to be an effect AND an instrument.
	 * override these funcs as necessary to support that. */
	virtual bool is_effect () const;
	virtual bool is_instrument () const;
	virtual bool is_utility () const; // this includes things like "generators" and "midi filters"
	virtual bool is_analyzer () const;

	virtual bool needs_midi_input () const;

	virtual std::vector<Plugin::PresetRecord> get_presets (bool user_only) const = 0;

	/* NOTE: this block of virtual methods looks like the interface
	 * to a Processor, but Plugin does not inherit from Processor.
	 * It is therefore not required that these precisely match
	 * the interface, but it is likely that they will evolve together. */

	/* @return true if the plugin can change its inputs or outputs on demand. */
	virtual bool reconfigurable_io () const { return false; }

	/* max [re]configurable outputs (if finite, 0 otherwise) */
	virtual uint32_t max_configurable_ouputs () const
	{
		return n_outputs.n_audio();
	}

protected:
	friend class PluginManager;
	uint32_t index; //< used for LADSPA, index in module
};

} // namespace ARDOUR

#endif /* __ardour_plugin_h__ */
