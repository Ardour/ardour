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
#include "ardour_ui.h"

/* Called for clicks on the dock icon. Can be used to unminimize or
 * create a new window for example.
 */
static OSErr
handle_reopen_application (const AppleEvent *inAppleEvent, 
                           AppleEvent       *outAppleEvent, 
                           long              inHandlerRefcon)
{
        g_print ("AEReopenApplication\n");

        return noErr;
}

static OSErr
handle_quit_application (const AppleEvent *inAppleEvent, 
                         AppleEvent       *outAppleEvent, 
                         long              inHandlerRefcon)
{
        g_print ("AEQuitApplication\n");
	
	ARDOUR_UI::instance()->quit ();

        return noErr;
}

void
ARDOUR_UI::platform_specific () (void)
{
        AEInstallEventHandler (kCoreEventClass, kAEReopenApplication, 
                               handle_reopen_application, 0, true);

        AEInstallEventHandler (kCoreEventClass, kAEQuitApplication, 
                               handle_quit_application, 0, true);
}
