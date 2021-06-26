/*
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#include <glibmm.h>
#include "pbd/timing.h"
#include "coreaudio_pcmio.h"

using namespace ARDOUR;

/* abstraction for deprecated CoreAudio */

static OSStatus GetPropertyWrapper (
		AudioDeviceID id, UInt32 elem, bool input, AudioDevicePropertyID prop, UInt32* size, void * data)
{
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = prop;
	switch (prop) {
		case kAudioDevicePropertyBufferFrameSize:
		case kAudioDevicePropertyBufferFrameSizeRange:
			property_address.mScope = kAudioObjectPropertyScopeGlobal;
			break;
		default:
			property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
			break;
	}
	property_address.mElement = kAudioObjectPropertyElementMaster;
	return AudioObjectGetPropertyData(id, &property_address, elem, NULL, size, data);
#else
	return AudioDeviceGetProperty(id, elem, input, prop, size, data);
#endif
}

static OSStatus SetPropertyWrapper (
		AudioDeviceID id, const AudioTimeStamp* when, UInt32 chn, bool input, AudioDevicePropertyID prop, UInt32 size, void * data)
{
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = prop;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	return AudioObjectSetPropertyData (id, &property_address, 0, NULL, size, data);
#else
	return AudioDeviceSetProperty (id, when, chn, input, prop, size, data);
#endif
}

static OSStatus GetHardwarePropertyInfoWrapper (AudioDevicePropertyID prop, UInt32* size)
{
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = prop;
	property_address.mScope = kAudioObjectPropertyScopeGlobal;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	return AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property_address, 0, NULL, size);
#else
	Boolean outWritable;
	return AudioHardwareGetPropertyInfo(prop, size, &outWritable);
#endif
}

static OSStatus GetHardwarePropertyWrapper (AudioDevicePropertyID prop, UInt32* size, void *d)
{
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = prop;
	property_address.mScope = kAudioObjectPropertyScopeGlobal;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	return AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0, NULL, size, d);
#else
	return AudioHardwareGetProperty (prop, size, d);
#endif
}

static OSStatus GetPropertyInfoWrapper (AudioDeviceID id, UInt32 elem, bool input, AudioDevicePropertyID prop, UInt32* size)
{
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = prop;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
	property_address.mElement = elem;
	return AudioObjectGetPropertyDataSize(id, &property_address, 0, NULL, size);
#else
	Boolean outWritable;
	return AudioDeviceGetPropertyInfo(id, elem, input, prop, size, &outWritable);
#endif
}

static OSStatus GetDeviceNameFromID(AudioDeviceID id, char* name)
{
    UInt32 size = 256;
    return GetPropertyWrapper (id, 0, 0, kAudioDevicePropertyDeviceName, &size, name);
}

static CFStringRef GetDeviceName(AudioDeviceID id)
{
    UInt32 size = sizeof(CFStringRef);
    CFStringRef UIname;
    OSStatus err = GetPropertyWrapper (id, 0, 0, kAudioDevicePropertyDeviceUID, &size, &UIname);
    return (err == noErr) ? UIname : NULL;
}

///////////////////////////////////////////////////////////////////////////////

#include "coreaudio_pcmio_aggregate.cc"

/* callbacks */

#ifdef COREAUDIO_108

static OSStatus property_callback_ptr (AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void* arg) {
	CoreAudioPCM * self = static_cast<CoreAudioPCM*>(arg);
	for (UInt32 i = 0; i < inNumberAddresses; ++i) {
		switch (inAddresses[i].mSelector) {
			case kAudioHardwarePropertyDevices:
				self->hw_changed_callback();
				break;
			case kAudioDeviceProcessorOverload:
				self->xrun_callback();
				break;
			case kAudioDevicePropertyBufferFrameSize:
				self->buffer_size_callback();
				break;
			case kAudioDevicePropertyNominalSampleRate:
				self->sample_rate_callback();
				break;
			default:
				break;
		}
	}
	return noErr;
}

#else

static OSStatus hw_changed_callback_ptr (AudioHardwarePropertyID inPropertyID, void* arg) {
	if (inPropertyID == kAudioHardwarePropertyDevices) {
		CoreAudioPCM * self = static_cast<CoreAudioPCM*>(arg);
		self->hw_changed_callback();
	}
	return noErr;
}

static OSStatus property_callback_ptr (
		AudioDeviceID inDevice,
		UInt32 inChannel,
		Boolean isInput,
		AudioDevicePropertyID inPropertyID,
		void* inClientData)
{
	CoreAudioPCM * d = static_cast<CoreAudioPCM*> (inClientData);
	switch (inPropertyID) {
		case kAudioDeviceProcessorOverload:
			d->xrun_callback();
			break;
		case kAudioDevicePropertyBufferFrameSize:
			d->buffer_size_callback();
			break;
		case kAudioDevicePropertyNominalSampleRate:
			d->sample_rate_callback();
			break;
	}
	return noErr;
}

#endif

static OSStatus render_callback_ptr (
		void* inRefCon,
		AudioUnitRenderActionFlags* ioActionFlags,
		const AudioTimeStamp* inTimeStamp,
		UInt32 inBusNumber,
		UInt32 inNumberSamples,
		AudioBufferList* ioData)
{
	CoreAudioPCM * d = static_cast<CoreAudioPCM*> (inRefCon);
	return d->render_callback(ioActionFlags, inTimeStamp, inBusNumber, inNumberSamples, ioData);
}


static OSStatus add_listener (AudioDeviceID id, AudioDevicePropertyID selector, void *arg) {
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = selector;
	property_address.mScope = kAudioObjectPropertyScopeGlobal;
	property_address.mElement = 0;
	return AudioObjectAddPropertyListener(id, &property_address, property_callback_ptr, arg);
#else
        return AudioDeviceAddPropertyListener(id, 0, true, selector, property_callback_ptr, arg);
#endif
}


///////////////////////////////////////////////////////////////////////////////

CoreAudioPCM::CoreAudioPCM ()
	: _auhal (0)
	, _device_ids (0)
	, _input_audio_buffer_list (0)
	, _active_device_id (0)
	, _aggregate_device_id (0)
	, _aggregate_plugin_id (0)
	, _state (-1)
	, _capture_channels (0)
	, _playback_channels (0)
	, _in_process (false)
	, _n_devices (0)
	, _process_callback (0)
	, _error_callback (0)
	, _hw_changed_callback (0)
	, _xrun_callback (0)
	, _buffer_size_callback (0)
	, _sample_rate_callback (0)
	, _device_ins (0)
	, _device_outs (0)
{
	pthread_mutex_init (&_discovery_lock, 0);

#ifdef COREAUDIO_108
	CFRunLoopRef theRunLoop = NULL;
	AudioObjectPropertyAddress property = { kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioHardwarePropertyDevices };
	AudioObjectSetPropertyData (kAudioObjectSystemObject, &property, 0, NULL, sizeof(CFRunLoopRef), &theRunLoop);

	property.mSelector = kAudioHardwarePropertyDevices;
	property.mScope = kAudioObjectPropertyScopeGlobal;
	property.mElement = 0;
	AudioObjectAddPropertyListener(kAudioObjectSystemObject, &property, property_callback_ptr, this);
#else
	AudioHardwareAddPropertyListener (kAudioHardwarePropertyDevices, hw_changed_callback_ptr, this);
#endif
}

CoreAudioPCM::~CoreAudioPCM ()
{
	if (_state == 0) {
		pcm_stop();
	}
	delete _device_ids;
	free(_device_ins);
	free(_device_outs);
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress prop;
	prop.mSelector = kAudioHardwarePropertyDevices;
	prop.mScope = kAudioObjectPropertyScopeGlobal;
	prop.mElement = 0;
	AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop, &property_callback_ptr, this);
#else
	AudioHardwareRemovePropertyListener(kAudioHardwarePropertyDevices, hw_changed_callback_ptr);
#endif
	free(_input_audio_buffer_list);
	pthread_mutex_destroy (&_discovery_lock);
}


void
CoreAudioPCM::hw_changed_callback() {
#ifndef NDEBUG
	printf("CoreAudio HW change..\n");
#endif
	discover();
	if (_hw_changed_callback) {
		_hw_changed_callback(_hw_changed_arg);
	}
}


int
CoreAudioPCM::available_sample_rates(uint32_t device_id, std::vector<float>& sampleRates)
{
	OSStatus err;
	UInt32 size = 0;
	sampleRates.clear();

	if (device_id >= _n_devices) {
		return -1;
	}

	err = GetPropertyInfoWrapper (_device_ids[device_id], 0, false, kAudioDevicePropertyAvailableNominalSampleRates, &size);

	if (err != kAudioHardwareNoError) {
		return -1;
	}

	uint32_t numRates = size / sizeof(AudioValueRange);
	AudioValueRange* supportedRates = new AudioValueRange[numRates];

	err = GetPropertyWrapper (_device_ids[device_id], 0, false, kAudioDevicePropertyAvailableNominalSampleRates, &size, supportedRates);

	if (err != kAudioHardwareNoError) {
		delete [] supportedRates;
		return -1;
	}

	static const float ardourRates[] = { 8000.0, 22050.0, 24000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};

	for(uint32_t i = 0; i < sizeof(ardourRates)/sizeof(float); ++i) {
		for(uint32_t j = 0; j < numRates; ++j) {
			if ((supportedRates[j].mMinimum <= ardourRates[i]) &&
					(supportedRates[j].mMaximum >= ardourRates[i])) {
				sampleRates.push_back (ardourRates[i]);
				break;
			}
		}
	}

	delete [] supportedRates;
	return 0;
}

int
CoreAudioPCM::available_buffer_sizes(uint32_t device_id, std::vector<uint32_t>& bufferSizes)
{
	OSStatus err;
	UInt32 size = 0;
	bufferSizes.clear();

	if (device_id >= _n_devices) {
		return -1;
	}

	AudioValueRange supportedRange;
	size = sizeof (AudioValueRange);

	err = GetPropertyWrapper (_device_ids[device_id], 0, 0, kAudioDevicePropertyBufferFrameSizeRange, &size, &supportedRange);
	if (err != noErr) {
		return -1;
	}

	static const uint32_t ardourSizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };

	for(uint32_t i = 0; i < sizeof(ardourSizes)/sizeof(uint32_t); ++i) {
		if ((supportedRange.mMinimum <= ardourSizes[i]) &&
				(supportedRange.mMaximum >= ardourSizes[i])) {
			bufferSizes.push_back (ardourSizes[i]);
		}
	}

	if (bufferSizes.empty()) {
		bufferSizes.push_back ((uint32_t)supportedRange.mMinimum);
		bufferSizes.push_back ((uint32_t)supportedRange.mMaximum);
	}
	return 0;
}

uint32_t
CoreAudioPCM::available_channels(uint32_t device_id, bool input)
{
	OSStatus err;
	UInt32 size = 0;
	AudioBufferList *bufferList = NULL;
	uint32_t channel_count = 0;

	if (device_id >= _n_devices) {
		return 0;
	}

	/* query number of inputs */
	err = GetPropertyInfoWrapper (_device_ids[device_id], 0, input, kAudioDevicePropertyStreamConfiguration, &size);
	if (kAudioHardwareNoError != err) {
		fprintf(stderr, "CoreaAudioPCM: kAudioDevicePropertyStreamConfiguration failed\n");
		return 0;
	}

	bufferList = (AudioBufferList *)(malloc(size));
	assert(bufferList);
	if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); return 0; }
	bufferList->mNumberBuffers = 0;
	err = GetPropertyWrapper (_device_ids[device_id], 0, input, kAudioDevicePropertyStreamConfiguration, &size, bufferList);

	if(kAudioHardwareNoError != err) {
		fprintf(stderr, "CoreaAudioPCM: kAudioDevicePropertyStreamConfiguration failed\n");
		free(bufferList);
		return 0;
	}

	for(UInt32 j = 0; j < bufferList->mNumberBuffers; ++j) {
		channel_count += bufferList->mBuffers[j].mNumberChannels;
	}
	free(bufferList);
	return channel_count;
}

void
CoreAudioPCM::get_stream_latencies(uint32_t device_id, bool input, std::vector<uint32_t>& latencies)
{
	OSStatus err;
	UInt32 size = 0;

	if (device_id >= _n_devices) {
		return;
	}

	err = GetPropertyInfoWrapper (_device_ids[device_id], 0, input, kAudioDevicePropertyStreams, &size);
	if (err != noErr) { return; }

	uint32_t stream_count = size / sizeof(UInt32);
	AudioStreamID streamIDs[stream_count];

	err = GetPropertyWrapper (_device_ids[device_id], 0, input, kAudioDevicePropertyStreams, &size, &streamIDs);
	if (err != noErr) {
		fprintf(stderr, "GetStreamLatencies kAudioDevicePropertyStreams\n");
		return;
	}

	for (uint32_t i = 0; i < stream_count; i++) {
		UInt32 stream_latency;
		size = sizeof(UInt32);
#ifdef COREAUDIO_108
		AudioObjectPropertyAddress property_address;
		property_address.mSelector = kAudioDevicePropertyStreams;
		property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
		property_address.mElement = i; // ??
		err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &stream_latency);
#else
		err = AudioStreamGetProperty(streamIDs[i], input, kAudioStreamPropertyLatency, &size, &stream_latency);
#endif
		if (err != noErr) {
			fprintf(stderr, "GetStreamLatencies kAudioStreamPropertyLatency\n");
			return;
		}
#ifndef NDEBUG
		printf("  ^ Stream %u latency: %u\n", (unsigned int)i, (unsigned int)stream_latency);
#endif
		latencies.push_back(stream_latency);
	}
}

uint32_t
CoreAudioPCM::get_latency(uint32_t device_id, bool input)
{
	OSStatus err;
	uint32_t latency = 0;
	UInt32 size = sizeof(UInt32);
	UInt32 lat0 = 0;
	UInt32 latS = 0;

	if (device_id >= _n_devices) {
		return 0;
	}

	err = GetPropertyWrapper (_device_ids[device_id], 0, input, kAudioDevicePropertyLatency, &size, &lat0);
	if (err != kAudioHardwareNoError) {
		fprintf(stderr, "GetLatency kAudioDevicePropertyLatency\n");
	}

	err = GetPropertyWrapper (_device_ids[device_id], 0, input, kAudioDevicePropertySafetyOffset, &size, &latS);
	if (err != kAudioHardwareNoError) {
		fprintf(stderr, "GetLatency kAudioDevicePropertySafetyOffset\n");
	}

#ifndef NDEBUG
	printf("%s Latency systemic+safetyoffset = %u + %u\n",
			input ? "Input" : "Output", (unsigned int)lat0, (unsigned int)latS);
#endif
	latency = lat0 + latS;

	uint32_t max_stream_latency = 0;
	std::vector<uint32_t> stream_latencies;
	get_stream_latencies(device_id, input, stream_latencies);
	for (size_t i = 0; i < stream_latencies.size(); ++i) {
		max_stream_latency = std::max(max_stream_latency, stream_latencies[i]);
	}
#if 0
	latency += max_stream_latency;
#endif

	return latency;
}

uint32_t
CoreAudioPCM::get_latency(bool input)
{
	if (_active_device_id == 0) {
		return 0;
	}
	return get_latency (_active_device_id, input);
}

uint32_t
CoreAudioPCM::current_buffer_size_id(AudioDeviceID id) {
	UInt32 buffer_size;
	UInt32 size = sizeof(UInt32);
	OSStatus err;
	err = GetPropertyWrapper (id, 0, 0, kAudioDevicePropertyBufferFrameSize, &size, &buffer_size);
	if (err != noErr) {
		return _samples_per_period;
	}
	return buffer_size;
}


float
CoreAudioPCM::current_sample_rate_id(AudioDeviceID id, bool input) {
	OSStatus err;
	UInt32 size = 0;
	Float64 rate;
	size = sizeof (rate);

	err = GetPropertyWrapper(id, 0, input, kAudioDevicePropertyNominalSampleRate, &size, &rate);
	if (err == noErr) {
		return rate;
	}
	return 0;
}

float
CoreAudioPCM::current_sample_rate(uint32_t device_id, bool input) {
	if (device_id >= _n_devices) {
		return -1;
	}
	return current_sample_rate_id(_device_ids[device_id], input);
}

float
CoreAudioPCM::sample_rate() {
	if (_active_device_id == 0) {
		return 0;
	}
	return current_sample_rate_id(_active_device_id, _playback_channels > 0 ? false : true);
}

int
CoreAudioPCM::set_device_sample_rate_id (AudioDeviceID id, float rate, bool input)
{
	std::vector<int>::iterator intIter;
	OSStatus err;
	UInt32 size = 0;

	if (current_sample_rate_id(id, input) == rate) {
		return 0;
	}

	Float64 newNominalRate = rate;
	size = sizeof (Float64);

	err = SetPropertyWrapper(id, NULL, 0, input, kAudioDevicePropertyNominalSampleRate, size, &newNominalRate);
	if (err != noErr) {
		fprintf(stderr, "CoreAudioPCM: failed to set samplerate\n");
		return 0;
	}

	int timeout = 3000; // 3 sec
	while (--timeout > 0) {
		if (current_sample_rate_id(id, input) == rate) {
			break;
		}
		Glib::usleep (1000);
	}
	fprintf(stderr, "CoreAudioPCM: CoreAudio: Setting SampleRate took %d ms.\n", (3000 - timeout));

	if (timeout == 0) {
		fprintf(stderr, "CoreAudioPCM: CoreAudio: Setting SampleRate timed out.\n");
		return -1;
	}

	return 0;
}

int
CoreAudioPCM::set_device_sample_rate (uint32_t device_id, float rate, bool input)
{
	if (device_id >= _n_devices) {
		return 0;
	}
	return set_device_sample_rate_id(_device_ids[device_id], rate, input);
}

void
CoreAudioPCM::discover()
{
	OSStatus err;
	UInt32 size = 0;

	if (pthread_mutex_trylock (&_discovery_lock)) {
		return;
	}

	if (_device_ids) {
		delete _device_ids; _device_ids = 0;
		free(_device_ins); _device_ins = 0;
		free(_device_outs); _device_outs = 0;
	}
	_devices.clear();
	_input_devices.clear();
	_output_devices.clear();
	_duplex_devices.clear();

	err = GetHardwarePropertyInfoWrapper (kAudioHardwarePropertyDevices, &size);

	_n_devices = size / sizeof (AudioDeviceID);
	size = _n_devices * sizeof (AudioDeviceID);

	_device_ids = new AudioDeviceID[_n_devices];
	_device_ins = (uint32_t*) calloc(_n_devices, sizeof(uint32_t));
	_device_outs = (uint32_t*) calloc(_n_devices, sizeof(uint32_t));

	assert(_device_ins && _device_outs && _device_ids);
	if (!_device_ins || !_device_ins || !_device_ids) {
		fprintf(stderr, "OUT OF MEMORY\n");
		_device_ids = 0;
		_device_ins = 0;
		_device_outs = 0;
		pthread_mutex_unlock (&_discovery_lock);
		return;
	}

	err = GetHardwarePropertyWrapper (kAudioHardwarePropertyDevices, &size, _device_ids);

	for (size_t idx = 0; idx < _n_devices; ++idx) {
		size = 64;
		char deviceName[64];
		err = GetPropertyWrapper (_device_ids[idx], 0, 0, kAudioDevicePropertyDeviceName, &size, deviceName);

		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "CoreAudioPCM: device name query failed\n");
			continue;
		}

		UInt32 inputChannelCount = available_channels(idx, true);
		UInt32 outputChannelCount = available_channels(idx, false);

		{
			std::string dn = deviceName;
			_device_ins[idx] = inputChannelCount;
			_device_outs[idx] = outputChannelCount;
#ifndef NDEBUG
			printf("CoreAudio Device: #%ld (id:%lu) '%s' in:%u out:%u\n", idx,
					(long unsigned int)_device_ids[idx],
					deviceName,
					(unsigned int)inputChannelCount, (unsigned int)outputChannelCount);
#endif
			if (outputChannelCount > 0 || inputChannelCount > 0) {
				_devices.insert (std::pair<size_t, std::string> (idx, dn));
			}
			if (inputChannelCount > 0) {
				_input_devices.insert (std::pair<size_t, std::string> (idx, dn));
			}
			if (outputChannelCount > 0) {
				_output_devices.insert (std::pair<size_t, std::string> (idx, dn));
			}
			if (outputChannelCount > 0 && inputChannelCount > 0) {
				_duplex_devices.insert (std::pair<size_t, std::string> (idx, dn));
			}
		}
	}
	pthread_mutex_unlock (&_discovery_lock);
}

void
CoreAudioPCM::xrun_callback ()
{
#ifndef NDEBUG
	printf("Coreaudio XRUN\n");
#endif
	if (_xrun_callback) {
		_xrun_callback(_xrun_arg);
	}
}

void
CoreAudioPCM::buffer_size_callback ()
{
	_samples_per_period = current_buffer_size_id(_active_device_id);

	if (_buffer_size_callback) {
		_buffer_size_callback(_buffer_size_arg);
	}
}

void
CoreAudioPCM::sample_rate_callback ()
{
#ifndef NDEBUG
	printf("Sample Rate Changed!\n");
#endif
	if (_sample_rate_callback) {
		_sample_rate_callback(_sample_rate_arg);
	}
}

void
CoreAudioPCM::pcm_stop ()
{
	if (!_auhal) return;

	AudioOutputUnitStop(_auhal);
	if (_state == 0) {
#ifdef COREAUDIO_108
		AudioObjectPropertyAddress prop;
		prop.mScope = kAudioObjectPropertyScopeGlobal;
		prop.mElement = 0;
		if (_active_device_id > 0) {
			prop.mSelector = kAudioDeviceProcessorOverload;
			AudioObjectRemovePropertyListener(_active_device_id, &prop, &property_callback_ptr, this);
			prop.mSelector = kAudioDevicePropertyBufferFrameSize;
			AudioObjectRemovePropertyListener(_active_device_id, &prop, &property_callback_ptr, this);
			prop.mSelector = kAudioDevicePropertyNominalSampleRate;
			AudioObjectRemovePropertyListener(_active_device_id, &prop, &property_callback_ptr, this);
		}
#else
		if (_active_device_id > 0) {
			AudioDeviceRemovePropertyListener(_active_device_id, 0, true, kAudioDeviceProcessorOverload, property_callback_ptr);
			AudioDeviceRemovePropertyListener(_active_device_id, 0, true, kAudioDevicePropertyBufferFrameSize, property_callback_ptr);
			AudioDeviceRemovePropertyListener(_active_device_id, 0, true, kAudioDevicePropertyNominalSampleRate, property_callback_ptr);
		}
#endif
	}
	if (_aggregate_plugin_id) {
		destroy_aggregate_device();
		discover();
	}

	AudioUnitUninitialize(_auhal);
#ifdef COREAUDIO_108
	AudioComponentInstanceDispose(_auhal);
#else
	CloseComponent(_auhal);
#endif
	_auhal = 0;
	_state = -1;
	_capture_channels = 0;
	_playback_channels = 0;
	_aggregate_plugin_id = 0;
	_aggregate_device_id = 0;
	_active_device_id = 0;

	free(_input_audio_buffer_list);
	_input_audio_buffer_list = 0;

	_input_names.clear();
	_output_names.clear();

	_error_callback = 0;
	_process_callback = 0;
	_xrun_callback = 0;
}

#ifndef NDEBUG
static void PrintStreamDesc (AudioStreamBasicDescription *inDesc)
{
	printf ("- - - - - - - - - - - - - - - - - - - -\n");
	printf ("  Sample Rate:%.2f",        inDesc->mSampleRate);
	printf ("  Format ID:%.*s\n",        (int)sizeof(inDesc->mFormatID), (char*)&inDesc->mFormatID);
	printf ("  Format Flags:%X\n",       (unsigned int)inDesc->mFormatFlags);
	printf ("  Bytes per Packet:%d\n",   (int)inDesc->mBytesPerPacket);
	printf ("  Frames per Packet:%d\n",  (int)inDesc->mFramesPerPacket);
	printf ("  Bytes per Frame:%d\n",    (int)inDesc->mBytesPerFrame);
	printf ("  Channels per Frame:%d\n", (int)inDesc->mChannelsPerFrame);
	printf ("  Bits per Channel:%d\n",   (int)inDesc->mBitsPerChannel);
	printf ("- - - - - - - - - - - - - - - - - - - -\n");
}
#endif

int
CoreAudioPCM::set_device_buffer_size_id (AudioDeviceID id, uint32_t samples_per_period)
{
	OSStatus err;
	UInt32 uint32val;

	uint32val = samples_per_period;
	err = SetPropertyWrapper(id, NULL, 0, true, kAudioDevicePropertyBufferFrameSize, sizeof(UInt32), &uint32val);
	if (err != noErr) { return -1; }
	err = SetPropertyWrapper(id, NULL, 0, false, kAudioDevicePropertyBufferFrameSize, sizeof(UInt32), &uint32val);
	if (err != noErr) { return -1; }
	return 0;
}

int
CoreAudioPCM::set_samples_per_period (uint32_t n_samples)
{

	if (_state != 0 || _active_device_id == 0) {
		return -1;
	}
	set_device_buffer_size_id (_active_device_id, n_samples);
	return 0;
}

int
CoreAudioPCM::pcm_start (
	uint32_t device_id_in, uint32_t device_id_out,
	uint32_t sample_rate, uint32_t samples_per_period,
	int (process_callback (void*, const uint32_t, const uint64_t)), void *process_arg,
	PBD::TimingStats& dsp_timer)
{

	assert(_device_ids);
	std::string errorMsg;
	_state = -99;

	// "None" = UINT32_MAX
	if (device_id_out >= _n_devices && device_id_in >= _n_devices) {
		return -1;
	}

	pthread_mutex_lock (&_discovery_lock);

	_process_callback = process_callback;
	_process_arg = process_arg;
	_samples_per_period = samples_per_period;
	_cur_samples_per_period = 0;
	_dsp_timer = &dsp_timer;
	_active_device_id = 0;
	_capture_channels = 0;
	_playback_channels = 0;

	const uint32_t chn_in = (device_id_in < _n_devices ? _device_ins[device_id_in] : 0) + ((device_id_out != device_id_in && device_id_out < _n_devices) ? _device_ins[device_id_out] : 0);
	const uint32_t chn_out =(device_id_out < _n_devices ? _device_outs[device_id_out] : 0) + ((device_id_out != device_id_in && device_id_in < _n_devices) ? _device_outs[device_id_in] : 0);

	assert (chn_in > 0 || chn_out > 0);

	ComponentResult err;
	UInt32 uint32val;
	UInt32 size;
	AudioDeviceID device_id;
	AudioStreamBasicDescription srcFormat, dstFormat;

#ifndef COREAUDIO_108
	ComponentDescription cd = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0};
	Component HALOutput = FindNextComponent(NULL, &cd);
	if (!HALOutput) { errorMsg="FindNextComponent"; _state = -2; goto error; }

	err = OpenAComponent(HALOutput, &_auhal);
	if (err != noErr) { errorMsg="OpenAComponent"; _state = -2; goto error; }
#else
	AudioComponentDescription cd = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0};
	AudioComponent HALOutput = AudioComponentFindNext(NULL, &cd);
	if (!HALOutput) { errorMsg="AudioComponentFindNext"; _state = -2; goto error; }

	err = AudioComponentInstanceNew(HALOutput, &_auhal);
	if (err != noErr) { errorMsg="AudioComponentInstanceNew"; _state = -2; goto error; }
#endif

	err = AudioUnitInitialize(_auhal);
	if (err != noErr) { errorMsg="AudioUnitInitialize"; _state = -3; goto error; }

	// explicitly change samplerate of the devices, TODO allow separate rates with aggregates
	if (set_device_sample_rate(device_id_in, sample_rate, true)) {
		errorMsg="Failed to set SampleRate, Capture Device"; _state = -4; goto error;
	}
	if (set_device_sample_rate(device_id_out, sample_rate, false)) {
		errorMsg="Failed to set SampleRate, Playback Device"; _state = -4; goto error;
	}

	// explicitly request device buffer size
	if (device_id_in < _n_devices && set_device_buffer_size_id(_device_ids[device_id_in], samples_per_period)) {
		errorMsg="kAudioDevicePropertyBufferFrameSize, Input"; _state = -5; goto error;
	}
	if (device_id_out < _n_devices && set_device_buffer_size_id(_device_ids[device_id_out], samples_per_period)) {
		errorMsg="kAudioDevicePropertyBufferFrameSize, Output"; _state = -5; goto error;
	}

	// create aggregate device..
	if (device_id_in < _n_devices && device_id_out < _n_devices && _device_ids[device_id_in] != _device_ids[device_id_out]) {
		if (0 == create_aggregate_device(_device_ids[device_id_in], _device_ids[device_id_out], sample_rate, &_aggregate_device_id)) {
			device_id = _aggregate_device_id;
		} else {
			_aggregate_device_id = 0;
			_aggregate_plugin_id = 0;
			errorMsg="Cannot create Aggregate Device"; _state = -12; goto error;
		}
	} else if (device_id_out < _n_devices) {
		device_id = _device_ids[device_id_out];
	} else {
		assert (device_id_in < _n_devices);
		device_id = _device_ids[device_id_in];
	}

	if (device_id_out != device_id_in) {
		assert(_aggregate_device_id > 0 || device_id_in >= _n_devices || device_id_out >= _n_devices);
	}

	// enableIO to progress further
	uint32val = (chn_in > 0) ? 1 : 0;
	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, AUHAL_INPUT_ELEMENT, &uint32val, sizeof(UInt32));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_EnableIO, Input"; _state = -7; goto error; }

	uint32val = (chn_out > 0) ? 1 : 0;
	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, AUHAL_OUTPUT_ELEMENT, &uint32val, sizeof(UInt32));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_EnableIO, Output"; _state = -7; goto error; }

	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device_id, sizeof(AudioDeviceID));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_CurrentDevice, Input"; _state = -7; goto error; }

	if (chn_in > 0) {
		// set sample format
		srcFormat.mSampleRate = sample_rate;
		srcFormat.mFormatID = kAudioFormatLinearPCM;
		srcFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
		srcFormat.mBytesPerPacket = sizeof(float);
		srcFormat.mFramesPerPacket = 1;
		srcFormat.mBytesPerFrame = sizeof(float);
		srcFormat.mChannelsPerFrame = chn_in;
		srcFormat.mBitsPerChannel = 32;

		err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, AUHAL_INPUT_ELEMENT, &srcFormat, sizeof(AudioStreamBasicDescription));
		if (err != noErr) { errorMsg="kAudioUnitProperty_StreamFormat, Output"; _state = -6; goto error; }

		err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT, (UInt32*)&_samples_per_period, sizeof(UInt32));
		if (err != noErr) { errorMsg="kAudioUnitProperty_MaximumFramesPerSlice, Input"; _state = -6; goto error; }
	}

	if (chn_out > 0) {
		dstFormat.mSampleRate = sample_rate;
		dstFormat.mFormatID = kAudioFormatLinearPCM;
		dstFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
		dstFormat.mBytesPerPacket = sizeof(float);
		dstFormat.mFramesPerPacket = 1;
		dstFormat.mBytesPerFrame = sizeof(float);
		dstFormat.mChannelsPerFrame = chn_out;
		dstFormat.mBitsPerChannel = 32;

		err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AUHAL_OUTPUT_ELEMENT, &dstFormat, sizeof(AudioStreamBasicDescription));
		if (err != noErr) { errorMsg="kAudioUnitProperty_StreamFormat Input"; _state = -5; goto error; }

		err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT, (UInt32*)&_samples_per_period, sizeof(UInt32));
		if (err != noErr) { errorMsg="kAudioUnitProperty_MaximumFramesPerSlice, Output"; _state = -5; goto error; }
	}

	/* read back stream descriptions */
	if (chn_in > 0) {
		size = sizeof(AudioStreamBasicDescription);
		err = AudioUnitGetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, AUHAL_INPUT_ELEMENT, &srcFormat, &size);
		if (err != noErr) { errorMsg="Get kAudioUnitProperty_StreamFormat, Output"; _state = -5; goto error; }
		_capture_channels = srcFormat.mChannelsPerFrame;
#ifndef NDEBUG
		PrintStreamDesc(&srcFormat);
#endif
	}

	if (chn_out > 0) {
		size = sizeof(AudioStreamBasicDescription);
		err = AudioUnitGetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AUHAL_OUTPUT_ELEMENT, &dstFormat, &size);
		if (err != noErr) { errorMsg="Get kAudioUnitProperty_StreamFormat, Input"; _state = -5; goto error; }
		_playback_channels = dstFormat.mChannelsPerFrame;

#ifndef NDEBUG
		PrintStreamDesc(&dstFormat);
#endif
	}

	/* prepare buffers for input */
	if (_capture_channels > 0) {
		_input_audio_buffer_list = (AudioBufferList*)malloc(sizeof(AudioBufferList) + (_capture_channels - 1) * sizeof(AudioBuffer));
		assert(_input_audio_buffer_list);
		if (!_input_audio_buffer_list) { errorMsg="Out of Memory."; _state = -8; goto error; }
	}

	_active_device_id = device_id;

	// add Listeners
	err = add_listener (_active_device_id, kAudioDeviceProcessorOverload, this);
	if (err != noErr) { errorMsg="kAudioDeviceProcessorOverload, Listen"; _state = -9; goto error; }

	err = add_listener (_active_device_id, kAudioDevicePropertyBufferFrameSize, this);
	if (err != noErr) { errorMsg="kAudioDevicePropertyBufferFrameSize, Listen"; _state = -9; goto error; }

	err = add_listener (_active_device_id, kAudioDevicePropertyNominalSampleRate, this);
	if (err != noErr) { errorMsg="kAudioDevicePropertyNominalSampleRate, Listen"; _state = -9; goto error; }

	_samples_per_period = current_buffer_size_id(_active_device_id);

	// Setup callback
	AURenderCallbackStruct renderCallback;
	memset (&renderCallback, 0, sizeof (renderCallback));
	renderCallback.inputProc = render_callback_ptr;
	renderCallback.inputProcRefCon = this;
	if (_playback_channels == 0) {
		err = AudioUnitSetProperty(_auhal,
				kAudioOutputUnitProperty_SetInputCallback,
				kAudioUnitScope_Output, 1,
				&renderCallback, sizeof (renderCallback));
	} else {
		err = AudioUnitSetProperty(_auhal,
				kAudioUnitProperty_SetRenderCallback,
				kAudioUnitScope_Output, 0,
				&renderCallback, sizeof (renderCallback));
	}

	if (err != noErr) { errorMsg="kAudioUnitProperty_SetRenderCallback"; _state = -10; goto error; }

	/* setup complete, now get going.. */
	if (AudioOutputUnitStart(_auhal) == noErr) {
		_input_names.clear();
		_output_names.clear();
		cache_port_names (device_id, true);
		cache_port_names (device_id, false);
		_state = 0;
		pthread_mutex_unlock (&_discovery_lock);

		// kick device
		if (set_device_buffer_size_id(_active_device_id, samples_per_period)) {
			errorMsg="kAudioDevicePropertyBufferFrameSize"; _state = -11; goto error;
		}

		return 0;
	}

error:
	assert (_state != 0);
	char *rv = (char*)&err;
	fprintf(stderr, "CoreaudioPCM Error: %c%c%c%c %s\n", rv[0], rv[1], rv[2], rv[3], errorMsg.c_str());
	pcm_stop();
	_active_device_id = 0;
	pthread_mutex_unlock (&_discovery_lock);
	return -1;
}

void
CoreAudioPCM::cache_port_names(AudioDeviceID id, bool input)
{
	uint32_t n_chn;

	if (input) {
		n_chn = _capture_channels;
	} else {
		n_chn = _playback_channels;;
	}
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioObjectPropertyElementName;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
#endif

	for (uint32_t c = 0; c < n_chn; ++c) {
		CFStringRef name = NULL;
		std::stringstream ss;
		UInt32 size = 0;
		OSStatus err;

#ifdef COREAUDIO_108
		property_address.mElement = c + 1;
		err = AudioObjectGetPropertyDataSize(id, &property_address, 0, NULL, &size);
#else
		err = AudioDeviceGetPropertyInfo (id, c + 1, input,
				kAudioDevicePropertyChannelNameCFString,
				&size,
				NULL);
#endif

		if (err == kAudioHardwareNoError) {
#ifdef COREAUDIO_108
			err = AudioObjectGetPropertyData(id, &property_address, c + 1, NULL, &size, &name);
#else
			err = AudioDeviceGetProperty (id, c + 1, input,
					kAudioDevicePropertyChannelNameCFString,
					&size,
					&name);
#endif
		}

		bool decoded = false;
		char* cstr_name = 0;
		if (err == kAudioHardwareNoError) {
			CFIndex length = CFStringGetLength(name);
			CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
			cstr_name = new char[maxSize];
			decoded = CFStringGetCString(name, cstr_name, maxSize, kCFStringEncodingUTF8);
		}

		ss << (c + 1);

		if (cstr_name && decoded && (0 != ::strlen(cstr_name) ) ) {
			ss << " - " <<  cstr_name;
		}
#if 0
		printf("%s %d Name: %s\n", input ? "Input" : "Output", c+1, ss.str().c_str());
#endif

		if (input) {
			_input_names.push_back (ss.str());
		} else {
			_output_names.push_back (ss.str());
		}

		if (name) {
			CFRelease (name);
		}
		delete [] cstr_name;
	}
}

std::string
CoreAudioPCM::cached_port_name(uint32_t port, bool input) const
{
	if (_state != 0) { return ""; }

	if (input) {
		if (port >= _input_names.size()) {
			return "";
		}
		return _input_names[port];
	} else {
		if (port >= _output_names.size()) {
			return "";
		}
		return _output_names[port];
	}
}


OSStatus
CoreAudioPCM::render_callback (
		AudioUnitRenderActionFlags* ioActionFlags,
		const AudioTimeStamp* inTimeStamp,
		UInt32 inBusNumber,
		UInt32 inNumberSamples,
		AudioBufferList* ioData)
{
	PBD::WaitTimerRAII tr (*_dsp_timer);
	OSStatus retVal = kAudioHardwareNoError;

	if (_samples_per_period < inNumberSamples) {
#ifndef NDEBUG
		printf("samples per period exceeds configured value, cycle skipped (%u < %u)\n",
				(unsigned int)_samples_per_period, (unsigned int)inNumberSamples);
#endif
		for (uint32_t i = 0; _playback_channels > 0 && i < ioData->mNumberBuffers; ++i) {
			float* ob = (float*) ioData->mBuffers[i].mData;
			memset(ob, 0, sizeof(float) * inNumberSamples);
		}
		return noErr;
	}

	assert(_playback_channels == 0 || ioData->mNumberBuffers == _playback_channels);

	UInt64 cur_cycle_start = AudioGetCurrentHostTime ();
	_cur_samples_per_period = inNumberSamples;

	if (_capture_channels > 0) {
		_input_audio_buffer_list->mNumberBuffers = _capture_channels;
		for (uint32_t i = 0; i < _capture_channels; ++i) {
			_input_audio_buffer_list->mBuffers[i].mNumberChannels = 1;
			_input_audio_buffer_list->mBuffers[i].mDataByteSize = inNumberSamples * sizeof(float);
			_input_audio_buffer_list->mBuffers[i].mData = NULL;
		}

		retVal = AudioUnitRender(_auhal, ioActionFlags, inTimeStamp, AUHAL_INPUT_ELEMENT, inNumberSamples, _input_audio_buffer_list);
	}

	if (retVal != kAudioHardwareNoError) {
#if 0
		char *rv = (char*)&retVal;
		printf("ERR %c%c%c%c\n", rv[0], rv[1], rv[2], rv[3]);
#endif
		if (_error_callback) {
			_error_callback(_error_arg);
		}
		return retVal;
	}

	_output_audio_buffer_list = ioData;

	_in_process = true;

	int rv = -1;

	if (_process_callback) {
		rv = _process_callback(_process_arg, inNumberSamples, cur_cycle_start);
	}

	_in_process = false;

	if (rv != 0 && _playback_channels > 0) {
		// clear output
		for (uint32_t i = 0; i < ioData->mNumberBuffers; ++i) {
			float* ob = (float*) ioData->mBuffers[i].mData;
			memset(ob, 0, sizeof(float) * inNumberSamples);
		}
	}
	return noErr;
}

int
CoreAudioPCM::get_capture_channel (uint32_t chn, float *input, uint32_t n_samples)
{
	if (!_in_process || chn > _capture_channels || n_samples > _cur_samples_per_period) {
		return -1;
	}
	assert(_input_audio_buffer_list->mNumberBuffers > chn);
	memcpy((void*)input, (void*)_input_audio_buffer_list->mBuffers[chn].mData, sizeof(float) * n_samples);
	return 0;

}
int
CoreAudioPCM::set_playback_channel (uint32_t chn, const float *output, uint32_t n_samples)
{
	if (!_in_process || chn > _playback_channels || n_samples > _cur_samples_per_period) {
		return -1;
	}

	assert(_output_audio_buffer_list && _output_audio_buffer_list->mNumberBuffers > chn);
	memcpy((void*)_output_audio_buffer_list->mBuffers[chn].mData, (void*)output, sizeof(float) * n_samples);
	return 0;
}


void
CoreAudioPCM::launch_control_app (uint32_t device_id)
{
	if (device_id >= _n_devices) {
		return;
	}

	CFStringRef config_app = NULL;
	UInt32 size = sizeof (config_app);
	OSStatus err;

	err = GetPropertyWrapper(_device_ids[device_id], 0, false, kAudioDevicePropertyConfigurationApplication, &size, &config_app);
	if (kAudioHardwareNoError != err) {
		return;
	}

	FSRef appFSRef;
	if (noErr == LSFindApplicationForInfo(kLSUnknownCreator, config_app, NULL, &appFSRef, NULL)) {
		LSOpenFSRef(&appFSRef, NULL);
	} else {
		// open default AudioMIDISetup if device app is not found
		CFStringRef audioMidiSetup = CFStringCreateWithCString(kCFAllocatorDefault, "com.apple.audio.AudioMIDISetup", kCFStringEncodingMacRoman);
		if (noErr == LSFindApplicationForInfo(kLSUnknownCreator, audioMidiSetup, NULL, &appFSRef, NULL)) {
			LSOpenFSRef(&appFSRef, NULL);
		}
	}
	if (config_app) {
		CFRelease (config_app);
	}
}
