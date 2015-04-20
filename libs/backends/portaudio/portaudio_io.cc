/*
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "portaudio_io.h"

#define INTERLEAVED_INPUT
#define INTERLEAVED_OUTPUT

using namespace ARDOUR;

PortAudioIO::PortAudioIO ()
	: _state (-1)
	, _initialized (false)
	, _capture_channels (0)
	, _playback_channels (0)
	, _stream (0)
	, _input_buffer (0)
	, _output_buffer (0)
	, _cur_sample_rate (0)
	, _cur_input_latency (0)
	, _cur_output_latency (0)
{
}

PortAudioIO::~PortAudioIO ()
{
	if (_state == 0) {
		pcm_stop();
	}
	if (_initialized) {
		Pa_Terminate();
	}

	for (std::map<int, paDevice*>::const_iterator i = _devices.begin (); i != _devices.end(); ++i) {
		delete i->second;
	}
	_devices.clear();

	free (_input_buffer); _input_buffer = NULL;
	free (_output_buffer); _output_buffer = NULL;
}


int
PortAudioIO::available_sample_rates(int device_id, std::vector<float>& sampleRates)
{
	static const float ardourRates[] = { 8000.0, 22050.0, 24000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};

	assert(_initialized);

	// TODO use  separate int device_input, int device_output ?!
	if (device_id == -1) {
		device_id = Pa_GetDefaultInputDevice();
	}
#ifndef NDEBUG
	printf("PortAudio: Querying Samplerates for device %d\n", device_id);
#endif

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

		for (uint32_t i = 0; i < sizeof(ardourRates)/sizeof(float); ++i) {
			if (paFormatIsSupported == Pa_IsFormatSupported(
						nfo->maxInputChannels > 0 ? &inputParam : NULL,
						nfo->maxOutputChannels > 0 ? &outputParam : NULL,
						ardourRates[i])) {
				sampleRates.push_back (ardourRates[i]);
			}
		}
	}

	if (sampleRates.empty()) {
		// fill in something..
		sampleRates.push_back (44100.0);
		sampleRates.push_back (48000.0);
	}

	return 0;
}

int
PortAudioIO::available_buffer_sizes(int device_id, std::vector<uint32_t>& bufferSizes)
{
	// TODO
	static const uint32_t ardourSizes[] = { 64, 128, 256, 512, 1024, 2048, 4096 };
	for(uint32_t i = 0; i < sizeof(ardourSizes)/sizeof(uint32_t); ++i) {
		bufferSizes.push_back (ardourSizes[i]);
	}
	return 0;
}

void
PortAudioIO::device_list (std::map<int, std::string> &devices) const {
	devices.clear();
	for (std::map<int, paDevice*>::const_iterator i = _devices.begin (); i != _devices.end(); ++i) {
		devices.insert (std::pair<int, std::string> (i->first, i->second->name));
	}
}

void
PortAudioIO::discover()
{
	for (std::map<int, paDevice*>::const_iterator i = _devices.begin (); i != _devices.end(); ++i) {
		delete i->second;
	}
	_devices.clear();
	
	PaError err = paNoError;

	if (!_initialized) {
		err = Pa_Initialize();
	}
	if (err != paNoError) {
		return;
	}

	_initialized = true;

	{
		const PaDeviceInfo* nfo_i = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice());
		const PaDeviceInfo* nfo_o = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice());
		if (nfo_i && nfo_o) {
			_devices.insert (std::pair<int, paDevice*> (-1,
						new paDevice("Default",
							nfo_i->maxInputChannels,
							nfo_o->maxOutputChannels
							)));
		}
	}

	int n_devices = Pa_GetDeviceCount();
#ifndef NDEBUG
	printf("PortAudio %d devices found:\n", n_devices);
#endif

	for (int i = 0 ; i < n_devices; ++i) {
		const PaDeviceInfo* nfo = Pa_GetDeviceInfo(i);
		if (!nfo) continue;
#ifndef NDEBUG
		printf(" (%d) '%s' in: %d (lat: %.1f .. %.1f) out: %d (lat: %.1f .. %.1f) sr:%.2f\n",
				i, nfo->name,
				nfo->maxInputChannels,
				nfo->defaultLowInputLatency * 1e3,
				nfo->defaultHighInputLatency * 1e3,
				nfo->maxOutputChannels,
				nfo->defaultLowOutputLatency * 1e3,
				nfo->defaultHighOutputLatency * 1e3,
				nfo->defaultSampleRate);
#endif
		if ( nfo->maxInputChannels == 0 && nfo->maxOutputChannels == 0) {
			continue;
		}
		_devices.insert (std::pair<int, paDevice*> (i, new paDevice(
						nfo->name,
						nfo->maxInputChannels,
						nfo->maxOutputChannels
						)));
	}
}

void
PortAudioIO::pcm_stop ()
{
	if (_stream) {
		Pa_CloseStream (_stream);
	}
	_stream = NULL;

	_capture_channels = 0;
	_playback_channels = 0;
	_cur_sample_rate = 0;
	_cur_input_latency = 0;
	_cur_output_latency = 0;

	free (_input_buffer); _input_buffer = NULL;
	free (_output_buffer); _output_buffer = NULL;
	_state = -1;
}

int
PortAudioIO::pcm_start()
{
	PaError err = Pa_StartStream (_stream);

	if (err != paNoError) {
		_state = -1;
		return -1;
	}
	return 0;
}

#ifdef __APPLE__
static uint32_t lower_power_of_two (uint32_t v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v >> 1;
}
#endif

int
PortAudioIO::pcm_setup (
		int device_input, int device_output,
		double sample_rate, uint32_t samples_per_period)
{
	_state = -2;

	// TODO error reporting sans fprintf()

	PaError err = paNoError;
	const PaDeviceInfo *nfo_in;
	const PaDeviceInfo *nfo_out;
	const PaStreamInfo *nfo_s;
		
	if (!_initialized) {
		err = Pa_Initialize();
	}
	if (err != paNoError) {
		fprintf(stderr, "PortAudio Initialization Failed\n");
		goto error;
	}
	_initialized = true;


	if (device_input == -1) {
		device_input = Pa_GetDefaultInputDevice();
	}
	if (device_output == -1) {
		device_output = Pa_GetDefaultOutputDevice();
	}

	_capture_channels = 0;
	_playback_channels = 0;
	_cur_sample_rate = 0;
	_cur_input_latency = 0;
	_cur_output_latency = 0;

#ifndef NDEBUG
	printf("PortAudio Device IDs: i:%d o:%d\n", device_input, device_output);
#endif

	nfo_in = Pa_GetDeviceInfo(device_input);
	nfo_out = Pa_GetDeviceInfo(device_output);

	if (!nfo_in && ! nfo_out) {
		fprintf(stderr, "PortAudio Cannot Query Device Info\n");
		goto error;
	}

	if (nfo_in) {
		_capture_channels = nfo_in->maxInputChannels;
	}
	if (nfo_out) {
		_playback_channels = nfo_out->maxOutputChannels;
	}

	if(_capture_channels == 0 && _playback_channels == 0) {
		fprintf(stderr, "PortAudio no Input and no output channels.\n");
		goto error;
	}


#ifdef __APPLE__
	// pa_mac_core_blocking.c pa_stable_v19_20140130
	// BUG: ringbuffer alloc requires power-of-two chn count.
	if ((_capture_channels & (_capture_channels - 1)) != 0) {
		printf("Adjusted capture channes to power of two (portaudio rb bug)\n");
		_capture_channels = lower_power_of_two (_capture_channels);
	}

	if ((_playback_channels & (_playback_channels - 1)) != 0) {
		printf("Adjusted capture channes to power of two (portaudio rb bug)\n");
		_playback_channels = lower_power_of_two (_playback_channels);
	}
#endif
	
#ifndef NDEBUG
	printf("PortAudio Channels: in:%d out:%d\n",
			_capture_channels, _playback_channels);
#endif

	PaStreamParameters inputParam;
	PaStreamParameters outputParam;

	if (nfo_in) {
		inputParam.device = device_input;
		inputParam.channelCount = _capture_channels;
#ifdef INTERLEAVED_INPUT
		inputParam.sampleFormat = paFloat32;
#else
		inputParam.sampleFormat = paFloat32 | paNonInterleaved;
#endif
		inputParam.suggestedLatency = nfo_in->defaultLowInputLatency;
		inputParam.hostApiSpecificStreamInfo = NULL;
	}

	if (nfo_out) {
		outputParam.device = device_output;
		outputParam.channelCount = _playback_channels;
#ifdef INTERLEAVED_OUTPUT
		outputParam.sampleFormat = paFloat32;
#else
		outputParam.sampleFormat = paFloat32 | paNonInterleaved;
#endif
		outputParam.suggestedLatency = nfo_out->defaultLowOutputLatency;
		outputParam.hostApiSpecificStreamInfo = NULL;
	}

	// XXX re-consider using callback API, testing needed.
	err = Pa_OpenStream (
			&_stream,
			_capture_channels > 0 ? &inputParam: NULL,
			_playback_channels > 0 ? &outputParam: NULL,
			sample_rate,
			samples_per_period,
			paClipOff | paDitherOff,
			NULL, NULL);

	if (err != paNoError) {
		fprintf(stderr, "PortAudio failed to start stream.\n");
		goto error;
	}

	nfo_s = Pa_GetStreamInfo (_stream);
	if (!nfo_s) {
		fprintf(stderr, "PortAudio failed to query stream information.\n");
		pcm_stop();
		goto error;
	}

	_cur_sample_rate = nfo_s->sampleRate;
	_cur_input_latency = nfo_s->inputLatency * _cur_sample_rate;
	_cur_output_latency = nfo_s->outputLatency * _cur_sample_rate;

#ifndef NDEBUG
	printf("PA Sample Rate  %.1f SPS\n", _cur_sample_rate);
	printf("PA Input Latency  %.1fms  %d spl\n", 1e3 * nfo_s->inputLatency, _cur_input_latency);
	printf("PA Output Latency %.1fms  %d spl\n", 1e3 * nfo_s->outputLatency, _cur_output_latency);
#endif

	_state = 0;

	if (_capture_channels > 0) {
		_input_buffer = (float*) malloc (samples_per_period * _capture_channels * sizeof(float));
		if (!_input_buffer) {
			fprintf(stderr, "PortAudio failed to allocate input buffer.\n");
			pcm_stop();
			goto error;
		}
	}

	if (_playback_channels > 0) {
		_output_buffer = (float*) calloc (samples_per_period * _playback_channels, sizeof(float));
		if (!_output_buffer) {
			fprintf(stderr, "PortAudio failed to allocate output buffer.\n");
			pcm_stop();
			goto error;
		}
	}

	return 0;

error:
	_capture_channels = 0;
	_playback_channels = 0;
	free (_input_buffer); _input_buffer = NULL;
	free (_output_buffer); _output_buffer = NULL;
	Pa_Terminate();
	return -1;
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
