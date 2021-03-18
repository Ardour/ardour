/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __libbackend_portaudio_pcmio_h__
#define __libbackend_portaudio_pcmio_h__

#include <map>
#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>

#include <stdint.h>

#include <portaudio.h>

namespace ARDOUR {

class PortAudioIO {
public:
	PortAudioIO (void);
	~PortAudioIO (void);

	enum StandardDevices {
		DeviceNone = -2,
		DeviceDefault = -1
	};

	void host_api_list (std::vector<std::string>&);
	bool set_host_api (const std::string& host_api_name);
	std::string get_host_api () const { return _host_api_name; }
	PaHostApiTypeId get_current_host_api_type () const;
	PaHostApiIndex get_host_api_index_from_name (const std::string& name);

	PaDeviceIndex get_default_input_device () const;
	PaDeviceIndex get_default_output_device () const;

	bool     update_devices();
	void     input_device_list (std::map<int, std::string> &devices) const;
	void     output_device_list (std::map<int, std::string> &devices) const;

	int available_sample_rates (int device_id, std::vector<float>& sample_rates);
	int available_buffer_sizes (int device_id, std::vector<uint32_t>& buffer_sizes);

#ifdef WITH_ASIO
	bool get_asio_buffer_properties (int device_id,
	                                 long& min_size_samples,
	                                 long& max_size_samples,
	                                 long& preferred_size_samples,
	                                 long& granularity);

	bool get_asio_buffer_sizes(int device_id,
	                           std::vector<uint32_t>& buffer_size,
	                           bool preferred_only);
#endif

	std::string control_app_name (int device_id) const;
	void launch_control_app (int device_id);

	PaErrorCode open_blocking_stream(int device_input,
	                                 int device_output,
	                                 double sample_rate,
	                                 uint32_t samples_per_period);

	PaErrorCode open_callback_stream(int device_input,
	                                 int device_output,
	                                 double sample_rate,
	                                 uint32_t samples_per_period,
	                                 PaStreamCallback* callback,
	                                 void* data);

	PaErrorCode start_stream(void);

	PaErrorCode close_stream(void);

	uint32_t n_playback_channels (void) const { return _playback_channels; }
	uint32_t n_capture_channels (void) const { return _capture_channels; }

	std::string get_input_channel_name (int device_id, uint32_t channel) const;
	std::string get_output_channel_name (int device_id, uint32_t channel) const;

	double   sample_rate (void) const { return _cur_sample_rate; }
	uint32_t capture_latency (void) const { return _cur_input_latency; }
	uint32_t playback_latency (void) const { return _cur_output_latency; }
	double   stream_time(void) const { if (_stream) return Pa_GetStreamTime (_stream); return 0; }

	int      next_cycle(uint32_t n_samples);
	int      get_capture_channel (uint32_t chn, float *input, uint32_t n_samples);
	int      set_playback_channel (uint32_t chn, const float *input, uint32_t n_samples);

	float* get_capture_buffer () { return _input_buffer; }
	float* get_playback_buffer () { return _output_buffer; }

private: // Methods

	static bool pa_initialize();
	static bool pa_deinitialize();
	static bool& pa_initialized();

	void clear_device_lists ();
	void add_none_devices ();
	void add_default_devices ();
	void add_devices ();
	std::string get_host_api_name_from_index (PaHostApiIndex index);

	bool get_output_stream_params(int device_output,
	                              PaStreamParameters& outputParam) const;
	bool get_input_stream_params(int device_input,
	                             PaStreamParameters& inputParam) const;

	bool set_sample_rate_and_latency_from_stream();
	bool allocate_buffers_for_blocking_api (uint32_t samples_per_period);

	PaErrorCode pre_stream_open(int device_input,
	                          PaStreamParameters& inputParam,
	                          int device_output,
	                          PaStreamParameters& outputParam,
	                          uint32_t sample_rate,
	                          uint32_t samples_per_period);

	void reset_stream_dependents ();

	static void get_default_sample_rates(std::vector<float>&);
	static void get_default_buffer_sizes(std::vector<uint32_t>&);

private: // Data
	uint32_t _capture_channels;
	uint32_t _playback_channels;

	PaStream *_stream;

	float *_input_buffer;
	float *_output_buffer;

	double _cur_sample_rate;
	uint32_t _cur_input_latency;
	uint32_t _cur_output_latency;

	struct paDevice {
		std::string name;
		uint32_t n_inputs;
		uint32_t n_outputs;

		paDevice (std::string n, uint32_t i, uint32_t o)
			: name (n)
			, n_inputs (i)
			, n_outputs (o)
		{}
	};

	std::map<int, paDevice *> _input_devices;
	std::map<int, paDevice *> _output_devices;

	PaHostApiIndex _host_api_index;
	std::string _host_api_name;

};

} // namespace

#endif /* __libbackend_portaudio_pcmio_h__ */
