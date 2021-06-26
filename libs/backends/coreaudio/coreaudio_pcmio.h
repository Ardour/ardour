/*
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libbackend_coreaudio_pcmio_h__
#define __libbackend_coreaudio_pcmio_h__

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#undef nil

#include <map>
#include <vector>
#include <string>

#define AUHAL_OUTPUT_ELEMENT 0
#define AUHAL_INPUT_ELEMENT 1


namespace PBD {
class Timing;
}

namespace ARDOUR {

class CoreAudioPCM {
public:
	CoreAudioPCM (void);
	~CoreAudioPCM (void);


	int      state (void) const { return _state; }
	uint32_t n_playback_channels (void) const { return _playback_channels; }
	uint32_t n_capture_channels (void) const { return _capture_channels; }

	void     discover();
	void     device_list (std::map<size_t, std::string> &devices) const { devices = _devices;}
	void     input_device_list (std::map<size_t, std::string> &devices) const { devices = _input_devices;}
	void     output_device_list (std::map<size_t, std::string> &devices) const { devices = _output_devices;}
	void     duplex_device_list (std::map<size_t, std::string> &devices) const { devices = _duplex_devices;}

	int      available_sample_rates (uint32_t device_id, std::vector<float>& sampleRates);
	int      available_buffer_sizes (uint32_t device_id, std::vector<uint32_t>& sampleRates);
	uint32_t available_channels (uint32_t device_id, bool input);
	float    current_sample_rate (uint32_t device_id, bool input = false);
	uint32_t get_latency (uint32_t device_id, bool input);
	uint32_t get_latency (bool input);

	std::string cached_port_name (uint32_t portnum, bool input) const;

	float    sample_rate ();
	uint32_t samples_per_period () const { return _samples_per_period; };

	int      set_samples_per_period (uint32_t);

	void     launch_control_app (uint32_t device_id);

	void     pcm_stop (void);
	int      pcm_start (
			uint32_t input_device,
			uint32_t output_device,
			uint32_t sample_rate,
			uint32_t samples_per_period,
			int (process_callback (void*, const uint32_t, const uint64_t)),
			void * process_arg,
			PBD::TimingStats& dsp_timer
		);

	void     set_error_callback (
			void ( error_callback (void*)),
			void * error_arg
			) {
		_error_callback = error_callback;
		_error_arg = error_arg;
	}

	void     set_hw_changed_callback (
			void ( callback (void*)),
			void * arg
			) {
		_hw_changed_callback = callback;
		_hw_changed_arg = arg;
	}

	void     set_xrun_callback (
			void ( callback (void*)),
			void * arg
			) {
		_xrun_callback = callback;
		_xrun_arg = arg;
	}
	void     set_buffer_size_callback (
			void ( callback (void*)),
			void * arg
			) {
		_buffer_size_callback = callback;
		_buffer_size_arg = arg;
	}
	void     set_sample_rate_callback (
			void ( callback (void*)),
			void * arg
			) {
		_sample_rate_callback = callback;
		_sample_rate_arg = arg;
	}

	// must be called from process_callback;
	int      get_capture_channel (uint32_t chn, float *input, uint32_t n_samples);
	int      set_playback_channel (uint32_t chn, const float *input, uint32_t n_samples);
	uint32_t n_samples() const { return _cur_samples_per_period; };

	// really private
	OSStatus render_callback (
			AudioUnitRenderActionFlags* ioActionFlags,
			const AudioTimeStamp* inTimeStamp,
			UInt32 inBusNumber,
			UInt32 inNumberSamples,
			AudioBufferList* ioData);

	void xrun_callback ();
	void buffer_size_callback ();
	void sample_rate_callback ();
	void hw_changed_callback ();

private:
	float    current_sample_rate_id (AudioDeviceID id, bool input);
	uint32_t current_buffer_size_id (AudioDeviceID id);
	int      set_device_sample_rate_id (AudioDeviceID id, float rate, bool input);
	int      set_device_buffer_size_id (AudioDeviceID id, uint32_t samples_per_period);
	int      set_device_sample_rate (uint32_t device_id, float rate, bool input);
	void     get_stream_latencies (uint32_t device_id, bool input, std::vector<uint32_t>& latencies);
	void     cache_port_names (AudioDeviceID id, bool input);

	void destroy_aggregate_device();
	int  create_aggregate_device(
			AudioDeviceID input_device_id,
			AudioDeviceID output_device_id,
			uint32_t sample_rate,
			AudioDeviceID *created_device);


	::AudioUnit _auhal;
	AudioDeviceID* _device_ids;
	AudioBufferList* _input_audio_buffer_list;
	AudioBufferList* _output_audio_buffer_list;

	AudioDeviceID _active_device_id;
	AudioDeviceID _aggregate_device_id;
	AudioDeviceID _aggregate_plugin_id;

	int _state;

	uint32_t _samples_per_period;
	uint32_t _cur_samples_per_period;
	uint32_t _capture_channels;
	uint32_t _playback_channels;
	bool     _in_process;
	size_t   _n_devices;

	int (* _process_callback) (void*, const uint32_t, const uint64_t);
	void * _process_arg;

	PBD::TimingStats* _dsp_timer;

	void (* _error_callback) (void*);
	void  * _error_arg;

	void (* _hw_changed_callback) (void*);
	void  * _hw_changed_arg;

	void (* _xrun_callback) (void*);
	void  * _xrun_arg;

	void (* _buffer_size_callback) (void*);
	void  * _buffer_size_arg;

	void (* _sample_rate_callback) (void*);
	void  * _sample_rate_arg;


	// TODO proper device info struct
	std::map<size_t, std::string> _devices;
	std::map<size_t, std::string> _input_devices;
	std::map<size_t, std::string> _output_devices;
	std::map<size_t, std::string> _duplex_devices;
	uint32_t * _device_ins;
	uint32_t * _device_outs;
	std::vector<std::string> _input_names;
	std::vector<std::string> _output_names;

	pthread_mutex_t _discovery_lock;
};

} // namespace

#endif /* __libbackend_coreaudio_pcmio_h__ */
