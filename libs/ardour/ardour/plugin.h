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
#include <sigc++/signal.h>
#include <glibmm/ustring.h>

#include "pbd/statefuldestructible.h" 
#include "pbd/controllable.h"

#include <jack/types.h>
#include "ardour/chan_count.h"
#include "ardour/chan_mapping.h"
#include "ardour/cycles.h"
#include "ardour/latent.h"
#include "ardour/plugin_insert.h"
#include "ardour/types.h"

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
	PluginInfo (const PluginInfo &o)
		: name(o.name), 
		category (o.category), 
		creator (o.creator),
		path (o.path), 
		n_inputs(o.n_inputs), 
		n_outputs(o.n_outputs),
		unique_id(o.unique_id), 
		index(o.index) {}
	virtual ~PluginInfo () { }

	std::string name;
	std::string category;
	Glib::ustring creator;
	Glib::ustring path;
	ChanCount n_inputs;
	ChanCount n_outputs;
	ARDOUR::PluginType type;
	
	std::string unique_id;

	virtual PluginPtr load (Session& session) = 0;

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

	    /* essentially a union of LADSPA and VST info */

	    bool integer_step;
	    bool toggled;
	    bool logarithmic;
	    bool sr_dependent;
	    std::string label;
	    float lower;
	    float upper;
	    float step;
	    float smallstep;
	    float largestep;
	    bool min_unbound;
	    bool max_unbound;
	};

	virtual std::string unique_id() const = 0;
	virtual const char * label() const = 0;
	virtual const char * name() const = 0;
	virtual const char * maker() const = 0;
	virtual uint32_t parameter_count () const = 0;
	virtual float default_value (uint32_t port) = 0;
	virtual float get_parameter(uint32_t which) const = 0;

	virtual int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const = 0;
	virtual uint32_t nth_parameter (uint32_t which, bool& ok) const = 0;
	virtual void activate () = 0;
	virtual void deactivate () = 0;
	virtual void set_block_size (nframes_t nframes) = 0;

	virtual int connect_and_run (BufferSet& bufs,
			ChanMapping in, ChanMapping out,
			nframes_t nframes, nframes_t offset) = 0;
	
	virtual std::set<Evoral::Parameter> automatable() const = 0;
	virtual std::string describe_parameter (Evoral::Parameter) = 0;
	virtual std::string state_node_name() const = 0;
	virtual void print_parameter (uint32_t, char*, uint32_t len) const = 0;

	virtual bool parameter_is_audio(uint32_t) const = 0;
	virtual bool parameter_is_control(uint32_t) const = 0;
	virtual bool parameter_is_input(uint32_t) const = 0;
	virtual bool parameter_is_output(uint32_t) const = 0;

	virtual bool save_preset (std::string uri) = 0;
	virtual bool load_preset (const std::string uri);

	struct PresetRecord {
		PresetRecord(const std::string& u, const std::string& l) : uri(u), label(l) {}
		std::string uri;
		std::string label;
	};

	virtual std::vector<PresetRecord> get_presets();

	const PresetRecord* preset_by_label(const std::string& label);
	const PresetRecord* preset_by_uri(const std::string& uri);

	virtual bool has_editor() const = 0;

	sigc::signal<void,uint32_t,float> ParameterChanged;

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
	virtual bool configure_io (ChanCount in, ChanCount out) { return true; }

	/* specific types of plugins can overload this. As of September 2008, only
	   AUPlugin does this.
	*/
	virtual bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const { return false; }
	virtual ChanCount output_streams() const;
	virtual ChanCount input_streams() const;

	PBD::Controllable *get_nth_control (uint32_t, bool do_not_create = false);
	void make_nth_control (uint32_t, const XMLNode&);

	PluginInfoPtr get_info() { return _info; }
	void set_info (const PluginInfoPtr inf) { _info = inf; }

	ARDOUR::AudioEngine& engine() const { return _engine; }
	ARDOUR::Session& session() const { return _session; }

	void set_cycles (uint32_t c) { _cycles = c; }
	cycles_t cycles() const { return _cycles; }

  protected:
	friend class PluginInsert;
	friend struct PluginInsert::PluginControl;
	
	virtual void set_parameter (uint32_t which, float val) = 0;
	
	bool save_preset (std::string uri, std::string domain /* vst, ladspa etc. */);

	ARDOUR::AudioEngine&     _engine;
	ARDOUR::Session&         _session;
	PluginInfoPtr            _info;
	uint32_t                 _cycles;
	std::map<std::string,PresetRecord>  presets;
};

PluginPtr find_plugin(ARDOUR::Session&, std::string unique_id, ARDOUR::PluginType);

} // namespace ARDOUR
 
#endif /* __ardour_plugin_h__ */
