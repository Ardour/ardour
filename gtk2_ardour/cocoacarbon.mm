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

void
ARDOUR_UI::platform_specific ()
{
	gtk_application_ready ();

	if (!ARDOUR_COMMAND_LINE::finder_invoked_ardour) {
		
		/* if invoked from the command line, make sure we're visible */
		
		[NSApp activateIgnoringOtherApps:1];
	} 
}

void
ARDOUR_UI::platform_setup ()
{
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
