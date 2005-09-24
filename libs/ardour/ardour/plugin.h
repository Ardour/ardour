/*
    Copyright (C) 2000 Paul Davis 

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

#ifndef __ardour_ladspa_h__
#define __ardour_ladspa_h__

#include <midi++/controllable.h>
#include <sigc++/signal.h>

#include <jack/types.h>
#include <ardour/types.h>
#include <ardour/stateful.h>
#include <ardour/plugin_state.h>
#include <ardour/cycles.h>

#include <list>
#include <vector>
#include <set>
#include <map>

using std::string;
using std::vector;
using std::list;
using std::set;
using std::map;

namespace ARDOUR {

class AudioEngine;
class Session;

class PluginInfo {
  public:
	enum Type {
		LADSPA,
		VST
	};

	PluginInfo () { };
	PluginInfo (const PluginInfo &o)
		: name(o.name), n_inputs(o.n_inputs), n_outputs(o.n_outputs),
		path (o.path), index(o.index) {}
	~PluginInfo () { };
	string name;
	string category;
	uint32_t n_inputs;
	uint32_t n_outputs;
	Type type;

  private:
	friend class PluginManager;
	string path;
	uint32_t index;
};

class Plugin : public Stateful, public sigc::trackable

{
  public:
	Plugin (ARDOUR::AudioEngine&, ARDOUR::Session&);
	Plugin (const Plugin&);
	~Plugin ();
	
	struct ParameterDescriptor {

	    /* essentially a union of LADSPA and VST info */

	    bool integer_step;
	    bool toggled;
	    bool logarithmic;
	    bool sr_dependent;
	    string label;
	    float lower;
	    float upper;
	    float step;
	    float smallstep;
	    float largestep;

		bool min_unbound;
		bool max_unbound;
	};

	virtual uint32_t unique_id() const = 0;
	virtual const char * label() const = 0;
	virtual const char * name() const = 0;
	virtual const char * maker() const = 0;
	virtual uint32_t parameter_count () const = 0;
	virtual float default_value (uint32_t port) = 0;
	virtual jack_nframes_t latency() const = 0;
	virtual void set_parameter (uint32_t which, float val) = 0;
	virtual float get_parameter(uint32_t which) const = 0;

	virtual int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const = 0;
	virtual uint32_t nth_parameter (uint32_t which, bool& ok) const = 0;
	virtual void activate () = 0;
	virtual void deactivate () = 0;
	virtual void set_block_size (jack_nframes_t nframes) = 0;

	virtual int connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, jack_nframes_t nframes, jack_nframes_t offset) = 0;
	virtual std::set<uint32_t> automatable() const = 0;
	virtual void store_state (ARDOUR::PluginState&) = 0;
	virtual void restore_state (ARDOUR::PluginState&) = 0;
	virtual string describe_parameter (uint32_t) = 0;
	virtual string state_node_name() const = 0;
	virtual void print_parameter (uint32_t, char*, uint32_t len) const = 0;

	virtual bool parameter_is_audio(uint32_t) const = 0;
	virtual bool parameter_is_control(uint32_t) const = 0;
	virtual bool parameter_is_input(uint32_t) const = 0;
	virtual bool parameter_is_output(uint32_t) const = 0;

	virtual bool save_preset(string name) = 0;
	virtual bool load_preset (const string preset_label);
	virtual list<string> get_presets();

	virtual bool has_editor() const = 0;

	sigc::signal<void,uint32_t,float> ParameterChanged;
	sigc::signal<void,Plugin *> GoingAway;
	
	void reset_midi_control (MIDI::Port*, bool);
	void send_all_midi_feedback ();
	MIDI::byte* write_midi_feedback (MIDI::byte*, int32_t& bufsize);
	MIDI::Controllable *get_nth_midi_control (uint32_t);

	PluginInfo & get_info() { return _info; }
	void set_info (const PluginInfo &inf) { _info = inf; }

	ARDOUR::AudioEngine& engine() const { return _engine; }
	ARDOUR::Session& session() const { return _session; }

	void set_cycles (uint32_t c) { _cycles = c; }
	cycles_t cycles() const { return _cycles; }

  protected:
	ARDOUR::AudioEngine& _engine;
	ARDOUR::Session& _session;
	PluginInfo _info;
	uint32_t _cycles;
	map<string,string> 	 presets;
	bool save_preset(string name, string domain /* vst, ladspa etc. */);

	void setup_midi_controls ();


	struct MIDIPortControl : public MIDI::Controllable {
	    MIDIPortControl (Plugin&, uint32_t abs_port_id, MIDI::Port *,
			     float lower, float upper, bool toggled, bool logarithmic);

	    void set_value (float);
	    void send_feedback (float);
	    MIDI::byte* write_feedback (MIDI::byte* buf, int32_t& bufsize, float val, bool force = false);

	    Plugin& plugin;
	    uint32_t absolute_port;
	    float upper;
	    float lower;
	    float range;
	    bool  toggled;
	    bool  logarithmic;

	    bool setting;
	    float last_written;
	};

	vector<MIDIPortControl*> midi_controls;

	
};

/* this is actually defined in plugin_manager.cc */

Plugin * find_plugin(ARDOUR::Session&, string name, PluginInfo::Type);

} // namespace ARDOUR
 
#endif /* __ardour_plugin_h__ */
