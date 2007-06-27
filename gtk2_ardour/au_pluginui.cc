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
#include <ardour/processor.h>

#include <gtkmm2ext/doi.h>

#include "au_pluginui.h"
#include "gui_thread.h"

#include <appleutility/CAAudioUnit.h>
#include <appleutility/CAComponent.h>

#include <AudioUnit/AudioUnit.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

AUPluginUI::AUPluginUI (boost::shared_ptr<PluginInsert> insert)
{
	if ((au = boost::dynamic_pointer_cast<AUPlugin> (insert->plugin())) == 0) {
		error << _("unknown type of editor-supplying plugin (note: no AudioUnit support in this version of ardour)") << endmsg;
		throw failed_constructor ();
	}

	OSStatus err = noErr;
	
	CAComponentDescription desc;
	Component carbonViewComponent = NULL;
	AudioUnitCarbonView carbonView = NULL;
	
	GetComponentInfo(au->get_comp()->Comp(), &desc, 0, 0, 0);
	carbonViewComponent = get_carbon_view_component(desc.componentSubType);
	err = OpenAComponent(carbonViewComponent, &carbonView);
	
	Rect rec;
	rec.top = 0;
	rec.left = 0;
	rec.bottom = 400;
	rec.right = 500;
	
	ProcessSerialNumber ourPSN;
	
	/* Here we will set the MacOSX native section of the process to the foreground for putting up this
     * dialog box.  First step is to get our process serial number.  We do this by calling 
     * GetCurrentProcess. 
     * First Argument: On success this PSN will be our PSN on return.
     * Return Value: A Macintosh error indicating success or failure.
     */
    err = GetCurrentProcess(&ourPSN);
    
    //If no error then set this process to be frontmost.
    if (err == noErr) {
        /* Calling SetFrontProcess to make us frontmost.
         * First Argument: The Process Serial Number of the process we want to make frontmost.  Here
         *    of course we pass our process serial number
         * Return Value: An error value indicating success or failure.  We just ignore the return
         *    value here.
         */
        (void)SetFrontProcess(&ourPSN);
    } else {
		error << "couldn't get current process" << endmsg;
	}
	
	err = CreateNewWindow (kDocumentWindowClass, kWindowStandardFloatingAttributes, &rec, &wr);
	
	ComponentResult auResult;
	ControlRef rootControl = NULL;
	GetRootControl(wr, &rootControl);
	
	int width = 500;
	int height = 400;
	Float32Point location = {30, 30};
	Float32Point size = {width, height};
	ControlRef audioUnitControl = NULL;
	
	auResult = AudioUnitCarbonViewCreate(carbonView,
	                                     au->get_au()->AU(),
	                                     wr,
	                                     rootControl,
	                                     &location,
	                                     &size,
	                                     &audioUnitControl);    
	
	ShowWindow (wr);
	BringToFront (wr);
//	AudioUnitCarbonViewSetEventListener(carbonView, EventListener, this);
#if 0
	set_name ("PluginEditor");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window*> (this)));
#endif	

	insert->GoingAway.connect (mem_fun(*this, &AUPluginUI::plugin_going_away));
	
	info << "AUPluginUI created" << endmsg;
}

AUPluginUI::~AUPluginUI ()
{
	// nothing to do here - plugin destructor destroys the GUI
}

void
AUPluginUI::plugin_going_away (ARDOUR::IOProcessor* ignored)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &AUPluginUI::plugin_going_away), ignored));

        delete_when_idle (this);
}

Component
AUPluginUI::get_carbon_view_component(OSType subtype)
{
	ComponentDescription desc;
	Component component;
	
	desc.componentType = kAudioUnitCarbonViewComponentType; // 'auvw'
	desc.componentSubType = subtype;
	desc.componentManufacturer = 0;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	// First see if we can find a carbon view designed specifically for this
	// plug-in:
	
	component = FindNextComponent(NULL, &desc);
	if (component)
	   return component;
	
	// If not, grab the generic carbon view, which will create a GUI for
	// any Audio Unit.
	
	desc.componentSubType = kAUCarbonViewSubType_Generic;
	component = FindNextComponent(NULL, &desc);
	
	return component;
}

