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
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <ardour/ardour.h>
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

AUPluginInfo::CachedInfoMap AUPluginInfo::cached_info;

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
	: Plugin (engine, session),
	  comp (_comp),
	  unit (new CAAudioUnit),
	  initialized (false),
	  buffers (0),
	  current_maxbuf (0),
	  current_offset (0),
	  current_buffers (0),
	frames_processed (0)
{			
	init ();
}

AUPlugin::AUPlugin (const AUPlugin& other)
	: Plugin (other)
	, comp (other.get_comp())
	, unit (new CAAudioUnit)
	, initialized (false)
	, buffers (0)
	, current_maxbuf (0)
	, current_offset (0)
	, current_buffers (0)
	, frames_processed (0)
	  
{
	init ();
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
AUPlugin::init ()
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

	/* these keep track of *configured* channel set up,
	   not potential set ups.
	*/

	input_channels = -1;
	output_channels = -1;

	if (_set_block_size (_session.get_block_size())) {
		error << _("AUPlugin: cannot set processing block size") << endmsg;
		throw failed_constructor();
	}

	discover_parameters ();

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

			d.integer_step = (info.unit & kAudioUnitParameterUnit_Indexed);
			d.toggled = (info.unit & kAudioUnitParameterUnit_Boolean) ||
				(d.integer_step && ((d.upper - d.lower) == 1.0));
			d.sr_dependent = (info.unit & kAudioUnitParameterUnit_SampleFrames);
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
AUPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

	return unit->Latency() * _session.frame_rate();
}

void
AUPlugin::set_parameter (uint32_t which, float val)
{
	if (which < descriptors.size()) {
		const AUParameterDescriptor& d (descriptors[which]);
		unit->SetParameter (d.id, d.scope, d.element, val);
	}
}

float
AUPlugin::get_parameter (uint32_t which) const
{
	float val = 0.0;
	if (which < descriptors.size()) {
		const AUParameterDescriptor& d (descriptors[which]);
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
AUPlugin::configure_io (ChanCount in, ChanCount out)
{
	AudioStreamBasicDescription streamFormat;

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

	streamFormat.mChannelsPerFrame = in.n_audio();

	if (set_input_format (streamFormat) != 0) {
		return -1;
	}

	streamFormat.mChannelsPerFrame = out.n_audio();

	if (set_output_format (streamFormat) != 0) {
		return -1;
	}

	return Plugin::configure_io (in, out);
}

bool
AUPlugin::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	int32_t ret = count_for_configuration (in, out);
	return ret >= 0;
}

int32_t
AUPlugin::count_for_configuration(ChanCount cin, ChanCount& out) const
{
	// XXX as of May 13th 2008, AU plugin support returns a count of either 1 or -1. We never
	// attempt to multiply-instantiate plugins to meet io configurations.

	int32_t plugcnt = -1;
	AUPluginInfoPtr pinfo = boost::dynamic_pointer_cast<AUPluginInfo>(get_info());
	int32_t in = cin.n_audio(); /* XXX handle MIDI one day ??? */

	vector<pair<int,int> >& io_configs = pinfo->cache.io_configs;

	for (vector<pair<int,int> >::iterator i = io_configs.begin(); i != io_configs.end(); ++i) {

		int32_t possible_in = i->first;
		int32_t possible_out = i->second;

		if (possible_out == 0) {
			warning << string_compose (_("AU %1 has zero outputs - configuration ignored"), name()) << endmsg;
			continue;
		}

		if (possible_in == 0) {

			/* instrument plugin, always legal but throws away inputs ...
			*/

			if (possible_out == -1) {
				/* out much match in (UNLIKELY!!) */
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out == -2) {
				/* any configuration possible, pick matching */
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out.set (DataType::AUDIO, -possible_out);
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out.set (DataType::AUDIO, possible_out);
				plugcnt = 1;
			}
		}
		
		if (possible_in == -1) {

			/* wildcard for input */

			if (possible_out == -1) {
				/* out much match in */
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out == -2) {
				/* any configuration possible, pick matching */
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out.set (DataType::AUDIO, -possible_out);
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out.set (DataType::AUDIO, possible_out);
				plugcnt = 1;
			}
		}	
			
		if (possible_in == -2) {

			if (possible_out == -1) {
				/* any configuration possible, pick matching */
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out == -2) {
				error << string_compose (_("AU plugin %1 has illegal IO configuration (-2,-2)"), name())
				      << endmsg;
				plugcnt = -1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out.set (DataType::AUDIO, -possible_out);
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out.set (DataType::AUDIO, possible_out);
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
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out == -2) {
				error << string_compose (_("AU plugin %1 has illegal IO configuration (-2,-2)"), name())
				      << endmsg;
				plugcnt = -1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out.set (DataType::AUDIO, -possible_out);
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out.set (DataType::AUDIO, possible_out);
				plugcnt = 1;
			}
		}

		if (possible_in == in) {

			/* exact number of inputs ... must match obviously */
			
			if (possible_out == -1) {
				/* out must match in */
				out.set (DataType::AUDIO, in);
				plugcnt = 1;
			} else if (possible_out == -2) {
				/* any output configuration, pick matching */
				out.set (DataType::AUDIO, in);
				plugcnt = -1;
			} else if (possible_out < -2) {
				/* explicit variable number of outputs, pick maximum */
				out.set (DataType::AUDIO, -possible_out);
				plugcnt = 1;
			} else {
				/* exact number of outputs */
				out.set (DataType::AUDIO, possible_out);
				plugcnt = 1;
			}
		}

	}

	/* no fit */
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
	IO::PortCountChanged (ChanCount (DataType::AUDIO, fmt.mChannelsPerFrame));

	return 0;
}

int
AUPlugin::set_stream_format (int scope, uint32_t cnt, AudioStreamBasicDescription& fmt)
{
	OSErr result;

	for (uint32_t i = 0; i < cnt; ++i) {
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


ChanCount
AUPlugin::input_streams() const
{
	ChanCount in;

	if (input_channels < 0) {
		warning << string_compose (_("AUPlugin: %1 input_streams() called without any format set!"), name()) << endmsg;
		in.set_audio (1);
	} else {
		in.set_audio (input_channels);
	}

	return in;
}


ChanCount
AUPlugin::output_streams() const
{
	ChanCount out;

	if (output_channels < 0) {
		warning << string_compose (_("AUPlugin: %1 output_streams() called without any format set!"), name()) << endmsg;
		out.set_audio (1);
	} else {
		out.set_audio (output_channels);
	}

	return out;
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
		return PluginPtr ();
	}
}

Glib::ustring
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

		/* XXX not sure of the best way to handle plugin versioning yet
		 */

		CAComponent cacomp (*info->descriptor);

		if (cacomp.GetResourceVersion (info->version) != noErr) {
			info->version = 0;
		}
		
		if (cached_io_configuration (info->unique_id, info->version, cacomp, info->cache, info->name)) {

			/* here we have to map apple's wildcard system to a simple pair
			   of values.
			*/

			info->n_inputs = ChanCount (DataType::AUDIO, info->cache.io_configs.front().first);
			info->n_outputs = ChanCount (DataType::AUDIO, info->cache.io_configs.front().second);

			if (info->cache.io_configs.size() > 1) {
				cerr << "ODD: variable IO config for " << info->unique_id << endl;
			}

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

	snprintf (buf, sizeof (buf), "%u", version);
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
	
	if (CAAudioUnit::Open (comp, unit) != noErr) {
		return false;
	}

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

void
AUPluginInfo::save_cached_info ()
{
	XMLNode* node;

	node = new XMLNode (X_("AudioUnitPluginCache"));
	
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

	Glib::ustring path = au_cache_path ();
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
	Glib::ustring path = au_cache_path ();
	XMLTree tree;
	
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return 0;
	}

	tree.read (path);
	const XMLNode* root (tree.root());

	if (root->name() != X_("AudioUnitPluginCache")) {
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

			std::string id = prop->value();
			
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
						out = atoi (iprop->value());
						
						AUPluginCachedInfo cinfo;
						cinfo.io_configs.push_back (pair<int,int> (in, out));
						add_cached_info (id, cinfo);
					}
				}
			}
		}
	}

	return 0;
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
