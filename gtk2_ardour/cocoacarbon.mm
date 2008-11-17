/*
    Copyright (C) 2007 Paul Davis 

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

#include <Carbon/Carbon.h>
#undef check // stupid, stupid carbon
#undef YES   // stupid, stupid gtkmm and/or NSObjC
#undef NO    // ditto

#include "ardour_ui.h"
#include "actions.h"
#include "opts.h"
#include <gtkmm2ext/sync-menu.h>

#include <Appkit/Appkit.h>
#include <gdk/gdkquartz.h>

sigc::signal<void,bool> ApplicationActivationChanged;
static EventHandlerRef  application_event_handler_ref;

/* Called for clicks on the dock icon. Can be used to unminimize or
 * create a new window for example.
 */

static OSErr
handle_reopen_application (const AppleEvent *inAppleEvent, 
                           AppleEvent       *outAppleEvent, 
                           long              inHandlerRefcon)
{
        return noErr;
}


static OSErr
handle_print_documents (const AppleEvent *inAppleEvent, 
                           AppleEvent       *outAppleEvent, 
                           long              inHandlerRefcon)
{
        return noErr;
}


static OSErr
handle_open_documents (const AppleEvent *inAppleEvent, 
		       AppleEvent       *outAppleEvent, 
		       long              inHandlerRefcon)
{
	AEDescList docs;

        if (AEGetParamDesc(inAppleEvent, keyDirectObject, typeAEList, &docs) == noErr) {
		long n = 0;
		AECountItems(&docs, &n);
		UInt8 strBuffer[PATH_MAX+1];

		/* ardour only opens 1 session at a time */

		FSRef ref;

		if (AEGetNthPtr(&docs, 1, typeFSRef, 0, 0, &ref, sizeof(ref), 0) == noErr) {
			if (FSRefMakePath(&ref, strBuffer, sizeof(strBuffer)) == noErr) {
				Glib::ustring utf8_path ((const char*) strBuffer);
				ARDOUR_UI::instance()->idle_load (utf8_path);
			}
		}
	}

        return noErr;
}

static OSErr
handle_open_application (const AppleEvent *inAppleEvent, 
                         AppleEvent       *outAppleEvent, 
                         long              inHandlerRefcon)
{
        return noErr;
}

static OSStatus 
application_event_handler (EventHandlerCallRef nextHandlerRef, EventRef event, void *userData) 
{
	UInt32 eventKind = GetEventKind (event);
	
	switch (eventKind) {
	case kEventAppActivated:
		ApplicationActivationChanged (true); // EMIT SIGNAL
		return eventNotHandledErr;

	case kEventAppDeactivated:
		ApplicationActivationChanged (false); // EMIT SIGNAL
		return eventNotHandledErr;
		
	default:
		// pass-thru all kEventClassApplication events we're not interested in.
		break;
	}
	return eventNotHandledErr;
}

void
ARDOUR_UI::platform_specific ()
{
	Gtk::Widget* widget = ActionManager::get_widget ("/ui/Main/Session/Quit");
	if (widget) {
		ige_mac_menu_set_quit_menu_item ((GtkMenuItem*) widget->gobj());
	}

	IgeMacMenuGroup* group = ige_mac_menu_add_app_menu_group ();

	widget = ActionManager::get_widget ("/ui/Main/Session/About");
	if (widget) {
		ige_mac_menu_add_app_menu_item (group, (GtkMenuItem*) widget->gobj(), 0);
	}
	widget = ActionManager::get_widget ("/ui/Main/Session/ToggleOptionsEditor");

	if (widget) {
		ige_mac_menu_add_app_menu_item (group, (GtkMenuItem*) widget->gobj(), 0);
	}
}

void
ARDOUR_UI::platform_setup ()
{
        AEInstallEventHandler (kCoreEventClass, kAEOpenDocuments, 
                               handle_open_documents, 0, true);

        AEInstallEventHandler (kCoreEventClass, kAEOpenApplication, 
                               handle_open_application, 0, true);

        AEInstallEventHandler (kCoreEventClass, kAEReopenApplication, 
                               handle_reopen_application, 0, true);

        AEInstallEventHandler (kCoreEventClass, kAEPrintDocuments, 
                               handle_print_documents, 0, true);

	EventTypeSpec applicationEventTypes[] = {
		{kEventClassApplication, kEventAppActivated },
		{kEventClassApplication, kEventAppDeactivated }
	};	
	
	EventHandlerUPP ehUPP = NewEventHandlerUPP (application_event_handler);
	
	InstallApplicationEventHandler (ehUPP, sizeof(applicationEventTypes) / sizeof(EventTypeSpec), 
					applicationEventTypes, 0, &application_event_handler_ref);
	if (!ARDOUR_COMMAND_LINE::finder_invoked_ardour) {
		
		/* if invoked from the command line, make sure we're visible */
		
		[NSApp activateIgnoringOtherApps:1];
	} 
}

bool
cocoa_open_uri (const char* uri)
{
	NSURL* nsurl = [NSURL initWithString:uri];
	return [[NSWorkspace sharedWorkspace] openURL:nsurl];
}
