/*
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glibmm.h>

#include "portaudio_io.h"

#ifdef WITH_ASIO
#include "pa_asio.h"
#endif

#include "pbd/compose.h"

#include "ardour/audio_backend.h"

#include "debug.h"

#define INTERLEAVED_INPUT
#define INTERLEAVED_OUTPUT

using namespace PBD;
using namespace ARDOUR;

PortAudioIO::PortAudioIO ()
	: _capture_channels (0)
	, _playback_channels (0)
	, _stream (0)
	, _input_buffer (0)
	, _output_buffer (0)
	, _cur_sample_rate (0)
	, _cur_input_latency (0)
	, _cur_output_latency (0)
	, _host_api_index(-1)
{
}

PortAudioIO::~PortAudioIO ()
{
	close_stream();

	pa_deinitialize ();
	clear_device_lists ();

	free (_input_buffer); _input_buffer = NULL;
	free (_output_buffer); _output_buffer = NULL;
}

std::string
PortAudioIO::control_app_name (int device_id) const
{
#ifdef WITH_ASIO
	if (get_current_host_api_type() == paASIO) {
		// is this used for anything, or just acts as a boolean?
		return "PortaudioASIO";
	}
#endif

	return std::string();
}

void
PortAudioIO::launch_control_app (int device_id)
{
#ifdef WITH_ASIO
	PaError err = PaAsio_ShowControlPanel (device_id, NULL);

	if (err != paNoError) {
		// error << ?
		DEBUG_AUDIO (string_compose (
		    "Unable to show control panel for device with index %1\n", device_id));
	}
#endif
}

void
PortAudioIO::get_default_sample_rates (std::vector<float>& rates)
{
	rates.push_back(8000.0);
	rates.push_back(22050.0);
	rates.push_back(24000.0);
	rates.push_back(44100.0);
	rates.push_back(48000.0);
	rates.push_back(88200.0);
	rates.push_back(96000.0);
	rates.push_back(176400.0);
	rates.push_back(192000.0);
}

int
PortAudioIO::available_sample_rates(int device_id, std::vector<float>& sampleRates)
{
	if (!pa_initialize()) return -1;

#ifdef WITH_ASIO
	if (get_current_host_api_type() == paASIO) {
		get_default_sample_rates(sampleRates);
		return 0;
	}
#endif

	// TODO use  separate int device_input, int device_output ?!
	if (device_id == DeviceDefault) {
		device_id = get_default_input_device ();
	}

	DEBUG_AUDIO (
	    string_compose ("Querying Samplerates for device %1\n", device_id));

	sampleRates.clear();
	const PaDeviceInfo* nfo = Pa_GetDeviceInfo(device_id);

	if (nfo) {
		PaStreamParameters inputParam;
		PaStreamParameters outputParam;

		inputParam.device = device_id;
		inputParam.channelCount = nfo->maxInputChannels;
		inputParam.sampleFormat = paFloat32;
		inputParam.suggestedLatency = 0;
		inputParam.hostApiSpecificStreamInfo = 0;

		outputParam.device = device_id;
		outputParam.channelCount = nfo->maxOutputChannels;
		outputParam.sampleFormat = paFloat32;
		outputParam.suggestedLatency = 0;
		outputParam.hostApiSpecificStreamInfo = 0;

		std::vector<float> rates;
		get_default_sample_rates(rates);

		for (std::vector<float>::const_iterator i = rates.begin(); i != rates.end();
		     ++i) {
			if (paFormatIsSupported ==
			    Pa_IsFormatSupported(nfo->maxInputChannels > 0 ? &inputParam : NULL,
			                         nfo->maxOutputChannels > 0 ? &outputParam : NULL,
			                         *i)) {
				sampleRates.push_back(*i);
			}
		}
	}

	if (sampleRates.empty()) {
		// fill in something..
		get_default_sample_rates(sampleRates);
	}

	return 0;
}

#ifdef WITH_ASIO
bool
PortAudioIO::get_asio_buffer_properties (int device_id,
                                         long& min_size_samples,
                                         long& max_size_samples,
                                         long& preferred_size_samples,
                                         long& granularity)
{
	// we shouldn't really need all these checks but it shouldn't hurt
	const PaDeviceInfo* device_info = Pa_GetDeviceInfo(device_id);

	if (!device_info) {
		DEBUG_AUDIO (string_compose (
		    "Unable to get device info from device index %1\n", device_id));
		return false;
	}

	if (get_current_host_api_type() != paASIO) {
		DEBUG_AUDIO (string_compose (
		    "ERROR device_id %1 is not an ASIO device\n", device_id));
		return false;
	}

	PaError err = PaAsio_GetAvailableBufferSizes (device_id,
	                                              &min_size_samples,
	                                              &max_size_samples,
	                                              &preferred_size_samples,
	                                              &granularity);

	if (err != paNoError) {
		DEBUG_AUDIO (string_compose (
		    "Unable to determine available buffer sizes for device %1\n", device_id));
		return false;
	}
	return true;
}

static
bool
is_power_of_two (uint32_t v)
{
	return ((v != 0) && !(v & (v - 1)));
}

bool
PortAudioIO::get_asio_buffer_sizes(int device_id,
                                   std::vector<uint32_t>& buffer_sizes,
                                   bool preferred_only)
{
	long min_size_samples = 0;
	long max_size_samples = 0;
	long preferred_size_samples = 0;
	long granularity = 0;

	if (!get_asio_buffer_properties (device_id,
	                                 min_size_samples,
	                                 max_size_samples,
	                                 preferred_size_samples,
	                                 granularity)) {
		DEBUG_AUDIO (string_compose (
		    "Unable to get device buffer properties from device index %1\n", device_id));
		return false;
	}

	DEBUG_AUDIO (string_compose ("ASIO buffer properties for device %1, "
	                             "min_size_samples: %2, max_size_samples: %3, "
	                             "preferred_size_samples: %4, granularity: %5\n",
	                             device_id,
	                             min_size_samples,
	                             max_size_samples,
	                             preferred_size_samples,
	                             granularity));

	bool driver_returns_one_size = (min_size_samples == max_size_samples) &&
	                               (min_size_samples == preferred_size_samples);

	if (preferred_only || driver_returns_one_size) {
		buffer_sizes.push_back(preferred_size_samples);
		return true;
	}

	long buffer_size = min_size_samples;

	// If min size and granularity are power of two then just use values that
	// are power of 2 even if the granularity allows for more values
	bool use_power_of_two =
	    is_power_of_two(min_size_samples) && is_power_of_two(granularity);

	if (granularity <= 0 || use_power_of_two) {
		// driver uses buffer sizes that are power of 2
		while (buffer_size <= max_size_samples) {
			buffer_sizes.push_back(buffer_size);
			buffer_size *= 2;
		}
	} else {
		if (min_size_samples == max_size_samples) {
			// The devices I have tested either return the same values for
			// min/max/preferred and changing buffer size is intended to only be
			// done via the control dialog or they return a range where min != max
			// but I guess min == max could happen if a driver only supports a single
			// buffer size
			buffer_sizes.push_back(min_size_samples);
			return true;
		}

		// If min_size_samples is not power of 2 use at most 8 of the possible
		// buffer sizes spread evenly between min and max
		long max_values = 8;
		while (((max_size_samples - min_size_samples) / granularity) > max_values) {
			granularity *= 2;
		}

		while (buffer_size < max_size_samples) {
			buffer_sizes.push_back(buffer_size);
			buffer_size += granularity;
		}
		buffer_sizes.push_back(max_size_samples);
	}
	return true;
}
#endif

void
PortAudioIO::get_default_buffer_sizes(std::vector<uint32_t>& buffer_sizes)
{
	buffer_sizes.push_back(64);
	buffer_sizes.push_back(128);
	buffer_sizes.push_back(256);
	buffer_sizes.push_back(512);
	buffer_sizes.push_back(1024);
	buffer_sizes.push_back(2048);
	buffer_sizes.push_back(4096);
}

int
PortAudioIO::available_buffer_sizes(int device_id, std::vector<uint32_t>& buffer_sizes)
{
#ifdef WITH_ASIO
	if (get_current_host_api_type() == paASIO) {
		if (get_asio_buffer_sizes (device_id, buffer_sizes, false)) {
			return 0;
		}
	}
#endif

	get_default_buffer_sizes (buffer_sizes);

	return 0;
}

void
PortAudioIO::input_device_list(std::map<int, std::string> &devices) const
{
	for (std::map<int, paDevice*>::const_iterator i = _input_devices.begin ();
	     i != _input_devices.end ();
	     ++i) {
		devices.insert (std::pair<int, std::string>(i->first, Glib::locale_to_utf8(i->second->name)));
	}
}

void
PortAudioIO::output_device_list(std::map<int, std::string> &devices) const
{
	for (std::map<int, paDevice*>::const_iterator i = _output_devices.begin ();
	     i != _output_devices.end ();
	     ++i) {
		devices.insert (std::pair<int, std::string>(i->first, Glib::locale_to_utf8(i->second->name)));
	}
}

bool&
PortAudioIO::pa_initialized()
{
	static bool s_initialized = false;
	return s_initialized;
}

bool
PortAudioIO::pa_initialize()
{
	if (pa_initialized()) return true;

	PaError err = Pa_Initialize();
	if (err != paNoError) {
		return false;
	}
	pa_initialized() = true;

	return true;
}

bool
PortAudioIO::pa_deinitialize()
{
	if (!pa_initialized()) return true;

	PaError err = Pa_Terminate();
	if (err != paNoError) {
		return false;
	}
	pa_initialized() = false;
	return true;
}

void
PortAudioIO::host_api_list (std::vector<std::string>& api_list)
{
	if (!pa_initialize()) return;

	PaHostApiIndex count = Pa_GetHostApiCount();

	if (count < 0) return;

	for (int i = 0; i < count; ++i) {
		const PaHostApiInfo* info = Pa_GetHostApiInfo (i);
		if (info->name != NULL) { // possible?
			api_list.push_back (info->name);
		}
	}
}


PaHostApiTypeId
PortAudioIO::get_current_host_api_type () const
{
	const PaHostApiInfo* info = Pa_GetHostApiInfo (_host_api_index);

	if (info == NULL) {
		DEBUG_AUDIO(string_compose(
		    "Unable to determine Host API type from index %1\n", _host_api_index));
		return (PaHostApiTypeId)0;
	}

	return info->type;
}

std::string
PortAudioIO::get_host_api_name_from_index (PaHostApiIndex index)
{
	std::vector<std::string> api_list;
	host_api_list(api_list);
	return api_list[index];
}

bool
PortAudioIO::set_host_api (const std::string& host_api_name)
{
	PaHostApiIndex new_index = get_host_api_index_from_name (host_api_name);

	if (new_index < 0) {
		DEBUG_AUDIO ("Portaudio: Error setting host API\n");
		return false;
	}
	_host_api_index = new_index;
	_host_api_name = host_api_name;
	return true;
}

PaHostApiIndex
PortAudioIO::get_host_api_index_from_name (const std::string& name)
{
	if (!pa_initialize()) return -1;

	PaHostApiIndex count = Pa_GetHostApiCount();

	if (count < 0) {
		DEBUG_AUDIO ("Host API count < 0\n");
		return -1;
	}

	for (int i = 0; i < count; ++i) {
		const PaHostApiInfo* info = Pa_GetHostApiInfo (i);
		if (info != NULL && info->name != NULL) { // possible?
			if (name == info->name) {
				return i;
			}
		}
	}
	DEBUG_AUDIO (string_compose ("Unable to get host API from name: %1\n", name));

	return -1;
}

PaDeviceIndex
PortAudioIO::get_default_input_device () const
{
	const PaHostApiInfo* info = Pa_GetHostApiInfo (_host_api_index);
	if (info == NULL) return -1;
	return info->defaultInputDevice;
}

PaDeviceIndex
PortAudioIO::get_default_output_device () const
{
	const PaHostApiInfo* info = Pa_GetHostApiInfo (_host_api_index);
	if (info == NULL) return -1;
	return info->defaultOutputDevice;
}

void
PortAudioIO::clear_device_lists ()
{
	for (std::map<int, paDevice*>::const_iterator i = _input_devices.begin (); i != _input_devices.end(); ++i) {
		delete i->second;
	}
	_input_devices.clear();

	for (std::map<int, paDevice*>::const_iterator i = _output_devices.begin (); i != _output_devices.end(); ++i) {
		delete i->second;
	}
	_output_devices.clear();
}

void
PortAudioIO::add_none_devices ()
{
	_input_devices.insert(std::pair<int, paDevice*>(
	    DeviceNone, new paDevice(AudioBackend::get_standard_device_name(AudioBackend::DeviceNone), 0, 0)));
	_output_devices.insert(std::pair<int, paDevice*>(
	    DeviceNone, new paDevice(AudioBackend::get_standard_device_name(AudioBackend::DeviceNone), 0, 0)));
}

void
PortAudioIO::add_default_devices ()
{
	const PaHostApiInfo* info = Pa_GetHostApiInfo (_host_api_index);
	if (info == NULL) return;

	const PaDeviceInfo* nfo_i = Pa_GetDeviceInfo(get_default_input_device());
	const PaDeviceInfo* nfo_o = Pa_GetDeviceInfo(get_default_output_device());
	if (nfo_i && nfo_o) {
		_input_devices.insert (std::pair<int, paDevice*> (DeviceDefault,
					new paDevice(AudioBackend::get_standard_device_name(AudioBackend::DeviceDefault),
						nfo_i->maxInputChannels,
						nfo_o->maxOutputChannels
						)));
		_output_devices.insert (std::pair<int, paDevice*> (DeviceDefault,
					new paDevice(AudioBackend::get_standard_device_name(AudioBackend::DeviceDefault),
						nfo_i->maxInputChannels,
						nfo_o->maxOutputChannels
						)));
	}
}

void
PortAudioIO::add_devices ()
{
	const PaHostApiInfo* info = Pa_GetHostApiInfo (_host_api_index);
	if (info == NULL) return;

	int n_devices = Pa_GetDeviceCount();

	DEBUG_AUDIO (string_compose ("PortAudio found %1 devices\n", n_devices));

	for (int i = 0 ; i < n_devices; ++i) {
		const PaDeviceInfo* nfo = Pa_GetDeviceInfo(i);

		if (!nfo) continue;
		if (nfo->hostApi != _host_api_index) continue;

		DEBUG_AUDIO (string_compose (" (%1) '%2' '%3' in: %4 (lat: %5 .. %6) out: %7 "
		                             "(lat: %8 .. %9) sr:%10\n",
		                             i,
		                             info->name,
		                             nfo->name,
		                             nfo->maxInputChannels,
		                             nfo->defaultLowInputLatency * 1e3,
		                             nfo->defaultHighInputLatency * 1e3,
		                             nfo->maxOutputChannels,
		                             nfo->defaultLowOutputLatency * 1e3,
		                             nfo->defaultHighOutputLatency * 1e3,
		                             nfo->defaultSampleRate));

		if ( nfo->maxInputChannels == 0 && nfo->maxOutputChannels == 0) {
			continue;
		}

		if (nfo->maxInputChannels > 0) {
			_input_devices.insert (std::pair<int, paDevice*> (i, new paDevice(
							nfo->name,
							nfo->maxInputChannels,
							nfo->maxOutputChannels
							)));
		}
		if (nfo->maxOutputChannels > 0) {
			_output_devices.insert (std::pair<int, paDevice*> (i, new paDevice(
							nfo->name,
							nfo->maxInputChannels,
							nfo->maxOutputChannels
							)));
		}
	}
}

bool
PortAudioIO::update_devices()
{
	DEBUG_AUDIO ("Update devices\n");
	if (_stream != NULL) return false;
	pa_deinitialize();
	if (!pa_initialize()) return false;

	clear_device_lists ();

	// ASIO doesn't support separate input/output devices so adding None
	// doesn't make sense
	if (get_current_host_api_type() != paASIO) {
		add_none_devices ();
	}
	add_devices ();
	return true;
}

void
PortAudioIO::reset_stream_dependents ()
{
	_capture_channels = 0;
	_playback_channels = 0;
	_cur_sample_rate = 0;
	_cur_input_latency = 0;
	_cur_output_latency = 0;
}

PaErrorCode
PortAudioIO::close_stream()
{
	if (!_stream) return paNoError;

	PaError err = Pa_CloseStream (_stream);

	if (err != paNoError) {
		return (PaErrorCode)err;
	}
	_stream = NULL;

	reset_stream_dependents();

	free (_input_buffer); _input_buffer = NULL;
	free (_output_buffer); _output_buffer = NULL;
	return paNoError;
}

PaErrorCode
PortAudioIO::start_stream()
{
	PaError err = Pa_StartStream (_stream);

	if (err != paNoError) {
		DEBUG_AUDIO(string_compose("PortAudio failed to start stream %1\n",
		                           Pa_GetErrorText(err)));
		return (PaErrorCode)err;
	}
	return paNoError;
}

bool
PortAudioIO::set_sample_rate_and_latency_from_stream ()
{
	const PaStreamInfo* nfo_s = Pa_GetStreamInfo(_stream);

	if (nfo_s == NULL) {
		return false;
	}

	_cur_sample_rate = nfo_s->sampleRate;
	_cur_input_latency = nfo_s->inputLatency * _cur_sample_rate;
	_cur_output_latency = nfo_s->outputLatency * _cur_sample_rate;

	DEBUG_AUDIO (string_compose ("PA Sample Rate %1 SPS\n", _cur_sample_rate));

	DEBUG_AUDIO (string_compose ("PA Input Latency %1ms, %2 spl\n",
	                             1e3 * nfo_s->inputLatency,
	                             _cur_input_latency));

	DEBUG_AUDIO (string_compose ("PA Output Latency %1ms, %2 spl\n",
	                             1e3 * nfo_s->outputLatency,
	                             _cur_output_latency));
	return true;
}

bool
PortAudioIO::allocate_buffers_for_blocking_api (uint32_t samples_per_period)
{
	if (_capture_channels > 0) {
		_input_buffer =
		    (float*)malloc(samples_per_period * _capture_channels * sizeof(float));
		if (!_input_buffer) {
			DEBUG_AUDIO("PortAudio failed to allocate input buffer.\n");
			return false;
		}
	}

	if (_playback_channels > 0) {
		_output_buffer =
		    (float*)calloc(samples_per_period * _playback_channels, sizeof(float));
		if (!_output_buffer) {
			DEBUG_AUDIO("PortAudio failed to allocate output buffer.\n");
			return false;
		}
	}
	return true;
}

bool
PortAudioIO::get_input_stream_params(int device_input,
                                     PaStreamParameters& inputParam) const
{
	const PaDeviceInfo *nfo_in = NULL;

	if (device_input == DeviceDefault) {
		device_input = get_default_input_device ();
	}

	if (device_input == DeviceNone) {
		return false;
	}

	nfo_in = Pa_GetDeviceInfo(device_input);

	if (nfo_in == NULL) {
		DEBUG_AUDIO ("PortAudio Cannot Query Input Device Info\n");
		return false;
	}

	inputParam.device = device_input;
	inputParam.channelCount = nfo_in->maxInputChannels;
#ifdef INTERLEAVED_INPUT
	inputParam.sampleFormat = paFloat32;
#else
	inputParam.sampleFormat = paFloat32 | paNonInterleaved;
#endif
	if (!inputParam.suggestedLatency) {
		inputParam.suggestedLatency = nfo_in->defaultLowInputLatency;
	}
	inputParam.hostApiSpecificStreamInfo = NULL;

	return true;
}

bool
PortAudioIO::get_output_stream_params(int device_output,
                                      PaStreamParameters& outputParam) const
{
	const PaDeviceInfo *nfo_out = NULL;

	if (device_output == DeviceDefault) {
		device_output = get_default_output_device ();
	}

	if (device_output == DeviceNone) {
		return false;
	}

	nfo_out = Pa_GetDeviceInfo(device_output);

	if (nfo_out == NULL) {
		DEBUG_AUDIO ("PortAudio Cannot Query Output Device Info\n");
		return false;
	}

	outputParam.device = device_output;
	outputParam.channelCount = nfo_out->maxOutputChannels;
#ifdef INTERLEAVED_OUTPUT
	outputParam.sampleFormat = paFloat32;
#else
	outputParam.sampleFormat = paFloat32 | paNonInterleaved;
#endif
	if (!outputParam.suggestedLatency) {
		outputParam.suggestedLatency = nfo_out->defaultLowOutputLatency;
	}
	outputParam.hostApiSpecificStreamInfo = NULL;

	return true;
}

PaErrorCode
PortAudioIO::pre_stream_open(int device_input,
                             PaStreamParameters& inputParam,
                             int device_output,
                             PaStreamParameters& outputParam,
                             uint32_t sample_rate,
                             uint32_t samples_per_period)
{
	if (!pa_initialize()) {
		DEBUG_AUDIO ("PortAudio Initialization Failed\n");
		return paNotInitialized;
	}

	reset_stream_dependents ();

	DEBUG_AUDIO (string_compose (
	    "PortAudio Device IDs: i:%1 o:%2\n", device_input, device_output));

	if (device_input == DeviceNone && device_output == DeviceNone) {
		return paBadIODeviceCombination;
	}

	if ((get_current_host_api_type() == paASIO) && sample_rate) {
		outputParam.suggestedLatency = inputParam.suggestedLatency = ((double)samples_per_period / (double)sample_rate);
	} else {
		outputParam.suggestedLatency = inputParam.suggestedLatency = 0;
	}

	if (get_input_stream_params(device_input, inputParam)) {
		_capture_channels = inputParam.channelCount;
	}

	if (get_output_stream_params(device_output, outputParam)) {
		_playback_channels = outputParam.channelCount;
	}

	if (_capture_channels == 0 && _playback_channels == 0) {
		DEBUG_AUDIO("PortAudio no input or output channels.\n");
		return paBadIODeviceCombination;
	}

	DEBUG_AUDIO (string_compose ("PortAudio Channels: in:%1 out:%2\n",
	                             _capture_channels,
	                             _playback_channels));

	return paNoError;
}

PaErrorCode
PortAudioIO::open_callback_stream(int device_input,
                                  int device_output,
                                  double sample_rate,
                                  uint32_t samples_per_period,
                                  PaStreamCallback* callback,
                                  void* data)
{
	PaStreamParameters inputParam;
	PaStreamParameters outputParam;

	PaErrorCode error_code =
	    pre_stream_open(device_input, inputParam, device_output, outputParam, sample_rate, samples_per_period);

	if (error_code != paNoError) return error_code;

	PaError err = paNoError;

	DEBUG_AUDIO ("Open Callback Stream\n");

	err = Pa_OpenStream(&_stream,
	                    _capture_channels > 0 ? &inputParam : NULL,
	                    _playback_channels > 0 ? &outputParam : NULL,
	                    sample_rate,
	                    samples_per_period,
	                    paDitherOff,
	                    callback,
	                    data);

	if (err != paNoError) {
		DEBUG_AUDIO(string_compose("PortAudio failed to open stream %1\n",
		                           Pa_GetErrorText(err)));
		return paInternalError;
	}

	if (!set_sample_rate_and_latency_from_stream()) {
		DEBUG_AUDIO ("PortAudio failed to query stream information.\n");
		close_stream();
		return paInternalError;
	}

	return paNoError;
}

PaErrorCode
PortAudioIO::open_blocking_stream(int device_input,
                                  int device_output,
                                  double sample_rate,
                                  uint32_t samples_per_period)
{
	PaStreamParameters inputParam;
	PaStreamParameters outputParam;

	PaErrorCode error_code =
	    pre_stream_open(device_input, inputParam, device_output, outputParam, sample_rate, samples_per_period);

	if (error_code != paNoError) return error_code;

	PaError err = paNoError;

	err = Pa_OpenStream (
			&_stream,
			_capture_channels > 0 ? &inputParam: NULL,
			_playback_channels > 0 ? &outputParam: NULL,
			sample_rate,
			samples_per_period,
			paDitherOff,
			NULL, NULL);

	if (err != paNoError) {
		DEBUG_AUDIO(string_compose("PortAudio failed to open stream %1\n",
		                           Pa_GetErrorText(err)));
		return (PaErrorCode)err;
	}

	if (!set_sample_rate_and_latency_from_stream()) {
		DEBUG_AUDIO ("PortAudio failed to query stream information.\n");
		close_stream();
		return paInternalError;
	}

	if (!allocate_buffers_for_blocking_api(samples_per_period)) {
		close_stream();
		return paInternalError;
	}
	return paNoError;
}

int
PortAudioIO::next_cycle (uint32_t n_samples)
{
	bool xrun = false;
	PaError err;
	err = Pa_IsStreamActive (_stream);
	if (err != 1) {
		//   0: inactive / aborted
		// < 0: error
		return -1;
	}

	// TODO, check drift..  process part with larger capacity first.
	// Pa_GetStreamReadAvailable(_stream) < Pa_GetStreamWriteAvailable(_stream)

	if (_playback_channels > 0) {
		err = Pa_WriteStream (_stream, _output_buffer, n_samples);
		if (err) xrun = true;
	}

	if (_capture_channels > 0) {
		err = Pa_ReadStream (_stream, _input_buffer, n_samples);
		if (err) {
			memset (_input_buffer, 0, sizeof(float) * n_samples * _capture_channels);
			xrun = true;
		}
	}


	return xrun ? 1 : 0;
}

std::string
PortAudioIO::get_input_channel_name (int device_id, uint32_t channel) const
{
#ifdef WITH_ASIO
	const char* channel_name;

	// This will return an error for non-ASIO devices so no need to check if
	// the device_id corresponds to an ASIO device.
	PaError err = PaAsio_GetInputChannelName (device_id, channel, &channel_name);

	if (err == paNoError) {
		DEBUG_AUDIO (
		    string_compose ("Input channel name for device %1, channel %2 is %3\n",
		                    device_id,
		                    channel,
		                    channel_name));
		return channel_name;
	}
#endif
	return std::string();
}

std::string
PortAudioIO::get_output_channel_name (int device_id, uint32_t channel) const
{
#ifdef WITH_ASIO
	const char* channel_name;

	PaError err = PaAsio_GetOutputChannelName (device_id, channel, &channel_name);

	if (err == paNoError) {
		DEBUG_AUDIO (
		    string_compose ("Output channel name for device %1, channel %2 is %3\n",
		                    device_id,
		                    channel,
		                    channel_name));
		return channel_name;
	}
#endif
	return std::string();
}

#ifdef INTERLEAVED_INPUT

int
PortAudioIO::get_capture_channel (uint32_t chn, float *input, uint32_t n_samples)
{
	assert(chn < _capture_channels);
	const uint32_t stride = _capture_channels;
	float *ptr = _input_buffer + chn;
	while (n_samples-- > 0) {
		*input++ = *ptr;
		ptr += stride;
	}
	return 0;
}

#else

int
PortAudioIO::get_capture_channel (uint32_t chn, float *input, uint32_t n_samples)
{
	assert(chn < _capture_channels);
	memcpy((void*)input, &(_input_buffer[chn * n_samples]), n_samples * sizeof(float));
	return 0;
}

#endif


#ifdef INTERLEAVED_OUTPUT

int
PortAudioIO::set_playback_channel (uint32_t chn, const float *output, uint32_t n_samples)
{
	assert(chn < _playback_channels);
	const uint32_t stride = _playback_channels;
	float *ptr = _output_buffer + chn;
	while (n_samples-- > 0) {
		*ptr = *output++;
		ptr += stride;
	}
	return 0;
}

#else

int
PortAudioIO::set_playback_channel (uint32_t chn, const float *output, uint32_t n_samples)
{
	assert(chn < _playback_channels);
	memcpy((void*)&(_output_buffer[chn * n_samples]), (void*)output, n_samples * sizeof(float));
	return 0;
}

#endif
