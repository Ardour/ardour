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

    $Id$
*/

#ifndef __ardour_ladspa_plugin_h__
#define __ardour_ladspa_plugin_h__

#include <list>
#include <set>
#include <vector>
#include <string>
#include <dlfcn.h>

#include <sigc++/signal.h>

#include <pbd/stateful.h> 

#include <jack/types.h>
#include <ardour/ladspa.h>
#include <ardour/plugin_state.h>
#include <ardour/plugin.h>
#include <ardour/ladspa_plugin.h>

using std::string;
using std::vector;
using std::list;

namespace ARDOUR {
class AudioEngine;
class Session;

class LadspaPlugin : public ARDOUR::Plugin
{
  public:
	LadspaPlugin (void *module, ARDOUR::AudioEngine&, ARDOUR::Session&, uint32_t index, nframes_t sample_rate);
	LadspaPlugin (const LadspaPlugin &);
	~LadspaPlugin ();

	/* Plugin interface */
	
	uint32_t unique_id() const                       { return descriptor->UniqueID; }
	const char * label() const                       { return descriptor->Label; }
	const char * name() const                        { return descriptor->Name; }
	const char * maker() const                       { return descriptor->Maker; }
	uint32_t parameter_count() const                 { return descriptor->PortCount; }
	float default_value (uint32_t port);
	nframes_t latency() const;
	void set_parameter (uint32_t port, float val);
	float get_parameter (uint32_t port) const;
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	std::set<uint32_t> automatable() const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;
	void activate () { 
		if (descriptor->activate) {
			descriptor->activate (handle);
		}
		was_activated = true;
	}
	void deactivate () {
		if (descriptor->deactivate) 
			descriptor->deactivate (handle);
	}
	void cleanup () {
		if (was_activated && descriptor->cleanup) {
			descriptor->cleanup (handle);
		}
	}
	void set_block_size (nframes_t nframes) {}
	
	int connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, nframes_t nframes, nframes_t offset);
	void store_state (ARDOUR::PluginState&);
	void restore_state (ARDOUR::PluginState&);
	string describe_parameter (uint32_t);
	string state_node_name() const { return "ladspa"; }
	void print_parameter (uint32_t, char*, uint32_t len) const;

	bool parameter_is_audio(uint32_t) const;
	bool parameter_is_control(uint32_t) const;
	bool parameter_is_input(uint32_t) const;
	bool parameter_is_output(uint32_t) const;
	bool parameter_is_toggled(uint32_t) const;

	XMLNode& get_state();
	int set_state(const XMLNode& node);
	bool save_preset(string name);

	bool has_editor() const { return false; }

	int require_output_streams (uint32_t);
	
	/* LADSPA extras */

	LADSPA_Properties properties() const             { return descriptor->Properties; }
	uint32_t index() const                      { return _index; }
	const char * copyright() const                   { return descriptor->Copyright; }
	LADSPA_PortDescriptor port_descriptor(uint32_t i) const { return descriptor->PortDescriptors[i]; }
	const LADSPA_PortRangeHint * port_range_hints() const { return descriptor->PortRangeHints; }
	const char * const * port_names() const          { return descriptor->PortNames; }
	void set_gain (float gain) {
		descriptor->set_run_adding_gain (handle, gain);
	}
	void run_adding (uint32_t nsamples) {
		descriptor->run_adding (handle, nsamples);
	}
	void connect_port (uint32_t port, float *ptr) {
		descriptor->connect_port (handle, port, ptr);
	}

  private:
	void                    *module;
	const LADSPA_Descriptor *descriptor;
	LADSPA_Handle            handle;
	nframes_t           sample_rate;
	LADSPA_Data             *control_data;
	LADSPA_Data             *shadow_data;
	LADSPA_Data             *latency_control_port;
	uint32_t                _index;
	bool                     was_activated;

	void init (void *mod, uint32_t index, nframes_t rate);
	void run (nframes_t nsamples);
	void latency_compute_run ();
};

class LadspaPluginInfo : public PluginInfo {
  public:	
	LadspaPluginInfo () { };
	~LadspaPluginInfo () { };

	PluginPtr load (Session& session);
};

typedef boost::shared_ptr<LadspaPluginInfo> LadspaPluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_ladspa_plugin_h__ */
