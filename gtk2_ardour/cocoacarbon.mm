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

#include <string>
#include <ctype.h>
#include <stdlib.h>
#include <pbd/error.h>
#include <gtkmm2ext/gtkapplication.h>
#include <gdk/gdkquartz.h>
#undef check
#undef YES
#undef NO

#include "ardour_ui.h"
#include "actions.h"
#include "command_line_options.h"

#include <CoreFoundation/CFLocale.h>
#import <CoreFoundation/CFString.h>
#import <Foundation/NSString.h>
#import <Foundation/NSAutoreleasePool.h>

using namespace std;
using namespace PBD;

void
ARDOUR_UI::platform_specific ()
{
	gtk_application_ready ();

	if (!get_cmdline_opts().finder_invoked_ardour) {
		
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

void
set_language_preference ()
{
	gtk_disable_setlocale ();

	if (g_getenv ("LANGUAGE") || g_getenv ("LC_ALL") || g_getenv ("LANG")) {
		return;
	}

	if (g_getenv ("ARDOUR_EN")) {
		return;
	}

	/* the gettext manual is potentially misleading about the utility of
	   LANGUAGE.  It notes that if LANGUAGE is set to include a dialect/region-free
	   language code, like "it", it will assume that you mean the main
	   dialect (e.g. "it_IT"). But in reality, it doesn't bother looking for
	   locale dirs with the full name, only the short code (it doesn't
	   know any better).
	   
	   Since Apple's preferred language list only consists of short language codes,
	   if we set LANGUAGE then gettext will not look for the relevant long-form
	   variants.
	*/

	/* how to get language preferences with CoreFoundation
	 */

	NSArray* languages = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];
	
	/* push into LANGUAGE */

	if (languages && [languages count] > 0) {

		int i, count = [languages count];
		for (i = 0; i < count; ++i) {
			if ([[languages objectAtIndex:i]
			     isEqualToString:@"en"]) {
				count = i+1;
				break;
			}
		}
		NSRange r = { 0, static_cast<NSUInteger> (count) };
		setenv ("LANGUAGE", [[[languages subarrayWithRange:r] componentsJoinedByString:@":"] UTF8String], 0);
		cout << "LANGUAGE set to " << getenv ("LANGUAGE") << endl;
	}

	/* now get AppleLocale value and use that for LANG */

	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	NSString* nslocale = (NSString*) CFLocaleGetValue (cflocale, kCFLocaleIdentifier);

	/* the full POSIX locale specification allows for lots of things. that could be an issue. Silly Apple.
	 */

	cout << "LANG set to " << [nslocale UTF8String] << endl;
	setenv ("LANG", [nslocale UTF8String], 0);
	CFRelease (cflocale);
}
