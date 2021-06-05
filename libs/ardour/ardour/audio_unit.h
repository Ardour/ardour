/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_audio_unit_h__
#define __ardour_audio_unit_h__

#include <stdint.h>
#include <boost/shared_ptr.hpp>

#include <list>
#include <set>
#include <string>
#include <vector>
#include <map>

#include "pbd/g_atomic_compat.h"
#include "ardour/plugin.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioUnitProperties.h>
#include "AUParamInfo.h"

#include <boost/shared_ptr.hpp>

class CAComponent;
class CAAudioUnit;
class CAComponentDescription;
struct AudioBufferList;

namespace ARDOUR {

class AudioEngine;
class Session;

struct LIBARDOUR_API AUParameterDescriptor : public ParameterDescriptor {
	// additional fields to make operations more efficient
	AudioUnitParameterID id;
	AudioUnitScope scope;
	AudioUnitElement element;
	bool automatable;
	AudioUnitParameterUnit au_unit;
};

class LIBARDOUR_API AUPlugin : public ARDOUR::Plugin
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
	void set_parameter (uint32_t which, float val, sampleoffset_t);
	float get_parameter (uint32_t which) const;

	PluginOutputConfiguration possible_output () const { return _output_configs; }

	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t nth_parameter (uint32_t which, bool& ok) const;
	void activate ();
	void deactivate ();
	void flush ();
	int set_block_size (pframes_t nframes);

	int connect_and_run (BufferSet& bufs,
			samplepos_t start, samplepos_t end, double speed,
			ChanMapping const& in, ChanMapping const& out,
			pframes_t nframes, samplecnt_t offset);
	std::set<Evoral::Parameter> automatable() const;
	std::string describe_parameter (Evoral::Parameter);
	IOPortDescription describe_io_port (DataType dt, bool input, uint32_t id) const;
	std::string state_node_name () const { return "audiounit"; }

	bool parameter_is_audio (uint32_t) const;
	bool parameter_is_control (uint32_t) const;
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;

	void set_info (PluginInfoPtr);

	int set_state(const XMLNode& node, int);

	bool load_preset (PresetRecord);
	std::string current_preset() const;

	bool has_editor () const;

	bool match_variable_io (ChanCount& in, ChanCount& aux_in, ChanCount& out);
	bool reconfigure_io (ChanCount in, ChanCount aux_in, ChanCount out);

	ChanCount output_streams() const;
	ChanCount input_streams() const;
	bool requires_fixed_size_buffers() const;

	void set_fixed_size_buffers (bool yn) {
		_requires_fixed_size_buffers = yn;
	}

	boost::shared_ptr<CAAudioUnit> get_au () { return unit; }
	boost::shared_ptr<CAComponent> get_comp () const { return comp; }

	OSStatus render_callback(AudioUnitRenderActionFlags *ioActionFlags,
	                         const AudioTimeStamp       *inTimeStamp,
	                         UInt32                      inBusNumber,
	                         UInt32                      inNumberSamples,
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

	/* this MUST be called from thread in which you want to receive notifications
	 * about parameter changes.
	 */
	int create_parameter_listener (AUEventListenerProc callback, void *arg, float interval_secs);

	/* these can be called from any thread but SHOULD be called from the same thread
	 * that will receive parameter change notifications.
	 */
	int listen_to_parameter (uint32_t param_id);
	int end_listen_to_parameter (uint32_t param_id);


  protected:
	std::string do_save_preset (std::string name);
	void do_remove_preset (std::string);

  private:
	samplecnt_t plugin_latency() const;
	void find_presets ();

	boost::shared_ptr<CAComponent> comp;
	boost::shared_ptr<CAAudioUnit> unit;

	bool initialized;
	int32_t input_channels;
	int32_t output_channels;
	std::vector<std::pair<int,int> > io_configs;
	samplecnt_t _last_nframes;
	mutable GATOMIC_QUAL guint _current_latency;
	bool _requires_fixed_size_buffers;
	AudioBufferList* buffers;
	bool _has_midi_input;
	bool _has_midi_output;
	PluginOutputConfiguration _output_configs;

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

	bool variable_inputs;
	bool variable_outputs;

	uint32_t configured_input_busses;
	uint32_t configured_output_busses;

	uint32_t *bus_inputs;
	uint32_t *bus_inused;
	uint32_t *bus_outputs;
	std::vector <std::string> _bus_name_in;
	std::vector <std::string> _bus_name_out;

	int set_stream_format (int scope, uint32_t bus, AudioStreamBasicDescription&);
	void discover_parameters ();
	void add_state (XMLNode *) const;

	typedef std::map<uint32_t, uint32_t> ParameterMap;
	ParameterMap parameter_map;
	uint32_t   input_maxbuf;
	samplecnt_t input_offset;
	samplecnt_t *cb_offsets;
	BufferSet* input_buffers;
	ChanMapping const * input_map;
	samplecnt_t samples_processed;

	std::vector<AUParameterDescriptor> descriptors;
	AUEventListenerRef _parameter_listener;
	void * _parameter_listener_arg;
	void init ();

	void discover_factory_presets ();

	samplepos_t transport_sample;
	float       transport_speed;
	float       last_transport_speed;
	pframes_t   preset_holdoff;

	static void _parameter_change_listener (void* /*arg*/, void* /*src*/, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value);
	void parameter_change_listener (void* /*arg*/, void* /*src*/, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value);
};

class LIBARDOUR_API AUPluginInfo : public PluginInfo {
public:
	 AUPluginInfo (boost::shared_ptr<CAComponentDescription>);
	~AUPluginInfo () {}

	PluginPtr load (Session& session);

	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;

	bool needs_midi_input () const;
	bool is_effect_without_midi_input () const;
	bool is_effect_with_midi_input () const;

	/* note: AU's have an explicit way to prompt for instrument/fx category */
	bool is_effect () const;
	bool is_instrument () const;
	bool is_utility () const;

	bool reconfigurable_io() const { return true; }
	uint32_t max_configurable_ouputs () const { return max_outputs; }

	UInt32 version;
	uint32_t max_outputs;
	std::vector<std::pair<int,int> > io_configs;

	static std::string convert_old_unique_id (std::string const&);

private:
	boost::shared_ptr<CAComponentDescription> descriptor;
};

typedef boost::shared_ptr<AUPluginInfo> AUPluginInfoPtr;

} // namespace ARDOUR

#endif // __ardour_audio_unit_h__
