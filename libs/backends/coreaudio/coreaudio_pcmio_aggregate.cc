/*
 * Copyright (C) 2004-2008 Grame
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <optional>
#include <vector>
#include <cstdlib>

void
CoreAudioPCM::destroy_aggregate_device ()
{
	if (_aggregate_plugin_id == 0) {
		return;
	}

	OSStatus err;

	AudioObjectPropertyAddress property_address;
	property_address.mSelector = kAudioPlugInDestroyAggregateDevice;
	property_address.mScope = kAudioObjectPropertyScopeGlobal;
	property_address.mElement = kAudioObjectPropertyElementMaster;
	UInt32 outDataSize = 0;

	err = AudioObjectGetPropertyDataSize(_aggregate_plugin_id, &property_address, 0, NULL, &outDataSize);
	if (err != noErr) {
		fprintf(stderr, "DestroyAggregateDevice : AudioObjectGetPropertyDataSize error\n");
		return;
	}

	err = AudioObjectGetPropertyData(_aggregate_plugin_id, &property_address, 0, NULL, &outDataSize, &_aggregate_device_id);
	if (err != noErr) {
		fprintf(stderr, "DestroyAggregateDevice : AudioObjectGetPropertyData error\n");
		return;
	}
#ifndef NDEBUG
	printf("DestroyAggregateDevice : OK (plugin: %u device:%u)\n",
			(unsigned int)_aggregate_plugin_id,
			(unsigned int)_aggregate_device_id);
#endif
}

int
CoreAudioPCM::create_aggregate_device (
		AudioDeviceID input_device_id,
		AudioDeviceID output_device_id,
		uint32_t sample_rate,
		AudioDeviceID* created_device)
{
	OSStatus err;
	AudioObjectID sub_device[32];
	UInt32 size = sizeof(sub_device);

	/* look up sub-devices */
	err = GetPropertyWrapper (input_device_id, 0, 0, kAudioAggregateDevicePropertyActiveSubDeviceList, &size, sub_device);
	std::vector<AudioDeviceID> input_device_ids;

	if (err != noErr) {
		input_device_ids.push_back(input_device_id);
	} else {
		uint32_t num_devices = size / sizeof(AudioObjectID);
		for (uint32_t i = 0; i < num_devices; ++i) {
			input_device_ids.push_back(sub_device[i]);
		}
	}

	size = sizeof(sub_device);
	err = GetPropertyWrapper (output_device_id, 0, 0, kAudioAggregateDevicePropertyActiveSubDeviceList, &size, sub_device);
	std::vector<AudioDeviceID> output_device_ids;

	if (err != noErr) {
		output_device_ids.push_back(output_device_id);
	} else {
		uint32_t num_devices = size / sizeof(AudioObjectID);
		for (uint32_t i = 0; i < num_devices; ++i) {
			output_device_ids.push_back(sub_device[i]);
		}
	}

	//---------------------------------------------------------------------------
	// Setup SR of both devices otherwise creating AD may fail...
	//---------------------------------------------------------------------------
	std::optional <UInt32> common_clock_domain;
	UInt32 clockdomain = 0;
	size = sizeof(UInt32);
	bool need_clock_drift_compensation = false;

	for (size_t i = 0; i < input_device_ids.size(); ++i) {
		set_device_sample_rate_id(input_device_ids[i], sample_rate, true);

		// Check clock domain
		err = GetPropertyWrapper (input_device_ids[i], 0, 0, kAudioDevicePropertyClockDomain, &size, &clockdomain);
		if (err == noErr) {
#ifndef NDEBUG
			printf("AggregateDevice: Clock Domain for Input(%zu) = %d\n", i, clockdomain);
#endif
			if (!common_clock_domain.has_value ()) {
				common_clock_domain = clockdomain;
				continue;
			}
			if (clockdomain != common_clock_domain.value ()) {
#ifndef NDEBUG
				printf("AggregateDevice: devices do not share the same clock.\n");
#endif
				need_clock_drift_compensation = true;
			}
		}
	}

	for (size_t i = 0; i < output_device_ids.size(); i++) {
		set_device_sample_rate_id(output_device_ids[i], sample_rate, true);

		// Check clock domain
		err = GetPropertyWrapper (output_device_ids[i], 0, 0, kAudioDevicePropertyClockDomain, &size, &clockdomain);
		if (err == noErr) {
#ifndef NDEBUG
			printf("AggregateDevice: Clock Domain for Output(%zu) = %d\n", i, clockdomain);
#endif
			if (!common_clock_domain.has_value ()) {
				common_clock_domain = clockdomain;
				continue;
			}
			if (clockdomain != common_clock_domain.value ()) {
#ifndef NDEBUG
				printf("AggregateDevice: devices do not share the same clock.\n");
#endif
				need_clock_drift_compensation = true;
			}
		}
	}

	// If no valid clock domain was found, then assume we have to compensate...
	if (!common_clock_domain.has_value ()) {
		need_clock_drift_compensation = true;
	}

#ifndef NDEBUG
	printf("AggregateDevice: need_clock_drift_compensation = %d\n", need_clock_drift_compensation);
#endif

	//---------------------------------------------------------------------------
	// Start to create a new aggregate by getting the base audio hardware plugin
	//---------------------------------------------------------------------------

#ifndef NDEBUG
	char device_name[256];
	for (size_t i = 0; i < input_device_ids.size(); ++i) {
		GetDeviceNameFromID(input_device_ids[i], device_name);
		printf("Separated input = '%s'\n", device_name);
	}

	for (size_t i = 0; i < output_device_ids.size(); ++i) {
		GetDeviceNameFromID(output_device_ids[i], device_name);
		printf("Separated output = '%s'\n", device_name);
	}
#endif

	err = GetHardwarePropertyInfoWrapper (kAudioHardwarePropertyPlugInForBundleID, &size);
	if (err != noErr) {
		fprintf(stderr, "AggregateDevice: AudioHardwareGetPropertyInfo kAudioHardwarePropertyPlugInForBundleID error\n");
		return -1;
	}

	AudioValueTranslation pluginAVT;

	CFStringRef inBundleRef = CFSTR("com.apple.audio.CoreAudio");

	pluginAVT.mInputData = &inBundleRef;
	pluginAVT.mInputDataSize = sizeof(inBundleRef);
	pluginAVT.mOutputData = &_aggregate_plugin_id;
	pluginAVT.mOutputDataSize = sizeof(AudioDeviceID);

	err = GetHardwarePropertyWrapper (kAudioHardwarePropertyPlugInForBundleID, &size, &pluginAVT);
	if (err != noErr) {
		fprintf(stderr, "AggregateDevice: AudioHardwareGetProperty kAudioHardwarePropertyPlugInForBundleID error\n");
		return -1;
	}

	//-------------------------------------------------
	// Create a CFDictionary for our aggregate device
	//-------------------------------------------------

	CFMutableDictionaryRef aggDeviceDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFStringRef AggregateDeviceNameRef = CFSTR("ArdourDuplex");
	CFStringRef AggregateDeviceUIDRef = CFSTR("com.ardour.CoreAudio");
	CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceNameKey), AggregateDeviceNameRef);
	CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceUIDKey), AggregateDeviceUIDRef);

	char const* p = getenv ("ARDOUR_COREAUDIO_DEBUG");
	int debug_flags = 0;
	if (p && *p) {
		debug_flags = atoi (p);
	}

	/* hide from list */
	int value = (debug_flags & 1) ? 0 : 1;
	CFNumberRef AggregateDeviceNumberRef = CFNumberCreate(NULL, kCFNumberIntType, &value);
	CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceIsPrivateKey), AggregateDeviceNumberRef);

	//-------------------------------------------------
	// Create a CFMutableArray for our sub-device list
	//-------------------------------------------------

	// we need to append the UID for each device to a CFMutableArray, so create one here
	CFMutableArrayRef subDevicesArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	std::vector<CFStringRef> captureDeviceUID;
	for (UInt32 i = 0; i < input_device_ids.size(); i++) {
		CFStringRef ref = GetDeviceName(input_device_ids[i]);
		if (ref == NULL) {
			return -1;
		}
		captureDeviceUID.push_back(ref);
		CFArrayAppendValue(subDevicesArray, ref);
	}

	std::vector<CFStringRef> playbackDeviceUID;
	for (UInt32 i = 0; i < output_device_ids.size(); i++) {
		CFStringRef ref = GetDeviceName(output_device_ids[i]);
		if (ref == NULL) {
			return -1;
		}
		playbackDeviceUID.push_back(ref);
		CFArrayAppendValue(subDevicesArray, ref);
	}

	//-----------------------------------------------------------------------
	// Feed the dictionary to the plugin, to create a blank aggregate device
	//-----------------------------------------------------------------------

	AudioObjectPropertyAddress pluginAOPA;
	pluginAOPA.mSelector = kAudioPlugInCreateAggregateDevice;
	pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
	pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
	UInt32 outDataSize = 0;

	err = AudioObjectGetPropertyDataSize(_aggregate_plugin_id, &pluginAOPA, 0, NULL, &outDataSize);
	if (err != noErr) {
		char *rv = (char*)&err;
		fprintf(stderr, "AggregateDevice: AudioObjectGetPropertyDataSize error '%c%c%c%c' 0x%08x\n", rv[0], rv[1], rv[2], rv[3], err);
		goto error;
	}

	err = AudioObjectGetPropertyData(_aggregate_plugin_id, &pluginAOPA, sizeof(aggDeviceDict), &aggDeviceDict, &outDataSize, created_device);
	if (err != noErr) {
		char *rv = (char*)&err;
		fprintf(stderr, "AggregateDevice: AudioObjectGetPropertyData error '%c%c%c%c' 0x%08x\n", rv[0], rv[1], rv[2], rv[3], err);
		goto error;
	}

	// pause for a bit to make sure that everything completed correctly
	// this is to work around a bug in the HAL where a new aggregate device seems to disappear briefly after it is created
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);

	//-------------------------
	// Set the sub-device list
	//-------------------------

	pluginAOPA.mSelector = kAudioAggregateDevicePropertyFullSubDeviceList;
	pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
	pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
	outDataSize = sizeof(CFMutableArrayRef);
	err = AudioObjectSetPropertyData(*created_device, &pluginAOPA, 0, NULL, outDataSize, &subDevicesArray);
	if (err != noErr) {
		fprintf(stderr, "AggregateDevice: AudioObjectSetPropertyData for sub-device list error\n");
		goto error;
	}

	// pause again to give the changes time to take effect
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);

	//-----------------------
	// Set the master device
	//-----------------------

	// set the master device manually (this is the device which will act as the master clock for the aggregate device)
	// pass in the UID of the device you want to use
	pluginAOPA.mSelector = kAudioAggregateDevicePropertyMasterSubDevice;
	pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
	pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
	outDataSize = sizeof(CFStringRef);

	if (debug_flags & 2) {
		/* try capture device first */
		err = AudioObjectSetPropertyData(*created_device, &pluginAOPA, 0, NULL, outDataSize, &captureDeviceUID[0]);
	} else {
		err = AudioObjectSetPropertyData(*created_device, &pluginAOPA, 0, NULL, outDataSize, &playbackDeviceUID[0]);
	}

	if (err != noErr) {
		/* fall back */
		if (debug_flags & 2) {
			fprintf(stderr, "AggregateDevice: AudioObjectSetPropertyData for capture-master device error\n");
			err = AudioObjectSetPropertyData(*created_device, &pluginAOPA, 0, NULL, outDataSize, &playbackDeviceUID[0]);
		} else {
			fprintf(stderr, "AggregateDevice: AudioObjectSetPropertyData for playback-master device error\n");
			err = AudioObjectSetPropertyData(*created_device, &pluginAOPA, 0, NULL, outDataSize, &captureDeviceUID[0]);
		}
	}

	if (err != noErr) {
		fprintf(stderr, "AggregateDevice: AudioObjectSetPropertyData for clock-master device error\n");
		goto error;
	}

	// pause again to give the changes time to take effect
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);

	// Prepare sub-devices for clock drift compensation
	// Workaround for bug in the HAL : until 10.6.2
	if (need_clock_drift_compensation) {

		AudioObjectPropertyAddress theAddressOwned  = { kAudioObjectPropertyOwnedObjects, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
		AudioObjectPropertyAddress theAddressDrift  = { kAudioSubDevicePropertyDriftCompensation, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
		UInt32 theQualifierDataSize = sizeof(AudioObjectID);
		AudioClassID inClass = kAudioSubDeviceClassID;
		void* theQualifierData = &inClass;
		size_t n_subdevices = 0;

#ifndef NDEBUG
		printf("Activate Clock drift compensation...\n");
#endif

		// Get the property data size
		err = AudioObjectGetPropertyDataSize(*created_device, &theAddressOwned, theQualifierDataSize, theQualifierData, &size);
		if (err != noErr) {
			fprintf(stderr, "AggregateDevice: kAudioObjectPropertyOwnedObjects error\n");
		}

		// Calculate the number of object IDs
		n_subdevices = size / sizeof(AudioObjectID);
#ifndef NDEBUG
		printf("AggregateDevice: clock drift compensation, sub-devices = %zu\n", n_subdevices);
#endif
		AudioObjectID subDevices[n_subdevices];
		size = sizeof(subDevices);

		err = AudioObjectGetPropertyData(*created_device, &theAddressOwned, theQualifierDataSize, theQualifierData, &size, subDevices);
		if (err != noErr) {
			fprintf(stderr, "AggregateDevice: kAudioObjectPropertyOwnedObjects error\n");
		}

		// Set kAudioSubDevicePropertyDriftCompensation property...
		for (size_t i = 0; i < n_subdevices; ++i) {
			UInt32 theDriftCompensationValue = 1;
			// TODO exclude master clock device
			err = AudioObjectSetPropertyData(subDevices[i], &theAddressDrift, 0, NULL, sizeof(UInt32), &theDriftCompensationValue);
			if (err != noErr) {
				fprintf(stderr, "AggregateDevice: kAudioSubDevicePropertyDriftCompensation error\n");
			}
		}
	}

	// pause again to give the changes time to take effect
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);

	//----------
	// Clean up
	//----------

	// release the private AD key
	CFRelease(AggregateDeviceNumberRef);

	// release the CF objects we have created - we don't need them any more
	CFRelease(aggDeviceDict);
	CFRelease(subDevicesArray);

	// release the device UID
	for (size_t i = 0; i < captureDeviceUID.size(); ++i) {
		CFRelease(captureDeviceUID[i]);
	}

	for (size_t i = 0; i < playbackDeviceUID.size(); ++i) {
		CFRelease(playbackDeviceUID[i]);
	}

#ifndef NDEBUG
	printf("AggregateDevice: new aggregate device %u\n", (unsigned int)*created_device);
#endif
	return 0;

error:
	destroy_aggregate_device();
	return -1;
}
