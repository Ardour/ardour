/*
    Copyright (C) 2004 Paul Davis 

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

#ifndef __ardour_vst_plugin_h__
#define __ardour_vst_plugin_h__

#include <list>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <dlfcn.h>

#include <sigc++/signal.h>
#include "pbd/stateful.h" 
#include <jack/types.h>
#include "ardour/plugin.h"

using std::string;
using std::vector;
using std::list;
using std::map;

struct _FSTHandle;
struct _FST;
typedef struct _FSTHandle FSTHandle;
typedef struct _FST FST;
class AEffect;

namespace ARDOUR {
class AudioEngine;
class Session;

class VSTPlugin : public ARDOUR::Plugin
{
  public:
	VSTPlugin (ARDOUR::AudioEngine&, ARDOUR::Session&, FSTHandle* handle);
	VSTPlugin (const VSTPlugin &);
	~VSTPlugin ();

	/* Plugin interface */
	
	std::string unique_id() const;
	const char * label() const;
	const char * name() const;
	const char * maker() const;
	uint32_t parameter_count() const;
	float default_value (uint32_t port);
	nframes_t signal_latency() const;
	void set_parameter (uint32_t port, float val);
	float get_parameter (uint32_t port) const;
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	std::set<uint32_t> automatable() const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;
	void activate ();
	void deactivate ();
	void set_block_size (nframes_t nframes);
	int connect_and_run (BufferSet&, uint32_t& in, uint32_t& out, nframes_t nframes, nframes_t offset);
	string describe_parameter (uint32_t);
	string state_node_name() const { return "vst"; }
	void print_parameter (uint32_t, char*, uint32_t len) const;

	bool parameter_is_audio(uint32_t i) const { return false; }
	bool parameter_is_control(uint32_t i) const { return true; }
	bool parameter_is_input(uint32_t i) const { return true; }
	bool parameter_is_output(uint32_t i) const { return false; }

	bool load_preset (const string preset_label );
	bool save_preset(string name);

	bool has_editor () const;

	XMLNode& get_state();
	int set_state(const XMLNode& node);

	AEffect* plugin() const { return _plugin; }
	FST* fst() const { return _fst; }


  private:
	FSTHandle* handle;
	FST*       _fst;
	AEffect*   _plugin;
	bool        been_resumed;
};

class VSTPluginInfo : public PluginInfo
{
  public:
	VSTPluginInfo () {}
	~VSTPluginInfo () {}
	
	PluginPtr load (Session& session);
};

typedef boost::shared_ptr<VSTPluginInfo> VSTPluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_vst_plugin_h__ */
