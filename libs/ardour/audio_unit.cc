/*
    Copyright (C) 2006 Paul Davis 
	Written by Taybin Rutkin
	
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

#include <pbd/transmitter.h>
#include <pbd/xml++.h>

#include <ardour/audioengine.h>
#include <ardour/audio_unit.h>
#include <ardour/session.h>
#include <ardour/utils.h>

#include <appleutility/CAAudioUnit.h>

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

AUPlugin::AUPlugin (AudioEngine& engine, Session& session, CAComponent* _comp)
	:
	Plugin (engine, session),
	comp (_comp),
	unit (new CAAudioUnit)
{			
	OSErr err = CAAudioUnit::Open (*comp, *unit);
	if (err != noErr) {
		error << _("AudioUnit: Could not convert CAComponent to CAAudioUnit") << endmsg;
		delete unit;
		delete comp;
		throw failed_constructor ();
	}
	
	unit->Initialize ();
}

AUPlugin::~AUPlugin ()
{
	if (unit) {
		unit->Uninitialize ();
		delete unit;
	}
	
	if (comp) {
		delete comp;
	}
	
	if (in_list) {
		delete in_list;
	}
	
	if (out_list) {
		delete out_list;
	}
}

AUPluginInfo::~AUPluginInfo ()
{
	if (desc) {
		delete desc;
	}
}

uint32_t
AUPlugin::unique_id () const
{
	return 0;
}

const char *
AUPlugin::label () const
{
	return "AUPlugin label";
}

const char *
AUPlugin::maker () const
{
	return "AUplugin maker";
}

uint32_t
AUPlugin::parameter_count () const
{
	return 0;
}

float
AUPlugin::default_value (uint32_t port)
{
	// AudioUnits don't have default values.  Maybe presets though?
	return 0;
}

jack_nframes_t
AUPlugin::latency () const
{
	return unit->Latency ();
}

void
AUPlugin::set_parameter (uint32_t which, float val)
{
	unit->SetParameter (parameter_map[which].first, parameter_map[which].second, 0, val);
}

float
AUPlugin::get_parameter (uint32_t which) const
{
	float outValue = 0.0;
	
	unit->GetParameter(parameter_map[which].first, parameter_map[which].second, 0, outValue);
	
	return outValue;
}

int
AUPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const
{
	return -1;
}

uint32_t
AUPlugin::nth_parameter (uint32_t which, bool& ok) const
{
	return 0;
}

void
AUPlugin::activate ()
{
	unit->GlobalReset ();
}

void
AUPlugin::deactivate ()
{
	// not needed.  GlobalReset () takes care of it.
}

void
AUPlugin::set_block_size (jack_nframes_t nframes)
{
	
}

int
AUPlugin::connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in, int32_t& out, jack_nframes_t nframes, jack_nframes_t offset)
{
	AudioUnitRenderActionFlags flags = 0;
	AudioTimeStamp ts;
	
	AudioBufferList abl;
	abl.mNumberBuffers = 1;
	abl.mBuffers[0].mNumberChannels = 1;
	abl.mBuffers[0].mDataByteSize = nframes * sizeof(Sample);
	abl.mBuffers[0].mData = &bufs[0];
	
	
	unit->Render (&flags, &ts, 0, 0, &abl);
	
	return 0;
}

set<uint32_t>
AUPlugin::automatable() const
{
	set<uint32_t> automates;
	
	return automates;
}

void
AUPlugin::store_state (ARDOUR::PluginState&)
{
	
}

void
AUPlugin::restore_state (ARDOUR::PluginState&)
{
	
}

string
AUPlugin::describe_parameter (uint32_t)
{
	return "";
}

void
AUPlugin::print_parameter (uint32_t, char*, uint32_t len) const
{
	
}

bool
AUPlugin::parameter_is_audio (uint32_t) const
{
	return false;
}

bool
AUPlugin::parameter_is_control (uint32_t) const
{
	return false;
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
	XMLNode* root = new XMLNode (state_node_name());
	
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
	return false;
}

PluginPtr
AUPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		CAComponent* comp = new CAComponent(*desc);
		
		if (!comp->IsValid()) {
			error << ("AudioUnit: not a valid Component") << endmsg;
		} else {
			plugin.reset (new AUPlugin (session.engine(), session, comp));
		}
		
		plugin->set_info(PluginInfoPtr(new AUPluginInfo(*this)));
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

	CAComponentDescription desc;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentSubType = 0;
	desc.componentManufacturer = 0;
	desc.componentType = kAudioUnitType_Effect;

	Component comp = 0;

	comp = FindNextComponent (NULL, &desc);
	while (comp != NULL) {
		CAComponentDescription temp;
		GetComponentInfo (comp, &temp, NULL, NULL, NULL);
		
		AUPluginInfoPtr plug(new AUPluginInfo);
		plug->name = AUPluginInfo::get_name (temp);
		plug->type = PluginInfo::AudioUnit;
		plug->n_inputs = 0;
		plug->n_outputs = 0;
		plug->category = "AudioUnit";
		plug->desc = new CAComponentDescription(temp);

		plugs.push_back(plug);
		
		comp = FindNextComponent (comp, &desc);
	}

	return plugs;
}

string
AUPluginInfo::get_name (CAComponentDescription& comp_desc)
{
	CFStringRef itemName = NULL;
	// Marc Poirier -style item name
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
	
	return CFStringRefToStdString(itemName);
}
