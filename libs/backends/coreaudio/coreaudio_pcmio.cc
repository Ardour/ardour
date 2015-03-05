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

#include "coreaudio_pcmio.h"
#include <string>


static OSStatus hardwarePropertyChangeCallback (AudioHardwarePropertyID inPropertyID, void* arg) {
	if (inPropertyID == kAudioHardwarePropertyDevices) {
		CoreAudioPCM * self = static_cast<CoreAudioPCM*>(arg);
		self->hwPropertyChange();
	}
	return noErr;
}

CoreAudioPCM::CoreAudioPCM ()
	: _auhal (0)
	, _deviceIDs (0)
	, _inputAudioBufferList (0)
	, _state (-1)
	, _capture_channels (0)
	, _playback_channels (0)
	, _in_process (false)
	, _numDevices (0)
	, _process_callback (0)
	, _error_callback (0)
	, _device_ins (0)
	, _device_outs (0)
{
#ifdef COREAUDIO_108 // TODO
	CFRunLoopRef theRunLoop = NULL;
	AudioObjectPropertyAddress property = { kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioHardwarePropertyDevices };
	AudioObjectSetPropertyData (kAudioObjectSystemObject, &property, 0, NULL, sizeof(CFRunLoopRef), &theRunLoop);
#endif
	AudioHardwareAddPropertyListener (kAudioHardwarePropertyDevices, hardwarePropertyChangeCallback, this);
}

CoreAudioPCM::~CoreAudioPCM ()
{
	if (_state == 0) {
		pcm_stop();
	}
	delete _deviceIDs;
	free(_device_ins);
	free(_device_outs);
	AudioHardwareRemovePropertyListener(kAudioHardwarePropertyDevices, hardwarePropertyChangeCallback);
	free(_inputAudioBufferList);
}


void
CoreAudioPCM::hwPropertyChange() {
	printf("hardwarePropertyChangeCallback\n");
	discover();
}

void
CoreAudioPCM::discover() {
	OSStatus err;
	UInt32 propSize = 0;

	// TODO trymutex lock.

	if (_deviceIDs) {
		delete _deviceIDs; _deviceIDs = 0;
		free(_device_ins); _device_ins = 0;
		free(_device_outs); _device_outs = 0;
	}
	_devices.clear();

#ifdef COREAUDIO_108
	AudioObjectPropertyAddress propertyAddress;
	propertyAddress.mSelector = kAudioHardwarePropertyDevices;
	propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	propertyAddress.mElement = kAudioObjectPropertyElementMaster;
	err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propSize);
#else
	err = AudioHardwareGetPropertyInfo (kAudioHardwarePropertyDevices, &propSize, NULL);
#endif

	_numDevices = propSize / sizeof (AudioDeviceID);
	propSize = _numDevices * sizeof (AudioDeviceID);

	_deviceIDs = new AudioDeviceID[_numDevices];
	_device_ins = (uint32_t*) calloc(_numDevices, sizeof(uint32_t));
	_device_outs = (uint32_t*) calloc(_numDevices, sizeof(uint32_t));

#ifdef COREAUDIO_108
	propertyAddress.mSelector = kAudioHardwarePropertyDevices;
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propSize, _deviceIDs);
#else
	err = AudioHardwareGetProperty (kAudioHardwarePropertyDevices, &propSize, _deviceIDs);
#endif

	for (size_t deviceIndex = 0; deviceIndex < _numDevices; deviceIndex++) {
		propSize = 64;
		char deviceName[64];
#ifdef COREAUDIO_108
		propertyAddress.mSelector = kAudioDevicePropertyDeviceName;
		propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
		err = AudioObjectGetPropertyData(_deviceIDs[deviceIndex], &propertyAddress, 0, NULL, &propSize, deviceName);
#else
		err = AudioDeviceGetProperty(_deviceIDs[deviceIndex], 0, 0, kAudioDevicePropertyDeviceName, &propSize, deviceName);
#endif

		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "device name query failed: %i\n", err);
			continue;
		}

		UInt32 size;
		UInt32 outputChannelCount = 0;
		UInt32 inputChannelCount = 0;
		AudioBufferList *bufferList = NULL;

		/* query number of inputs */
#ifdef COREAUDIO_108
		size = 0;
		propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
		propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
		err = AudioObjectGetPropertyDataSize(_deviceIDs[deviceIndex], &propertyAddress, 0, NULL, &size);
		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
			continue;
		}

		bufferList = (AudioBufferList *)(malloc(size));
		assert(bufferList);
		if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); break; }

		err = AudioObjectGetPropertyData(_deviceIDs[deviceIndex], &propertyAddress, 0, NULL, &size, bufferList);

#else
		err = AudioDeviceGetPropertyInfo (_deviceIDs[deviceIndex], 0, AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
			continue;
		}
		bufferList = (AudioBufferList *)(malloc(size));
		assert(bufferList);
		if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); break; }

		bufferList->mNumberBuffers = 0;
		err = AudioDeviceGetProperty(_deviceIDs[deviceIndex], 0, AUHAL_OUTPUT_ELEMENT, kAudioDevicePropertyStreamConfiguration, &size, bufferList);
#endif
		if(kAudioHardwareNoError != err) {
			fprintf(stderr, "kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
			free(bufferList);
			continue;
		}

		for(UInt32 j = 0; j < bufferList->mNumberBuffers; ++j) {
			outputChannelCount += bufferList->mBuffers[j].mNumberChannels;
		}
		free(bufferList);


		/* query number of inputs */
#ifdef COREAUDIO_108
		size = 0;
		propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
		propertyAddress.mScope = kAudioDevicePropertyScopeInput;
		err = AudioObjectGetPropertyDataSize(_deviceIDs[deviceIndex], &propertyAddress, 0, NULL, &size);
		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
			continue;
		}

		bufferList = (AudioBufferList *)(malloc(size));
		assert(bufferList);
		if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); break; }

		err = AudioObjectGetPropertyData(_deviceIDs[deviceIndex], &propertyAddress, 0, NULL, &size, bufferList);
#else
		err = AudioDeviceGetPropertyInfo (_deviceIDs[deviceIndex], 0, AUHAL_INPUT_ELEMENT, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
		if (kAudioHardwareNoError != err) {
			fprintf(stderr, "kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
			continue;
		}
		bufferList = (AudioBufferList *)(malloc(size));
		assert(bufferList);
		if (!bufferList) { fprintf(stderr, "OUT OF MEMORY\n"); break; }

		bufferList->mNumberBuffers = 0;
		err = AudioDeviceGetProperty(_deviceIDs[deviceIndex], 0, AUHAL_INPUT_ELEMENT, kAudioDevicePropertyStreamConfiguration, &size, bufferList);
#endif
		if(kAudioHardwareNoError != err) {
			fprintf(stderr, "kAudioDevicePropertyStreamConfiguration failed: %i\n", err);
			free(bufferList);
			continue;
		}

		for(UInt32 j = 0; j < bufferList->mNumberBuffers; ++j) {
			inputChannelCount += bufferList->mBuffers[j].mNumberChannels;
		}
		free(bufferList);



		{
			std::string dn = deviceName;
			_device_ins[deviceIndex] = inputChannelCount;
			_device_outs[deviceIndex] = outputChannelCount;
			printf("CoreAudio Device: #%ld '%s' in:%d out:%d\n", deviceIndex, deviceName, inputChannelCount, outputChannelCount);
			if (outputChannelCount > 0 && inputChannelCount > 0) {
				_devices.insert (std::pair<size_t, std::string> (deviceIndex, dn));
			}
		}
	}
}

void
CoreAudioPCM::pcm_stop ()
{
	printf("CoreAudioPCM::pcm_stop\n");
	if (!_auhal) return;

	AudioOutputUnitStop(_auhal);
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

	free(_inputAudioBufferList);
	_inputAudioBufferList = 0;

	_error_callback = 0;
	_process_callback = 0;
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



int
CoreAudioPCM::pcm_start (
		uint32_t device_id_in, uint32_t device_id_out,
		uint32_t sample_rate, uint32_t samples_per_period,
		int (process_callback (void*)), void *process_arg)
{

	assert(_deviceIDs);
	_state = -2;

	if (device_id_out >= _numDevices || device_id_in >= _numDevices) {
		return -1;
	}

	_process_callback = process_callback;
	_process_arg = process_arg;
	_max_samples_per_period = samples_per_period;
	_cur_samples_per_period = 0;

	ComponentResult err;
	UInt32 enableIO;
	AudioStreamBasicDescription srcFormat, dstFormat;
	
	AudioComponentDescription cd = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0};
	AudioComponent HALOutput = AudioComponentFindNext(NULL, &cd);
	if (!HALOutput) { goto error; }

	err = AudioComponentInstanceNew(HALOutput, &_auhal);
	if (err != noErr) { goto error; }

	err = AudioUnitInitialize(_auhal);
	if (err != noErr) { goto error; }

	enableIO = 1;
	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, AUHAL_INPUT_ELEMENT, &enableIO, sizeof(enableIO));
	if (err != noErr) { goto error; }
	enableIO = 1;
	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, AUHAL_OUTPUT_ELEMENT, &enableIO, sizeof(enableIO));
	if (err != noErr) { goto error; }

	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT, &_deviceIDs[device_id_out], sizeof(AudioDeviceID));
	if (err != noErr) { goto error; }

	err = AudioUnitSetProperty(_auhal, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT, &_deviceIDs[device_id_in], sizeof(AudioDeviceID));
	if (err != noErr) { goto error; }

	// Set buffer size
	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT, (UInt32*)&_max_samples_per_period, sizeof(UInt32));
	if (err != noErr) { goto error; }
	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT, (UInt32*)&_max_samples_per_period, sizeof(UInt32));
	if (err != noErr) { goto error; }


	// set sample format
	srcFormat.mSampleRate = sample_rate;
	srcFormat.mFormatID = kAudioFormatLinearPCM;
	srcFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
	srcFormat.mBytesPerPacket = sizeof(float);
	srcFormat.mFramesPerPacket = 1;
	srcFormat.mBytesPerFrame = sizeof(float);
	srcFormat.mChannelsPerFrame = _device_ins[device_id_in];
	srcFormat.mBitsPerChannel = 32;

	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, AUHAL_INPUT_ELEMENT, &srcFormat, sizeof(AudioStreamBasicDescription));
	if (err != noErr) { goto error; }

	dstFormat.mSampleRate = sample_rate;
	dstFormat.mFormatID = kAudioFormatLinearPCM;
	dstFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
	dstFormat.mBytesPerPacket = sizeof(float);
	dstFormat.mFramesPerPacket = 1;
	dstFormat.mBytesPerFrame = sizeof(float);
	dstFormat.mChannelsPerFrame = _device_outs[device_id_out];
	dstFormat.mBitsPerChannel = 32;

	err = AudioUnitSetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AUHAL_OUTPUT_ELEMENT, &dstFormat, sizeof(AudioStreamBasicDescription));
	if (err != noErr) { goto error; }

	UInt32 size;
	size = sizeof(AudioStreamBasicDescription);
	err = AudioUnitGetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, AUHAL_INPUT_ELEMENT, &srcFormat, &size);
	if (err != noErr) { goto error; }
	_capture_channels = srcFormat.mChannelsPerFrame;
#ifndef NDEBUG
	PrintStreamDesc(&srcFormat);
#endif

	size = sizeof(AudioStreamBasicDescription);
	err = AudioUnitGetProperty(_auhal, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AUHAL_OUTPUT_ELEMENT, &dstFormat, &size);
	if (err != noErr) { goto error; }
	_playback_channels = dstFormat.mChannelsPerFrame;

#ifndef NDEBUG
	PrintStreamDesc(&dstFormat);
#endif

	_inputAudioBufferList = (AudioBufferList*)malloc(sizeof(UInt32) + _capture_channels * sizeof(AudioBuffer));

	// Setup callbacks
	AURenderCallbackStruct renderCallback;
	memset (&renderCallback, 0, sizeof (renderCallback));
	renderCallback.inputProc = render_callback_ptr;
	renderCallback.inputProcRefCon = this;
	err = AudioUnitSetProperty(_auhal,
			kAudioUnitProperty_SetRenderCallback,
			kAudioUnitScope_Output, AUHAL_OUTPUT_ELEMENT,
			&renderCallback, sizeof (renderCallback));
	if (err != noErr) { goto error; }

	printf("SETUP OK..\n");

	if (AudioOutputUnitStart(_auhal) == noErr) {
		printf("Coreaudio Started..\n");
		_state = 0;
		return 0;
	}

error:
	pcm_stop();
	_state = -3;
	return -1;
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


        _inputAudioBufferList->mNumberBuffers = _capture_channels;
	for (int i = 0; i < _capture_channels; ++i) {
		_inputAudioBufferList->mBuffers[i].mNumberChannels = 1;
		_inputAudioBufferList->mBuffers[i].mDataByteSize = inNumberFrames * sizeof(float);
		_inputAudioBufferList->mBuffers[i].mData = NULL;
	}

        retVal = AudioUnitRender(_auhal, ioActionFlags, inTimeStamp, AUHAL_INPUT_ELEMENT, inNumberFrames, _inputAudioBufferList);

        if (retVal != kAudioHardwareNoError) {
		char *rv = (char*)&retVal;
		printf("ERR %c%c%c%c\n", rv[0], rv[1], rv[2], rv[3]);
		if (_error_callback) {
			_error_callback(_error_arg);
		}
		return retVal;
	}

	_outputAudioBufferList = ioData;

	_in_process = true;

	int rv = -1;

	if (_process_callback) {
		rv = _process_callback(_process_arg);
	}

	_in_process = false;

	if (rv != 0) {
		// clear output
		for (int i = 0; i < ioData->mNumberBuffers; ++i) {
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
	assert(_inputAudioBufferList->mNumberBuffers > chn);
	memcpy((void*)input, (void*)_inputAudioBufferList->mBuffers[chn].mData, sizeof(float) * n_samples);
	return 0;

}
int
CoreAudioPCM::set_playback_channel (uint32_t chn, const float *output, uint32_t n_samples)
{
	if (!_in_process || chn > _playback_channels || n_samples > _cur_samples_per_period) {
		return -1;
	}

	assert(_outputAudioBufferList->mNumberBuffers > chn);
	memcpy((void*)_outputAudioBufferList->mBuffers[chn].mData, (void*)output, sizeof(float) * n_samples);
	return 0;
}
