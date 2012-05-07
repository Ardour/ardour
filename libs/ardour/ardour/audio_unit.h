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
#include <map>

#include "ardour/plugin.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioUnitProperties.h>
#include "appleutility/AUParamInfo.h"

#include <boost/shared_ptr.hpp>

class CAComponent;
class CAAudioUnit;
class CAComponentDescription;
struct AudioBufferList;

namespace ARDOUR {

class AudioEngine;
class Session;

struct AUParameterDescriptor : public Plugin::ParameterDescriptor {
	// additional fields to make operations more efficient
	AudioUnitParameterID id;
	AudioUnitScope scope;
	AudioUnitElement element;
	float default_value;
	bool automatable;
	AudioUnitParameterUnit unit;
};

class AUPlugin : public ARDOUR::Plugin
{
  public:
	AUPlugin (AudioEngine& engine, Session& session, boost::shared_ptr<CAComponent> comp);
	AUPlugin (const AUPlugin& other);
	virtual ~AUPlugin ();

	std::string unique_id () const;
	const char * label () const;
	const char * name () const { return _info->name.c_str(); }
	const char * maker () const { return _info->creator.c_str(); }
	uint32_t parameter_count () const;
	float default_value (uint32_t port);
	framecnt_t signal_latency() const;
	void set_parameter (uint32_t which, float val);
	float get_parameter (uint32_t which) const;

	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t nth_parameter (uint32_t which, bool& ok) const;
	void activate ();
	void deactivate ();
	void flush ();
	int set_block_size (pframes_t nframes);

	int connect_and_run (BufferSet& bufs,
			     ChanMapping in, ChanMapping out,
			     pframes_t nframes, framecnt_t offset);
	std::set<Evoral::Parameter> automatable() const;
	std::string describe_parameter (Evoral::Parameter);
	std::string state_node_name () const { return "audiounit"; }
	void print_parameter (uint32_t, char*, uint32_t len) const;

	bool parameter_is_audio (uint32_t) const;
	bool parameter_is_control (uint32_t) const;
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;
	
	void set_info (PluginInfoPtr);

	int set_state(const XMLNode& node, int);

	bool load_preset (PresetRecord);
	std::string current_preset() const;

	bool has_editor () const;

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	ChanCount output_streams() const;
	ChanCount input_streams() const;
	bool configure_io (ChanCount in, ChanCount out);
	bool requires_fixed_size_buffers() const;

	void set_fixed_size_buffers (bool yn) {
		_requires_fixed_size_buffers = yn;
	}

	boost::shared_ptr<CAAudioUnit> get_au () { return unit; }
	boost::shared_ptr<CAComponent> get_comp () const { return comp; }

	OSStatus render_callback(AudioUnitRenderActionFlags *ioActionFlags,
	                         const AudioTimeStamp       *inTimeStamp,
	                         UInt32                      inBusNumber,
	                         UInt32                      inNumberFrames,
	                         AudioBufferList*            ioData);

	/* "host" callbacks */

	OSStatus get_beat_and_tempo_callback (Float64* outCurrentBeat,
					      Float64* outCurrentTempo);

	OSStatus get_musical_time_location_callback (UInt32*  outDeltaSampleOffsetToNextBeat,
						     Float32*  outTimeSig_Numerator,
						     UInt32*   outTimeSig_Denominator,
						     Float64*  outCurrentMeasureDownBeat);

	OSStatus get_transport_state_callback (Boolean*  outIsPlaying,
					       Boolean*  outTransportStateChanged,
					       Float64*  outCurrentSampleInTimeLine,
					       Boolean*  outIsCycling,
					       Float64*  outCycleStartBeat,
					       Float64*  outCycleEndBeat);

	static std::string maybe_fix_broken_au_id (const std::string&);

        /* this MUST be called from thread in which you want to receive notifications
	   about parameter changes.
	*/
	int create_parameter_listener (AUEventListenerProc callback, void *arg, float interval_secs);
        /* these can be called from any thread but SHOULD be called from the same thread
	   that will receive parameter change notifications.
	*/
	int listen_to_parameter (uint32_t param_id);
	int end_listen_to_parameter (uint32_t param_id);


  protected:
	std::string do_save_preset (std::string name);
	void do_remove_preset (std::string);

  private:
	void find_presets ();

	boost::shared_ptr<CAComponent> comp;
	boost::shared_ptr<CAAudioUnit> unit;

	bool initialized;
	int32_t input_channels;
	int32_t output_channels;
	std::vector<std::pair<int,int> > io_configs;
	pframes_t _current_block_size;
	framecnt_t _last_nframes;
	bool _requires_fixed_size_buffers;
	AudioBufferList* buffers;
	bool _has_midi_input;
	bool _has_midi_output;

	/* despite all the cool work that apple did on their AU preset
	   system, they left factory presets and user presets as two
	   entirely different kinds of things, handled by two entirely
	   different parts of the API. Resolve this.
	*/

	/* XXX these two maps should really be shared across all instances of this AUPlugin */

	typedef std::map<std::string,std::string> UserPresetMap;
	UserPresetMap user_preset_map;
	typedef std::map<std::string,int> FactoryPresetMap;
	FactoryPresetMap factory_preset_map;

	UInt32 global_elements;
	UInt32 output_elements;
	UInt32 input_elements;

	int set_output_format (AudioStreamBasicDescription&);
	int set_input_format (AudioStreamBasicDescription&);
	int set_stream_format (int scope, uint32_t cnt, AudioStreamBasicDescription&);
	void discover_parameters ();
	void add_state (XMLNode *) const;

	typedef std::map<uint32_t, uint32_t> ParameterMap;
	ParameterMap parameter_map;
	uint32_t   input_maxbuf;
	framecnt_t input_offset;
	framecnt_t cb_offset;
	BufferSet* input_buffers;
	framecnt_t frames_processed;

	std::vector<AUParameterDescriptor> descriptors;
	AUEventListenerRef _parameter_listener;
	void * _parameter_listener_arg;
	void init ();

	void discover_factory_presets ();

	bool      last_transport_rolling;
	float     last_transport_speed;

	static void _parameter_change_listener (void* /*arg*/, void* /*src*/, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value);
	void parameter_change_listener (void* /*arg*/, void* /*src*/, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value);
};

typedef boost::shared_ptr<AUPlugin> AUPluginPtr;

struct AUPluginCachedInfo {
	std::vector<std::pair<int,int> > io_configs;
};

class AUPluginInfo : public PluginInfo {
  public:
	 AUPluginInfo (boost::shared_ptr<CAComponentDescription>);
	~AUPluginInfo ();

	PluginPtr load (Session& session);

	bool needs_midi_input ();
	bool is_effect () const;
	bool is_effect_without_midi_input () const;
	bool is_effect_with_midi_input () const;
	bool is_instrument () const;

	AUPluginCachedInfo cache;

	bool reconfigurable_io() const { return true; }

	static PluginInfoList* discover ();
	static void get_names (CAComponentDescription&, std::string& name, std::string& maker);
	static std::string stringify_descriptor (const CAComponentDescription&);

	static int load_cached_info ();

  private:
	boost::shared_ptr<CAComponentDescription> descriptor;
	UInt32 version;

	static void discover_music (PluginInfoList&);
	static void discover_fx (PluginInfoList&);
	static void discover_generators (PluginInfoList&);
	static void discover_instruments (PluginInfoList&);
	static void discover_by_description (PluginInfoList&, CAComponentDescription&);
	static Glib::ustring au_cache_path ();

	typedef std::map<std::string,AUPluginCachedInfo> CachedInfoMap;
	static CachedInfoMap cached_info;

	static bool cached_io_configuration (const std::string&, UInt32, CAComponent&, AUPluginCachedInfo&, const std::string& name);
	static void add_cached_info (const std::string&, AUPluginCachedInfo&);
	static void save_cached_info ();
};

typedef boost::shared_ptr<AUPluginInfo> AUPluginInfoPtr;

} // namespace ARDOUR

#endif // __ardour_audio_unit_h__
