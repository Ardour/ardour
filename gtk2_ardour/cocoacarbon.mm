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

#include "ardour_ui.h"
#include "actions.h"
#include "opts.h"
#include "sync-menu.h"

#include <Appkit/Appkit.h>

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
	cerr << "reopen app\n";
        return noErr;
}

static OSErr
handle_quit_application (const AppleEvent *inAppleEvent, 
                         AppleEvent       *outAppleEvent, 
                         long              inHandlerRefcon)
{
	cerr << "quit app\n";
	ARDOUR_UI::instance()->quit ();

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
        AEInstallEventHandler (kCoreEventClass, kAEReopenApplication, 
                               handle_reopen_application, 0, true);

        AEInstallEventHandler (kCoreEventClass, kAEQuitApplication, 
                               handle_quit_application, 0, true);

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

	EventTypeSpec applicationEventTypes[] = {
		{kEventClassApplication, kEventAppActivated },
		{kEventClassApplication, kEventAppDeactivated }
	};	
	
	EventHandlerUPP ehUPP = NewEventHandlerUPP (application_event_handler);
	
	InstallApplicationEventHandler (ehUPP, sizeof(applicationEventTypes) / sizeof(EventTypeSpec), 
					applicationEventTypes, 0, &application_event_handler_ref);
}

void
ARDOUR_UI::platform_setup ()
{
	if (!ARDOUR_COMMAND_LINE::finder_invoked_ardour) {
		
		/* if invoked from the command line, make sure we're visible */
		
		[NSApp activateIgnoringOtherApps:YES];
	} 
}
