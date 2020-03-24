/*
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#include <string>
#include <ctype.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>

#include <pbd/error.h>
#include <gtkmm2ext/gtkapplication.h>
#include <gdk/gdkquartz.h>

#undef check
#undef YES
#undef NO
#ifdef verify
#undef verify
#endif

#include "ardour_ui.h"
#include "actions.h"
#include "opts.h"

#include <CoreFoundation/CFLocale.h>
#import <CoreFoundation/CFString.h>
#import <Foundation/NSString.h>
#import <Foundation/NSAutoreleasePool.h>

using namespace std;
using namespace PBD;

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

	/* Prevent "App Nap" */

void
no_app_nap ()
{

#ifndef NSActivityLatencyCritical
#define NSActivityLatencyCritical 0xFF00000000ULL
#endif

	if ( [ [ NSProcessInfo processInfo ] respondsToSelector:@selector(beginActivityWithOptions:reason:) ] ) {
		cout << "Disabling MacOS AppNap\n";
		[ [ NSProcessInfo processInfo] beginActivityWithOptions:NSActivityLatencyCritical reason:@"realtime audio" ];
	}
}

/** Query Darwin kernel version.
 * @return major kernel version or -1 on failure
 *
 * kernel version is 4 ahead of OS X release
 * 19.x.x - OS 10.15 (Catalina)
 * 18.x.x - OS 10.14 (Mojave)
 * 17.x.x - OS 10.13 (High Sierra)
 * 16.x.x - OS 10.12 (Sierra)
 * ...
 * 10.x.x - OS 10.6  (Snow Leopard)
 */
int
query_darwin_version ()
{
	char str[256] = {0};
	size_t size = sizeof(str);

	if (0 == sysctlbyname ("kern.osrelease", str, &size, NULL, 0)) {
		short int v[3];
		if (3 == sscanf (str, "%hd.%hd.%hd", &v[0], &v[1], &v[2])) {
			return v[0]; // major version only
		}
	} else {
		struct utsname name;
		uname (&name);
		int v[3];
		if (3 == sscanf (name.release, "%d.%d.%d", &v[0], &v[1], &v[2])) {
			return v[0]; // major version only
		}
		if (2 == sscanf (name.release, "%d.%d", &v[0], &v[1])) {
			return v[0]; // major version only
		}
	}
	return -1;
}
