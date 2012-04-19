/*
    Copyright (C) 2006-2009 Paul Davis 
    Some portions Copyright (C) Sophia Poirier.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <sstream>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <pbd/transmitter.h>
#include <pbd/xml++.h>
#include <pbd/whitespace.h>
#include <pbd/pathscanner.h>
#include <pbd/localeguard.h>

#include <glibmm/thread.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/audio_unit.h>
#include <ardour/session.h>
#include <ardour/tempo.h>
#include <ardour/utils.h>

#include <appleutility/CAAudioUnit.h>
#include <appleutility/CAAUParameter.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreServices/CoreServices.h>

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

//#define TRACE_AU_API
#ifdef TRACE_AU_API
#define TRACE_API(fmt,...) fprintf (stderr, fmt, ## __VA_ARGS__)
#else
#define TRACE_API(fmt,...)
#endif

#ifndef AU_STATE_SUPPORT
static bool seen_get_state_message = false;
static bool seen_set_state_message = false;
static bool seen_loading_message = false;
static bool seen_saving_message = false;
#endif

AUPluginInfo::CachedInfoMap AUPluginInfo::cached_info;

static string preset_search_path = "/Library/Audio/Presets:/Network/Library/Audio/Presets";
static string preset_suffix = ".aupreset";
static bool preset_search_path_initialized = false;
static bool debug_io_config = true;

static OSStatus 
_render_callback(void *userData,
		 AudioUnitRenderActionFlags *ioActionFlags,
		 const AudioTimeStamp    *inTimeStamp,
		 UInt32       inBusNumber,
		 UInt32       inNumberFrames,
		 AudioBufferList*       ioData)
{
	if (userData) {
		return ((AUPlugin*)userData)->render_callback (ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
	}
	return paramErr;
}

static OSStatus 
_get_beat_and_tempo_callback (void*                userData,
			      Float64*             outCurrentBeat, 
			      Float64*             outCurrentTempo)
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
		return ((AUPlugin*)userData)->get_transport_state_callback (outIsPlaying, outTransportStateChanged,
									    outCurrentSampleInTimeLine, outIsCycling,
									    outCycleStartBeat, outCycleEndBeat);
	}
	return paramErr;
}


static int 
save_property_list (CFPropertyListRef propertyList, std::string path)

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
			/* tell any UI's that this file already exists and ask them what to do */
			bool overwrite = Plugin::PresetFileExists(); // EMIT SIGNAL
			if (overwrite) {
				fd = open (path.c_str(), O_WRONLY, 0664);
				continue;
			} else {
				return 0;
			}
		}
		error << string_compose (_("Cannot open preset file %1 (%2)"), path, strerror (errno)) << endmsg;
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
load_property_list (std::string path) 
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
Boolean ComponentDescriptionsMatch_General(const ComponentDescription * inComponentDescription1, const ComponentDescription * inComponentDescription2, Boolean inIgnoreType);
Boolean ComponentDescriptionsMatch_General(const ComponentDescription * inComponentDescription1, const ComponentDescription * inComponentDescription2, Boolean inIgnoreType)
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
Boolean ComponentAndDescriptionMatch_General(Component inComponent, const ComponentDescription * inComponentDescription, Boolean inIgnoreType);
Boolean ComponentAndDescriptionMatch_General(Component inComponent, const ComponentDescription * inComponentDescription, Boolean inIgnoreType)
{
	OSErr status;
	ComponentDescription desc;

	if ( (inComponent == NULL) || (inComponentDescription == NULL) )
		return FALSE;

	// get the ComponentDescription of the input Component
	status = GetComponentInfo(inComponent, &desc, NULL, NULL, NULL);
	if (status != noErr)
		return FALSE;

	// check if the Component's ComponentDescription matches the input ComponentDescription
	return ComponentDescriptionsMatch_General(&desc, inComponentDescription, inIgnoreType);
}

//--------------------------------------------------------------------------
// determine if 2 ComponentDescriptions are basically equal
// (by that, I mean that the important identifying values are compared, 
// but not the ComponentDescription flags)
Boolean ComponentDescriptionsMatch(const ComponentDescription * inComponentDescription1, const ComponentDescription * inComponentDescription2)
{
	return ComponentDescriptionsMatch_General(inComponentDescription1, inComponentDescription2, FALSE);
}

//--------------------------------------------------------------------------
// determine if 2 ComponentDescriptions have matching sub-type and manufacturer codes
Boolean ComponentDescriptionsMatch_Loose(const ComponentDescription * inComponentDescription1, const ComponentDescription * inComponentDescription2)
{
	return ComponentDescriptionsMatch_General(inComponentDescription1, inComponentDescription2, TRUE);
}

//--------------------------------------------------------------------------
// determine if a ComponentDescription basically matches that of a particular Component
Boolean ComponentAndDescriptionMatch(Component inComponent, const ComponentDescription * inComponentDescription)
{
	return ComponentAndDescriptionMatch_General(inComponent, inComponentDescription, FALSE);
}

//--------------------------------------------------------------------------
// determine if a ComponentDescription matches only the sub-type and manufacturer codes of a particular Component
Boolean ComponentAndDescriptionMatch_Loosely(Component inComponent, const ComponentDescription * inComponentDescription)
{
	return ComponentAndDescriptionMatch_General(inComponent, inComponentDescription, TRUE);
}


AUPlugin::AUPlugin (AudioEngine& engine, Session& session, boost::shared_ptr<CAComponent> _comp)
	: Plugin (engine, session)
	, comp (_comp)
	, unit (new CAAudioUnit)
	, initialized (false)
	, _current_block_size (0)
	, _requires_fixed_size_buffers (false)
	, buffers (0)
	, current_maxbuf (0)
	, current_offset (0)
	, current_buffers (0)
	, frames_processed (0)
	, _parameter_listener (0)
	, _parameter_listener_arg (0)
	, last_transport_rolling (false)
	, last_transport_speed (0.0)
{			
	if (!preset_search_path_initialized) {
		std::string p = Glib::get_home_dir();
		p += "/Library/Audio/Presets:";
		p += preset_search_path;
		preset_search_path = p;
		preset_search_path_initialized = true;
	}

	init ();
}

AUPlugin::AUPlugin (const AUPlugin& other)
	: Plugin (other)
	, comp (other.get_comp())
	, unit (new CAAudioUnit)
	, initialized (false)
	, _current_block_size (0)
	, _last_nframes (0)
	, _requires_fixed_size_buffers (false)
	, buffers (0)
	, current_maxbuf (0)
	, current_offset (0)
	, current_buffers (0)
	, frames_processed (0)
	, _parameter_listener (0)
	, _parameter_listener_arg (0)
	, last_transport_rolling (false)
	, last_transport_speed (0.0)
{
	init ();
}

AUPlugin::~AUPlugin ()
{
	if (_parameter_listener) {
		AUListenerDispose (_parameter_listener);
		_parameter_listener = 0;
	}
	
	if (unit) {
		TRACE_API ("about to call uninitialize in plugin destructor\n");
		unit->Uninitialize ();
	}

	if (buffers) {
		free (buffers);
	}
}

void
AUPlugin::discover_factory_presets ()
{
	CFArrayRef presets;
	UInt32 dataSize = sizeof (presets);
	OSStatus err;
	
	TRACE_API ("get property FactoryPresets in global scope\n");
	if ((err = unit->GetProperty (kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, (void*) &presets, &dataSize)) != 0) {
		cerr << "cannot get factory preset info: " << err << endl;
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
	}
	
	CFRelease (presets);
}

void
AUPlugin::init ()
{
	OSErr err;

	try {
		TRACE_API ("opening AudioUnit\n");
		err = CAAudioUnit::Open (*(comp.get()), *unit);
	} catch (...) {
		error << _("Exception thrown during AudioUnit plugin loading - plugin ignored") << endmsg;
		throw failed_constructor();
	}

	if (err != noErr) {
		error << _("AudioUnit: Could not convert CAComponent to CAAudioUnit") << endmsg;
		throw failed_constructor ();
	}
	
	TRACE_API ("count global elements\n");
	unit->GetElementCount (kAudioUnitScope_Global, global_elements);
	TRACE_API ("count input elements\n");
	unit->GetElementCount (kAudioUnitScope_Input, input_elements);
	TRACE_API ("count output elements\n");
	unit->GetElementCount (kAudioUnitScope_Output, output_elements);

        if (input_elements > 0) {
                AURenderCallbackStruct renderCallbackInfo;
                
                renderCallbackInfo.inputProc = _render_callback;
                renderCallbackInfo.inputProcRefCon = this;
                
                TRACE_API ("set render callback in input scope\n");
                if ((err = unit->SetProperty (kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
                                              0, (void*) &renderCallbackInfo, sizeof(renderCallbackInfo))) != 0) {
                        cerr << "cannot install render callback (err = " << err << ')' << endl;
                        throw failed_constructor();
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
	TRACE_API ("set host callbacks in global scope\n");
	unit->SetProperty (kAudioUnitProperty_HostCallbacks, 
			   kAudioUnitScope_Global, 
			   0, //elementID 
			   &info,
			   sizeof (HostCallbackInfo));

	/* these keep track of *configured* channel set up,
	   not potential set ups.
	*/

	input_channels = -1;
	output_channels = -1;

	if (set_block_size (_session.get_block_size())) {
		error << _("AUPlugin: cannot set processing block size") << endmsg;
		throw failed_constructor();
	}

	create_parameter_listener (AUPlugin::_parameter_change_listener, this, 0.05);
	discover_parameters ();
	discover_factory_presets ();

	Plugin::setup_controls ();
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

		AUParamInfo param_info (unit->AU(), false, false, scopes[i]);
		
		for (uint32_t i = 0; i < param_info.NumParams(); ++i) {

			AUParameterDescriptor d;

			d.id = param_info.ParamID (i);

			const CAAUParameter* param = param_info.GetParamInfo (d.id);
			const AudioUnitParameterInfo& info (param->ParamInfo());

			const int len = CFStringGetLength (param->GetName());;
			char local_buffer[len*2];
			Boolean good = CFStringGetCString(param->GetName(),local_buffer,len*2,kCFStringEncodingMacRoman);
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
			d.default_value = info.defaultValue;

			d.integer_step = (info.unit == kAudioUnitParameterUnit_Indexed);
			d.toggled = (info.unit == kAudioUnitParameterUnit_Boolean) ||
				(d.integer_step && ((d.upper - d.lower) == 1.0));
			d.sr_dependent = (info.unit == kAudioUnitParameterUnit_SampleFrames);
			d.automatable = !d.toggled && 
				!(info.flags & kAudioUnitParameterFlag_NonRealTime) &&
				(info.flags & kAudioUnitParameterFlag_IsWritable);
			
			d.logarithmic = (info.flags & kAudioUnitParameterFlag_DisplayLogarithmic);
			d.unit = info.unit;

			d.step = 1.0;
			d.smallstep = 0.1;
			d.largestep = 10.0;
			d.min_unbound = 0; // lower is bound
			d.max_unbound = 0; // upper is bound

			descriptors.push_back (d);

			uint32_t last_param = descriptors.size() - 1;
			parameter_map.insert (pair<uint32_t,uint32_t> (d.id, last_param));
			listen_to_parameter (last_param);
		}
	}
}

static unsigned int
four_ints_to_four_byte_literal (unsigned char n[4])
{
	/* this is actually implementation dependent. sigh. this is what gcc
	   and quite a few others do.
	 */
	return ((n[0] << 24) + (n[1] << 16) + (n[2] << 8) + n[3]);
}

std::string
AUPlugin::maybe_fix_broken_au_id (const std::string& id)
{
	if (isdigit (id[0])) {
		return id;
	}

	/* ID format is xxxx-xxxx-xxxx
	   where x maybe \xNN or a printable character.
	   
	   Split at the '-' and and process each part into an integer. 
	   Then put it back together.
	*/


	unsigned char nascent[4];
	const char* cstr = id.c_str();
	const char* estr = cstr + id.size();
	uint32_t n[3];
	int in;
	int next_int;
	char short_buf[3];
	stringstream s;

	in = 0;
	next_int = 0;
	short_buf[2] = '\0';

	while (*cstr && next_int < 4) {

		if (*cstr == '\\') {

			if (estr - cstr < 3) {

				/* too close to the end for \xNN parsing: treat as literal characters */

				cerr << "Parse " << cstr << " as a literal \\" << endl;
				nascent[in] = *cstr;
				++cstr;
				++in;

			} else {
				
				if (cstr[1] == 'x' && isxdigit (cstr[2]) && isxdigit (cstr[3])) {
					
					/* parse \xNN */
					
					memcpy (short_buf, &cstr[2], 2);
					nascent[in] = strtol (short_buf, NULL, 16);
					cstr += 4;
					++in;
					
				} else {

					/* treat as literal characters */
					cerr << "Parse " << cstr << " as a literal \\" << endl;
					nascent[in] = *cstr;
					++cstr;
					++in;
				}
			}

		} else {

			nascent[in] = *cstr;
			++cstr;
			++in;
		}

		if (in && (in % 4 == 0)) {
			/* nascent is ready */
			n[next_int] = four_ints_to_four_byte_literal (nascent);
			in = 0;
			next_int++;

			/* swallow space-hyphen-space */

			if (next_int < 3) {
				++cstr;
				++cstr;
				++cstr;
			}
		}
	}

	if (next_int != 3) {
		goto err;
	}

	s << n[0] << '-' << n[1] << '-' << n[2];
	
	return s.str();

  err:
	return string();
}

string
AUPlugin::unique_id () const
{
	return AUPluginInfo::stringify_descriptor (comp->Desc());
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
		return descriptors[port].default_value;
	}

	return 0;
}

nframes_t
AUPlugin::latency () const
{
	return unit->Latency() * _session.frame_rate();
}

void
AUPlugin::set_parameter (uint32_t which, float val)
{
	if (which >= descriptors.size()) {
		return;
	}

	const AUParameterDescriptor& d (descriptors[which]);
	TRACE_API ("set parameter %d in scope %d element %d to %f\n", d.id, d.scope, d.element, val);
	unit->SetParameter (d.id, d.scope, d.element, val);
	
	/* tell the world what we did */
	
	AudioUnitEvent theEvent;
	
	theEvent.mEventType = kAudioUnitEvent_ParameterValueChange;
	theEvent.mArgument.mParameter.mAudioUnit = unit->AU();
	theEvent.mArgument.mParameter.mParameterID = d.id;
	theEvent.mArgument.mParameter.mScope = d.scope;
	theEvent.mArgument.mParameter.mElement = d.element;
	
	TRACE_API ("notify about parameter change\n");

	/* use the AU Event API to notify about the change. Our own listener will 
	   end up "emitting" ParameterChanged, and this way ParameterChanged is 
	   used whether the change to the parameter comes from this function
	   or within the plugin or anywhere else, since they should all notify
	   via AUEventListenerNotify().
	*/

	AUEventListenerNotify (NULL, NULL, &theEvent);
}

float
AUPlugin::get_parameter (uint32_t which) const
{
	float val = 0.0;
	if (which < descriptors.size()) {
		const AUParameterDescriptor& d (descriptors[which]);
		TRACE_API ("get value of parameter %d in scope %d element %d\n", d.id, d.scope, d.element);
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
		TRACE_API ("call Initialize in activate()\n");
		if ((err = unit->Initialize()) != noErr) {
			error << string_compose (_("AUPlugin: %1 cannot initialize plugin (err = %2)"), name(), err) << endmsg;
		} else {
			frames_processed = 0;
			initialized = true;
		}
	}
}

void
AUPlugin::deactivate ()
{
	TRACE_API ("call Uninitialize in deactivate()\n");
	unit->Uninitialize ();
	initialized = false;
}

void
AUPlugin::flush ()
{
	TRACE_API ("call Reset in flush()\n");
	unit->GlobalReset ();
}

bool
AUPlugin::requires_fixed_size_buffers() const
{
	return _requires_fixed_size_buffers;
}


int
AUPlugin::set_block_size (nframes_t nframes)
{
	bool was_initialized = initialized;
	UInt32 numFrames = nframes;
	OSErr err;

	if (initialized) {
		deactivate ();
	}

	TRACE_API ("set MaximumFramesPerSlice in global scope to %u\n", numFrames);
	if ((err = unit->SetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 
				      0, &numFrames, sizeof (numFrames))) != noErr) {
		cerr << "cannot set max frames (err = " << err << ')' << endl;
		return -1;
	}

	if (was_initialized) {
		activate ();
	}

	_current_block_size = nframes;

	return 0;
}

int32_t
AUPlugin::configure_io (int32_t in, int32_t out)
{
	AudioStreamBasicDescription streamFormat;
	bool was_initialized = initialized;

	if (initialized) {
		//if we are already running with the requested i/o config, bail out here
		if ( (in==input_channels) && (out==output_channels) ) {
			return 0;
		} else {
			deactivate ();
		}
	}

	streamFormat.mSampleRate = _session.frame_rate();
	streamFormat.mFormatID = kAudioFormatLinearPCM;
	streamFormat.mFormatFlags = kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved;

#ifdef __LITTLE_ENDIAN__
	/* relax */
#else
	streamFormat.mFormatFlags |= kAudioFormatFlagIsBigEndian;
#endif

	streamFormat.mBitsPerChannel = 32;
	streamFormat.mFramesPerPacket = 1;

	/* apple says that for non-interleaved data, these
	   values always refer to a single channel.
	*/
	streamFormat.mBytesPerPacket = 4;
	streamFormat.mBytesPerFrame = 4;

	streamFormat.mChannelsPerFrame = in;

	if (set_input_format (streamFormat) != 0) {
		return -1;
	}

	streamFormat.mChannelsPerFrame = out;

	if (set_output_format (streamFormat) != 0) {
		return -1;
	}

	int ret = Plugin::configure_io (in, out);

	if (ret == 0) {
		if (was_initialized) {
			activate ();
		}
	}

	return ret;
}

int32_t
AUPlugin::can_do (int32_t in, int32_t& out)
{
	// XXX as of May 13th 2008, AU plugin support returns a count of either 1 or -1. We never
	// attempt to multiply-instantiate plugins to meet io configurations.

	int32_t plugcnt = -1;
	AUPluginInfoPtr pinfo = boost::dynamic_pointer_cast<AUPluginInfo>(get_info());

	vector<pair<int,int> >& io_configs = pinfo->cache.io_configs;

	if (debug_io_config) {
		cerr << name() << " has " << io_configs.size() << " IO Configurations\n";
	}

	//Ardour expects the plugin to tell it the output configuration
	//but AU plugins can have multiple I/O configurations
	//in most cases (since we don't allow special routing like sidechains in A2, we want to preserve the number of streams
	//so first lets see if there's a configuration that keeps out==in
	out = in;
	for (vector<pair<int,int> >::iterator i = io_configs.begin(); i != io_configs.end(); ++i) {
		int32_t possible_in = i->first;
		int32_t possible_out = i->second;

		if (possible_in == in && possible_out== out) {
			cerr << "\tCHOSEN: in " << in << " out " << out << endl;
			return 1;
		}
	}

	/* now allow potentially "imprecise" matches */
	out = -1;
	for (vector<pair<int,int> >::iterator i = io_configs.begin(); i != io_configs.end(); ++i) {

		int32_t possible_in = i->first;
		int32_t possible_out = i->second;

		if (debug_io_config) {
			cerr << "\tin " << possible_in << " out " << possible_out << endl;
		}

		if (possible_out == 0) {
			warning << string_compose (_("AU %1 has zero outputs - configuration ignored"), name()) << endmsg;
			continue;
		}

		if (possible_in == 0) {

			/* instrument plugin, always legal but throws away inputs ...
			*/

			if (possible_out == -1) {
				/* out much match in (UNLIKELY!!) */
				out = in;
				plugcnt = 1;
			} else if (possible_out == -2) {
				/* any configuration possible, pick matching */
				out = in;
				plugcnt = 1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out = -possible_out;
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out = possible_out;
				plugcnt = 1;
			}
		}
		
		if (possible_in == -1) {

			/* wildcard for input */

			if (possible_out == -1) {
				/* out much match in */
				out = in;
				plugcnt = 1;
			} else if (possible_out == -2) {
				/* any configuration possible, pick matching */
				out = in;
				plugcnt = 1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out = -possible_out;
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out = possible_out;
				plugcnt = 1;
			}
		}	
			
		if (possible_in == -2) {

			if (possible_out == -1) {
				/* any configuration possible, pick matching */
				out = in;
				plugcnt = 1;
			} else if (possible_out == -2) {
				error << string_compose (_("AU plugin %1 has illegal IO configuration (-2,-2)"), name())
				      << endmsg;
				plugcnt = -1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out = -possible_out;
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out = possible_out;
				plugcnt = 1;
			}
		}

		if (possible_in < -2) {

			/* explicit variable number of inputs */

			if (in > -possible_in) {
				/* request is too large */
				plugcnt = -1;
			}

			if (possible_out == -1) {
				/* out must match in */
				out = in;
				plugcnt = 1;
			} else if (possible_out == -2) {
				error << string_compose (_("AU plugin %1 has illegal IO configuration (-2,-2)"), name())
				      << endmsg;
				plugcnt = -1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out = -possible_out;
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out = possible_out;
				plugcnt = 1;
			}
		}

		if (possible_in == in) {

			/* exact number of inputs ... must match obviously */
			
			if (possible_out == -1) {
				/* out must match in */
				out = in;
				plugcnt = 1;
			} else if (possible_out == -2) {
				/* any output configuration, pick matching */
				out = in;
				plugcnt = -1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out = -possible_out;
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out = possible_out;
				plugcnt = 1;
			}
		}

		if (plugcnt == 1) {
			break;
		}

	}

	if (debug_io_config) {
		if (plugcnt > 0) {
			cerr << "\tCHOSEN: in " << in << " out " << out << " plugcnt will be " << plugcnt << endl;
		} else {
			cerr << "\tFAIL: no configs match requested in " << in << endl;
		}
	}

	return plugcnt;
}

int
AUPlugin::set_input_format (AudioStreamBasicDescription& fmt)
{
	return set_stream_format (kAudioUnitScope_Input, input_elements, fmt);
}

int
AUPlugin::set_output_format (AudioStreamBasicDescription& fmt)
{
	if (set_stream_format (kAudioUnitScope_Output, output_elements, fmt) != 0) {
		return -1;
	}

	if (buffers) {
		free (buffers);
		buffers = 0;
	}
	
	buffers = (AudioBufferList *) malloc (offsetof(AudioBufferList, mBuffers) + 
					      fmt.mChannelsPerFrame * sizeof(AudioBuffer));

	Glib::Mutex::Lock em (_session.engine().process_lock());
	IO::MoreOutputs (fmt.mChannelsPerFrame);

	return 0;
}

int
AUPlugin::set_stream_format (int scope, uint32_t cnt, AudioStreamBasicDescription& fmt)
{
	OSErr result;

	for (uint32_t i = 0; i < cnt; ++i) {
		TRACE_API ("set stream format for %s, scope = %d element %d\n",
			   (scope == kAudioUnitScope_Input ? "input" : "output"),
			   scope, cnt);
		if ((result = unit->SetFormat (scope, i, fmt)) != 0) {
			error << string_compose (_("AUPlugin: could not set stream format for %1/%2 (err = %3)"),
						 (scope == kAudioUnitScope_Input ? "input" : "output"), i, result) << endmsg;
			return -1;
		}
	}

	if (scope == kAudioUnitScope_Input) {
		input_channels = fmt.mChannelsPerFrame;
	} else {
		output_channels = fmt.mChannelsPerFrame;
	}

	return 0;
}

uint32_t
AUPlugin::input_streams() const
{
	if (input_channels < 0) {
		warning << string_compose (_("AUPlugin: %1 input_streams() called without any format set!"), name()) << endmsg;
		return 1;
	}
	return input_channels;
}


uint32_t
AUPlugin::output_streams() const
{
	if (output_channels < 0) {
		warning << string_compose (_("AUPlugin: %1 output_streams() called without any format set!"), name()) << endmsg;
		return 1;
	}
	return output_channels;
}

OSStatus 
AUPlugin::render_callback(AudioUnitRenderActionFlags *ioActionFlags,
			  const AudioTimeStamp    *inTimeStamp,
			  UInt32       inBusNumber,
			  UInt32       inNumberFrames,
			  AudioBufferList*       ioData)
{
	/* not much to do - the data is already in the buffers given to us in connect_and_run() */

	if (current_maxbuf == 0) {
		error << _("AUPlugin: render callback called illegally!") << endmsg;
		return kAudioUnitErr_CannotDoInCurrentContext;
	}
	uint32_t limit = min ((uint32_t) ioData->mNumberBuffers, current_maxbuf);
	for (uint32_t i = 0; i < limit; ++i) {
		ioData->mBuffers[i].mNumberChannels = 1;
		ioData->mBuffers[i].mDataByteSize = sizeof (Sample) * inNumberFrames;
		ioData->mBuffers[i].mData = (*current_buffers)[i] + cb_offset + current_offset;
		// cerr << "chn " << i << " rendering from " << ioData->mBuffers[i].mData << endl;
	}

	cb_offset += inNumberFrames;

	return noErr;
}

int
AUPlugin::connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, nframes_t nframes, nframes_t offset)
{
	AudioUnitRenderActionFlags flags = 0;
	AudioTimeStamp ts;
	OSErr err;

	if (requires_fixed_size_buffers() && (nframes != _last_nframes)) {
		unit->GlobalReset();
		_last_nframes = nframes;
	}

	current_buffers = &bufs;
	current_maxbuf = maxbuf;
	current_offset = offset;
	cb_offset = 0;

	buffers->mNumberBuffers = min ((uint32_t) output_channels, maxbuf);

	for (uint32_t i = 0; i < buffers->mNumberBuffers; ++i) {
		buffers->mBuffers[i].mNumberChannels = 1;
		buffers->mBuffers[i].mDataByteSize = nframes * sizeof (Sample);
		buffers->mBuffers[i].mData = 0;
	}

	ts.mSampleTime = frames_processed;
	ts.mFlags = kAudioTimeStampSampleTimeValid;

	if ((err = unit->Render (&flags, &ts, 0, nframes, buffers)) == noErr) {

		current_maxbuf = 0;
		frames_processed += nframes;
		
		uint32_t limit = min ((uint32_t) buffers->mNumberBuffers, maxbuf);
		uint32_t i;

		for (i = 0; i < limit; ++i) {
			if (bufs[i] + offset != buffers->mBuffers[i].mData) {
				// cerr << "chn " << i << " rendered into " << bufs[i]+offset << endl;
				memcpy (bufs[i]+offset, buffers->mBuffers[i].mData, nframes * sizeof (Sample));
			}
		}

		/* now silence any buffers that were passed in but the that the plugin
		   did not fill/touch/use.
		*/

		for (;i < maxbuf; ++i) {
			memset (bufs[i]+offset, 0, nframes * sizeof (Sample));
		}

		return 0;
	} 

	cerr << name() << " render status " << err << endl;
	return -1;
}

OSStatus 
AUPlugin::get_beat_and_tempo_callback (Float64* outCurrentBeat, 
				       Float64* outCurrentTempo)
{
	TempoMap& tmap (_session.tempo_map());

	TRACE_API ("AU calls ardour beat&tempo callback\n");

	/* more than 1 meter or more than 1 tempo means that a simplistic computation 
	   (and interpretation) of a beat position will be incorrect. So refuse to 
	   offer the value.
	*/

	if (tmap.n_tempos() > 1 || tmap.n_meters() > 1) {
		return kAudioUnitErr_CannotDoInCurrentContext;
	}

	BBT_Time bbt;
	TempoMap::Metric metric = tmap.metric_at (_session.transport_frame() + current_offset);
	tmap.bbt_time_with_metric (_session.transport_frame() + current_offset, bbt, metric);

	if (outCurrentBeat) {
		float beat;
		beat = metric.meter().beats_per_bar() * bbt.bars;
		beat += bbt.beats;
		beat += bbt.ticks / Meter::ticks_per_beat;
		*outCurrentBeat = beat;
	}

	if (outCurrentTempo) {
		*outCurrentTempo = floor (metric.tempo().beats_per_minute());
	}

	return noErr;

}

OSStatus 
AUPlugin::get_musical_time_location_callback (UInt32*   outDeltaSampleOffsetToNextBeat,
					      Float32*  outTimeSig_Numerator,
					      UInt32*   outTimeSig_Denominator,
					      Float64*  outCurrentMeasureDownBeat)
{
	TempoMap& tmap (_session.tempo_map());

	TRACE_API ("AU calls ardour music time location callback\n");

	/* more than 1 meter or more than 1 tempo means that a simplistic computation 
	   (and interpretation) of a beat position will be incorrect. So refuse to 
	   offer the value.
	*/

	if (tmap.n_tempos() > 1 || tmap.n_meters() > 1) {
		return kAudioUnitErr_CannotDoInCurrentContext;
	}

	BBT_Time bbt;
	TempoMap::Metric metric = tmap.metric_at (_session.transport_frame() + current_offset);
	tmap.bbt_time_with_metric (_session.transport_frame() + current_offset, bbt, metric);

	if (outDeltaSampleOffsetToNextBeat) {
		if (bbt.ticks == 0) {
			/* on the beat */
			*outDeltaSampleOffsetToNextBeat = 0;
		} else {
			*outDeltaSampleOffsetToNextBeat = (UInt32) floor (((Meter::ticks_per_beat - bbt.ticks)/Meter::ticks_per_beat) * // fraction of a beat to next beat
									  metric.tempo().frames_per_beat(_session.frame_rate(), metric.meter())); // frames per beat
		}
	}
	
	if (outTimeSig_Numerator) {
		*outTimeSig_Numerator = (UInt32) lrintf (metric.meter().beats_per_bar());
	}
	if (outTimeSig_Denominator) {
		*outTimeSig_Denominator = (UInt32) lrintf (metric.meter().note_divisor());
	}

	if (outCurrentMeasureDownBeat) {

		/* beat for the start of the bar. 
		   1|1|0 -> 1
		   2|1|0 -> 1 + beats_per_bar
		   3|1|0 -> 1 + (2 * beats_per_bar)
		   etc. 
		*/

		*outCurrentMeasureDownBeat = 1 + metric.meter().beats_per_bar() * (bbt.bars - 1);
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
	bool rolling;
	float speed;

	TRACE_API ("AU calls ardour transport state callback\n");

	rolling = _session.transport_rolling();
	speed = _session.transport_speed ();

	if (outIsPlaying) {
		*outIsPlaying = _session.transport_rolling();
	}

	if (outTransportStateChanged) {
		if (rolling != last_transport_rolling) {
			*outTransportStateChanged = true;
		} else if (speed != last_transport_speed) {
			*outTransportStateChanged = true;
		} else {
			*outTransportStateChanged = false;
		}
	}

	if (outCurrentSampleInTimeLine) {
		/* this assumes that the AU can only call this host callback from render context,
		   where current_offset is valid.
		*/
		*outCurrentSampleInTimeLine = _session.transport_frame() + current_offset;
	}

	if (outIsCycling) {
		Location* loc = _session.locations()->auto_loop_location();

		*outIsCycling = (loc && _session.transport_rolling() && _session.get_play_loop());

		if (*outIsCycling) {

			if (outCycleStartBeat || outCycleEndBeat) {

				TempoMap& tmap (_session.tempo_map());

				/* more than 1 meter means that a simplistic computation (and interpretation) of 
				   a beat position will be incorrect. so refuse to offer the value.
				*/

				if (tmap.n_meters() > 1) {
					return kAudioUnitErr_CannotDoInCurrentContext;
				}
				
				BBT_Time bbt;

				if (outCycleStartBeat) {
					TempoMap::Metric metric = tmap.metric_at (loc->start() + current_offset);
					_session.tempo_map().bbt_time_with_metric (loc->start(), bbt, metric);

					float beat;
					beat = metric.meter().beats_per_bar() * bbt.bars;
					beat += bbt.beats;
					beat += bbt.ticks / Meter::ticks_per_beat;
					
					*outCycleStartBeat = beat;
				}

				if (outCycleEndBeat) {
					TempoMap::Metric metric = tmap.metric_at (loc->end() + current_offset);
					_session.tempo_map().bbt_time_with_metric (loc->end(), bbt, metric);
					
					float beat;
					beat = metric.meter().beats_per_bar() * bbt.bars;
					beat += bbt.beats;
					beat += bbt.ticks / Meter::ticks_per_beat;
					
					*outCycleEndBeat = beat;
				}
			}
		}
	}

	last_transport_rolling = rolling;
	last_transport_speed = speed;

	return noErr;
}

set<uint32_t>
AUPlugin::automatable() const
{
	set<uint32_t> automates;

	for (uint32_t i = 0; i < descriptors.size(); ++i) {
		if (descriptors[i].automatable) {
			automates.insert (i);
		}
	}

	return automates;
}

string
AUPlugin::describe_parameter (uint32_t param)
{
        if (param < descriptors.size()) {
                return descriptors[param].label;
        } else {
                return "??";
        }
}

void
AUPlugin::print_parameter (uint32_t param, char* buf, uint32_t len) const
{
	// NameValue stuff here
}

bool
AUPlugin::parameter_is_audio (uint32_t) const
{
	return false;
}

bool
AUPlugin::parameter_is_control (uint32_t) const
{
	return true;
}

bool
AUPlugin::parameter_is_input (uint32_t) const
{
	return false;
}

bool
AUPlugin::parameter_is_output (uint32_t) const
{
	return false;
}

XMLNode&
AUPlugin::get_state()
{
	LocaleGuard lg (X_("POSIX"));
	XMLNode *root = new XMLNode (state_node_name());

#ifdef AU_STATE_SUPPORT
	CFDataRef xmlData;
	CFPropertyListRef propertyList;

	TRACE_API ("get preset state\n");
	if (unit->GetAUPreset (propertyList) != noErr) {
		return *root;
	}

	// Convert the property list into XML data.
	
	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, propertyList);

	if (!xmlData) {
		error << _("Could not create XML version of property list") << endmsg;
		return *root;
	}

	/* re-parse XML bytes to create a libxml++ XMLTree that we can merge into
	   our state node. GACK!
	*/

	XMLTree t;

	if (t.read_buffer (string ((const char*) CFDataGetBytePtr (xmlData), CFDataGetLength (xmlData)))) {
		if (t.root()) {
			root->add_child_copy (*t.root());
		}
	}

	CFRelease (xmlData);
	CFRelease (propertyList);
#else
	if (!seen_get_state_message) {
		info << string_compose (_("Saving AudioUnit settings is not supported in this build of %1. Consider paying for a newer version"),
					PROGRAM_NAME)
		     << endmsg;
		seen_get_state_message = true;
	}
#endif
	
	return *root;
}

int
AUPlugin::set_state(const XMLNode& node)
{
#ifdef AU_STATE_SUPPORT
	int ret = -1;
	CFPropertyListRef propertyList;
	LocaleGuard lg (X_("POSIX"));

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
		TRACE_API ("set preset\n");
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
	
	return ret;
#else
	if (!seen_set_state_message) {
		info << string_compose (_("Restoring AudioUnit settings is not supported in this build of %1. Consider paying for a newer version"),
					PROGRAM_NAME)
		     << endmsg;
	}
	return 0;
#endif
}

bool
AUPlugin::load_preset (const string preset_label)
{
#ifdef AU_STATE_SUPPORT
	bool ret = false;
	CFPropertyListRef propertyList;
	std::string path;
	UserPresetMap::iterator ux;
	FactoryPresetMap::iterator fx;

	/* look first in "user" presets */

	if ((ux = user_preset_map.find (preset_label)) != user_preset_map.end()) {
	
		if ((propertyList = load_property_list (ux->second)) != 0) {
			TRACE_API ("set preset from user presets\n");
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

	} else if ((fx = factory_preset_map.find (preset_label)) != factory_preset_map.end()) {
		
		AUPreset preset;
		
		preset.presetNumber = fx->second;
		preset.presetName = CFStringCreateWithCString (kCFAllocatorDefault, fx->first.c_str(), kCFStringEncodingUTF8);
		
		TRACE_API ("set preset from factory presets\n");

		if (unit->SetPresentPreset (preset) == 0) {
			ret = true;

			/* tell the world */

			AudioUnitParameter changedUnit;
			changedUnit.mAudioUnit = unit->AU();
			changedUnit.mParameterID = kAUParameterListener_AnyParameter;
			AUParameterListenerNotify (NULL, NULL, &changedUnit);
		}
	}
		
	return ret;
#else
	if (!seen_loading_message) {
		info << string_compose (_("Loading AudioUnit presets is not supported in this build of %1. Consider paying for a newer version"),
					PROGRAM_NAME)
		     << endmsg;
		seen_loading_message = true;
	}
	return true;
#endif
}

bool
AUPlugin::save_preset (string preset_name)
{
#ifdef AU_STATE_SUPPORT
	CFPropertyListRef propertyList;
	vector<std::string> v;
	std::string user_preset_path;
	bool ret = true;

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
		return false;
	}

	TRACE_API ("get current preset\n");
	if (unit->GetAUPreset (propertyList) != noErr) {
		return false;
	}

	// add the actual preset name */

	v.push_back (preset_name + preset_suffix);
		
	// rebuild

	user_preset_path = Glib::build_filename (v);
	
	set_preset_name_in_plist (propertyList, preset_name);

	if (save_property_list (propertyList, user_preset_path)) {
		error << string_compose (_("Saving plugin state to %1 failed"), user_preset_path) << endmsg;
		ret = false;
	}

	CFRelease(propertyList);

	return ret;
#else
	if (!seen_saving_message) {
		info << string_compose (_("Saving AudioUnit presets is not supported in this build of %1. Consider paying for a newer version"),
					PROGRAM_NAME)
		     << endmsg;
		seen_saving_message = true;
	}
	return false;
#endif
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
GetAUComponentDescriptionFromStateData(CFPropertyListRef inAUStateData, ComponentDescription * outComponentDescription)
{
        CFDictionaryRef auStateDictionary;
        ComponentDescription tempDesc = {0};
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

		Plugin* p = (Plugin *) arg;
		string match = p->maker();
		match += '/';
		match += p->name();

		ret = str.find (match) != string::npos;

		if (ret == false) {
			string m = p->maker ();
			string n = p->name ();
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

bool 
check_and_get_preset_name (Component component, const string& pathstr, string& preset_name)
{
        OSStatus status;
        CFPropertyListRef plist;
	ComponentDescription presetDesc;
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
	
#ifdef AU_STATE_SUPPORT
	CFPropertyListRef propertyList;

	TRACE_API ("get current preset for current_preset()\n");
	if (unit->GetAUPreset (propertyList) == noErr) {
		preset_name = get_preset_name_in_plist (propertyList);
		CFRelease(propertyList);
	}
#endif
	return preset_name;
}

vector<string>
AUPlugin::get_presets ()
{
	vector<string> presets;

#ifdef AU_STATE_SUPPORT
	vector<string*>* preset_files;
	PathScanner scanner;

	user_preset_map.clear ();

	preset_files = scanner (preset_search_path, au_preset_filter, this, true, true, -1, true);
	
	if (!preset_files) {
		return presets;
	}

	for (vector<string*>::iterator x = preset_files->begin(); x != preset_files->end(); ++x) {

		string path = *(*x);
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
		} 

		delete *x;
	}

	delete preset_files;

        /* now fill the vector<string> with the names we have */

	for (UserPresetMap::iterator i = user_preset_map.begin(); i != user_preset_map.end(); ++i) {
		presets.push_back (i->first);
	}

        /* add factory presets */

	for (FactoryPresetMap::iterator i = factory_preset_map.begin(); i != factory_preset_map.end(); ++i) {
		presets.push_back (i->first);
	}

#endif
	
	return presets;
}

bool
AUPlugin::has_editor () const
{
	// even if the plugin doesn't have its own editor, the AU API can be used
	// to create one that looks native.
	return true;
}

AUPluginInfo::AUPluginInfo (boost::shared_ptr<CAComponentDescription> d)
	: descriptor (d)
{
	type = ARDOUR::AudioUnit;
}

AUPluginInfo::~AUPluginInfo ()
{
	type = ARDOUR::AudioUnit;
}

PluginPtr
AUPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		TRACE_API ("load AU as a component\n");
		boost::shared_ptr<CAComponent> comp (new CAComponent(*descriptor));
		
		if (!comp->IsValid()) {
			error << ("AudioUnit: not a valid Component") << endmsg;
		} else {
			plugin.reset (new AUPlugin (session.engine(), session, comp));
		}
		
		AUPluginInfo *aup = new AUPluginInfo (*this);
		plugin->set_info (PluginInfoPtr (aup));
		boost::dynamic_pointer_cast<AUPlugin> (plugin)->set_fixed_size_buffers (aup->creator == "Universal Audio");
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ();
	}
}

std::string
AUPluginInfo::au_cache_path ()
{
	return Glib::build_filename (ARDOUR::get_user_ardour_path(), "au_cache");
}

PluginInfoList
AUPluginInfo::discover ()
{
	XMLTree tree;

	if (!Glib::file_test (au_cache_path(), Glib::FILE_TEST_EXISTS)) {
		ARDOUR::BootMessage (_("Discovering AudioUnit plugins (could take some time ...)"));
	}

	PluginInfoList plugs;
	
	discover_fx (plugs);
	discover_music (plugs);
	discover_generators (plugs);

	return plugs;
}

void
AUPluginInfo::discover_music (PluginInfoList& plugs)
{
	CAComponentDescription desc;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentSubType = 0;
	desc.componentManufacturer = 0;
	desc.componentType = kAudioUnitType_MusicEffect;

	discover_by_description (plugs, desc);
}

void
AUPluginInfo::discover_fx (PluginInfoList& plugs)
{
	CAComponentDescription desc;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentSubType = 0;
	desc.componentManufacturer = 0;
	desc.componentType = kAudioUnitType_Effect;

	discover_by_description (plugs, desc);
}

void
AUPluginInfo::discover_generators (PluginInfoList& plugs)
{
	CAComponentDescription desc;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentSubType = 0;
	desc.componentManufacturer = 0;
	desc.componentType = kAudioUnitType_Generator;

	discover_by_description (plugs, desc);
}

void
AUPluginInfo::discover_by_description (PluginInfoList& plugs, CAComponentDescription& desc)
{
	Component comp = 0;

	comp = FindNextComponent (NULL, &desc);

	while (comp != NULL) {
		CAComponentDescription temp;
		GetComponentInfo (comp, &temp, NULL, NULL, NULL);

		AUPluginInfoPtr info (new AUPluginInfo 
				      (boost::shared_ptr<CAComponentDescription> (new CAComponentDescription(temp))));

		/* although apple designed the subtype field to be a "category" indicator,
		   its really turned into a plugin ID field for a given manufacturer. Hence
		   there are no categories for AudioUnits. However, to keep the plugins
		   showing up under "categories", we'll use the "type" as a high level
		   selector.
		   
		   NOTE: no panners, format converters or i/o AU's for our purposes
		 */

		switch (info->descriptor->Type()) {
		case kAudioUnitType_Panner:
		case kAudioUnitType_OfflineEffect:
		case kAudioUnitType_FormatConverter:
			continue;

		case kAudioUnitType_Output:
			info->category = _("AudioUnit Outputs");
			break;
		case kAudioUnitType_MusicDevice:
			info->category = _("AudioUnit Instruments");
			break;
		case kAudioUnitType_MusicEffect:
			info->category = _("AudioUnit MusicEffects");
			break;
		case kAudioUnitType_Effect:
			info->category = _("AudioUnit Effects");
			break;
		case kAudioUnitType_Mixer:
			info->category = _("AudioUnit Mixers");
			break;
		case kAudioUnitType_Generator:
			info->category = _("AudioUnit Generators");
			break;
		default:
			info->category = _("AudioUnit (Unknown)");
			break;
		}

		AUPluginInfo::get_names (temp, info->name, info->creator);
		
		info->type = ARDOUR::AudioUnit;
		info->unique_id = stringify_descriptor (*info->descriptor);

		/* XXX not sure of the best way to handle plugin versioning yet
		 */

		CAComponent cacomp (*info->descriptor);

		if (cacomp.GetResourceVersion (info->version) != noErr) {
			info->version = 0;
		}
		
		if (cached_io_configuration (info->unique_id, info->version, cacomp, info->cache, info->name)) {

			/* here we have to map apple's wildcard system to a simple pair
			   of values. in ::can_do() we use the whole system, but here
			   we need a single pair of values. XXX probably means we should
			   remove any use of these values.
			*/

			info->n_inputs = info->cache.io_configs.front().first;
			info->n_outputs = info->cache.io_configs.front().second;

			cerr << "detected AU: " << info->name.c_str() << "  (" << info->cache.io_configs.size() << " i/o configurations) - " << info->unique_id << endl;

			plugs.push_back (info);

		} else {
			error << string_compose (_("Cannot get I/O configuration info for AU %1"), info->name) << endmsg;
		}
		
		comp = FindNextComponent (comp, &desc);
	}
}

bool
AUPluginInfo::cached_io_configuration (const std::string& unique_id, 
				       UInt32 version,
				       CAComponent& comp, 
				       AUPluginCachedInfo& cinfo, 
				       const std::string& name)
{
	std::string id;
	char buf[32];

	/* concatenate unique ID with version to provide a key for cached info lookup.
	   this ensures we don't get stale information, or should if plugin developers
	   follow Apple "guidelines".
	 */

	snprintf (buf, sizeof (buf), "%u", (uint32_t) version);
	id = unique_id;
	id += '/';
	id += buf;

	CachedInfoMap::iterator cim = cached_info.find (id);

	if (cim != cached_info.end()) {
		cinfo = cim->second;
		return true;
	}

	CAAudioUnit unit;
	AUChannelInfo* channel_info;
	UInt32 cnt;
	int ret;
	
	ARDOUR::BootMessage (string_compose (_("Checking AudioUnit: %1"), name));
	
	try {

		if (CAAudioUnit::Open (comp, unit) != noErr) {
			return false;
		}

	} catch (...) {

		warning << string_compose (_("Could not load AU plugin %1 - ignored"), name) << endmsg;
		cerr << string_compose (_("Could not load AU plugin %1 - ignored"), name) << endl;
		return false;

	}
		
	TRACE_API ("get AU channel info\n");
	if ((ret = unit.GetChannelInfo (&channel_info, cnt)) < 0) {
		return false;
	}

	if (ret > 0) {
		/* no explicit info available */

		cinfo.io_configs.push_back (pair<int,int> (-1, -1));

	} else {
		
		/* store each configuration */
		
		for (uint32_t n = 0; n < cnt; ++n) {
			cinfo.io_configs.push_back (pair<int,int> (channel_info[n].inChannels,
								   channel_info[n].outChannels));
		}

		free (channel_info);
	}

	add_cached_info (id, cinfo);
	save_cached_info ();

	return true;
}

void
AUPluginInfo::add_cached_info (const std::string& id, AUPluginCachedInfo& cinfo)
{
	cached_info[id] = cinfo;
}

#define AU_CACHE_VERSION "2.0"

void
AUPluginInfo::save_cached_info ()
{
	XMLNode* node;

	node = new XMLNode (X_("AudioUnitPluginCache"));
	node->add_property( "version", AU_CACHE_VERSION );
	
	for (map<string,AUPluginCachedInfo>::iterator i = cached_info.begin(); i != cached_info.end(); ++i) {
		XMLNode* parent = new XMLNode (X_("plugin"));
		parent->add_property ("id", i->first);
		node->add_child_nocopy (*parent);

		for (vector<pair<int, int> >::iterator j = i->second.io_configs.begin(); j != i->second.io_configs.end(); ++j) {

			XMLNode* child = new XMLNode (X_("io"));
			char buf[32];

			snprintf (buf, sizeof (buf), "%d", j->first);
			child->add_property (X_("in"), buf);
			snprintf (buf, sizeof (buf), "%d", j->second);
			child->add_property (X_("out"), buf);
			parent->add_child_nocopy (*child);
		}

	}

	std::string path = au_cache_path ();
	XMLTree tree;

	tree.set_root (node);

	if (!tree.write (path)) {
		error << string_compose (_("could not save AU cache to %1"), path) << endmsg;
		unlink (path.c_str());
	}
}

int
AUPluginInfo::load_cached_info ()
{
	std::string path = au_cache_path ();
	XMLTree tree;
	
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return 0;
	}

	if ( !tree.read (path) ) {
		error << "au_cache is not a valid XML file.  AU plugins will be re-scanned" << endmsg;
		return -1;
	}
	
	const XMLNode* root (tree.root());

	if (root->name() != X_("AudioUnitPluginCache")) {
		return -1;
	}
	
	//initial version has incorrectly stored i/o info, and/or garbage chars.
	const XMLProperty* version = root->property(X_("version"));
	if (! ((version != NULL) && (version->value() == X_(AU_CACHE_VERSION)))) {
		error << "au_cache is not correct version.  AU plugins will be re-scanned" << endmsg;
		return -1;
	}
	
	cached_info.clear ();

	const XMLNodeList children = root->children();

	for (XMLNodeConstIterator iter = children.begin(); iter != children.end(); ++iter) {
		
		const XMLNode* child = *iter;
		
		if (child->name() == X_("plugin")) {

			const XMLNode* gchild;
			const XMLNodeList gchildren = child->children();
			const XMLProperty* prop = child->property (X_("id"));

			if (!prop) {
				continue;
			}
			
			string id = prop->value();
			string fixed;
			string version;

			string::size_type slash = id.find_last_of ('/');

			if (slash == string::npos) {
				continue;
			}

			version = id.substr (slash);
			id = id.substr (0, slash);
			fixed = AUPlugin::maybe_fix_broken_au_id (id);

			if (fixed.empty()) {
				error << string_compose (_("Your AudioUnit configuration cache contains an AU plugin whose ID cannot be understood - ignored (%1)"), id) << endmsg;
				continue;
			} 

			id = fixed;
			id += version;

			AUPluginCachedInfo cinfo;

			for (XMLNodeConstIterator giter = gchildren.begin(); giter != gchildren.end(); giter++) {

				gchild = *giter;

				if (gchild->name() == X_("io")) {

					int in;
					int out;
					const XMLProperty* iprop;
					const XMLProperty* oprop;

					if (((iprop = gchild->property (X_("in"))) != 0) &&
					    ((oprop = gchild->property (X_("out"))) != 0)) {
						in = atoi (iprop->value());
						out = atoi (oprop->value());
						
						cinfo.io_configs.push_back (pair<int,int> (in, out));
					}
				}
			}

			if (cinfo.io_configs.size()) {
				add_cached_info (id, cinfo);
			}
		}
	}

	return 0;
}

void
AUPluginInfo::get_names (CAComponentDescription& comp_desc, std::string& name, std::string& maker)
{
	CFStringRef itemName = NULL;

	// Marc Poirier-style item name
	CAComponent auComponent (comp_desc);
	if (auComponent.IsValid()) {
		CAComponentDescription dummydesc;
		Handle nameHandle = NewHandle(sizeof(void*));
		if (nameHandle != NULL) {
			OSErr err = GetComponentInfo(auComponent.Comp(), &dummydesc, nameHandle, NULL, NULL);
			if (err == noErr) {
				ConstStr255Param nameString = (ConstStr255Param) (*nameHandle);
				if (nameString != NULL) {
					itemName = CFStringCreateWithPascalString(kCFAllocatorDefault, nameString, CFStringGetSystemEncoding());
				}
			}
			DisposeHandle(nameHandle);
		}
	}
    
	// if Marc-style fails, do the original way
	if (itemName == NULL) {
		CFStringRef compTypeString = UTCreateStringForOSType(comp_desc.componentType);
		CFStringRef compSubTypeString = UTCreateStringForOSType(comp_desc.componentSubType);
		CFStringRef compManufacturerString = UTCreateStringForOSType(comp_desc.componentManufacturer);
    
		itemName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ - %@ - %@"), 
			compTypeString, compManufacturerString, compSubTypeString);
    
		if (compTypeString != NULL)
			CFRelease(compTypeString);
		if (compSubTypeString != NULL)
			CFRelease(compSubTypeString);
		if (compManufacturerString != NULL)
			CFRelease(compManufacturerString);
	}
	
	string str = CFStringRefToStdString(itemName);
	string::size_type colon = str.find (':');

	if (colon) {
		name = str.substr (colon+1);
		maker = str.substr (0, colon);
		strip_whitespace_edges (maker);
		strip_whitespace_edges (name);
	} else {
		name = str;
		maker = "unknown";
		strip_whitespace_edges (name);
	}
}

std::string
AUPluginInfo::stringify_descriptor (const CAComponentDescription& desc)
{
	stringstream s;

	/* note: OSType is a compiler-implemenation-defined value,
	   historically a 32 bit integer created with a multi-character
	   constant such as 'abcd'. It is, fundamentally, an abomination.
	*/

	s << desc.Type();
	s << '-';
	s << desc.SubType();
	s << '-';
	s << desc.Manu();

	return s.str();
}

int
AUPlugin::create_parameter_listener (AUEventListenerProc cb, void* arg, float interval_secs)
{
        CFRunLoopRef run_loop =  CFRunLoopGetCurrent ();
	CFStringRef  loop_mode = kCFRunLoopDefaultMode;

	if (AUEventListenerCreate (cb, arg, run_loop, loop_mode, interval_secs, interval_secs, &_parameter_listener) != noErr) {
		return -1;
	}

	_parameter_listener_arg = arg;

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

	return 0;
}

void
AUPlugin::_parameter_change_listener (void* arg, void* src, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value)
{
	((AUPlugin*) arg)->parameter_change_listener (arg, src, event, host_time, new_value);
}

void
AUPlugin::parameter_change_listener (void* /*arg*/, void* /*src*/, const AudioUnitEvent* event, UInt64 host_time, Float32 new_value)
{
	ParameterMap::iterator i = parameter_map.find (event->mArgument.mParameter.mParameterID);

	if (i != parameter_map.end()) {
		ParameterChanged (i->second, new_value);
	}
}
