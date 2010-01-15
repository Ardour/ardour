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

#include <gtkmm2ext/gtkapplication.h>
#include <gdk/gdkquartz.h>
#undef check
#undef YES
#undef NO

#include "ardour_ui.h"
#include "actions.h"
#include "opts.h"

sigc::signal<void,bool> ApplicationActivationChanged;

@interface AppNotificationObject : NSObject {}
- (AppNotificationObject*) init; 
@end

@implementation AppNotificationObject
- (AppNotificationObject*) init
{
	self = [ super init ];

	if (self) {
		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(appDidBecomeActive:)
		 name:NSApplicationDidBecomeActiveNotification
		 object:[NSApplication sharedApplication]];

		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(appDidBecomeInactive:)
		 name:NSApplicationWillResignActiveNotification 
		 object:[NSApplication sharedApplication]];
	}

	return self;
}

- (void)appDidBecomeActive:(NSNotification *)notification
{
	ApplicationActivationChanged (true);
}

- (void)appDidBecomeInactive:(NSNotification *)notification
{
	ApplicationActivationChanged (false);
}

@end

@interface ArdourApplicationDelegate : NSObject {}
@end

@implementation ArdourApplicationDelegate
-(BOOL) application:(NSApplication*) theApplication openFile:(NSString*) file
{
	Glib::ustring utf8_path ([file UTF8String]);
	ARDOUR_UI::instance()->idle_load (utf8_path);
	return 1;
}
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *)sender
{
	Gtkmm2ext::UI::instance()->quit ();
	return NSTerminateCancel;
}
@end

void
ARDOUR_UI::platform_specific ()
{
	Gtk::Widget* widget;

	GtkApplicationMenuGroup* group = gtk_application_add_app_menu_group ();

	widget = ActionManager::get_widget ("/ui/Main/Help/About");
	if (widget) {
		gtk_application_add_app_menu_item (group, (GtkMenuItem*) widget->gobj(), 0);
	}

	widget = ActionManager::get_widget ("/ui/Main/WindowMenu/ToggleOptionsEditor");
	if (widget) {
		gtk_application_add_app_menu_item (group, (GtkMenuItem*) widget->gobj(), 0);
	}

	[ NSApp finishLaunching ];

	if (!ARDOUR_COMMAND_LINE::finder_invoked_ardour) {
		
		/* if invoked from the command line, make sure we're visible */
		
		[NSApp activateIgnoringOtherApps:1];
	} 
}

void
ARDOUR_UI::platform_setup ()
{
	/* this will stick around for ever ... is that OK ? */
	
	[ [AppNotificationObject alloc] init];
	[ NSApp setDelegate: [ArdourApplicationDelegate new]];

}

bool
cocoa_open_url (const char* uri)
{
	NSString* struri = [[NSString alloc] initWithUTF8String:uri];
	NSURL* nsurl = [[NSURL alloc] initWithString:struri];

	bool ret = [[NSWorkspace sharedWorkspace] openURL:nsurl];

	[struri release];
	[nsurl release];

	return ret;
}
