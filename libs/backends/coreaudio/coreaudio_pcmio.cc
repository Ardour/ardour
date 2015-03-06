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

#include <glibmm.h>
#include "coreaudio_pcmio.h"

#ifdef COREAUDIO_108
static OSStatus hw_changed_callback_ptr(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void* arg) {
	CoreAudioPCM * self = static_cast<CoreAudioPCM*>(arg);
	self->hw_changed_callback();
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
#endif

#ifdef COREAUDIO_108
static OSStatus xrun_callback_ptr(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void* arg) {
	CoreAudioPCM * self = static_cast<CoreAudioPCM*>(arg);
	self->xrun_callback();
	return noErr;
}
#else
static OSStatus xrun_callback_ptr(
		AudioDeviceID inDevice,
		UInt32 inChannel,
		Boolean isInput,
		AudioDevicePropertyID inPropertyID,
		void* inClientData)
{
	CoreAudioPCM * d = static_cast<CoreAudioPCM*> (inClientData);
	d->xrun_callback();
	return noErr;
}
#endif

static OSStatus render_callback_ptr (
		void* inRefCon,
		AudioUnitRenderActionFlags* ioActionFlags,
		const AudioTimeStamp* inTimeStamp,
		UInt32 inBusNumber,
		UInt32 inNumberFrames,
		AudioBufferList* ioData)
{
	CoreAudioPCM * d = static_cast<CoreAudioPCM*> (inRefCon);
	return d->render_callback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}


CoreAudioPCM::CoreAudioPCM ()
	: _auhal (0)
	, _device_ids (0)
	, _input_audio_buffer_list (0)
	, _active_input (0)
	, _active_output (0)
	, _state (-1)
	, _capture_channels (0)
	, _playback_channels (0)
	, _in_process (false)
	, _n_devices (0)
	, _process_callback (0)
	, _error_callback (0)
	, _hw_changed_callback (0)
	, _xrun_callback (0)
	, _device_ins (0)
	, _device_outs (0)
{
	pthread_mutex_init (&_discovery_lock, 0);

#ifdef COREAUDIO_108
	CFRunLoopRef theRunLoop = NULL;
	AudioObjectPropertyAddress property = { kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioHardwarePropertyDevices };
	AudioObjectSetPropertyData (kAudioObjectSystemObject, &property, 0, NULL, sizeof(CFRunLoopRef), &theRunLoop);

	AudioObjectPropertyAddress prop;
	prop.mSelector = kAudioHardwarePropertyDevices;
	prop.mScope = kAudioObjectPropertyScopeGlobal;
	prop.mElement = 0;
	AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop, hw_changed_callback_ptr, this);
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
	AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop, &hw_changed_callback_ptr, this);
#else
	AudioHardwareRemovePropertyListener(kAudioHardwarePropertyDevices, hw_changed_callback_ptr);
#endif
	free(_input_audio_buffer_list);
	pthread_mutex_destroy (&_discovery_lock);
}


void
CoreAudioPCM::hw_changed_callback() {
	printf("CHANGE..\n");
	discover();
	// TODO Filter events..
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

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
	property_address.mScope = kAudioDevicePropertyScopeOutput;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectGetPropertyDataSize(_device_ids[device_id], &property_address, 0, NULL, &size);
#else
	err = AudioDeviceGetPropertyInfo(_device_ids[device_id], 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &size, NULL);
#endif

	if (err != kAudioHardwareNoError) {
		return -1;
	}

	int numRates = size / sizeof(AudioValueRange);
	AudioValueRange* supportedRates = new AudioValueRange[numRates];

#ifdef COREAUDIO_108
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, supportedRates);
#else
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &size, supportedRates);
#endif

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

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &supportedRange);
#else
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, 0, kAudioDevicePropertyBufferFrameSizeRange, &size, &supportedRange);
#endif

	if (err != kAudioHardwareNoError) {
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
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyStreamConfiguration;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	err = AudioObjectGetPropertyDataSize(_device_ids[device_id], &property_address, 0, NULL, &size);
	if (kAudioHardwareNoError != err) {
		fprintf(stderr, "CoreaAudioPCM: kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
		return 0;
	}

	bufferList = (AudioBufferList *)(malloc(size));
	assert(bufferList);
	if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); return 0; }

	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, bufferList);

#else
	err = AudioDeviceGetPropertyInfo (_device_ids[device_id], 0, input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyStreamConfiguration, &size, NULL);
	if (kAudioHardwareNoError != err) {
		fprintf(stderr, "CoreaAudioPCM: kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
		return 0;
	}

	bufferList = (AudioBufferList *)(malloc(size));
	assert(bufferList);
	if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); return 0; }

	bufferList->mNumberBuffers = 0;
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyStreamConfiguration, &size, bufferList);
#endif

	if(kAudioHardwareNoError != err) {
		fprintf(stderr, "CoreaAudioPCM: kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
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
CoreAudioPCM::get_stream_latencies(uint32 device_id, bool input, std::vector<uint32>& latencies)
{
	OSStatus err;
	UInt32 size = 0;

	if (device_id >= _n_devices) {
		return;
	}

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyStreams;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectGetPropertyDataSize(_device_ids[device_id], &property_address, 0, NULL, &size);
#else
	Boolean	outWritable;
	const int elem = input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT;
	err = AudioDeviceGetPropertyInfo(_device_ids[device_id], 0, elem, kAudioDevicePropertyStreams, &size, &outWritable);
#endif
	if (err != noErr) {
		return;
	}

	uint32 stream_count = size / sizeof(UInt32);
	AudioStreamID streamIDs[stream_count];

#ifdef COREAUDIO_108
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &streamIDs);
#else
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, elem, kAudioDevicePropertyStreams, &size, streamIDs);
#endif
	if (err != noErr) {
		fprintf(stderr, "GetStreamLatencies kAudioDevicePropertyStreams\n");
		return;
	}

	for (uint32 i = 0; i < stream_count; i++) {
		UInt32 stream_latency;
		size = sizeof(UInt32);
#ifdef COREAUDIO_108
		property_address.mSelector = kAudioDevicePropertyStreams;
		err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &stream_latency);
#else
		err = AudioStreamGetProperty(streamIDs[i], elem, kAudioStreamPropertyLatency, &size, &stream_latency);
#endif
		if (err != noErr) {
			fprintf(stderr, "GetStreamLatencies kAudioStreamPropertyLatency\n");
			return;
		}
#ifndef NDEBUG
		printf("  ^ Stream %d latency: %d\n", i, stream_latency);
#endif
		latencies.push_back(stream_latency);
	}
}

uint32_t
CoreAudioPCM::get_latency(uint32 device_id, bool input)
{
	OSStatus err;
	uint32 latency = 0;
	UInt32 size = sizeof(UInt32);
	UInt32 lat0 = 0;
	UInt32 latS = 0;

	if (device_id >= _n_devices) {
		return 0;
	}

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyLatency;
	property_address.mScope = input? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	property_address.mElement = 0;
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &lat0);
#else
	const int elem = input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT;
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, elem, kAudioDevicePropertyLatency, &size, &lat0);
#endif
	if (err != kAudioHardwareNoError) {
		fprintf(stderr, "GetLatency kAudioDevicePropertyLatency\n");
	}

#ifdef COREAUDIO_108
	property_address.mSelector = kAudioDevicePropertySafetyOffset;
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &latS);
#else
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, elem, kAudioDevicePropertySafetyOffset, &size, &latS);
#endif
	if (err != kAudioHardwareNoError) {
		fprintf(stderr, "GetLatency kAudioDevicePropertySafetyOffset\n");
	}

#ifndef NDEBUG
	printf("%s Latency systemic+safetyoffset = %d + %d\n",
			input ? "Input" : "Output", lat0, latS);
#endif
	latency = lat0 + latS;

	uint32_t max_stream_latency = 0;
	std::vector<uint32> stream_latencies;
	get_stream_latencies(device_id, input, stream_latencies);
	for (size_t i = 0; i < stream_latencies.size(); ++i) {
		max_stream_latency = std::max(max_stream_latency, stream_latencies[i]);
	}
	latency += max_stream_latency;

	return latency;
}




float
CoreAudioPCM::current_sample_rate(uint32 device_id, bool input) {
	OSStatus err;
	UInt32 size = 0;

	if (device_id >= _n_devices) {
		return -1;
	}

	float sample_rate = 0;

	Float64 rate;
	size = sizeof (rate);

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyNominalSampleRate;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &rate);
#else
	err = AudioDeviceGetPropertyInfo(_device_ids[device_id], 0, input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyNominalSampleRate, &size, &rate);
#endif

	if (err == kAudioHardwareNoError) {
		sample_rate = rate;
	}

	// prefer input, if vailable

#ifdef COREAUDIO_108
	property_address.mSelector = kAudioDevicePropertyNominalSampleRate;
	property_address.mScope = kAudioDevicePropertyScopeInput;
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &rate);
#else
	err = AudioDeviceGetPropertyInfo(_device_ids[device_id], 0, AUHAL_INPUT_ELEMENT, kAudioDevicePropertyNominalSampleRate, &size, &rate);
#endif

	if (err == kAudioHardwareNoError) {
		sample_rate = rate;
	}

	return sample_rate;
}

int
CoreAudioPCM::set_device_sample_rate (uint32 device_id, float rate, bool input)
{
	std::vector<int>::iterator intIter;
	OSStatus err;
	UInt32 size = 0;

	if (current_sample_rate(device_id, input) == rate) {
		return 0;
	}

	Float64 newNominalRate = rate;
	size = sizeof (Float64);

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyNominalSampleRate;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectSetPropertyData (_device_ids[device_id], &property_address, 0, NULL, size, &newNominalRate);
#else
	err = AudioDeviceSetProperty(_device_ids[device_id], NULL, 0, input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyNominalSampleRate, size, &newNominalRate);
#endif
	if (err != noErr) {
		fprintf(stderr, "CoreAudioPCM: failed to set samplerate\n");
		return 0;
	}

	int timeout = 3000; // 3 sec
	while (--timeout > 0) {
		if (current_sample_rate(device_id) == rate) {
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

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioHardwarePropertyDevices;
	property_address.mScope = kAudioObjectPropertyScopeGlobal;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property_address, 0, NULL, &size);
#else
	err = AudioHardwareGetPropertyInfo (kAudioHardwarePropertyDevices, &size, NULL);
#endif

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


#ifdef COREAUDIO_108
	property_address.mSelector = kAudioHardwarePropertyDevices;
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0, NULL, &size, _device_ids);
#else
	err = AudioHardwareGetProperty (kAudioHardwarePropertyDevices, &size, _device_ids);
#endif

	for (size_t idx = 0; idx < _n_devices; ++idx) {
		size = 64;
		char deviceName[64];
#ifdef COREAUDIO_108
		property_address.mSelector = kAudioDevicePropertyDeviceName;
		property_address.mScope = kAudioDevicePropertyScopeOutput;
		err = AudioObjectGetPropertyData(_device_ids[idx], &property_address, 0, NULL, &size, deviceName);
#else
		err = AudioDeviceGetProperty(_device_ids[idx], 0, 0, kAudioDevicePropertyDeviceName, &size, deviceName);
#endif

		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "CoreAudioPCM: device name query failed: %i\n", err);
			continue;
		}

		UInt32 inputChannelCount = available_channels(idx, true);
		UInt32 outputChannelCount = available_channels(idx, false);

		{
			std::string dn = deviceName;
			_device_ins[idx] = inputChannelCount;
			_device_outs[idx] = outputChannelCount;
#ifndef NDEBUG
			printf("CoreAudio Device: #%ld '%s' in:%d out:%d\n", idx, deviceName, inputChannelCount, outputChannelCount);
#endif
			if (outputChannelCount > 0 && inputChannelCount > 0) {
				_devices.insert (std::pair<size_t, std::string> (idx, dn));
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
CoreAudioPCM::pcm_stop ()
{
	if (!_auhal) return;

	AudioOutputUnitStop(_auhal);
	if (_state == 0) {
#ifdef COREAUDIO_108
		AudioObjectPropertyAddress prop;
		prop.mSelector = kAudioDeviceProcessorOverload;
		prop.mScope = kAudioObjectPropertyScopeGlobal;
		prop.mElement = 0;
		if (_active_output > 0) {
			AudioObjectRemovePropertyListener(_active_input, &prop, &xrun_callback_ptr, this);
		}
		if (_active_input > 0 && _active_output != _active_input) {
			AudioObjectRemovePropertyListener(_active_output, &prop, &xrun_callback_ptr, this);
		}
#else
		AudioDeviceRemovePropertyListener(_active_input, 0 , true, kAudioDeviceProcessorOverload, xrun_callback_ptr);
		AudioDeviceRemovePropertyListener(_active_output, 0 , false, kAudioDeviceProcessorOverload, xrun_callback_ptr);
#endif
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
	printf ("  Sample Rate:%f", inDesc->mSampleRate);
	printf ("  Format ID:%.*s\n", (int)sizeof(inDesc->mFormatID), (char*)&inDesc->mFormatID);
	printf ("  Format Flags:%X\n", inDesc->mFormatFlags);
	printf ("  Bytes per Packet:%d\n", inDesc->mBytesPerPacket);
	printf ("  Frames per Packet:%d\n", inDesc->mFramesPerPacket);
	printf ("  Bytes per Frame:%d\n", inDesc->mBytesPerFrame);
	printf ("  Channels per Frame:%d\n", inDesc->mChannelsPerFrame);
	printf ("  Bits per Channel:%d\n", inDesc->mBitsPerChannel);
	printf ("- - - - - - - - - - - - - - - - - - - -\n");
}
#endif

int
CoreAudioPCM::pcm_start (
		uint32_t device_id_in, uint32_t device_id_out,
		uint32_t sample_rate, uint32_t samples_per_period,
		int (process_callback (void*)), void *process_arg)
{

	assert(_device_ids);
	std::string errorMsg;
	_state = -2;

	if (device_id_out >= _n_devices || device_id_in >= _n_devices) {
		return -1;
	}

	_process_callback = process_callback;
	_process_arg = process_arg;
	_max_samples_per_period = samples_per_period;
	_cur_samples_per_period = 0;
	_active_input = _active_output = 0;

	ComponentResult err;
	UInt32 uint32val;
	AudioStreamBasicDescription srcFormat, dstFormat;

	AudioComponentDescription cd = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0};
	AudioComponent HALOutput = AudioComponentFindNext(NULL, &cd);
	if (!HALOutput) { errorMsg="AudioComponentFindNext"; goto error; }

	err = AudioComponentInstanceNew(HALOutput, &_auhal);
	if (err != noErr) { errorMsg="AudioComponentInstanceNew"; goto error; }

	err = AudioUnitInitialize(_auhal);
	if (err != noErr) { errorMsg="AudioUnitInitialize"; goto error; }

	// explicitly change samplerate of the device
	if (set_device_sample_rate(device_id_in, sample_rate, true)) {
		errorMsg="Failed to set SampleRate, Capture Device"; goto error;
	}
	if (set_device_sample_rate(device_id_out, sample_rate, false)) {
		errorMsg="Failed to set SampleRate, Playback Device"; goto error;
	}

	// explicitly request device buffer size
	uint32val = samples_per_period;
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyBufferFrameSize;
	property_address.mScope = kAudioDevicePropertyScopeInput;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectSetPropertyData (_device_ids[device_id_in], &property_address, 0, NULL, sizeof(UInt32), &uint32val);
	if (err != noErr) { errorMsg="kAudioDevicePropertyBufferFrameSize, Input"; goto error; }

	property_address.mScope = kAudioDevicePropertyScopeOutput;
	err = AudioObjectSetPropertyData (_device_ids[device_id_out], &property_address, 0, NULL, sizeof(UInt32), &uint32val);
	if (err != noErr) { errorMsg="kAudioDevicePropertyBufferFrameSize, Output"; goto error; }
#else
	err = AudioDeviceSetProperty(_device_ids[device_id_in], NULL, 0, AUHAL_INPUT_ELEMENT, kAudioDevicePropertyBufferFrameSize, sizeof(UInt32), &uint32val);
	if (err != noErr) { errorMsg="kAudioDevicePropertyBufferFrameSize, Input"; goto error; }
	err = AudioDeviceSetProperty(_device_ids[device_id_out], NULL, 0, AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyBufferFrameSize, sizeof(UInt32), &uint32val);
	if (err != noErr) { errorMsg="kAudioDevicePropertyBufferFrameSize, Output"; goto error; }
#endif

	uint32val = 1;
	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, AUHAL_INPUT_ELEMENT, &uint32val, sizeof(UInt32));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_EnableIO, Input"; goto error; }
	uint32val = 1;
	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, AUHAL_OUTPUT_ELEMENT, &uint32val, sizeof(UInt32));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_EnableIO, Output"; goto error; }

	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT, &_device_ids[device_id_out], sizeof(AudioDeviceID));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_CurrentDevice, Output"; goto error; }

	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT, &_device_ids[device_id_in], sizeof(AudioDeviceID));
	if (err != noErr) { errorMsg="kAudioOutputUnitProperty_CurrentDevice, Input"; goto error; }

	// Set buffer size
	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT, (UInt32*)&_max_samples_per_period, sizeof(UInt32));
	if (err != noErr) { errorMsg="kAudioUnitProperty_MaximumFramesPerSlice, Input"; goto error; }
	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT, (UInt32*)&_max_samples_per_period, sizeof(UInt32));
	if (err != noErr) { errorMsg="kAudioUnitProperty_MaximumFramesPerSlice, Output"; goto error; }

	// set sample format
	srcFormat.mSampleRate = sample_rate;
	srcFormat.mFormatID = kAudioFormatLinearPCM;
	srcFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
	srcFormat.mBytesPerPacket = sizeof(float);
	srcFormat.mFramesPerPacket = 1;
	srcFormat.mBytesPerFrame = sizeof(float);
	srcFormat.mChannelsPerFrame = _device_ins[device_id_in];
	srcFormat.mBitsPerChannel = 32;

#if 0
	property_address = { kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
	err = AudioObjectSetPropertyData (_device_ids[device_id_in], &property_address, 0, NULL, sizeof(AudioStreamBasicDescription), &srcFormat);
#else
	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, AUHAL_INPUT_ELEMENT, &srcFormat, sizeof(AudioStreamBasicDescription));
#endif
	if (err != noErr) { errorMsg="kAudioUnitProperty_StreamFormat, Output"; goto error; }

	dstFormat.mSampleRate = sample_rate;
	dstFormat.mFormatID = kAudioFormatLinearPCM;
	dstFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
	dstFormat.mBytesPerPacket = sizeof(float);
	dstFormat.mFramesPerPacket = 1;
	dstFormat.mBytesPerFrame = sizeof(float);
	dstFormat.mChannelsPerFrame = _device_outs[device_id_out];
	dstFormat.mBitsPerChannel = 32;

#if 0
	property_address = { kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput, 0 };
	err = AudioObjectSetPropertyData (_device_ids[device_id_out], &property_address, 0, NULL, sizeof(AudioStreamBasicDescription), &dstFormat);
#else
	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AUHAL_OUTPUT_ELEMENT, &dstFormat, sizeof(AudioStreamBasicDescription));
#endif
	if (err != noErr) { errorMsg="kAudioUnitProperty_StreamFormat Input"; goto error; }

	/* read back stream descriptions */
	UInt32 size;
	size = sizeof(AudioStreamBasicDescription);
	err = AudioUnitGetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, AUHAL_INPUT_ELEMENT, &srcFormat, &size);
	if (err != noErr) { errorMsg="Get kAudioUnitProperty_StreamFormat, Output"; goto error; }
	_capture_channels = srcFormat.mChannelsPerFrame;
#ifndef NDEBUG
	PrintStreamDesc(&srcFormat);
#endif

	size = sizeof(AudioStreamBasicDescription);
	err = AudioUnitGetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AUHAL_OUTPUT_ELEMENT, &dstFormat, &size);
	if (err != noErr) { errorMsg="Get kAudioUnitProperty_StreamFormat, Input"; goto error; }
	_playback_channels = dstFormat.mChannelsPerFrame;

#ifndef NDEBUG
	PrintStreamDesc(&dstFormat);
#endif

	/* prepare buffers for input */
	_input_audio_buffer_list = (AudioBufferList*)malloc(sizeof(UInt32) + _capture_channels * sizeof(AudioBuffer));
	assert(_input_audio_buffer_list);
	if (!_input_audio_buffer_list) { errorMsg="Out of Memory."; goto error; }

	_active_input = _device_ids[device_id_in];
	_active_output = _device_ids[device_id_out];

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress prop;
	prop.mSelector = kAudioDeviceProcessorOverload;
	prop.mScope = kAudioObjectPropertyScopeGlobal;
	prop.mElement = 0;
	AudioObjectAddPropertyListener(_active_output, &prop, xrun_callback_ptr, this);
	if (err != noErr) { errorMsg="kAudioDeviceProcessorOverload, Output"; goto error; }
	if (_active_input != _active_output)  {
		AudioObjectAddPropertyListener(_active_input, &prop, xrun_callback_ptr, this);
		if (err != noErr) { errorMsg="kAudioDeviceProcessorOverload, Input"; goto error; }
	}
#else
	err = AudioDeviceAddPropertyListener(_device_ids[device_id_out], 0 , false, kAudioDeviceProcessorOverload, xrun_callback_ptr, this);
	if (err != noErr) { errorMsg="kAudioDeviceProcessorOverload, Output"; goto error; }
	err = AudioDeviceAddPropertyListener(_device_ids[device_id_in], 0 , true, kAudioDeviceProcessorOverload, xrun_callback_ptr, this);
	if (err != noErr) { errorMsg="kAudioDeviceProcessorOverload, Input"; goto error; }
#endif

	// Setup callbacks
	AURenderCallbackStruct renderCallback;
	memset (&renderCallback, 0, sizeof (renderCallback));
	renderCallback.inputProc = render_callback_ptr;
	renderCallback.inputProcRefCon = this;
	err = AudioUnitSetProperty(_auhal,
			kAudioUnitProperty_SetRenderCallback,
			kAudioUnitScope_Output, AUHAL_OUTPUT_ELEMENT,
			&renderCallback, sizeof (renderCallback));
	if (err != noErr) { errorMsg="kAudioUnitProperty_SetRenderCallback"; goto error; }

	/* setup complete, now get going.. */
	if (AudioOutputUnitStart(_auhal) == noErr) {
		_input_names.clear();
		_output_names.clear();
		cache_port_names( device_id_in, true);
		cache_port_names( device_id_out, false);
		_state = 0;
		return 0;
	}

error:
	char *rv = (char*)&err;
	fprintf(stderr, "CoreaudioPCM Error: %c%c%c%c %s\n", rv[0], rv[1], rv[2], rv[3], errorMsg.c_str());
	pcm_stop();
	_state = -3;
	_active_input = _active_output = 0;
	return -1;
}

void
CoreAudioPCM::cache_port_names(uint32 device_id, bool input)
{
	uint32_t n_chn;
	assert (device_id < _n_devices);

	if (input) {
		n_chn = _capture_channels;
	} else {
		n_chn = _playback_channels;;
	}
#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioObjectPropertyElementName;
	property_address.mScope = input ? kAudioDevicePropertyScopeInput: kAudioDevicePropertyScopeOutput;
#else
	const int elem = input ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT;
#endif

	for (uint32_t c = 0; c < n_chn; ++c) {
		CFStringRef name = NULL;
		std::stringstream ss;
		UInt32 size = 0;
		OSStatus err;

#ifdef COREAUDIO_108
		property_address.mElement = c + 1;
		err = AudioObjectGetPropertyDataSize(_device_ids[device_id], &property_address, 0, NULL, &size);
#else
		err = AudioDeviceGetPropertyInfo (_device_ids[device_id], c + 1, elem,
				kAudioDevicePropertyChannelNameCFString,
				&size,
				NULL);
#endif

		if (err == kAudioHardwareNoError) {
#ifdef COREAUDIO_108
			err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, c + 1, NULL, &size, &name);
#else
			err = AudioDeviceGetProperty (_device_ids[device_id], c + 1, elem,
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

		if (cstr_name && decoded && (0 != std::strlen(cstr_name) ) ) {
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
CoreAudioPCM::cached_port_name(uint32 port, bool input) const
{
	if (_state != 0) { return ""; }

	if (input) {
		if (port > _input_names.size()) {
			return "";
		}
		return _input_names[port];
	} else {
		if (port > _output_names.size()) {
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
		UInt32 inNumberFrames,
		AudioBufferList* ioData)
{
	OSStatus retVal = 0;

	assert(_max_samples_per_period >= inNumberFrames);
	assert(ioData->mNumberBuffers = _playback_channels);

	_cur_samples_per_period = inNumberFrames;


	_input_audio_buffer_list->mNumberBuffers = _capture_channels;
	for (uint32_t i = 0; i < _capture_channels; ++i) {
		_input_audio_buffer_list->mBuffers[i].mNumberChannels = 1;
		_input_audio_buffer_list->mBuffers[i].mDataByteSize = inNumberFrames * sizeof(float);
		_input_audio_buffer_list->mBuffers[i].mData = NULL;
	}

	retVal = AudioUnitRender(_auhal, ioActionFlags, inTimeStamp, AUHAL_INPUT_ELEMENT, inNumberFrames, _input_audio_buffer_list);

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
		rv = _process_callback(_process_arg);
	}

	_in_process = false;

	if (rv != 0) {
		// clear output
		for (uint32_t i = 0; i < ioData->mNumberBuffers; ++i) {
			float* ob = (float*) ioData->mBuffers[i].mData;
			memset(ob, 0, sizeof(float) * inNumberFrames);
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

	assert(_output_audio_buffer_list->mNumberBuffers > chn);
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

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioDevicePropertyConfigurationApplication;
	property_address.mScope = kAudioDevicePropertyScopeOutput;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectGetPropertyData(_device_ids[device_id], &property_address, 0, NULL, &size, &config_app);
#else
	err = AudioDeviceGetProperty(_device_ids[device_id], 0, 0, kAudioDevicePropertyConfigurationApplication, &size, &config_app);
#endif
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
