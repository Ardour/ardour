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

#include <ardour/audio_unit.h>
#include <ardour/utils.h>

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>

using namespace ARDOUR;

PluginInfoList
AUPluginInfo::discover ()
{
	PluginInfoList plugs;

	int numTypes = 2;    // this magic number was retrieved from the apple AUHost example.

	ComponentDescription desc;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentSubType = 0;
	desc.componentManufacturer = 0;

	for (int i = 0; i < numTypes; ++i) {
		if (i == 1) {
			desc.componentType = kAudioUnitType_MusicEffect;
		} else {
			desc.componentType = kAudioUnitType_Effect;
		}

		Component comp = 0;

		comp = FindNextComponent (NULL, &desc);
		while (comp != NULL) {
			ComponentDescription temp;
			GetComponentInfo (comp, &temp, NULL, NULL, NULL);
			
			AUPluginInfoPtr plug(new AUPluginInfo);
			plug->name = AUPluginInfo::get_name (temp);
			plug->type = PluginInfo::AudioUnit;
			plug->n_inputs = 0;
			plug->n_outputs = 0;
			plug->category = "AudioUnit";
			plug->desc = CompDescPtr(new ComponentDescription(temp));

			plugs.push_back(plug);
			
			comp = FindNextComponent (comp, &desc);
		}
	}

	return plugs;
}

std::string
AUPluginInfo::get_name (ComponentDescription& comp_desc)
{
		CFStringRef itemName = NULL;
		// Marc Poirier -style item name
		Component auComponent = FindNextComponent (0, &comp_desc);
		if (auComponent != NULL) {
			ComponentDescription dummydesc;
			Handle nameHandle = NewHandle(sizeof(void*));
			if (nameHandle != NULL) {
				OSErr err = GetComponentInfo(auComponent, &dummydesc, nameHandle, NULL, NULL);
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
