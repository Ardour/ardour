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
	*/

	/* how to get language preferences with CoreFoundation
	 */

	NSArray* languages = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];

	/* push into LANGUAGE */

	int count;

	if (languages && ((count = [languages count]) > 0)) {

		bool have_translatable_languages = true;

		const char *cstr = [[languages objectAtIndex:0] UTF8String];
		const size_t len = strlen (cstr);

		if (len > 1 && cstr[0] == 'e' && cstr[1] == 'n') {
			if (len == 2) {
				/* primary language is english (no region). Do not set
				   LANGUAGE, gettext should not translate
				*/
				have_translatable_languages = false;
				cout << "User has en as primary language choice. " << PROGRAM_NAME << " will not be translated\n";
			} else if (len == 5 && cstr[len-2] == 'U' && cstr[len-1] == 'S') {
				/* primary language choice is english (US). Stop looking, and do not set
				   LANGUAGE. gettext needs to just skip translation entirely.
				*/
				have_translatable_languages = false;
				cout << "User has en_US as primary language choice. " << PROGRAM_NAME << " will not be translated\n";
			}

			/* else en-<FOO> ... still leave the door open for translation
			   to other version of english (e.g. en_IN, en_GB, etc)
			*/
		}

		if (have_translatable_languages) {

			NSRange r = { 0, static_cast<NSUInteger> (count) };

			std::string stupid_apple_string = [[[languages subarrayWithRange:r] componentsJoinedByString:@":"] UTF8String];

			/* Apple's language preference tokens use "-" to separate the two letter ISO language code from the two-letter
			   ISO region code. So for a German speaker in Germany whose macOS system settings reflect these realities the user
			   language preference will be "de-DE".

			   Why Apple did this when the standard everywhere else is to use an underscore is unclear. However, we do know that
			   neither gettext not setlocale(2) will work with these hyphen separated tokens, so fix them.
			*/

			for (std::string::iterator s = stupid_apple_string.begin(); s != stupid_apple_string.end(); ++s) {
				if (*s == '-') {
					*s = '_';
				}
			}

			setenv ("LANGUAGE", stupid_apple_string.c_str(), 0);
			cout << "LANGUAGE set to " << getenv ("LANGUAGE") << endl;
		}
	}

	/* now get AppleLocale value and use that to set LC_ALL because Launchd-started processes (i.e. GUI apps)
	   do not have this environment variable set, and without it, setlocale (LC_ALL, "") will fail.

	   Note that it doesn't matter much what we set LC_ALL to for gettext's purposes, but other aspects of the
	   locale system do need a value that mostly/sort-of/kind-of represents the user's own preferences. So, get
	   that from CoreFoundation APIs.
	 */

	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	NSString* nslocale = (NSString*) CFLocaleGetValue (cflocale, kCFLocaleIdentifier);

	cout << "Apple's CoreFoundation API says that this user's locale is " << [nslocale UTF8String] << endl;
	setenv ("LC_ALL", [nslocale UTF8String], 0);
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
