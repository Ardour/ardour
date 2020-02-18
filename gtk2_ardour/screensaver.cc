/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "utils.h"

#ifdef PLATFORM_WINDOWS
/* http://www.blackwasp.co.uk/DisableScreensaver.aspx */
#include <windows.h>

void
ARDOUR_UI_UTILS::inhibit_screensaver (bool inhibit)
{
	if (inhibit) {
		SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
	} else {
		SetThreadExecutionState (ES_CONTINUOUS);
	}
}

#elif defined __APPLE__

#include <IOKit/pwr_mgt/IOPMLib.h>

static IOReturn success = kIOReturnError;
static IOPMAssertionID assertion_id;

void
ARDOUR_UI_UTILS::inhibit_screensaver (bool inhibit)
{
	if (inhibit == (success == kIOReturnSuccess)) {
		return;
	}

	if (inhibit) {
		/* kIOPMAssertionTypeNoDisplaySleep prevents display sleep,
		 * kIOPMAssertionTypeNoIdleSleep prevents idle sleep
		 */
#ifdef __ppc__ /* OS X 10.5 compat API */
		success = IOPMAssertionCreate (kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &assertion_id);
#else
		static const CFStringRef name = CFSTR("Ardour DAW");
		success = IOPMAssertionCreateWithName (kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, name, &assertion_id);
#endif
	} else {
		if (kIOReturnSuccess == IOPMAssertionRelease (assertion_id)) {
			success = kIOReturnError; // mark as inactive
		}
	}
}

#else /* Linux / X11 */

#include <gtkmm.h>
#include "ardour/system_exec.h"

static sigc::connection glib_timer;

static bool
xdg_screensaver_reset ()
{
	char** args = (char**) malloc (5 * sizeof(char*));
	args[0] = strdup ("/bin/sh");
	args[1] = strdup ("-c");
	args[2] = strdup ("xdg-screensaver");
	args[3] = strdup ("reset");
	args[4] = 0;

	ARDOUR::SystemExec xdg_exec ("/bin/sh", args);
	if (xdg_exec.start ()) {
		return false;
	}
	xdg_exec.wait ();
	return true; /* keep going */
}

void
ARDOUR_UI_UTILS::inhibit_screensaver (bool inhibit)
{
	glib_timer.disconnect ();
	if (inhibit) {
		xdg_screensaver_reset ();
		glib_timer = Glib::signal_timeout().connect_seconds (sigc::ptr_fun (&xdg_screensaver_reset), 45, Glib::PRIORITY_DEFAULT_IDLE);
	}
}

#endif
