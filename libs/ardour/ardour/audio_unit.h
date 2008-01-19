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
#include <boost/shared_ptr.hpp>

#include <list>
#include <set>
#include <string>
#include <vector>

#include <ardour/plugin.h>

#include <AudioUnit/AudioUnit.h>

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
	AUPlugin (AudioEngine& engine, Session& session, boost::shared_ptr<CAComponent> comp);
	virtual ~AUPlugin ();
	
        std::string unique_id () const;
	const char * label () const;
	const char * name () const { return _info->name.c_str(); }
	const char * maker () const { return _info->creator.c_str(); }
	uint32_t parameter_count () const;
	float default_value (uint32_t port);
	nframes_t signal_latency () const;
	void set_parameter (uint32_t which, float val);
	float get_parameter (uint32_t which) const;
    
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t nth_parameter (uint32_t which, bool& ok) const;
	void activate ();
	void deactivate ();
	void set_block_size (nframes_t nframes);
    
	int connect_and_run (BufferSet& bufs, uint32_t& in, uint32_t& out, nframes_t nframes, nframes_t offset);
	
	std::set<uint32_t> automatable() const;
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
	
	bool fixed_io() const { return false; }
	int32_t can_support_input_configuration (int32_t in);
	int32_t compute_output_streams (int32_t nplugins);
	uint32_t output_streams() const;
	uint32_t input_streams() const;

	boost::shared_ptr<CAAudioUnit> get_au () { return unit; }
	boost::shared_ptr<CAComponent> get_comp () { return comp; }
    
        OSStatus render_callback(AudioUnitRenderActionFlags *ioActionFlags,
				 const AudioTimeStamp    *inTimeStamp,
				 UInt32       inBusNumber,
				 UInt32       inNumberFrames,
				 AudioBufferList*       ioData);
  private:
        boost::shared_ptr<CAComponent> comp;
        boost::shared_ptr<CAAudioUnit> unit;
	
	AudioStreamBasicDescription streamFormat;
        bool initialized;
        int format_set;
	AudioBufferList* buffers;
	
	UInt32 global_elements;
	UInt32 output_elements;
	UInt32 input_elements;
	
	int set_output_format ();
	int set_input_format ();
	int set_stream_format (int scope, uint32_t cnt);
        int _set_block_size (nframes_t nframes);

	std::vector<std::pair<uint32_t, uint32_t> > parameter_map;
	uint32_t current_maxbuf;
        nframes_t current_offset;
        nframes_t cb_offset;
        vector<Sample*>* current_buffers;
        nframes_t frames_processed;
};
	
typedef boost::shared_ptr<AUPlugin> AUPluginPtr;

class AUPluginInfo : public PluginInfo {
  public:	
	 AUPluginInfo (boost::shared_ptr<CAComponentDescription>);
	~AUPluginInfo ();

	PluginPtr load (Session& session);

	static PluginInfoList discover ();
	static void get_names (CAComponentDescription&, std::string& name, Glib::ustring& maker);
        static std::string stringify_descriptor (const CAComponentDescription&);

  private:
	boost::shared_ptr<CAComponentDescription> descriptor;

	static void discover_music (PluginInfoList&);
	static void discover_fx (PluginInfoList&);
	static void discover_by_description (PluginInfoList&, CAComponentDescription&);
};

typedef boost::shared_ptr<AUPluginInfo> AUPluginInfoPtr;

} // namespace ARDOUR

#endif // __ardour_audio_unit_h__
