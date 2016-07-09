/*
    Copyright (C) 2000-2006 Paul Davis

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

#ifndef __ardour_plugin_h__
#define __ardour_plugin_h__

#include <boost/shared_ptr.hpp>
#include <string>
#include <set>

#include "pbd/statefuldestructible.h"
#include "pbd/controllable.h"

#include "ardour/buffer_set.h"
#include "ardour/chan_count.h"
#include "ardour/chan_mapping.h"
#include "ardour/cycles.h"
#include "ardour/latent.h"
#include "ardour/libardour_visibility.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/types.h"
#include "ardour/variant.h"

#include <vector>
#include <set>
#include <map>

namespace ARDOUR {

class AudioEngine;
class Session;
class BufferSet;
class PluginInsert;
class Plugin;
class PluginInfo;
class AutomationControl;

typedef boost::shared_ptr<Plugin> PluginPtr;
typedef boost::shared_ptr<PluginInfo> PluginInfoPtr;
typedef std::list<PluginInfoPtr> PluginInfoList;
typedef std::set<uint32_t> PluginOutputConfiguration;

/** A plugin is an external module (usually 3rd party provided) loaded into Ardour
 * for the purpose of digital signal processing.
 *
 * This class provides an abstraction for methords provided by
 * all supported plugin standards such as presets, name, parameters etc.
 *
 * Plugins are not used directly in Ardour but always wrapped by a PluginInsert.
 */
class LIBARDOUR_API Plugin : public PBD::StatefulDestructible, public Latent
{
  public:
	Plugin (ARDOUR::AudioEngine&, ARDOUR::Session&);
	Plugin (const Plugin&);
	virtual ~Plugin ();

	XMLNode& get_state ();
	virtual int set_state (const XMLNode &, int version);

	virtual void set_insert_id (PBD::ID id) {}
	virtual void set_state_dir (const std::string& d = "") {}

	virtual std::string unique_id() const = 0;
	virtual const char * label() const = 0;
	virtual const char * name() const = 0;
	virtual const char * maker() const = 0;
	virtual uint32_t parameter_count () const = 0;
	virtual float default_value (uint32_t port) = 0;
	virtual float get_parameter(uint32_t which) const = 0;
	virtual std::string get_docs () const { return ""; }
	virtual std::string get_parameter_docs (uint32_t /*which*/) const { return ""; }

	struct UILayoutHint {
		UILayoutHint ()
			: x0(-1), x1(-1), y0(-1), y1(-1), knob(false) {}
		int x0;
		int x1;
		int y0;
		int y1;
		bool knob;
	};

	virtual bool get_layout (uint32_t which, UILayoutHint&) const { return false; }

	virtual int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const = 0;
	virtual uint32_t nth_parameter (uint32_t which, bool& ok) const = 0;
	virtual void activate () = 0;
	virtual void deactivate () = 0;
	virtual void flush () { deactivate(); activate(); }

	virtual int set_block_size (pframes_t nframes) = 0;
	virtual bool requires_fixed_sized_buffers() const { return false; }
	virtual bool inplace_broken() const { return false; }

	virtual int connect_and_run (BufferSet& bufs,
			framepos_t start, framepos_t end, double speed,
			ChanMapping in, ChanMapping out,
			pframes_t nframes, framecnt_t offset);

	virtual std::set<Evoral::Parameter> automatable() const = 0;
	virtual std::string describe_parameter (Evoral::Parameter) = 0;
	virtual std::string state_node_name() const = 0;
	virtual void print_parameter (uint32_t, char*, uint32_t len) const = 0;

	virtual bool parameter_is_audio(uint32_t) const = 0;
	virtual bool parameter_is_control(uint32_t) const = 0;
	virtual bool parameter_is_input(uint32_t) const = 0;
	virtual bool parameter_is_output(uint32_t) const = 0;

	virtual uint32_t designated_bypass_port () { return UINT32_MAX; }

	struct LIBARDOUR_API IOPortDescription {
		public:
		IOPortDescription (const std::string& n)
			: name (n)
			, is_sidechain (false)
		{}
		IOPortDescription (const IOPortDescription &other)
			: name (other.name)
			, is_sidechain (other.is_sidechain)
		{}
		std::string name;
		bool is_sidechain;
	};

	virtual IOPortDescription describe_io_port (DataType dt, bool input, uint32_t id) const;
	virtual PluginOutputConfiguration possible_output () const;

	virtual void set_automation_control (uint32_t /*port_index*/, boost::shared_ptr<ARDOUR::AutomationControl>) { }

	virtual boost::shared_ptr<ScalePoints> get_scale_points(uint32_t /*port_index*/) const {
		return boost::shared_ptr<ScalePoints>();
	}

	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	void monitoring_changed ();

	typedef struct {
		unsigned char *data;
		int width;
		int height;
		int stride;
	} Display_Image_Surface;

	virtual bool has_inline_display () { return false; }
	virtual Display_Image_Surface* render_inline_display (uint32_t, uint32_t) { return NULL; }
	PBD::Signal0<void> QueueDraw;

	struct PresetRecord {
	    PresetRecord () : valid (false) {}
	    PresetRecord (const std::string& u, const std::string& l, bool s = true) : uri (u), label (l), user (s), valid (true)  {}

	    bool operator!= (PresetRecord const & a) const {
		    return uri != a.uri || label != a.label;
	    }

	    std::string uri;
	    std::string label;
	    bool user;
	    bool valid;
	};

	PresetRecord save_preset (std::string);
	void remove_preset (std::string);

	virtual bool load_preset (PresetRecord);
	void clear_preset ();

	const PresetRecord * preset_by_label (const std::string &);
	const PresetRecord * preset_by_uri (const std::string &);

	std::vector<PresetRecord> get_presets ();

	/** @return true if this plugin will respond to MIDI program
	 * change messages by changing presets.
	 *
	 * This is hard to return a correct value for because most plugin APIs
	 * do not specify plugin behaviour. However, if you want to force
	 * the display of plugin built-in preset names rather than MIDI program
	 * numbers, return true. If you want a generic description, return
	 * false.
	 */
	virtual bool presets_are_MIDI_programs() const { return false; }

	/** @return true if this plugin is General MIDI compliant, false
	 * otherwise.
	 *
	 * It is important to note that it is is almost impossible for a host
	 * (e.g. Ardour) to determine this for just about any plugin API
	 * known as of June 2012
	 */
	virtual bool current_preset_uses_general_midi() const { return false; }

	/** @return Last preset to be requested; the settings may have
	 * been changed since; find out with parameter_changed_since_last_preset.
	 */
	PresetRecord last_preset () const {
		return _last_preset;
	}

	bool parameter_changed_since_last_preset () const {
		return _parameter_changed_since_last_preset;
	}

	virtual int first_user_preset_index () const {
		return 0;
	}

	/** the max possible latency a plugin will have */
	virtual framecnt_t max_latency () const { return 0; } // TODO = 0, require implementation

	/** Emitted when a preset is added or removed, respectively */
	PBD::Signal0<void> PresetAdded;
	PBD::Signal0<void> PresetRemoved;

	/** Emitted when any preset has been changed */
	static PBD::Signal2<void, std::string, Plugin*> PresetsChanged;

	/** Emitted when a preset has been loaded */
	PBD::Signal0<void> PresetLoaded;

	/** Emitted when a parameter is altered in a way that may have
	 *  changed the settings with respect to any loaded preset.
	 */
	PBD::Signal0<void> PresetDirty;

	virtual bool has_editor () const = 0;

	/** Emitted when a parameter is altered by something outside of our
	 * control, most typically a Plugin GUI/editor
	 */
	PBD::Signal2<void, uint32_t, float> ParameterChangedExternally;

	virtual bool configure_io (ChanCount /*in*/, ChanCount /*out*/) { return true; }

	/* specific types of plugins can overload this. As of September 2008, only
	   AUPlugin does this.
	*/
	virtual bool can_support_io_configuration (const ChanCount& /*in*/, ChanCount& /*out*/, ChanCount* imprecise = 0) { return false; }
	virtual ChanCount output_streams() const;
	virtual ChanCount input_streams() const;

	PluginInfoPtr get_info() const { return _info; }
	virtual void set_info (const PluginInfoPtr inf);

	ARDOUR::AudioEngine& engine() const { return _engine; }
	ARDOUR::Session& session() const { return _session; }

	void set_cycles (uint32_t c) { _cycles = c; }
	cycles_t cycles() const { return _cycles; }

	typedef std::map<uint32_t, ParameterDescriptor> PropertyDescriptors;

	/** Get a descrption of all properties supported by this plugin.
	 *
	 * Properties are distinct from parameters in that they are potentially
	 * dynamic, referred to by key, and do not correspond 1:1 with ports.
	 *
	 * For LV2 plugins, properties are implemented by sending/receiving set/get
	 * messages to/from the plugin via event ports.
	 */
	virtual const PropertyDescriptors& get_supported_properties() const {
		static const PropertyDescriptors nothing;
		return nothing;
	}

	virtual const ParameterDescriptor& get_property_descriptor(uint32_t id) const {
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
	virtual void set_property(uint32_t key, const Variant& value) {}

	/** Emit PropertyChanged for all current property values. */
	virtual void announce_property_values() {}

	/** Emitted when a property is changed in the plugin. */
	PBD::Signal2<void, uint32_t, Variant> PropertyChanged;

	PBD::Signal1<void,uint32_t> StartTouch;
	PBD::Signal1<void,uint32_t> EndTouch;

protected:

	friend class PluginInsert;
	friend class Session;

	/* Called when a parameter of the plugin is changed outside of this
	 * host's control (typical via a plugin's own GUI/editor)
	 */
	void parameter_changed_externally (uint32_t which, float val);

	/* should be overridden by plugin API specific derived types to
	 * actually implement changing the parameter. The derived type should
	 * call this after the change is made.
	 */
	virtual void set_parameter (uint32_t which, float val);

	/** Do the actual saving of the current plugin settings to a preset of the provided name.
	 *  Should return a URI on success, or an empty string on failure.
	 */
	virtual std::string do_save_preset (std::string) = 0;
	/** Do the actual removal of a preset of the provided name */
	virtual void do_remove_preset (std::string) = 0;

	ARDOUR::AudioEngine&     _engine;
	ARDOUR::Session&         _session;
	PluginInfoPtr            _info;
	uint32_t                 _cycles;
	std::map<std::string, PresetRecord> _presets;

private:

	/** Fill _presets with our presets */
	virtual void find_presets () = 0;

	void update_presets (std::string src_unique_id, Plugin* src );

	/** Add state to an existing XMLNode */
	virtual void add_state (XMLNode *) const = 0;

	bool _have_presets;
	MidiStateTracker _tracker;
	BufferSet _pending_stop_events;
	bool _have_pending_stop_events;
	PresetRecord _last_preset;
	bool _parameter_changed_since_last_preset;

	PBD::ScopedConnection _preset_connection;

	void resolve_midi ();
};

struct PluginPreset {
	PluginInfoPtr   _pip;
	Plugin::PresetRecord _preset;

	PluginPreset (PluginInfoPtr pip, const Plugin::PresetRecord *preset = NULL)
		: _pip (pip)
	{
		if (preset) {
			_preset.uri    = preset->uri;
			_preset.label  = preset->label;
			_preset.user   = preset->user;
			_preset.valid  = preset->valid;
		}
	}
};

typedef boost::shared_ptr<PluginPreset> PluginPresetPtr;
typedef std::list<PluginPresetPtr> PluginPresetList;

PluginPtr find_plugin(ARDOUR::Session&, std::string unique_id, ARDOUR::PluginType);

class LIBARDOUR_API PluginInfo {
  public:
	PluginInfo () { }
	virtual ~PluginInfo () { }

	std::string name;
	std::string category;
	std::string creator;
	std::string path;
	ChanCount n_inputs;
	ChanCount n_outputs;
	ARDOUR::PluginType type;

	std::string unique_id;

	virtual PluginPtr load (Session& session) = 0;
	virtual bool is_instrument() const;
	virtual bool needs_midi_input() const { return is_instrument (); }
	virtual bool in_category (const std::string &) const { return false; }

	virtual std::vector<Plugin::PresetRecord> get_presets (bool user_only) const = 0;

	/* NOTE: this block of virtual methods looks like the interface
	   to a Processor, but Plugin does not inherit from Processor.
	   It is therefore not required that these precisely match
	   the interface, but it is likely that they will evolve together.
	*/

	/* this returns true if the plugin can change its inputs or outputs on demand.
	   LADSPA, LV2 and VST plugins cannot do this. AudioUnits can.
	*/

	virtual bool reconfigurable_io() const { return false; }

  protected:
	friend class PluginManager;
	uint32_t index;
};

} // namespace ARDOUR

#endif /* __ardour_plugin_h__ */
