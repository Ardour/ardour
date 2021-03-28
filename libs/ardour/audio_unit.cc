/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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

#include <sstream>
#include <fstream>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <boost/algorithm/string.hpp>

#include "pbd/gstdio_compat.h"
#include "pbd/transmitter.h"
#include "pbd/xml++.h"
#include "pbd/convert.h"
#include "pbd/whitespace.h"
#include "pbd/file_utils.h"
#include "pbd/locale_guard.h"

#include <glibmm/threads.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "ardour/ardour.h"
#include "ardour/audio_unit.h"
#include "ardour/audioengine.h"
#include "ardour/audio_buffer.h"
#include "ardour/auv2_scan.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/io.h"
#include "ardour/midi_buffer.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"

#include "CAAudioUnit.h"
#include "CAAUParameter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#ifdef WITH_CARBON
#include <Carbon/Carbon.h>
#endif

#ifdef COREAUDIO105
#define ArdourComponent Component
#define ArdourDescription ComponentDescription
#define ArdourFindNext FindNextComponent
#else
#define ArdourComponent AudioComponent
#define ArdourDescription AudioComponentDescription
#define ArdourFindNext AudioComponentFindNext
#endif

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

static string preset_search_path = "/Library/Audio/Presets:/Network/Library/Audio/Presets";
static string preset_suffix = ".aupreset";
static bool preset_search_path_initialized = false;

static OSStatus
_render_callback(void *userData,
		 AudioUnitRenderActionFlags *ioActionFlags,
		 const AudioTimeStamp    *inTimeStamp,
		 UInt32       inBusNumber,
		 UInt32       inNumberSamples,
		 AudioBufferList*       ioData)
{
	if (userData) {
		return ((AUPlugin*)userData)->render_callback (ioActionFlags, inTimeStamp, inBusNumber, inNumberSamples, ioData);
	}
	return paramErr;
}

static OSStatus
_get_beat_and_tempo_callback (void*    userData,
                              Float64* outCurrentBeat,
                              Float64* outCurrentTempo)
{
	if (userData) {
		return ((AUPlugin*)userData)->get_beat_and_tempo_callback (outCurrentBeat, outCurrentTempo);
	}

	return paramErr;
}

static OSStatus
_get_musical_time_location_callback (void *     userData,
                                     UInt32 *   outDeltaSampleOffsetToNextBeat,
                                     Float32 *  outTimeSig_Numerator,
                                     UInt32 *   outTimeSig_Denominator,
                                     Float64 *  outCurrentMeasureDownBeat)
{
	if (userData) {
		return ((AUPlugin*)userData)->get_musical_time_location_callback (outDeltaSampleOffsetToNextBeat,
										  outTimeSig_Numerator,
										  outTimeSig_Denominator,
										  outCurrentMeasureDownBeat);
	}
	return paramErr;
}

static OSStatus
_get_transport_state_callback (void*     userData,
			       Boolean*  outIsPlaying,
			       Boolean*  outTransportStateChanged,
			       Float64*  outCurrentSampleInTimeLine,
			       Boolean*  outIsCycling,
			       Float64*  outCycleStartBeat,
			       Float64*  outCycleEndBeat)
{
	if (userData) {
		return ((AUPlugin*)userData)->get_transport_state_callback (
			outIsPlaying, outTransportStateChanged,
			outCurrentSampleInTimeLine, outIsCycling,
			outCycleStartBeat, outCycleEndBeat);
	}
	return paramErr;
}


static int
save_property_list (CFPropertyListRef propertyList, Glib::ustring path)

{
	CFDataRef xmlData;
	int fd;

	// Convert the property list into XML data.

	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, propertyList);

	if (!xmlData) {
		error << _("Could not create XML version of property list") << endmsg;
		return -1;
	}

	// Write the XML data to the file.

	fd = open (path.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0664);
	while (fd < 0) {
		if (errno == EEXIST) {
			error << string_compose (_("Preset file %1 exists; not overwriting"),
			                         path) << endmsg;
		} else {
			error << string_compose (_("Cannot open preset file %1 (%2)"),
			                         path, strerror (errno)) << endmsg;
		}
		CFRelease (xmlData);
		return -1;
	}

	size_t cnt = CFDataGetLength (xmlData);

	if (write (fd, CFDataGetBytePtr (xmlData), cnt) != (ssize_t) cnt) {
		CFRelease (xmlData);
		close (fd);
		return -1;
	}

	close (fd);
	return 0;
}


static CFPropertyListRef
load_property_list (Glib::ustring path)
{
	int fd;
	CFPropertyListRef propertyList = 0;
	CFDataRef         xmlData;
	CFStringRef       errorString;

	// Read the XML file.

	if ((fd = open (path.c_str(), O_RDONLY)) < 0) {
		return propertyList;

	}

	off_t len = lseek (fd, 0, SEEK_END);
	char* buf = new char[len];
	lseek (fd, 0, SEEK_SET);

	if (read (fd, buf, len) != len) {
		delete [] buf;
		close (fd);
		return propertyList;
	}

	close (fd);

	xmlData = CFDataCreateWithBytesNoCopy (kCFAllocatorDefault, (UInt8*) buf, len, kCFAllocatorNull);

	// Reconstitute the dictionary using the XML data.

	propertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
							xmlData,
							kCFPropertyListImmutable,
							&errorString);

	CFRelease (xmlData);
	delete [] buf;

	return propertyList;
}

//-----------------------------------------------------------------------------
static void
set_preset_name_in_plist (CFPropertyListRef plist, string preset_name)
{
	if (!plist) {
		return;
	}
	CFStringRef pn = CFStringCreateWithCString (kCFAllocatorDefault, preset_name.c_str(), kCFStringEncodingUTF8);

	if (CFGetTypeID (plist) == CFDictionaryGetTypeID()) {
		CFDictionarySetValue ((CFMutableDictionaryRef)plist, CFSTR(kAUPresetNameKey), pn);
	}

	CFRelease (pn);
}

//-----------------------------------------------------------------------------
static std::string
get_preset_name_in_plist (CFPropertyListRef plist)
{
	std::string ret;

	if (!plist) {
		return ret;
	}

	if (CFGetTypeID (plist) == CFDictionaryGetTypeID()) {
		const void *p = CFDictionaryGetValue ((CFMutableDictionaryRef)plist, CFSTR(kAUPresetNameKey));
		if (p) {
			CFStringRef str = (CFStringRef) p;
			int len = CFStringGetLength(str);
			len =  (len * 2) + 1;
			char local_buffer[len];
			if (CFStringGetCString (str, local_buffer, len, kCFStringEncodingUTF8)) {
				ret = local_buffer;
			}
		}
	}
	return ret;
}

//--------------------------------------------------------------------------
// general implementation for ComponentDescriptionsMatch() and ComponentDescriptionsMatch_Loosely()
// if inIgnoreType is true, then the type code is ignored in the ComponentDescriptions
Boolean ComponentDescriptionsMatch_General(const ArdourDescription * inComponentDescription1, const ArdourDescription * inComponentDescription2, Boolean inIgnoreType);
Boolean ComponentDescriptionsMatch_General(const ArdourDescription * inComponentDescription1, const ArdourDescription * inComponentDescription2, Boolean inIgnoreType)
{
	if ( (inComponentDescription1 == NULL) || (inComponentDescription2 == NULL) )
		return FALSE;

	if ( (inComponentDescription1->componentSubType == inComponentDescription2->componentSubType)
			&& (inComponentDescription1->componentManufacturer == inComponentDescription2->componentManufacturer) )
	{
		// only sub-type and manufacturer IDs need to be equal
		if (inIgnoreType)
			return TRUE;
		// type, sub-type, and manufacturer IDs all need to be equal in order to call this a match
		else if (inComponentDescription1->componentType == inComponentDescription2->componentType)
			return TRUE;
	}

	return FALSE;
}

//--------------------------------------------------------------------------
// general implementation for ComponentAndDescriptionMatch() and ComponentAndDescriptionMatch_Loosely()
// if inIgnoreType is true, then the type code is ignored in the ComponentDescriptions
Boolean ComponentAndDescriptionMatch_General(ArdourComponent inComponent, const ArdourDescription * inComponentDescription, Boolean inIgnoreType);
Boolean ComponentAndDescriptionMatch_General(ArdourComponent inComponent, const ArdourDescription * inComponentDescription, Boolean inIgnoreType)
{
	OSErr status;
	ArdourDescription desc;

	if ( (inComponent == NULL) || (inComponentDescription == NULL) )
		return FALSE;

	// get the ComponentDescription of the input Component
#ifdef COREAUDIO105
	status = GetComponentInfo(inComponent, &desc, NULL, NULL, NULL);
#else
	status = AudioComponentGetDescription (inComponent, &desc);
#endif
	if (status != noErr)
		return FALSE;

	// check if the Component's ComponentDescription matches the input ComponentDescription
	return ComponentDescriptionsMatch_General(&desc, inComponentDescription, inIgnoreType);
}

//--------------------------------------------------------------------------
// determine if 2 ComponentDescriptions are basically equal
// (by that, I mean that the important identifying values are compared,
// but not the ComponentDescription flags)
Boolean ComponentDescriptionsMatch(const ArdourDescription * inComponentDescription1, const ArdourDescription * inComponentDescription2)
{
	return ComponentDescriptionsMatch_General(inComponentDescription1, inComponentDescription2, FALSE);
}

//--------------------------------------------------------------------------
// determine if 2 ComponentDescriptions have matching sub-type and manufacturer codes
Boolean ComponentDescriptionsMatch_Loose(const ArdourDescription * inComponentDescription1, const ArdourDescription * inComponentDescription2)
{
	return ComponentDescriptionsMatch_General(inComponentDescription1, inComponentDescription2, TRUE);
}

//--------------------------------------------------------------------------
// determine if a ComponentDescription basically matches that of a particular Component
Boolean ComponentAndDescriptionMatch(ArdourComponent inComponent, const ArdourDescription * inComponentDescription)
{
	return ComponentAndDescriptionMatch_General(inComponent, inComponentDescription, FALSE);
}

//--------------------------------------------------------------------------
// determine if a ComponentDescription matches only the sub-type and manufacturer codes of a particular Component
Boolean ComponentAndDescriptionMatch_Loosely(ArdourComponent inComponent, const ArdourDescription * inComponentDescription)
{
	return ComponentAndDescriptionMatch_General(inComponent, inComponentDescription, TRUE);
}


AUPlugin::AUPlugin (AudioEngine& engine, Session& session, boost::shared_ptr<CAComponent> _comp)
	: Plugin (engine, session)
	, comp (_comp)
	, unit (new CAAudioUnit)
	, initialized (false)
	, _last_nframes (0)
	, _requires_fixed_size_buffers (false)
	, buffers (0)
	, variable_inputs (false)
	, variable_outputs (false)
	, configured_input_busses (0)
	, configured_output_busses (0)
	, bus_inputs (0)
	, bus_inused (0)
	, bus_outputs (0)
	, input_maxbuf (0)
	, input_offset (0)
	, cb_offsets (0)
	, input_buffers (0)
	, input_map (0)
	, samples_processed (0)
	, _parameter_listener (0)
	, _parameter_listener_arg (0)
	, transport_sample (0)
	, transport_speed (0)
	, last_transport_speed (0.0)
	, preset_holdoff (0)
{
	if (!preset_search_path_initialized) {
		Glib::ustring p = Glib::get_home_dir();
		p += "/Library/Audio/Presets:";
		p += preset_search_path;
		preset_search_path = p;
		preset_search_path_initialized = true;
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Preset Path: %1\n", preset_search_path));
	}

	init ();
}


AUPlugin::AUPlugin (const AUPlugin& other)
	: Plugin (other)
	, comp (other.get_comp())
	, unit (new CAAudioUnit)
	, initialized (false)
	, _last_nframes (0)
	, _requires_fixed_size_buffers (false)
	, buffers (0)
	, variable_inputs (false)
	, variable_outputs (false)
	, configured_input_busses (0)
	, configured_output_busses (0)
	, bus_inputs (0)
	, bus_inused (0)
	, bus_outputs (0)
	, input_maxbuf (0)
	, input_offset (0)
	, cb_offsets (0)
	, input_buffers (0)
	, input_map (0)
	, samples_processed (0)
	, _parameter_listener (0)
	, _parameter_listener_arg (0)
	, transport_sample (0)
	, transport_speed (0)
	, last_transport_speed (0.0)
	, preset_holdoff (0)

{
	init ();

	XMLNode root (other.state_node_name ());
	other.add_state (&root);
	set_state (root, Stateful::loading_state_version);

	for (size_t i = 0; i < descriptors.size(); ++i) {
		set_parameter (i, other.get_parameter (i), 0);
	}
}

AUPlugin::~AUPlugin ()
{
	if (_parameter_listener) {
		AUListenerDispose (_parameter_listener);
		_parameter_listener = 0;
	}

	if (unit) {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "about to call uninitialize in plugin destructor\n");
		unit->Uninitialize ();
	}

	free (buffers);
	free (bus_inputs);
	free (bus_inused);
	free (bus_outputs);
	free (cb_offsets);
}

void
AUPlugin::discover_factory_presets ()
{
	CFArrayRef presets;
	UInt32 dataSize;
	Boolean isWritable;
	OSStatus err;

	if ((err = unit->GetPropertyInfo (kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, &dataSize, &isWritable)) != 0) {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "no factory presets for AU\n");
		return;
	}

	assert (dataSize == sizeof (presets));

	if ((err = unit->GetProperty (kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, (void*) &presets, &dataSize)) != 0) {
		error << string_compose (_("cannot get factory preset info: errcode %1"), err) << endmsg;
		return;
	}

	if (!presets) {
		return;
	}

	CFIndex cnt = CFArrayGetCount (presets);

	for (CFIndex i = 0; i < cnt; ++i) {
		AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex (presets, i);

		string name = CFStringRefToStdString (preset->presetName);
		factory_preset_map[name] = preset->presetNumber;
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Factory Preset: %1 > %2\n", name, preset->presetNumber));
	}

	CFRelease (presets);
}

void
AUPlugin::init ()
{
	g_atomic_int_set (&_current_latency, UINT_MAX);

	OSErr err;

	/* these keep track of *configured* channel set up,
	 * not potential set ups.
	 */

	input_channels = -1;
	output_channels = -1;

	try {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "opening AudioUnit\n");
		err = CAAudioUnit::Open (*(comp.get()), *unit);
	} catch (...) {
		error << _("Exception thrown during AudioUnit plugin loading - plugin ignored") << endmsg;
		throw failed_constructor();
	}

	if (err != noErr) {
		error << _("AudioUnit: Could not convert CAComponent to CAAudioUnit") << endmsg;
		throw failed_constructor ();
	}

	DEBUG_TRACE (DEBUG::AudioUnitConfig, "count global elements\n");
	unit->GetElementCount (kAudioUnitScope_Global, global_elements);
	DEBUG_TRACE (DEBUG::AudioUnitConfig, "count input elements\n");
	unit->GetElementCount (kAudioUnitScope_Input, input_elements);
	DEBUG_TRACE (DEBUG::AudioUnitConfig, "count output elements\n");
	unit->GetElementCount (kAudioUnitScope_Output, output_elements);


	if (input_elements > 0) {
		cb_offsets = (samplecnt_t*) calloc (input_elements, sizeof(samplecnt_t));
		bus_inputs = (uint32_t*) calloc (input_elements, sizeof(uint32_t));
		bus_inused = (uint32_t*) calloc (input_elements, sizeof(uint32_t));
	}
	if (output_elements > 0) {
		bus_outputs = (uint32_t*) calloc (output_elements, sizeof(uint32_t));
	}

	for (size_t i = 0; i < output_elements; ++i) {
		unit->Reset (kAudioUnitScope_Output, i);
		AudioStreamBasicDescription fmt;
		err = unit->GetFormat (kAudioUnitScope_Output, i, fmt);
		if (err == noErr) {
			bus_outputs[i] = fmt.mChannelsPerFrame;
		}
		CFStringRef name;
		UInt32 sz = sizeof (CFStringRef);
		if (AudioUnitGetProperty (unit->AU(), kAudioUnitProperty_ElementName, kAudioUnitScope_Output,
					i, &name, &sz) == noErr
				&& sz > 0) {
			_bus_name_out.push_back (CFStringRefToStdString (name));
			CFRelease(name);
		} else {
			_bus_name_out.push_back (string_compose ("Audio-Bus %1", i));
		}
	}

	for (size_t i = 0; i < input_elements; ++i) {
		unit->Reset (kAudioUnitScope_Input, i);
		AudioStreamBasicDescription fmt;
		err = unit->GetFormat (kAudioUnitScope_Input, i, fmt);
		if (err == noErr) {
			bus_inputs[i] = fmt.mChannelsPerFrame;
			bus_inused[i] = bus_inputs[i];
		}
		CFStringRef name;
		UInt32 sz = sizeof (CFStringRef);
		if (AudioUnitGetProperty (unit->AU(), kAudioUnitProperty_ElementName, kAudioUnitScope_Input,
					i, &name, &sz) == noErr
				&& sz > 0) {
			_bus_name_in.push_back (CFStringRefToStdString (name));
			CFRelease(name);
		} else {
			_bus_name_in.push_back (string_compose ("Audio-Bus %1", i));
		}
	}

	/* tell the plugin about tempo/meter/transport callbacks in case it wants them */

	HostCallbackInfo info;
	memset (&info, 0, sizeof (HostCallbackInfo));
	info.hostUserData = this;
	info.beatAndTempoProc = _get_beat_and_tempo_callback;
	info.musicalTimeLocationProc = _get_musical_time_location_callback;
	info.transportStateProc = _get_transport_state_callback;

	//ignore result of this - don't care if the property isn't supported
	DEBUG_TRACE (DEBUG::AudioUnitConfig, "set host callbacks in global scope\n");
	unit->SetProperty (kAudioUnitProperty_HostCallbacks,
			   kAudioUnitScope_Global,
			   0, //elementID
			   &info,
			   sizeof (HostCallbackInfo));

	if (set_block_size (_session.get_block_size())) {
		error << _("AUPlugin: cannot set processing block size") << endmsg;
		throw failed_constructor();
	}

	create_parameter_listener (AUPlugin::_parameter_change_listener, this, 0.05);
	discover_parameters ();
	discover_factory_presets ();

	// Plugin::setup_controls ();
}

void
AUPlugin::discover_parameters ()
{
	/* discover writable parameters */

	AudioUnitScope scopes[] = {
		kAudioUnitScope_Global,
		kAudioUnitScope_Output,
		kAudioUnitScope_Input
	};

	descriptors.clear ();

	for (uint32_t i = 0; i < sizeof (scopes) / sizeof (scopes[0]); ++i) {

		AUParamInfo param_info (unit->AU(), false, /* include read only */ true, scopes[i]);

		for (uint32_t i = 0; i < param_info.NumParams(); ++i) {

			AUParameterDescriptor d;

			d.id = param_info.ParamID (i);

			const CAAUParameter* param = param_info.GetParamInfo (d.id);
			const AudioUnitParameterInfo& info (param->ParamInfo());

			const int len = CFStringGetLength (param->GetName());
			char local_buffer[len*2];
			Boolean good = CFStringGetCString (param->GetName(), local_buffer ,len*2 , kCFStringEncodingUTF8);
			if (!good) {
				d.label = "???";
			} else {
				d.label = local_buffer;
			}

			d.scope = param_info.GetScope ();
			d.element = param_info.GetElement ();

			/* info.units to consider */
			/*
			  kAudioUnitParameterUnit_Generic             = 0
			  kAudioUnitParameterUnit_Indexed             = 1
			  kAudioUnitParameterUnit_Boolean             = 2
			  kAudioUnitParameterUnit_Percent             = 3
			  kAudioUnitParameterUnit_Seconds             = 4
			  kAudioUnitParameterUnit_SampleFrames        = 5
			  kAudioUnitParameterUnit_Phase               = 6
			  kAudioUnitParameterUnit_Rate                = 7
			  kAudioUnitParameterUnit_Hertz               = 8
			  kAudioUnitParameterUnit_Cents               = 9
			  kAudioUnitParameterUnit_RelativeSemiTones   = 10
			  kAudioUnitParameterUnit_MIDINoteNumber      = 11
			  kAudioUnitParameterUnit_MIDIController      = 12
			  kAudioUnitParameterUnit_Decibels            = 13
			  kAudioUnitParameterUnit_LinearGain          = 14
			  kAudioUnitParameterUnit_Degrees             = 15
			  kAudioUnitParameterUnit_EqualPowerCrossfade = 16
			  kAudioUnitParameterUnit_MixerFaderCurve1    = 17
			  kAudioUnitParameterUnit_Pan                 = 18
			  kAudioUnitParameterUnit_Meters              = 19
			  kAudioUnitParameterUnit_AbsoluteCents       = 20
			  kAudioUnitParameterUnit_Octaves             = 21
			  kAudioUnitParameterUnit_BPM                 = 22
			  kAudioUnitParameterUnit_Beats               = 23
			  kAudioUnitParameterUnit_Milliseconds        = 24
			  kAudioUnitParameterUnit_Ratio               = 25
			*/

			/* info.flags to consider */

			/*

			  kAudioUnitParameterFlag_CFNameRelease       = (1L << 4)
			  kAudioUnitParameterFlag_HasClump            = (1L << 20)
			  kAudioUnitParameterFlag_HasName             = (1L << 21)
			  kAudioUnitParameterFlag_DisplayLogarithmic  = (1L << 22)
			  kAudioUnitParameterFlag_IsHighResolution    = (1L << 23)
			  kAudioUnitParameterFlag_NonRealTime         = (1L << 24)
			  kAudioUnitParameterFlag_CanRamp             = (1L << 25)
			  kAudioUnitParameterFlag_ExpertMode          = (1L << 26)
			  kAudioUnitParameterFlag_HasCFNameString     = (1L << 27)
			  kAudioUnitParameterFlag_IsGlobalMeta        = (1L << 28)
			  kAudioUnitParameterFlag_IsElementMeta       = (1L << 29)
			  kAudioUnitParameterFlag_IsReadable          = (1L << 30)
			  kAudioUnitParameterFlag_IsWritable          = (1L << 31)
			*/

			d.lower = info.minValue;
			d.upper = info.maxValue;
			d.normal = info.defaultValue;

			d.integer_step = (info.unit == kAudioUnitParameterUnit_Indexed);
			d.toggled = (info.unit == kAudioUnitParameterUnit_Boolean) ||
				(d.integer_step && ((d.upper - d.lower) == 1.0));
			d.sr_dependent = (info.unit == kAudioUnitParameterUnit_SampleFrames);
			d.automatable = /* !d.toggled && -- ardour can automate toggles, can AU ? */
				!(info.flags & kAudioUnitParameterFlag_NonRealTime) &&
				(info.flags & kAudioUnitParameterFlag_IsWritable);

			d.logarithmic = (info.flags & kAudioUnitParameterFlag_DisplayLogarithmic);
			d.au_unit = info.unit;
			switch (info.unit) {
			case kAudioUnitParameterUnit_Decibels:
				d.unit = ParameterDescriptor::DB;
				break;
			case kAudioUnitParameterUnit_MIDINoteNumber:
				d.unit = ParameterDescriptor::MIDI_NOTE;
				break;
			case kAudioUnitParameterUnit_Hertz:
				d.unit = ParameterDescriptor::HZ;
				break;
			}

			d.update_steps();

			descriptors.push_back (d);

			uint32_t last_param = descriptors.size() - 1;
			parameter_map.insert (pair<uint32_t,uint32_t> (d.id, last_param));
			listen_to_parameter (last_param);
		}
	}
}

string
AUPlugin::unique_id () const
{
	assert (_info->unique_id == auv2_stringify_descriptor (comp->Desc()));
	return auv2_stringify_descriptor (comp->Desc());
}

const char *
AUPlugin::label () const
{
	return _info->name.c_str();
}

uint32_t
AUPlugin::parameter_count () const
{
	return descriptors.size();
}

float
AUPlugin::default_value (uint32_t port)
{
	if (port < descriptors.size()) {
		return descriptors[port].normal;
	}

	return 0;
}

samplecnt_t
AUPlugin::plugin_latency () const
{
	guint lat = g_atomic_int_get (&_current_latency);;
	if (lat == UINT_MAX) {
		lat = unit->Latency() * _session.sample_rate();
		g_atomic_int_set (&_current_latency, lat);
	}
	return lat;
}

void
AUPlugin::set_parameter (uint32_t which, float val, sampleoffset_t when)
{
	if (which >= descriptors.size()) {
		return;
	}

	if (get_parameter(which) == val) {
		return;
	}

	const AUParameterDescriptor& d (descriptors[which]);
	DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("set parameter %1 in scope %2 element %3 to %4\n", d.id, d.scope, d.element, val));
	unit->SetParameter (d.id, d.scope, d.element, val);

	/* tell the world what we did */

	AudioUnitEvent theEvent;

	theEvent.mEventType = kAudioUnitEvent_ParameterValueChange;
	theEvent.mArgument.mParameter.mAudioUnit = unit->AU();
	theEvent.mArgument.mParameter.mParameterID = d.id;
	theEvent.mArgument.mParameter.mScope = d.scope;
	theEvent.mArgument.mParameter.mElement = d.element;

	DEBUG_TRACE (DEBUG::AudioUnitProcess, "notify about parameter change\n");
        /* Note the 1st argument, which means "Don't notify us about a change we made ourselves" */
        AUEventListenerNotify (_parameter_listener, NULL, &theEvent);

	Plugin::set_parameter (which, val, when);
}

float
AUPlugin::get_parameter (uint32_t which) const
{
	float val = 0.0;
	if (which < descriptors.size()) {
		const AUParameterDescriptor& d (descriptors[which]);
		DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("get value of parameter %1 in scope %2 element %3\n", d.id, d.scope, d.element));
		unit->GetParameter(d.id, d.scope, d.element, val);
	}
	return val;
}

int
AUPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& pd) const
{
	if (which < descriptors.size()) {
		pd = descriptors[which];
		return 0;
	}
	return -1;
}

uint32_t
AUPlugin::nth_parameter (uint32_t which, bool& ok) const
{
	if (which < descriptors.size()) {
		ok = true;
		return which;
	}
	ok = false;
	return 0;
}

void
AUPlugin::activate ()
{
	if (!initialized) {
		OSErr err;
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "call Initialize in activate()\n");
		if ((err = unit->Initialize()) != noErr) {
			error << string_compose (_("AUPlugin: %1 cannot initialize plugin (err = %2)"), name(), err) << endmsg;
		} else {
			samples_processed = 0;
			initialized = true;
		}
	}
}

void
AUPlugin::deactivate ()
{
	DEBUG_TRACE (DEBUG::AudioUnitConfig, "call Uninitialize in deactivate()\n");
	unit->Uninitialize ();
	initialized = false;
}

void
AUPlugin::flush ()
{
	DEBUG_TRACE (DEBUG::AudioUnitConfig, "call Reset in flush()\n");
	unit->GlobalReset ();
}

bool
AUPlugin::requires_fixed_size_buffers() const
{
	return _requires_fixed_size_buffers;
}


int
AUPlugin::set_block_size (pframes_t nframes)
{
	bool was_initialized = initialized;
	UInt32 numSamples = nframes;
	OSErr err;

	if (initialized) {
		deactivate ();
	}

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("set MaximumFramesPerSlice in global scope to %1\n", numSamples));
	if ((err = unit->SetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
				      0, &numSamples, sizeof (numSamples))) != noErr) {
		error << string_compose (_("AU: cannot set max samples (err = %1)"), err) << endmsg;
		return -1;
	}

	if (was_initialized) {
		activate ();
	}

	return 0;
}

bool
AUPlugin::reconfigure_io (ChanCount in, ChanCount aux_in, ChanCount out)
{
	AudioStreamBasicDescription streamFormat;
	bool was_initialized = initialized;

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("AUPlugin::reconfigure_io %1 for in: %2 aux-in %3 out: %4 out\n", name(), in, aux_in, out));

	//TODO handle cases of no-input, only sidechain
	// (needs special-casing of configured_input_busses)
	if (input_elements == 1 || in.n_audio () == 0) {
		in += aux_in;
		aux_in.reset ();
	}

	const int32_t audio_in = in.n_audio();
	const int32_t audio_out = out.n_audio();

	if (initialized) {
		/* if we are already running with the requested i/o config, bail out here */
		if ((audio_in + aux_in.n_audio () == input_channels) && (audio_out == output_channels)) {
			return true;
		} else {
			deactivate ();
		}
	}

	streamFormat.mSampleRate  = _session.sample_rate();
	streamFormat.mFormatID    = kAudioFormatLinearPCM;
	streamFormat.mFormatFlags = kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved;

#ifdef __LITTLE_ENDIAN__
	/* relax */
#else
	streamFormat.mFormatFlags |= kAudioFormatFlagIsBigEndian;
#endif

	streamFormat.mBitsPerChannel = 32;
	streamFormat.mFramesPerPacket = 1;

	/* apple says that for non-interleaved data, these
	 * values always refer to a single channel.
	 */
	streamFormat.mBytesPerPacket = 4;
	streamFormat.mBytesPerFrame = 4;

	configured_input_busses  = 0;
	configured_output_busses = 0;

	/* reset busses */
	for (size_t i = 0; i < output_elements; ++i) {
		unit->Reset (kAudioUnitScope_Output, i);
	}
	for (size_t i = 0; i < input_elements; ++i) {
		bus_inused[i] = 0;
		unit->Reset (kAudioUnitScope_Input, i);
		/* remove any input callbacks */
		AURenderCallbackStruct renderCallbackInfo;
		renderCallbackInfo.inputProc = 0;
		renderCallbackInfo.inputProcRefCon = 0;
		unit->SetProperty (kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, i, (void*) &renderCallbackInfo, sizeof(renderCallbackInfo));
	}

	/* now assign the channels to available busses */
	uint32_t used_in = 0;
	uint32_t used_out = 0;

	if (input_elements == 0 || audio_in == 0) {
		configured_input_busses = 0;
	} else if (variable_inputs || input_elements == 1 || audio_in < bus_inputs[0]) {
		/* we only ever use the first bus and configure it to match */
		if (variable_inputs && input_elements > 1) {
			info << string_compose (_("AU %1 has multiple input busses and variable port count."), name()) << endmsg;
		}
		streamFormat.mChannelsPerFrame = audio_in;
		if (set_stream_format (kAudioUnitScope_Input, 0, streamFormat) != 0) {
			warning << string_compose (_("AU %1 failed to reconfigure input: %2"), name(), audio_in) << endmsg;
			return false;
		}
		bus_inused[0] = audio_in;
		configured_input_busses = 1;
		used_in = audio_in;
	} else {
		/* more inputs than the first bus' channel-count: distribute sequentially */
		configured_input_busses = 0;
		uint32_t remain = audio_in + aux_in.n_audio ();
		aux_in.reset (); /* now taken care of */
		for (uint32_t bus = 0; remain > 0 && bus < input_elements; ++bus) {
			uint32_t cnt = std::min (remain, bus_inputs[bus]);
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("%1 configure input bus: %2 chn: %3", name(), bus, cnt));

			streamFormat.mChannelsPerFrame = cnt;
			if (set_stream_format (kAudioUnitScope_Input, bus, streamFormat) != 0) {
				if (cnt > 0) {
					return false;
				}
			}
			bus_inused[bus] = cnt;
			used_in += cnt;
			remain -= cnt;
			if (cnt == 0) { continue; }
			++configured_input_busses;
		}
	}

	/* add additional busses, connect aux-inputs */
	if (configured_input_busses == 1 && aux_in.n_audio () > 0 && input_elements > 1) {
		uint32_t remain = aux_in.n_audio ();
		for (uint32_t bus = 1; remain > 0 && bus < input_elements; ++bus) {
			uint32_t cnt = std::min (remain, bus_inputs[bus]);
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("%1 configure aux input bus: %2 chn: %3", name(), bus, cnt));

			streamFormat.mChannelsPerFrame = cnt;
			if (set_stream_format (kAudioUnitScope_Input, bus, streamFormat) != 0) {
				if (cnt > 0) {
					return false;
				}
			}
			bus_inused[bus] = cnt;
			used_in += cnt;
			remain -= cnt;
			if (cnt == 0) { continue; }
			++configured_input_busses;
		}
	}

	if (variable_outputs || output_elements == 1) {
		if (output_elements > 1) {
			warning << string_compose (_("AU %1 has multiple output busses and variable port count."), name()) << endmsg;
		}

		streamFormat.mChannelsPerFrame = audio_out;
		if (set_stream_format (kAudioUnitScope_Output, 0, streamFormat) != 0) {
			warning << string_compose (_("AU %1 failed to reconfigure output: %2"), name(), audio_out) << endmsg;
			return false;
		}
		configured_output_busses = 1;
		used_out = audio_out;
	} else {
		uint32_t remain = audio_out;
		configured_output_busses = 0;
		for (uint32_t bus = 0; remain > 0 && bus < output_elements; ++bus) {
			uint32_t cnt = std::min (remain, bus_outputs[bus]);
			if (cnt == 0) { continue; }
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("%1 configure output bus: %2 chn: %3", name(), bus, cnt));
			streamFormat.mChannelsPerFrame = cnt;
			if (set_stream_format (kAudioUnitScope_Output, bus, streamFormat) != 0) {
				return false;
			}
			used_out += cnt;
			remain -= cnt;
			++configured_output_busses;
		}
	}

	for (size_t i = 0; used_in > 0 && i < configured_input_busses; ++i) {
		/* setup render callback: the plugin calls this to get input data */
		AURenderCallbackStruct renderCallbackInfo;
		renderCallbackInfo.inputProc = _render_callback;
		renderCallbackInfo.inputProcRefCon = this;
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "set render callback in input scope\n");
		OSErr err;
		if ((err = unit->SetProperty (kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
						i, (void*) &renderCallbackInfo, sizeof(renderCallbackInfo))) != 0) {
			error << string_compose (_("AU: %1 cannot install render callback (err = %2)"), name(), err) << endmsg;
		}
	}

	free (buffers);
	buffers = (AudioBufferList *) malloc (offsetof(AudioBufferList, mBuffers) + used_out * sizeof(::AudioBuffer));

	input_channels = used_in;
	output_channels = used_out;

	/* reset plugin info to show currently configured state */
	_info->n_inputs = ChanCount (DataType::AUDIO, used_in) + ChanCount (DataType::MIDI, _has_midi_input ? 1 : 0);
	_info->n_outputs = ChanCount (DataType::AUDIO, used_out);

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("AUPlugin::configured %1 used-in: %2 used-out %3, in-bus: %4 out-bus: %5, I/O %6 %7\n",
	             name(), used_in, used_out, configured_input_busses, configured_output_busses, _info->n_inputs, _info->n_outputs));

	if (was_initialized) {
		activate ();
	}

	return true;
}

ChanCount
AUPlugin::input_streams() const
{
	ChanCount c;
	if (input_channels < 0) {
		// force PluginIoReConfigure -- see also commit msg e38eb06
		c.set (DataType::AUDIO, 0);
		c.set (DataType::MIDI, 0);
	} else {
		c.set (DataType::AUDIO, input_channels);
		c.set (DataType::MIDI, _has_midi_input ? 1 : 0);
	}
	return c;
}


ChanCount
AUPlugin::output_streams() const
{
	ChanCount c;
	if (output_channels < 0) {
		// force PluginIoReConfigure - see also commit msg e38eb06
		c.set (DataType::AUDIO, 0);
		c.set (DataType::MIDI, 0);
	} else {
		c.set (DataType::AUDIO, output_channels);
		c.set (DataType::MIDI, _has_midi_output ? 1 : 0);
	}
	return c;
}

bool
AUPlugin::match_variable_io (ChanCount& in, ChanCount& aux_in, ChanCount& out)
{
	_output_configs.clear ();

	/* if the plugin has no input busses, treat side-chain as normal input */
	const int32_t audio_in = in.n_audio() + ((input_elements == 1) ? aux_in.n_audio() : 0);
	/* preferred setting (provided by plugin_insert) */
	const int32_t preferred_out = out.n_audio ();

	AUPluginInfoPtr pinfo = boost::dynamic_pointer_cast<AUPluginInfo>(get_info());
	vector<pair<int,int> > io_configs = pinfo->io_configs;

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::AudioUnitConfig)) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, string_compose ("AU Initial I/O Config list for %1 n_cfg: %2, in-bus %4 out-bus: %5\n", name(), io_configs.size(), input_elements, output_elements));
		for (vector<pair<int,int> >::iterator i = io_configs.begin(); i != io_configs.end(); ++i) {
			DEBUG_STR_APPEND(a, string_compose (" - I/O  %1 / %2\n", i->first, i->second));
		}
		DEBUG_TRACE (DEBUG::AudioUnitConfig, DEBUG_STR(a).str());
	}
#endif

	/* add output busses as sum to possible outputs */
#ifndef NDEBUG
	bool outs_added = false;
#endif
	if (output_elements > 1) {
		const vector<pair<int,int> >& ioc (pinfo->io_configs);
		for (vector<pair<int,int> >::const_iterator i = ioc.begin(); i != ioc.end(); ++i) {
			int32_t possible_in = i->first;
			int32_t possible_out = i->second;
			if (possible_out < 0) {
				continue;
			}
			for (uint32_t i = 1; i < output_elements; ++i) {
				int32_t c = bus_outputs[i];
				for (uint32_t j = 1; j < i; ++j) {
					c += bus_outputs [j];
				}
				io_configs.push_back (pair<int,int> (possible_in, possible_out + c));
			}
#ifndef NDEBUG
			outs_added = true;
#endif
			/* only add additional, optional busses to first available config.
			 * AUPluginInfo::cached_io_configuration () already incrementally
			 * adds busses (for instruments w/ multiple configurations)
			 */
			break;
		}
	}

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("%1 has %2 IO configurations, looking for in: %3 aux: %4 out: %5\n", name(), io_configs.size(), in, aux_in, out));

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::AudioUnitConfig) && outs_added) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, string_compose ("AU Final I/O Config list for %1 n_cfg: %2\n", name(), io_configs.size()));
		for (vector<pair<int,int> >::iterator i = io_configs.begin(); i != io_configs.end(); ++i) {
			DEBUG_STR_APPEND(a, string_compose (" - I/O  %1 / %2\n", i->first, i->second));
		}
		DEBUG_TRACE (DEBUG::AudioUnitConfig, DEBUG_STR(a).str());
	}
#endif

	/* kAudioUnitProperty_SupportedNumChannels
	 * https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/TheAudioUnit/TheAudioUnit.html#//apple_ref/doc/uid/TP40003278-CH12-SW20
	 *
	 * - both fields are -1
	 *   e.g. inChannels = -1 outChannels = -1
	 *    This is the default case. Any number of input and output channels, as long as the numbers match
	 *
	 * - one field is -1, the other field is positive
	 *   e.g. inChannels = -1 outChannels = 2
	 *    Any number of input channels, exactly two output channels
	 *
	 * - one field is -1, the other field is -2
	 *   e.g. inChannels = -1 outChannels = -2
	 *    Any number of input channels, any number of output channels
	 *
	 * - both fields have non-negative values
	 *   e.g. inChannels = 2 outChannels = 6
	 *    Exactly two input channels, exactly six output channels
	 *   e.g. inChannels = 0 outChannels = 2
	 *    No input channels, exactly two output channels (such as for an instrument unit with stereo output)
	 *
	 * - both fields have negative values, neither of which is –1 or –2
	 *   e.g. inChannels = -4 outChannels = -8
	 *    Up to four input channels and up to eight output channels
	 */

	int32_t audio_out = -1;
	float penalty = 9999;
	int32_t used_possible_in = 0;
	bool found = false;

#if defined (__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wtautological-compare"
#endif

#define FOUNDCFG_PENALTY(n_in, n_out, p) { \
  _output_configs.insert (n_out);          \
  if (p < penalty) {                       \
    used_possible_in = possible_in;        \
    audio_out = (n_out);                   \
    in.set (DataType::AUDIO, (n_in));      \
    penalty = p;                           \
    found = true;                          \
    variable_inputs = possible_in < 0;     \
    variable_outputs = possible_out < 0;   \
  }                                        \
}

#define FOUNDCFG_IMPRECISE(n_in, n_out) {                   \
  const float p = fabsf ((float)(n_out) - preferred_out) *  \
                      (((n_out) > preferred_out) ? 1.1 : 1) \
                + fabsf ((float)(n_in) - audio_in) *        \
                      (((n_in) > audio_in) ? 275 : 250);    \
  FOUNDCFG_PENALTY(n_in, n_out, p);                         \
}

#define FOUNDCFG(n_out)              \
  FOUNDCFG_IMPRECISE(audio_in, n_out)

#define ANYTHINGGOES         \
  _output_configs.insert (0);

#define UPTO(nch) {                    \
  for (int32_t n = 1; n <= nch; ++n) { \
    _output_configs.insert (n);        \
  }                                    \
}

	for (vector<pair<int,int> >::iterator i = io_configs.begin(); i != io_configs.end(); ++i) {

		int32_t possible_in = i->first;
		int32_t possible_out = i->second;

		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("\tpossible in %1 possible out %2\n", possible_in, possible_out));

		/* exact match */
		if ((possible_in == audio_in) && (possible_out == preferred_out)) {
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("\tCHOSEN: %1 in %2 out to match in %3 out %4\n",
						possible_in, possible_out,
						in, out));
			/* Set penalty so low that this output configuration
			 * will trump any other one */
			FOUNDCFG_PENALTY(audio_in, preferred_out, -1);
			break;
		}

		if (possible_out == 0) {
			warning << string_compose (_("AU %1 has zero outputs - configuration ignored"), name()) << endmsg;
			/* XXX surely this is just a send? (e.g. AUNetSend) */
			continue;
		}

		/* now allow potentially "imprecise" matches */
		if (possible_in == -1 || possible_in == -2) {
			/* wildcard for input */
			if (possible_out == possible_in) {
				/* either both -1 or both -2 (invalid and
				 * interpreted as both -1): out must match in */
				FOUNDCFG (audio_in);
			} else if (possible_out == -3 - possible_in) {
				/* one is -1, the other is -2: any output configuration
				 * possible, pick what the insert prefers */
				FOUNDCFG (preferred_out);
				ANYTHINGGOES;
			} else if (possible_out < -2) {
				/* variable number of outputs up to -N,
				 * invalid if in == -2 but we accept it anyway */
				FOUNDCFG (min (-possible_out, preferred_out));
				UPTO (-possible_out)
			} else {
				/* exact number of outputs */
				FOUNDCFG (possible_out);
			}
		}

		if (possible_in < -2 || possible_in >= 0) {
			/* specified number, exact or up to */
			int32_t desired_in;
			if (possible_in >= 0) {
				/* configuration can only match possible_in */
				desired_in = possible_in;
			} else {
				/* configuration can match up to -possible_in */
				desired_in = min (-possible_in, audio_in);
			}
			 if (possible_out == -1 || possible_out == -2) {
				/* any output configuration possible
				 * out == -2 is invalid, interpreted as out == -1
				 * Really imprecise only if desired_in != audio_in */
				FOUNDCFG_IMPRECISE (desired_in, preferred_out);
				ANYTHINGGOES;
			} else if (possible_out < -2) {
				/* variable number of outputs up to -N
				 * not specified if in > 0, but we accept it anyway
				 * Really imprecise only if desired_in != audio_in */
				FOUNDCFG_IMPRECISE (desired_in, min (-possible_out, preferred_out));
				UPTO (-possible_out)
			} else {
				/* exact number of outputs
				 * Really imprecise only if desired_in != audio_in */
				FOUNDCFG_IMPRECISE (desired_in, possible_out);
			}
		}

	}

	if (!found) {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("\tFAIL: no io configs match %1\n", in));
		return false;
	}

	if (used_possible_in < -2 && audio_in == 0 && aux_in.n_audio () == 0) {
		/* input-port count cannot be zero, use as many ports
		 * as outputs, but at most abs(possible_in) */
		uint32_t n_in = max (1, min (audio_out, -used_possible_in));
		in.set (DataType::AUDIO, n_in);
	}

#if 0
	if (aux_in.n_audio () > 0 && input_elements > 1) {
		in.set (DataType::AUDIO, in.n_audio() + aux_in.n_audio());
	}
#endif

	out.set (DataType::MIDI, 0); /// XXX currently always zero
	out.set (DataType::AUDIO, audio_out);

	if (input_elements == 1) {
		/* subtract aux-ins that were treated as default inputs */
		in.set (DataType::AUDIO, in.n_audio() - aux_in.n_audio());
	}

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("\tCHOSEN: in: %1 aux-in: %2 out: %3\n", in, aux_in, out));

#if defined (__clang__)
# pragma clang diagnostic pop
#endif
	return true;
}

int
AUPlugin::set_stream_format (int scope, uint32_t bus, AudioStreamBasicDescription& fmt)
{
	OSErr result;

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("set stream format for %1, scope = %2 element %3\n",
				(scope == kAudioUnitScope_Input ? "input" : "output"),
				scope, bus));
	if ((result = unit->SetFormat (scope, bus, fmt)) != 0) {
		error << string_compose (_("AUPlugin: could not set stream format for %1/%2 (err = %3)"),
				(scope == kAudioUnitScope_Input ? "input" : "output"), bus, result) << endmsg;
		return -1;
	}
	return 0;
}

OSStatus
AUPlugin::render_callback(AudioUnitRenderActionFlags*,
			  const AudioTimeStamp*,
			  UInt32 bus,
			  UInt32 inNumberSamples,
			  AudioBufferList* ioData)
{
	/* not much to do with audio - the data is already in the buffers given to us in connect_and_run() */

	DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("%1: render callback, samples %2 bus %3 bufs %4\n", name(), inNumberSamples, bus, ioData->mNumberBuffers));

	if (input_maxbuf == 0) {
		DEBUG_TRACE (DEBUG::AudioUnitProcess, "AUPlugin: render callback called illegally!");
		error << _("AUPlugin: render callback called illegally!") << endmsg;
		return kAudioUnitErr_CannotDoInCurrentContext;
	}

	assert (bus < input_elements);
	uint32_t busoff = 0;
	for (uint32_t i = 0; i < bus; ++i) {
		busoff += bus_inused[i];
	}

	uint32_t limit = min ((uint32_t) ioData->mNumberBuffers, input_maxbuf);

	ChanCount bufs_count (DataType::AUDIO, 1);
	BufferSet& silent_bufs = _session.get_silent_buffers(bufs_count);

	/* apply bus offsets */

	for (uint32_t i = 0; i < limit; ++i) {
		ioData->mBuffers[i].mNumberChannels = 1;
		ioData->mBuffers[i].mDataByteSize = sizeof (Sample) * inNumberSamples;

		bool valid = false;
		uint32_t idx = input_map->get (DataType::AUDIO, i + busoff, &valid);
		if (valid) {
			ioData->mBuffers[i].mData = input_buffers->get_audio (idx).data (cb_offsets[bus] + input_offset);
		} else {
			ioData->mBuffers[i].mData = silent_bufs.get_audio(0).data (cb_offsets[bus] + input_offset);
		}
	}
	cb_offsets[bus] += inNumberSamples;
	return noErr;
}

int
AUPlugin::connect_and_run (BufferSet& bufs,
		samplepos_t start, samplepos_t end, double speed,
		ChanMapping const& in_map, ChanMapping const& out_map,
		pframes_t nframes, samplecnt_t offset)
{
	Plugin::connect_and_run(bufs, start, end, speed, in_map, out_map, nframes, offset);

	/* remain at zero during pre-roll at zero */
	transport_speed = end > 0 ? speed : 0;
	transport_sample = std::max (start, samplepos_t (0));

	AudioUnitRenderActionFlags flags = 0;
	AudioTimeStamp ts;
	OSErr err;

	if (preset_holdoff > 0) {
		preset_holdoff -= std::min (nframes, preset_holdoff);
	}

	if (requires_fixed_size_buffers() && (nframes != _last_nframes)) {
		unit->GlobalReset();
		_last_nframes = nframes;
	}

	/* test if we can run in-place; only compare audio buffers */
	bool inplace = true; // TODO check plugin-insert in-place ?
	ChanMapping::Mappings inmap (in_map.mappings ());
	ChanMapping::Mappings outmap (out_map.mappings ());
	if (outmap[DataType::AUDIO].size () == 0 || inmap[DataType::AUDIO].size() == 0) {
		inplace = false;
	}
	if (inmap[DataType::AUDIO].size() > 0 && inmap != outmap) {
		inplace = false;
	}

	DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("%1 in %2 out %3 MIDI %4 bufs %5 (available %6) InBus %7 OutBus %8 Inplace: %9 var-i/o %10 %11\n",
				name(), input_channels, output_channels, _has_midi_input,
				bufs.count(), bufs.available(),
				configured_input_busses, configured_output_busses, inplace, variable_inputs, variable_outputs));

	/* the apparent number of buffers matches our input configuration, but we know that the bufferset
	 * has the capacity to handle our outputs.
	 */

	assert (bufs.available() >= ChanCount (DataType::AUDIO, output_channels));

	input_buffers = &bufs;
	input_map = &in_map;
	input_maxbuf = bufs.count().n_audio(); // number of input audio buffers
	input_offset = offset;
	for (size_t i = 0; i < input_elements; ++i) {
		cb_offsets[i] = 0;
	}

	ChanCount bufs_count (DataType::AUDIO, 1);
	BufferSet& scratch_bufs = _session.get_scratch_buffers(bufs_count);

	if (_has_midi_input) {
		uint32_t nmidi = bufs.count().n_midi();
		for (uint32_t i = 0; i < nmidi; ++i) {
			/* one MIDI port/buffer only */
			MidiBuffer& m = bufs.get_midi (i);
			for (MidiBuffer::iterator i = m.begin(); i != m.end(); ++i) {
				Evoral::Event<samplepos_t> ev (*i);
				if (ev.is_channel_event()) {
					const uint8_t* b = ev.buffer();
					DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("%1: MIDI event %2\n", name(), ev));
					unit->MIDIEvent (b[0], b[1], b[2], ev.time());
				}
				/* XXX need to handle sysex and other message types */
			}
		}
	}

	assert (input_maxbuf < 512);
	std::bitset<512> used_outputs;

	bool ok = true;
	uint32_t busoff = 0;
	uint32_t remain = output_channels;
	for (uint32_t bus = 0; remain > 0 && bus < configured_output_busses; ++bus) {
		uint32_t cnt;
		if (variable_outputs || (output_elements == configured_output_busses && configured_output_busses == 1)) {
			cnt = output_channels;
		} else {
			cnt = std::min (remain, bus_outputs[bus]);
		}
		assert (cnt > 0);

		buffers->mNumberBuffers = cnt;

		for (uint32_t i = 0; i < cnt; ++i) {
			buffers->mBuffers[i].mNumberChannels = 1;
			/* setting this to 0 indicates to the AU that it *can* provide buffers here
			 * if necessary. if it can process in-place, it will use the buffers provided
			 * as input by ::render_callback() above.
			 *
			 * a non-null values tells the plugin to render into the buffer pointed
			 * at by the value.
			 * https://developer.apple.com/documentation/audiotoolbox/1438430-audiounitrender?language=objc
			 */
			if (inplace) {
				buffers->mBuffers[i].mDataByteSize = 0;
				buffers->mBuffers[i].mData = 0;
			} else {
				buffers->mBuffers[i].mDataByteSize = nframes * sizeof (Sample);
				bool valid = false;
				uint32_t idx = out_map.get (DataType::AUDIO, i + busoff, &valid);
				if (valid) {
					buffers->mBuffers[i].mData = bufs.get_audio (idx).data (offset);
				} else {
					buffers->mBuffers[i].mData = scratch_bufs.get_audio(0).data(offset);
				}
			}
		}

		/* does this really mean anything ?  */
		ts.mSampleTime = samples_processed;
		ts.mFlags = kAudioTimeStampSampleTimeValid;

		DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("%1 render flags=%2 time=%3 nframes=%4 bus=%5 buffers=%6\n",
					name(), flags, samples_processed, nframes, bus, buffers->mNumberBuffers));

		if ((err = unit->Render (&flags, &ts, bus, nframes, buffers)) == noErr) {

			DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose ("%1 rendered %2 buffers of %3\n",
						name(), buffers->mNumberBuffers, output_channels));

			uint32_t limit = std::min ((uint32_t) buffers->mNumberBuffers, cnt);
			for (uint32_t i = 0; i < limit; ++i) {
				bool valid = false;
				uint32_t idx = out_map.get (DataType::AUDIO, i + busoff, &valid);
				if (!valid) {
					continue;
				}
				if (buffers->mBuffers[i].mData == 0 || buffers->mBuffers[i].mNumberChannels != 1) {
					continue;
				}
				used_outputs.set (i + busoff);
				Sample* expected_buffer_address = bufs.get_audio (idx).data (offset);
				if (expected_buffer_address != buffers->mBuffers[i].mData) {
					/* plugin provided its own buffer for output so copy it back to where we want it */
					memcpy (expected_buffer_address, buffers->mBuffers[i].mData, nframes * sizeof (Sample));
				}
			}
		} else {
			DEBUG_TRACE (DEBUG::AudioUnitProcess, string_compose (_("AU: render error for %1, bus %2 status = %3\n"), name(), bus, err));
			error << string_compose (_("AU: render error for %1, bus %2 status = %3"), name(), bus, err) << endmsg;
			ok = false;
			break;
		}

		remain -= cnt;
		busoff += bus_outputs[bus];
	}

	/* now silence any buffers that were passed in but the that the plugin
	 * did not fill/touch/use.
	 *
	 * TODO: optimize, when plugin-insert is processing in-place
	 * unconnected buffers are (also) cleared there.
	 */
	for (uint32_t i = 0; i < input_maxbuf; ++i) {
		if (used_outputs.test (i)) { continue; }
		bool valid = false;
		uint32_t idx = out_map.get (DataType::AUDIO, i, &valid);
		if (!valid) continue;
		memset (bufs.get_audio (idx).data (offset), 0, nframes * sizeof (Sample));
	}

	input_maxbuf = 0;

	if (ok) {
		samples_processed += nframes;
		return 0;
	}
	return -1;
}

OSStatus
AUPlugin::get_beat_and_tempo_callback (Float64* outCurrentBeat,
				       Float64* outCurrentTempo)
{
	using namespace Temporal;
	TempoMap::SharedPtr tmap (TempoMap::use());

	DEBUG_TRACE (DEBUG::AudioUnitProcess, "AU calls ardour beat&tempo callback\n");

	if (outCurrentBeat) {
		DoubleableBeats db (tmap->quarters_at_sample (transport_sample));
		*outCurrentBeat = db.to_double();
	}

	if (outCurrentTempo) {
		*outCurrentTempo = tmap->tempo_at (transport_sample).quarter_notes_per_minute();
	}

	return noErr;

}

OSStatus
AUPlugin::get_musical_time_location_callback (UInt32*   outDeltaSampleOffsetToNextBeat,
					      Float32*  outTimeSig_Numerator,
					      UInt32*   outTimeSig_Denominator,
					      Float64*  outCurrentMeasureDownBeat)
{
	using namespace Temporal;
	TempoMap::SharedPtr tmap (TempoMap::use());

	DEBUG_TRACE (DEBUG::AudioUnitProcess, "AU calls ardour music time location callback\n");

	TempoMetric metric = tmap->metric_at (timepos_t (transport_sample + input_offset));
	BBT_Time bbt = tmap->bbt_at (timepos_t (transport_sample));

	if (outDeltaSampleOffsetToNextBeat) {
		if (bbt.ticks == 0) {
			/* on the beat */
			*outDeltaSampleOffsetToNextBeat = 0;
		} else {
			const Beats next_beat = tmap->quarters_at_sample (transport_sample).round_up_to_beat ();
			samplepos_t const next_beat_sample = metric.tempo().sample_at (next_beat);

			*outDeltaSampleOffsetToNextBeat = next_beat_sample - transport_sample;
		}
	}

	if (outTimeSig_Numerator) {
		*outTimeSig_Numerator = (UInt32) lrintf (metric.meter().divisions_per_bar());
	}
	if (outTimeSig_Denominator) {
		*outTimeSig_Denominator = (UInt32) lrintf (metric.meter().note_value());
	}

	if (outCurrentMeasureDownBeat) {

		/* beat for the start of the bar.
		   1|1|0 -> 1
		   2|1|0 -> 1 + divisions_per_bar
		   3|1|0 -> 1 + (2 * divisions_per_bar)
		   etc.
		*/
		bbt.beats = 1;
		bbt.ticks = 0;

		DoubleableBeats db = tmap->quarters_at (bbt);
		*outCurrentMeasureDownBeat = db.to_double ();
	}

	return noErr;
}

OSStatus
AUPlugin::get_transport_state_callback (Boolean*  outIsPlaying,
					Boolean*  outTransportStateChanged,
					Float64*  outCurrentSampleInTimeLine,
					Boolean*  outIsCycling,
					Float64*  outCycleStartBeat,
					Float64*  outCycleEndBeat)
{
	using namespace Temporal;

	const bool rolling = (transport_speed != 0);
	const bool last_transport_rolling = (last_transport_speed != 0);

	DEBUG_TRACE (DEBUG::AudioUnitProcess, "AU calls ardour transport state callback\n");


	if (outIsPlaying) {
		*outIsPlaying = rolling;
	}

	if (outTransportStateChanged) {
		if (rolling != last_transport_rolling) {
			*outTransportStateChanged = true;
		} else if (transport_speed != last_transport_speed) {
			*outTransportStateChanged = true;
		} else {
			*outTransportStateChanged = false;
		}
	}

	if (outCurrentSampleInTimeLine) {
		/* this assumes that the AU can only call this host callback from render context,
		   where input_offset is valid.
		*/
		*outCurrentSampleInTimeLine = transport_sample;
	}

	if (outIsCycling) {
		// TODO check bounce-processing
		Location* loc = _session.locations()->auto_loop_location();

		*outIsCycling = (loc && rolling && _session.get_play_loop());

		if (*outIsCycling) {

			if (outCycleStartBeat || outCycleEndBeat) {

				TempoMap::SharedPtr tmap (TempoMap::use());

				Temporal::BBT_Time bbt;

				if (outCycleStartBeat) {
					DoubleableBeats db (tmap->quarters_at (loc->start()));
					*outCycleStartBeat = db.to_double ();
				}

				if (outCycleEndBeat) {
					DoubleableBeats db (tmap->quarters_at (loc->end()));
					*outCycleEndBeat = db.to_double ();
				}
			}
		}
	}

	last_transport_speed = transport_speed;

	return noErr;
}

set<Evoral::Parameter>
AUPlugin::automatable() const
{
	set<Evoral::Parameter> automates;

	for (uint32_t i = 0; i < descriptors.size(); ++i) {
		if (descriptors[i].automatable) {
			automates.insert (automates.end(), Evoral::Parameter (PluginAutomation, 0, i));
		}
	}

	return automates;
}

Plugin::IOPortDescription
AUPlugin::describe_io_port (ARDOUR::DataType dt, bool input, uint32_t id) const
{
	std::stringstream ss;
	switch (dt) {
		case DataType::AUDIO:
			break;
		case DataType::MIDI:
			ss << _("Midi");
			break;
		default:
			ss << _("?");
			break;
	}

	std::string busname;
	bool is_sidechain = false;

	if (dt == DataType::AUDIO) {
		if (input) {
			uint32_t pid = id;
			for (uint32_t bus = 0; bus < input_elements; ++bus) {
				if (pid < bus_inused[bus]) {
					id = pid;
					ss << _bus_name_in[bus];
					ss << " / Bus " << (1 + bus);
					busname = _bus_name_in[bus];
					is_sidechain = bus > 0;
					busname = _bus_name_in[bus];
					break;
				}
				pid -= bus_inused[bus];
			}
		}
		else {
			uint32_t pid = id;
			for (uint32_t bus = 0; bus < output_elements; ++bus) {
				if (pid < bus_outputs[bus]) {
					id = pid;
					ss << _bus_name_out[bus];
					ss << " / Bus " << (1 + bus);
					busname = _bus_name_out[bus];
					break;
				}
				pid -= bus_outputs[bus];
			}
		}
	}

	if (input) {
		ss << " " << _("In") << " ";
	} else {
		ss << " " << _("Out") << " ";
	}

	ss << (id + 1);

	Plugin::IOPortDescription iod (ss.str());
	iod.is_sidechain = is_sidechain;
	if (!busname.empty()) {
		iod.group_name = busname;
		iod.group_channel = id;
	}
	return iod;
}

string
AUPlugin::describe_parameter (Evoral::Parameter param)
{
	if (param.type() == PluginAutomation && param.id() < parameter_count()) {
		return descriptors[param.id()].label;
	} else {
		return "??";
	}
}

bool
AUPlugin::parameter_is_audio (uint32_t) const
{
	return false;
}

bool
AUPlugin::parameter_is_control (uint32_t param) const
{
	assert(param < descriptors.size());
	if (descriptors[param].automatable) {
		/* corrently ardour expects all controls to be automatable
		 * IOW ardour GUI elements mandate an Evoral::Parameter
		 * for all input+control ports.
		 */
		return true;
	}
	return false;
}

bool
AUPlugin::parameter_is_input (uint32_t param) const
{
	/* AU params that are both readable and writeable,
	 * are listed in kAudioUnitScope_Global
	 */
	return (descriptors[param].scope == kAudioUnitScope_Input || descriptors[param].scope == kAudioUnitScope_Global);
}

bool
AUPlugin::parameter_is_output (uint32_t param) const
{
	assert(param < descriptors.size());
	// TODO check if ardour properly handles ports
	// that report is_input + is_output == true
	// -> add || descriptors[param].scope == kAudioUnitScope_Global
	return (descriptors[param].scope == kAudioUnitScope_Output);
}

void
AUPlugin::add_state (XMLNode* root) const
{
	LocaleGuard lg;
	CFDataRef xmlData;
	CFPropertyListRef propertyList;

	DEBUG_TRACE (DEBUG::AudioUnitConfig, "get preset state\n");
	if (unit->GetAUPreset (propertyList) != noErr) {
		return;
	}

	// Convert the property list into XML data.

	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, propertyList);

	if (!xmlData) {
		error << _("Could not create XML version of property list") << endmsg;
		return;
	}

	/* re-parse XML bytes to create a libxml++ XMLTree that we can merge into
	   our state node. GACK!
	*/

	XMLTree t;

	if (t.read_buffer (string ((const char*) CFDataGetBytePtr (xmlData), CFDataGetLength (xmlData)).c_str())) {
		if (t.root()) {
			root->add_child_copy (*t.root());
		}
	}

	CFRelease (xmlData);
	CFRelease (propertyList);
}

int
AUPlugin::set_state(const XMLNode& node, int version)
{
	int ret = -1;
	CFPropertyListRef propertyList;
	LocaleGuard lg;

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to AUPlugin::set_state") << endmsg;
		return -1;
	}

	if (node.children().empty()) {
		return -1;
	}

	XMLNode* top = node.children().front();
	XMLNode* copy = new XMLNode (*top);

	XMLTree t;
	t.set_root (copy);

	const string& xml = t.write_buffer ();
	CFDataRef xmlData = CFDataCreateWithBytesNoCopy (kCFAllocatorDefault, (UInt8*) xml.data(), xml.length(), kCFAllocatorNull);
	CFStringRef errorString;

	propertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
							xmlData,
							kCFPropertyListImmutable,
							&errorString);

	CFRelease (xmlData);

	if (propertyList) {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "set preset\n");
		if (unit->SetAUPreset (propertyList) == noErr) {
			ret = 0;

			/* tell the world */

			AudioUnitParameter changedUnit;
			changedUnit.mAudioUnit = unit->AU();
			changedUnit.mParameterID = kAUParameterListener_AnyParameter;
			AUParameterListenerNotify (NULL, NULL, &changedUnit);
		}
		CFRelease (propertyList);
	}

	Plugin::set_state (node, version);
	return ret;
}

bool
AUPlugin::load_preset (PresetRecord r)
{
	bool ret = false;
	CFPropertyListRef propertyList;
	Glib::ustring path;
	UserPresetMap::iterator ux;
	FactoryPresetMap::iterator fx;

	/* look first in "user" presets */

	if ((ux = user_preset_map.find (r.label)) != user_preset_map.end()) {

		if ((propertyList = load_property_list (ux->second)) != 0) {
			DEBUG_TRACE (DEBUG::AudioUnitConfig, "set preset from user presets\n");
			if (unit->SetAUPreset (propertyList) == noErr) {
				ret = true;

				/* tell the world */

				AudioUnitParameter changedUnit;
				changedUnit.mAudioUnit = unit->AU();
				changedUnit.mParameterID = kAUParameterListener_AnyParameter;
				AUParameterListenerNotify (NULL, NULL, &changedUnit);
			}
			CFRelease(propertyList);
		}

	} else if ((fx = factory_preset_map.find (r.label)) != factory_preset_map.end()) {

		AUPreset preset;

		preset.presetNumber = fx->second;
		preset.presetName = CFStringCreateWithCString (kCFAllocatorDefault, fx->first.c_str(), kCFStringEncodingUTF8);

		DEBUG_TRACE (DEBUG::AudioUnitConfig, "set preset from factory presets\n");

		if (unit->SetPresentPreset (preset) == 0) {
			ret = true;

			/* tell the world */

			AudioUnitParameter changedUnit;
			changedUnit.mAudioUnit = unit->AU();
			changedUnit.mParameterID = kAUParameterListener_AnyParameter;
			AUParameterListenerNotify (NULL, NULL, &changedUnit);
		}
	}
	if (ret) {
		preset_holdoff = std::max (_session.get_block_size() * 2.0, _session.sample_rate() * .2);
	}

	return ret && Plugin::load_preset (r);
}

void
AUPlugin::do_remove_preset (std::string preset_name)
{
	vector<Glib::ustring> v;

	std::string m = maker();
	std::string n = name();

	strip_whitespace_edges (m);
	strip_whitespace_edges (n);

	v.push_back (Glib::get_home_dir());
	v.push_back ("Library");
	v.push_back ("Audio");
	v.push_back ("Presets");
	v.push_back (m);
	v.push_back (n);
	v.push_back (preset_name + preset_suffix);

	Glib::ustring user_preset_path = Glib::build_filename (v);

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Deleting Preset file %1\n", user_preset_path));

	if (g_unlink (user_preset_path.c_str())) {
		error << string_compose (X_("Could not delete preset at \"%1\": %2"), user_preset_path, strerror (errno)) << endmsg;
	}
}

string
AUPlugin::do_save_preset (string preset_name)
{
	CFPropertyListRef propertyList;
	vector<Glib::ustring> v;
	Glib::ustring user_preset_path;

	std::string m = maker();
	std::string n = name();

	strip_whitespace_edges (m);
	strip_whitespace_edges (n);

	v.push_back (Glib::get_home_dir());
	v.push_back ("Library");
	v.push_back ("Audio");
	v.push_back ("Presets");
	v.push_back (m);
	v.push_back (n);

	user_preset_path = Glib::build_filename (v);

	if (g_mkdir_with_parents (user_preset_path.c_str(), 0775) < 0) {
		error << string_compose (_("Cannot create user plugin presets folder (%1)"), user_preset_path) << endmsg;
		return string();
	}

	DEBUG_TRACE (DEBUG::AudioUnitConfig, "get current preset\n");
	if (unit->GetAUPreset (propertyList) != noErr) {
		return string();
	}

	// add the actual preset name */

	v.push_back (preset_name + preset_suffix);

	// rebuild

	user_preset_path = Glib::build_filename (v);

	/* delete old preset if it exists */
	g_unlink (user_preset_path.c_str());

	set_preset_name_in_plist (propertyList, preset_name);

	if (save_property_list (propertyList, user_preset_path)) {
		error << string_compose (_("Saving plugin state to %1 failed"), user_preset_path) << endmsg;
		return string();
	}

	CFRelease(propertyList);

	user_preset_map[preset_name] = user_preset_path;;

	DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Saving Preset to %1\n", user_preset_path));

	return user_preset_path;
}

//-----------------------------------------------------------------------------
// this is just a little helper function used by GetAUComponentDescriptionFromPresetFile()
static SInt32
GetDictionarySInt32Value(CFDictionaryRef inAUStateDictionary, CFStringRef inDictionaryKey, Boolean * outSuccess)
{
	CFNumberRef cfNumber;
	SInt32 numberValue = 0;
	Boolean dummySuccess;

	if (outSuccess == NULL)
		outSuccess = &dummySuccess;
	if ( (inAUStateDictionary == NULL) || (inDictionaryKey == NULL) )
	{
		*outSuccess = FALSE;
		return 0;
	}

	cfNumber = (CFNumberRef) CFDictionaryGetValue(inAUStateDictionary, inDictionaryKey);
	if (cfNumber == NULL)
	{
		*outSuccess = FALSE;
		return 0;
	}
	*outSuccess = CFNumberGetValue(cfNumber, kCFNumberSInt32Type, &numberValue);
	if (*outSuccess)
		return numberValue;
	else
		return 0;
}

static OSStatus
GetAUComponentDescriptionFromStateData(CFPropertyListRef inAUStateData, ArdourDescription * outComponentDescription)
{
	CFDictionaryRef auStateDictionary;
	ArdourDescription tempDesc = {0,0,0,0,0};
	SInt32 versionValue;
	Boolean gotValue;

	if ( (inAUStateData == NULL) || (outComponentDescription == NULL) )
		return paramErr;

	// the property list for AU state data must be of the dictionary type
	if (CFGetTypeID(inAUStateData) != CFDictionaryGetTypeID()) {
		return kAudioUnitErr_InvalidPropertyValue;
	}

	auStateDictionary = (CFDictionaryRef)inAUStateData;

	// first check to make sure that the version of the AU state data is one that we know understand
	// XXX should I really do this?  later versions would probably still hold these ID keys, right?
	versionValue = GetDictionarySInt32Value(auStateDictionary, CFSTR(kAUPresetVersionKey), &gotValue);

	if (!gotValue) {
		return kAudioUnitErr_InvalidPropertyValue;
	}
#define kCurrentSavedStateVersion 0
	if (versionValue != kCurrentSavedStateVersion) {
		return kAudioUnitErr_InvalidPropertyValue;
	}

	// grab the ComponentDescription values from the AU state data
	tempDesc.componentType = (OSType) GetDictionarySInt32Value(auStateDictionary, CFSTR(kAUPresetTypeKey), NULL);
	tempDesc.componentSubType = (OSType) GetDictionarySInt32Value(auStateDictionary, CFSTR(kAUPresetSubtypeKey), NULL);
	tempDesc.componentManufacturer = (OSType) GetDictionarySInt32Value(auStateDictionary, CFSTR(kAUPresetManufacturerKey), NULL);
	// zero values are illegit for specific ComponentDescriptions, so zero for any value means that there was an error
	if ( (tempDesc.componentType == 0) || (tempDesc.componentSubType == 0) || (tempDesc.componentManufacturer == 0) )
		return kAudioUnitErr_InvalidPropertyValue;

	*outComponentDescription = tempDesc;
	return noErr;
}


static bool au_preset_filter (const string& str, void* arg)
{
	/* Not a dotfile, has a prefix before a period, suffix is aupreset */

	bool ret;

	ret = (str[0] != '.' && str.length() > 9 && str.find (preset_suffix) == (str.length() - preset_suffix.length()));

	if (ret && arg) {

		/* check the preset file path name against this plugin
		   ID. The idea is that all preset files for this plugin
		   include "<manufacturer>/<plugin-name>" in their path.
		*/

		AUPluginInfo* p = (AUPluginInfo *) arg;
		string match = p->creator;
		match += '/';
		match += p->name;

		ret = str.find (match) != string::npos;

		if (ret == false) {
			string m = p->creator;
			string n = p->name;
			strip_whitespace_edges (m);
			strip_whitespace_edges (n);
			match = m;
			match += '/';
			match += n;

			ret = str.find (match) != string::npos;
		}
	}

	return ret;
}

static bool
check_and_get_preset_name (ArdourComponent component, const string& pathstr, string& preset_name)
{
	OSStatus status;
	CFPropertyListRef plist;
	ArdourDescription presetDesc;
	bool ret = false;

	plist = load_property_list (pathstr);

	if (!plist) {
		return ret;
	}

	// get the ComponentDescription from the AU preset file

	status = GetAUComponentDescriptionFromStateData(plist, &presetDesc);

	if (status == noErr) {
		if (ComponentAndDescriptionMatch_Loosely(component, &presetDesc)) {

			/* try to get the preset name from the property list */

			if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {

				const void* psk = CFDictionaryGetValue ((CFMutableDictionaryRef)plist, CFSTR(kAUPresetNameKey));

				if (psk) {

					const char* p = CFStringGetCStringPtr ((CFStringRef) psk, kCFStringEncodingUTF8);

					if (!p) {
						char buf[PATH_MAX+1];

						if (CFStringGetCString ((CFStringRef)psk, buf, sizeof (buf), kCFStringEncodingUTF8)) {
							preset_name = buf;
						}
					}
				}
			}
		}
	}

	CFRelease (plist);

	return true;
}

std::string
AUPlugin::current_preset() const
{
	string preset_name;

	CFPropertyListRef propertyList;

	DEBUG_TRACE (DEBUG::AudioUnitConfig, "get current preset for current_preset()\n");
	if (unit->GetAUPreset (propertyList) == noErr) {
		preset_name = get_preset_name_in_plist (propertyList);
		CFRelease(propertyList);
	}

	return preset_name;
}

void
AUPlugin::find_presets ()
{
	vector<string> preset_files;

	user_preset_map.clear ();

	PluginInfoPtr nfo = get_info();
	find_files_matching_filter (preset_files, preset_search_path, au_preset_filter,
			boost::dynamic_pointer_cast<AUPluginInfo> (nfo).get(),
			true, true, true);

	if (preset_files.empty()) {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "AU No Preset Files found for given plugin.\n");
	}

	for (vector<string>::iterator x = preset_files.begin(); x != preset_files.end(); ++x) {

		string path = *x;
		string preset_name;

		/* make an initial guess at the preset name using the path */

		preset_name = Glib::path_get_basename (path);
		preset_name = preset_name.substr (0, preset_name.find_last_of ('.'));

		/* check that this preset file really matches this plugin
		   and potentially get the "real" preset name from
		   within the file.
		*/

		if (check_and_get_preset_name (get_comp()->Comp(), path, preset_name)) {
			user_preset_map[preset_name] = path;
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Preset File: %1 > %2\n", preset_name, path));
		} else {
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU INVALID Preset: %1 > %2\n", preset_name, path));
		}

	}

	/* now fill the vector<string> with the names we have */

	for (UserPresetMap::iterator i = user_preset_map.begin(); i != user_preset_map.end(); ++i) {
		_presets.insert (make_pair (i->second, Plugin::PresetRecord (i->second, i->first)));
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Adding User Preset: %1 > %2\n", i->first, i->second));
	}

	/* add factory presets */

	for (FactoryPresetMap::iterator i = factory_preset_map.begin(); i != factory_preset_map.end(); ++i) {
		string const uri = string_compose ("AU2:%1", std::setw(4), std::setfill('0'), i->second);
		_presets.insert (make_pair (uri, Plugin::PresetRecord (uri, i->first, false)));
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Adding Factory Preset: %1 > %2\n", i->first, i->second));
	}
}

bool
AUPlugin::has_editor () const
{
	// even if the plugin doesn't have its own editor, the AU API can be used
	// to create one that looks native.
	return true;
}

/* ****************************************************************************/

AUPluginInfo::AUPluginInfo (boost::shared_ptr<CAComponentDescription> d)
	: version (0)
	, max_outputs (0)
	, descriptor (d)
{
	type = ARDOUR::AudioUnit;
}

PluginPtr
AUPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		DEBUG_TRACE (DEBUG::AudioUnitConfig, "load AU as a component\n");
		boost::shared_ptr<CAComponent> comp (new CAComponent(*descriptor));

		if (!comp->IsValid()) {
			error << ("AudioUnit: not a valid Component") << endmsg;
			return PluginPtr ();
		} else {
			plugin.reset (new AUPlugin (session.engine(), session, comp));
		}

		AUPluginInfo *aup = new AUPluginInfo (*this);
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("plugin info for %1 = %2\n", this, aup));
		plugin->set_info (PluginInfoPtr (aup));
		boost::dynamic_pointer_cast<AUPlugin> (plugin)->set_fixed_size_buffers (aup->creator == "Universal Audio");
		return plugin;
	}

	catch (failed_constructor &err) {
		DEBUG_TRACE (DEBUG::AudioUnitConfig, "failed to load component/plugin\n");
		return PluginPtr ();
	}
}

std::vector<Plugin::PresetRecord>
AUPluginInfo::get_presets (bool user_only) const
{
	std::vector<Plugin::PresetRecord> p;
	boost::shared_ptr<CAComponent> comp;

	try {
		comp = boost::shared_ptr<CAComponent>(new CAComponent(*descriptor));
		if (!comp->IsValid()) {
			throw failed_constructor();
		}
	} catch (failed_constructor& err) {
		return p;
	}

	// user presets

	if (!preset_search_path_initialized) {
		Glib::ustring p = Glib::get_home_dir();
		p += "/Library/Audio/Presets:";
		p += preset_search_path;
		preset_search_path = p;
		preset_search_path_initialized = true;
		DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Preset Path: %1\n", preset_search_path));
	}

	vector<string> preset_files;
	find_files_matching_filter (preset_files, preset_search_path, au_preset_filter, const_cast<AUPluginInfo*>(this), true, true, true);

	for (vector<string>::iterator x = preset_files.begin(); x != preset_files.end(); ++x) {
		string path = *x;
		string preset_name;
		preset_name = Glib::path_get_basename (path);
		preset_name = preset_name.substr (0, preset_name.find_last_of ('.'));
		if (check_and_get_preset_name (comp.get()->Comp(), path, preset_name)) {
			p.push_back (Plugin::PresetRecord (path, preset_name));
		}
	}

	if (user_only) {
		return p;
	}

	// factory presets

	CFArrayRef presets;
	UInt32 dataSize;
	Boolean isWritable;

	boost::shared_ptr<CAAudioUnit> unit (new CAAudioUnit);
	if (noErr != CAAudioUnit::Open (*(comp.get()), *unit)) {
		return p;
	}
	if (noErr != unit->GetPropertyInfo (kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, &dataSize, &isWritable)) {
		unit->Uninitialize ();
		return p;
	}
	if (noErr != unit->GetProperty (kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, (void*) &presets, &dataSize)) {
		unit->Uninitialize ();
		return p;
	}
	if (!presets) {
		unit->Uninitialize ();
		return p;
	}

	CFIndex cnt = CFArrayGetCount (presets);
	for (CFIndex i = 0; i < cnt; ++i) {
		AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex (presets, i);
		string const uri = string_compose ("%1", i);
		string name = CFStringRefToStdString (preset->presetName);
		p.push_back (Plugin::PresetRecord (uri, name, false));
	}
	CFRelease (presets);
	unit->Uninitialize ();

	return p;
}

bool
AUPluginInfo::needs_midi_input () const
{
	return is_effect_with_midi_input () || is_instrument ();
}

bool
AUPluginInfo::is_effect () const
{
	return is_effect_without_midi_input() || is_effect_with_midi_input();
}

bool
AUPluginInfo::is_effect_without_midi_input () const
{
	return descriptor->IsAUFX();
}

bool
AUPluginInfo::is_effect_with_midi_input () const
{
	return descriptor->IsAUFM();
}

bool
AUPluginInfo::is_instrument () const
{
	return descriptor->IsMusicDevice();
}

bool
AUPluginInfo::is_utility () const
{
	return (descriptor->IsGenerator() || descriptor->componentType == 'aumi');
	// kAudioUnitType_MidiProcessor  ..looks like we aren't even scanning for these yet?
}

std::string
AUPluginInfo::convert_old_unique_id (std::string const& id)
{
	vector<std::string> p;
	boost::split (p, id, boost::is_any_of ("-"));
	if (p.size () == 3) {
		OSType t (PBD::atoi (p[0]));
		OSType s (PBD::atoi (p[1]));
		OSType m (PBD::atoi (p[2]));
		CAComponentDescription desc (t, s, m);
		return auv2_stringify_descriptor (desc);
	}
	return id;
}

void
AUPlugin::set_info (PluginInfoPtr info)
{
	Plugin::set_info (info);

	AUPluginInfoPtr pinfo = boost::dynamic_pointer_cast<AUPluginInfo>(get_info());
	_has_midi_input = pinfo->needs_midi_input ();
	_has_midi_output = false;
}

int
AUPlugin::create_parameter_listener (AUEventListenerProc cb, void* arg, float interval_secs)
{
#ifdef WITH_CARBON
	CFRunLoopRef run_loop = (CFRunLoopRef) GetCFRunLoopFromEventLoop(GetCurrentEventLoop());
#else
	CFRunLoopRef run_loop = CFRunLoopGetCurrent();
#endif
	CFStringRef  loop_mode = kCFRunLoopDefaultMode;

	if (AUEventListenerCreate (cb, arg, run_loop, loop_mode, interval_secs, interval_secs, &_parameter_listener) != noErr) {
		return -1;
	}

	_parameter_listener_arg = arg;

	// listen for latency changes
	AudioUnitEvent event;
	event.mEventType = kAudioUnitEvent_PropertyChange;
	event.mArgument.mProperty.mAudioUnit = unit->AU();
	event.mArgument.mProperty.mPropertyID = kAudioUnitProperty_Latency;
	event.mArgument.mProperty.mScope = kAudioUnitScope_Global;
	event.mArgument.mProperty.mElement = 0;

	if (AUEventListenerAddEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		PBD::error << "Failed to create latency event listener\n";
		// TODO don't cache _current_latency
	}

	return 0;
}

int
AUPlugin::listen_to_parameter (uint32_t param_id)
{
	AudioUnitEvent      event;

	if (!_parameter_listener || param_id >= descriptors.size()) {
		return -2;
	}

	event.mEventType = kAudioUnitEvent_ParameterValueChange;
	event.mArgument.mParameter.mAudioUnit = unit->AU();
	event.mArgument.mParameter.mParameterID = descriptors[param_id].id;
	event.mArgument.mParameter.mScope = descriptors[param_id].scope;
	event.mArgument.mParameter.mElement = descriptors[param_id].element;

	if (AUEventListenerAddEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		return -1;
	}

	event.mEventType = kAudioUnitEvent_BeginParameterChangeGesture;
	event.mArgument.mParameter.mAudioUnit = unit->AU();
	event.mArgument.mParameter.mParameterID = descriptors[param_id].id;
	event.mArgument.mParameter.mScope = descriptors[param_id].scope;
	event.mArgument.mParameter.mElement = descriptors[param_id].element;

	if (AUEventListenerAddEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		return -1;
	}

	event.mEventType = kAudioUnitEvent_EndParameterChangeGesture;
	event.mArgument.mParameter.mAudioUnit = unit->AU();
	event.mArgument.mParameter.mParameterID = descriptors[param_id].id;
	event.mArgument.mParameter.mScope = descriptors[param_id].scope;
	event.mArgument.mParameter.mElement = descriptors[param_id].element;

	if (AUEventListenerAddEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		return -1;
	}

	return 0;
}

int
AUPlugin::end_listen_to_parameter (uint32_t param_id)
{
	AudioUnitEvent      event;

	if (!_parameter_listener || param_id >= descriptors.size()) {
		return -2;
	}

	event.mEventType = kAudioUnitEvent_ParameterValueChange;
	event.mArgument.mParameter.mAudioUnit = unit->AU();
	event.mArgument.mParameter.mParameterID = descriptors[param_id].id;
	event.mArgument.mParameter.mScope = descriptors[param_id].scope;
	event.mArgument.mParameter.mElement = descriptors[param_id].element;

	if (AUEventListenerRemoveEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		return -1;
	}

	event.mEventType = kAudioUnitEvent_BeginParameterChangeGesture;
	event.mArgument.mParameter.mAudioUnit = unit->AU();
	event.mArgument.mParameter.mParameterID = descriptors[param_id].id;
	event.mArgument.mParameter.mScope = descriptors[param_id].scope;
	event.mArgument.mParameter.mElement = descriptors[param_id].element;

	if (AUEventListenerRemoveEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		return -1;
	}

	event.mEventType = kAudioUnitEvent_EndParameterChangeGesture;
	event.mArgument.mParameter.mAudioUnit = unit->AU();
	event.mArgument.mParameter.mParameterID = descriptors[param_id].id;
	event.mArgument.mParameter.mScope = descriptors[param_id].scope;
	event.mArgument.mParameter.mElement = descriptors[param_id].element;

	if (AUEventListenerRemoveEventType (_parameter_listener, _parameter_listener_arg, &event) != noErr) {
		return -1;
	}

	return 0;
}

void
AUPlugin::_parameter_change_listener (void* arg, void* src, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value)
{
	((AUPlugin*) arg)->parameter_change_listener (arg, src, event, host_time, new_value);
}

void
AUPlugin::parameter_change_listener (void* /*arg*/, void* src, const AudioUnitEvent* event, UInt64 /*host_time*/, Float32 new_value)
{
	if (event->mEventType == kAudioUnitEvent_PropertyChange) {
		if (event->mArgument.mProperty.mPropertyID == kAudioUnitProperty_Latency) {
			DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose("AU Latency Change Event %1 <> %2\n", new_value, unit->Latency()));
			guint lat = unit->Latency() * _session.sample_rate();
			g_atomic_int_set (&_current_latency, lat);
		}
		return;
	}

        ParameterMap::iterator i;

        if ((i = parameter_map.find (event->mArgument.mParameter.mParameterID)) == parameter_map.end()) {
                return;
        }

        switch (event->mEventType) {
        case kAudioUnitEvent_BeginParameterChangeGesture:
                StartTouch (i->second);
                break;
        case kAudioUnitEvent_EndParameterChangeGesture:
                EndTouch (i->second);
                break;
        case kAudioUnitEvent_ParameterValueChange:
                /* whenever we change a parameter, we request that we are NOT notified of the change, so anytime we arrive here, it
                   means that something else (i.e. the plugin GUI) made the change.
                */
                if (preset_holdoff > 0) {
	                ParameterChangedExternally (i->second, new_value);
                } else {
                        Plugin::parameter_changed_externally (i->second, new_value);
		}
                break;
        default:
                break;
        }
}
