/*
    Copyright (C) 2006 Paul Davis 
	Written by Taybin Rutkin

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

#ifndef __ardour_audio_unit_h__
#define __ardour_audio_unit_h__

#include <stdint.h>

#include <list>
#include <set>
#include <string>
#include <vector>

#include <ardour/plugin.h>

#include <boost/shared_ptr.hpp>

class CAComponent;
class CAAudioUnit;
class CAComponentDescription;
struct AudioBufferList;

namespace ARDOUR {

class AudioEngine;
class Session;

class AUPlugin : public ARDOUR::Plugin
{
  public:
	AUPlugin (AudioEngine& engine, Session& session, CAComponent* comp);
	virtual ~AUPlugin ();
	
	uint32_t unique_id () const;
	const char * label () const;
	const char * name () const { return _info->name.c_str(); }
	const char * maker () const;
	uint32_t parameter_count () const;
	float default_value (uint32_t port);
	nframes_t latency () const;
	void set_parameter (uint32_t which, float val);
	float get_parameter (uint32_t which) const;
    
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t nth_parameter (uint32_t which, bool& ok) const;
	void activate ();
	void deactivate ();
	void set_block_size (nframes_t nframes);
    
	int connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, nframes_t nframes, nframes_t offset);
	std::set<uint32_t> automatable() const;
	void store_state (ARDOUR::PluginState&);
	void restore_state (ARDOUR::PluginState&);
	string describe_parameter (uint32_t);
	string state_node_name () const { return "audiounit"; }
	void print_parameter (uint32_t, char*, uint32_t len) const;
    
	bool parameter_is_audio (uint32_t) const;
	bool parameter_is_control (uint32_t) const;
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;
    
	XMLNode& get_state();
	int set_state(const XMLNode& node);
	
	bool save_preset (string name);
	bool load_preset (const string preset_label);
	std::vector<std::string> get_presets ();
    
	bool has_editor () const;
	
	CAAudioUnit* get_au () { return unit; }
	CAComponent* get_comp () { return comp; }
	
  private:
	CAComponent* comp;
    CAAudioUnit* unit;

	AudioBufferList* in_list;
	AudioBufferList* out_list;

	std::vector<std::pair<uint32_t, uint32_t> > parameter_map;
};

typedef boost::shared_ptr<AUPlugin> AUPluginPtr;

class AUPluginInfo : public PluginInfo {
  public:	
	AUPluginInfo () { };
	~AUPluginInfo ();

	CAComponentDescription* desc;

	static PluginInfoList discover ();
	PluginPtr load (Session& session);

  private:
	static std::string get_name (CAComponentDescription&);
	void setup_nchannels (CAComponentDescription&);
};

typedef boost::shared_ptr<AUPluginInfo> AUPluginInfoPtr;

} // namespace ARDOUR

#endif // __ardour_audio_unit_h__
