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

#include "pbd/statefuldestructible.h"
#include "pbd/controllable.h"

#include <jack/types.h>
#include "ardour/chan_count.h"
#include "ardour/chan_mapping.h"
#include "ardour/cycles.h"
#include "ardour/latent.h"
#include "ardour/plugin_insert.h"
#include "ardour/types.h"
#include "ardour/midi_state_tracker.h"

#include <vector>
#include <set>
#include <map>

namespace ARDOUR {

class AudioEngine;
class Session;
class BufferSet;

class Plugin;

typedef boost::shared_ptr<Plugin> PluginPtr;

class PluginInfo {
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

  protected:
	friend class PluginManager;
	uint32_t index;
};

typedef boost::shared_ptr<PluginInfo> PluginInfoPtr;
typedef std::list<PluginInfoPtr> PluginInfoList;

class Plugin : public PBD::StatefulDestructible, public Latent
{
  public:
	Plugin (ARDOUR::AudioEngine&, ARDOUR::Session&);
	Plugin (const Plugin&);
	virtual ~Plugin ();

	struct ParameterDescriptor {

		/* XXX: it would probably be nice if this initialised everything */
		ParameterDescriptor ()
			: enumeration (false)
		{}

		/* essentially a union of LADSPA and VST info */

		bool integer_step;
		bool toggled;
		bool logarithmic;
		bool sr_dependent;
		std::string label;
		float lower; ///< if this is a frequency, it will be in Hz (not a fraction of the sample rate)
		float upper; ///< if this is a frequency, it will be in Hz (not a fraction of the sample rate)
		float step;
		float smallstep;
		float largestep;
		bool min_unbound;
		bool max_unbound;
		bool enumeration;
	};

	XMLNode& get_state ();
	virtual int set_state (const XMLNode &, int version);

	virtual void set_insert_info (const PluginInsert*) {}

	virtual std::string unique_id() const = 0;
	virtual const char * label() const = 0;
	virtual const char * name() const = 0;
	virtual const char * maker() const = 0;
	virtual uint32_t parameter_count () const = 0;
	virtual float default_value (uint32_t port) = 0;
	virtual float get_parameter(uint32_t which) const = 0;
	virtual std::string get_parameter_docs(uint32_t which) const { return ""; }

	virtual int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const = 0;
	virtual uint32_t nth_parameter (uint32_t which, bool& ok) const = 0;
	virtual void activate () = 0;
	virtual void deactivate () = 0;
	virtual void flush () { deactivate(); activate(); }

	virtual int set_block_size (pframes_t nframes) = 0;

	virtual int connect_and_run (BufferSet& bufs,
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

	typedef std::map<const std::string, const float> ScalePoints;

	virtual boost::shared_ptr<ScalePoints> get_scale_points(uint32_t /*port_index*/) const {
		return boost::shared_ptr<ScalePoints>();
	}

	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	void monitoring_changed ();

	struct PresetRecord {
		PresetRecord () : user (true) {}
		PresetRecord (const std::string& u, const std::string& l, bool s = true) : uri (u), label (l), user (s)  {}

		bool operator!= (PresetRecord const & a) const {
			return uri != a.uri || label != a.label;
		}

		std::string uri;
		std::string label;
		bool user;
	};

	PresetRecord save_preset (std::string);
	void remove_preset (std::string);

	virtual bool load_preset (PresetRecord);

	const PresetRecord * preset_by_label (const std::string &);
	const PresetRecord * preset_by_uri (const std::string &);

	std::vector<PresetRecord> get_presets ();

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

	/** Emitted when a preset is added or removed, respectively */
	PBD::Signal0<void> PresetAdded;
	PBD::Signal0<void> PresetRemoved;

	/** Emitted when a preset has been loaded */
	PBD::Signal0<void> PresetLoaded;

	virtual bool has_editor () const = 0;

	/** Emitted when any parameter changes */
	PBD::Signal2<void, uint32_t, float> ParameterChanged;

	/* NOTE: this block of virtual methods looks like the interface
	   to a Processor, but Plugin does not inherit from Processor.
	   It is therefore not required that these precisely match
	   the interface, but it is likely that they will evolve together.
	*/

	/* this returns true if the plugin can change its inputs or outputs on demand.
	   LADSPA, LV2 and VST plugins cannot do this. AudioUnits can.
	*/

	virtual bool reconfigurable_io() const { return false; }

	/* this is only called if reconfigurable_io() returns true */
	virtual bool configure_io (ChanCount /*in*/, ChanCount /*out*/) { return true; }

	/* specific types of plugins can overload this. As of September 2008, only
	   AUPlugin does this.
	*/
	virtual bool can_support_io_configuration (const ChanCount& /*in*/, ChanCount& /*out*/) const { return false; }
	virtual ChanCount output_streams() const;
	virtual ChanCount input_streams() const;

	PluginInfoPtr get_info() const { return _info; }
	virtual void set_info (const PluginInfoPtr inf);

	ARDOUR::AudioEngine& engine() const { return _engine; }
	ARDOUR::Session& session() const { return _session; }

	void set_cycles (uint32_t c) { _cycles = c; }
	cycles_t cycles() const { return _cycles; }

protected:

	friend class PluginInsert;
	friend struct PluginInsert::PluginControl;

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

	/** Add state to an existing XMLNode */
	virtual void add_state (XMLNode *) const = 0;

	bool _have_presets;
	MidiStateTracker _tracker;
	BufferSet _pending_stop_events;
	bool _have_pending_stop_events;
	PresetRecord _last_preset;
	bool _parameter_changed_since_last_preset;

	void resolve_midi ();
};

PluginPtr find_plugin(ARDOUR::Session&, std::string unique_id, ARDOUR::PluginType);

} // namespace ARDOUR

#endif /* __ardour_plugin_h__ */
