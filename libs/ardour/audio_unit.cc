/*
    Copyright (C) 2006 Paul Davis 
	
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

#include <pbd/transmitter.h>
#include <pbd/xml++.h>
#include <pbd/whitespace.h>

#include <glibmm/thread.h>

#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/audio_unit.h>
#include <ardour/session.h>
#include <ardour/utils.h>

#include <appleutility/CAAudioUnit.h>
#include <appleutility/CAAUParameter.h>

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

static OSStatus 
_render_callback(void *userData,
		 AudioUnitRenderActionFlags *ioActionFlags,
		 const AudioTimeStamp    *inTimeStamp,
		 UInt32       inBusNumber,
		 UInt32       inNumberFrames,
		 AudioBufferList*       ioData)
{
	return ((AUPlugin*)userData)->render_callback (ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

AUPlugin::AUPlugin (AudioEngine& engine, Session& session, boost::shared_ptr<CAComponent> _comp)
	:
	Plugin (engine, session),
	comp (_comp),
	unit (new CAAudioUnit),
	initialized (false),
	buffers (0),
	current_maxbuf (0),
	current_offset (0),
	current_buffers (0),
	frames_processed (0)
{			
	OSErr err = CAAudioUnit::Open (*(comp.get()), *unit);

	if (err != noErr) {
		error << _("AudioUnit: Could not convert CAComponent to CAAudioUnit") << endmsg;
		throw failed_constructor ();
	}
	
	AURenderCallbackStruct renderCallbackInfo;

	renderCallbackInfo.inputProc = _render_callback;
	renderCallbackInfo.inputProcRefCon = this;

	if ((err = unit->SetProperty (kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
					 0, (void*) &renderCallbackInfo, sizeof(renderCallbackInfo))) != 0) {
		cerr << "cannot install render callback (err = " << err << ')' << endl;
		throw failed_constructor();
	}

	unit->GetElementCount (kAudioUnitScope_Global, global_elements);
	unit->GetElementCount (kAudioUnitScope_Input, input_elements);
	unit->GetElementCount (kAudioUnitScope_Output, output_elements);

	// set up the basic stream format. these fields do not change
			    
	streamFormat.mSampleRate = session.frame_rate();
	streamFormat.mFormatID = kAudioFormatLinearPCM;
	streamFormat.mFormatFlags = kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved;
	streamFormat.mBitsPerChannel = 32;
	streamFormat.mFramesPerPacket = 1;

	// subject to later modification as we discover channel counts

	streamFormat.mBytesPerPacket = 4;
	streamFormat.mBytesPerFrame = 4;
	streamFormat.mChannelsPerFrame = 1;

	format_set = 0;

	if (_set_block_size (_session.get_block_size())) {
		error << _("AUPlugin: cannot set processing block size") << endmsg;
		throw failed_constructor();
	}

	discover_parameters ();

	Plugin::setup_controls ();
}

AUPlugin::~AUPlugin ()
{
	if (unit) {
		unit->Uninitialize ();
	}

	if (buffers) {
		free (buffers);
	}
}

void
AUPlugin::discover_parameters ()
{
	/* discover writable parameters */
	
	cerr << "get param info, there are " << global_elements << " global elements\n";

	AudioUnitScope scopes[] = { 
		kAudioUnitScope_Global,
		kAudioUnitScope_Output,
		kAudioUnitScope_Input
	};

	descriptors.clear ();

	for (uint32_t i = 0; i < sizeof (scopes) / sizeof (scopes[0]); ++i) {

		AUParamInfo param_info (unit->AU(), false, false, scopes[i]);
		
		cerr << "discovered " << param_info.NumParams() << " parameters in scope " << i << endl;
		
		for (uint32_t i = 0; i < param_info.NumParams(); ++i) {

			AUParameterDescriptor d;

			d.id = param_info.ParamID (i);

			const CAAUParameter* param = param_info.GetParamInfo (d.id);
			const AudioUnitParameterInfo& info (param->ParamInfo());

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

			d.integer_step = (info.unit & kAudioUnitParameterUnit_Indexed);
			d.toggled = (info.unit & kAudioUnitParameterUnit_Boolean);
			d.sr_dependent = (info.unit & kAudioUnitParameterUnit_SampleFrames);

			d.automatable = !(info.flags & kAudioUnitParameterFlag_NonRealTime);
			d.logarithmic = (info.flags & kAudioUnitParameterFlag_DisplayLogarithmic);

			const int len = CFStringGetLength (param->GetName());;
			char local_buffer[len*2];
			Boolean good = CFStringGetCString(param->GetName(),local_buffer,len*2,kCFStringEncodingMacRoman);
			if (!good) {
				d.label = "???";
			} else {
				d.label = local_buffer;
			}

			d.lower = info.minValue;
			d.upper = info.maxValue;
			d.default_value = info.defaultValue;
			d.step = 1.0;
			d.smallstep = 0.1;
			d.largestep = 10.0;
			d.min_unbound = 0; // lower is bound
			d.max_unbound = 0; // upper is bound

			descriptors.push_back (d);
		}
	}
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
	return unit->Latency ();
}

void
AUPlugin::set_parameter (uint32_t which, float val)
{
	// unit->SetParameter (id, 0, val);
}

float
AUPlugin::get_parameter (uint32_t which) const
{
	float outValue = 0.0;
	
	// unit->GetParameter(parameter_map[which].first, parameter_map[which].second, 0, outValue);
	
	return outValue;
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
		if ((err = unit->Initialize()) != noErr) {
			error << string_compose (_("AUPlugin: cannot initialize plugin (err = %1)"), err) << endmsg;
		} else {
			frames_processed = 0;
			initialized = true;
		}
	}
}

void
AUPlugin::deactivate ()
{
	unit->GlobalReset ();
}

void
AUPlugin::set_block_size (nframes_t nframes)
{
	_set_block_size (nframes);
}

int
AUPlugin::_set_block_size (nframes_t nframes)
{
	bool was_initialized = initialized;
	UInt32 numFrames = nframes;
	OSErr err;

	if (initialized) {
		unit->Uninitialize ();
	}

	if ((err = unit->SetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 
				      0, &numFrames, sizeof (numFrames))) != noErr) {
		cerr << "cannot set max frames (err = " << err << ')' << endl;
		return -1;
	}

	if (was_initialized) {
		activate ();
	}

	return 0;
}

int32_t 
AUPlugin::can_support_input_configuration (int32_t in)
{	
	streamFormat.mChannelsPerFrame = in;
	/* apple says that for non-interleaved data, these
	   values always refer to a single channel.
	*/
	streamFormat.mBytesPerPacket = 4;
	streamFormat.mBytesPerFrame = 4;

	if (set_input_format () == 0) {
		return 1;
	} else {
		return -1;
	}
}

int
AUPlugin::set_input_format ()
{
	return set_stream_format (kAudioUnitScope_Input, input_elements);
}

int
AUPlugin::set_output_format ()
{
	return set_stream_format (kAudioUnitScope_Output, output_elements);
}

int
AUPlugin::set_stream_format (int scope, uint32_t cnt)
{
	OSErr result;

	for (uint32_t i = 0; i < cnt; ++i) {
		if ((result = unit->SetFormat (scope, i, streamFormat)) != 0) {
			error << string_compose (_("AUPlugin: could not set stream format for %1/%2 (err = %3)"),
						 (scope == kAudioUnitScope_Input ? "input" : "output"), i, result) << endmsg;
			return -1;
		}
	}

	if (scope == kAudioUnitScope_Input) {
		format_set |= 0x1;
	} else {
		format_set |= 0x2;
	}

	return 0;
}

int32_t 
AUPlugin::compute_output_streams (int32_t nplugins)
{
	/* we will never replicate AU plugins - either they can do the I/O we need
	   or not. thus, we can ignore nplugins entirely.
	*/
	
	if (set_output_format() == 0) {

		if (buffers) {
			free (buffers);
			buffers = 0;
		}

		buffers = (AudioBufferList *) malloc (offsetof(AudioBufferList, mBuffers) + 
						      streamFormat.mChannelsPerFrame * sizeof(AudioBuffer));

		Glib::Mutex::Lock em (_session.engine().process_lock());
		IO::MoreOutputs (streamFormat.mChannelsPerFrame);

		return streamFormat.mChannelsPerFrame;
	} else {
		return -1;
	}
}

uint32_t
AUPlugin::output_streams() const
{
	if (!(format_set & 0x2)) {
		warning << _("AUPlugin: output_streams() called without any format set!") << endmsg;
		return 1;
	}
	return streamFormat.mChannelsPerFrame;
}


uint32_t
AUPlugin::input_streams() const
{
	if (!(format_set & 0x1)) {
		warning << _("AUPlugin: input_streams() called without any format set!") << endmsg;
		return 1;
	}
	return streamFormat.mChannelsPerFrame;
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

	for (uint32_t i = 0; i < current_maxbuf; ++i) {
		ioData->mBuffers[i].mNumberChannels = 1;
		ioData->mBuffers[i].mDataByteSize = sizeof (Sample) * inNumberFrames;
		ioData->mBuffers[i].mData = (*current_buffers)[i] + cb_offset + current_offset;
	}

	cb_offset += inNumberFrames;

	return noErr;
}

int
AUPlugin::connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, nframes_t nframes, nframes_t offset)
{
	AudioUnitRenderActionFlags flags = 0;
	AudioTimeStamp ts;

	current_buffers = &bufs;
	current_maxbuf = maxbuf;
	current_offset = offset;
	cb_offset = 0;

	buffers->mNumberBuffers = maxbuf;

	for (uint32_t i = 0; i < maxbuf; ++i) {
		buffers->mBuffers[i].mNumberChannels = 1;
		buffers->mBuffers[i].mDataByteSize = nframes * sizeof (Sample);
		buffers->mBuffers[i].mData = 0;
	}

	ts.mSampleTime = frames_processed;
	ts.mFlags = kAudioTimeStampSampleTimeValid;

	if (unit->Render (&flags, &ts, 0, nframes, buffers) == noErr) {

		current_maxbuf = 0;
		frames_processed += nframes;
		
		for (uint32_t i = 0; i < maxbuf; ++i) {
			if (bufs[i] + offset != buffers->mBuffers[i].mData) {
				memcpy (bufs[i]+offset, buffers->mBuffers[i].mData, nframes * sizeof (Sample));
			}
		}
		return 0;
	}

	return -1;
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
	return descriptors[param].label;
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
	XMLNode *root = new XMLNode (state_node_name());
	LocaleGuard lg (X_("POSIX"));
	return *root;
}

int
AUPlugin::set_state(const XMLNode& node)
{
	return -1;
}

bool
AUPlugin::save_preset (string name)
{
	return false;
}

bool
AUPlugin::load_preset (const string preset_label)
{
	return false;
}

vector<string>
AUPlugin::get_presets ()
{
	vector<string> presets;
	
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

}

AUPluginInfo::~AUPluginInfo ()
{
}

PluginPtr
AUPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		boost::shared_ptr<CAComponent> comp (new CAComponent(*descriptor));
		
		if (!comp->IsValid()) {
			error << ("AudioUnit: not a valid Component") << endmsg;
		} else {
			plugin.reset (new AUPlugin (session.engine(), session, comp));
		}
		
		plugin->set_info (PluginInfoPtr (new AUPluginInfo (*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

PluginInfoList
AUPluginInfo::discover ()
{
	PluginInfoList plugs;
	
	discover_fx (plugs);
	discover_music (plugs);

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
AUPluginInfo::discover_by_description (PluginInfoList& plugs, CAComponentDescription& desc)
{
	Component comp = 0;

	comp = FindNextComponent (NULL, &desc);

	while (comp != NULL) {
		CAComponentDescription temp;
		GetComponentInfo (comp, &temp, NULL, NULL, NULL);

		AUPluginInfoPtr info (new AUPluginInfo 
				      (boost::shared_ptr<CAComponentDescription> (new CAComponentDescription(temp))));

		/* no panners, format converters or i/o AU's for our purposes
		 */

		switch (info->descriptor->Type()) {
		case kAudioUnitType_Panner:
		case kAudioUnitType_OfflineEffect:
		case kAudioUnitType_FormatConverter:
			continue;
		default:
			break;
		}

		switch (info->descriptor->SubType()) {
		case kAudioUnitSubType_DefaultOutput:
		case kAudioUnitSubType_SystemOutput:
		case kAudioUnitSubType_GenericOutput:
		case kAudioUnitSubType_AUConverter:
			continue;
			break;

		case kAudioUnitSubType_DLSSynth:
			info->category = "DLSSynth";
			break;

		case kAudioUnitType_MusicEffect:
			info->category = "MusicEffect";
			break;

		case kAudioUnitSubType_Varispeed:
			info->category = "Varispeed";
			break;

		case kAudioUnitSubType_Delay:
			info->category = "Delay";
			break;

		case kAudioUnitSubType_LowPassFilter:
			info->category = "LowPassFilter";
			break;

		case kAudioUnitSubType_HighPassFilter:
			info->category = "HighPassFilter";
			break;

		case kAudioUnitSubType_BandPassFilter:
			info->category = "BandPassFilter";
			break;

		case kAudioUnitSubType_HighShelfFilter:
			info->category = "HighShelfFilter";
			break;

		case kAudioUnitSubType_LowShelfFilter:
			info->category = "LowShelfFilter";
			break;

		case kAudioUnitSubType_ParametricEQ:
			info->category = "ParametricEQ";
			break;

		case kAudioUnitSubType_GraphicEQ:
			info->category = "GraphicEQ";
			break;

		case kAudioUnitSubType_PeakLimiter:
			info->category = "PeakLimiter";
			break;

		case kAudioUnitSubType_DynamicsProcessor:
			info->category = "DynamicsProcessor";
			break;

		case kAudioUnitSubType_MultiBandCompressor:
			info->category = "MultiBandCompressor";
			break;

		case kAudioUnitSubType_MatrixReverb:
			info->category = "MatrixReverb";
			break;

		case kAudioUnitType_Mixer:
			info->category = "Mixer";
			break;

		case kAudioUnitSubType_StereoMixer:
			info->category = "StereoMixer";
			break;

		case kAudioUnitSubType_3DMixer:
			info->category = "3DMixer";
			break;

		case kAudioUnitSubType_MatrixMixer:
			info->category = "MatrixMixer";
			break;

		default:
			info->category = "";
		}

		AUPluginInfo::get_names (temp, info->name, info->creator);

		info->type = ARDOUR::AudioUnit;
		info->unique_id = stringify_descriptor (*info->descriptor);

		/* mark the plugin as having flexible i/o */
		
		info->n_inputs = -1;
		info->n_outputs = -1;


		plugs.push_back (info);
		
		comp = FindNextComponent (comp, &desc);
	}
}

void
AUPluginInfo::get_names (CAComponentDescription& comp_desc, std::string& name, Glib::ustring& maker)
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
		// strip_whitespace_edges (maker);
		// strip_whitespace_edges (name);
	} else {
		name = str;
		maker = "unknown";
	}
}

// from CAComponentDescription.cpp (in libs/appleutility in ardour source)
extern char *StringForOSType (OSType t, char *writeLocation);

std::string
AUPluginInfo::stringify_descriptor (const CAComponentDescription& desc)
{
	char str[24];
	stringstream s;

	s << StringForOSType (desc.Type(), str);
	s << " - ";
		
	s << StringForOSType (desc.SubType(), str);
	s << " - ";
		
	s << StringForOSType (desc.Manu(), str);

	return s.str();
}
