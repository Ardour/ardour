/*
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <glibmm.h>
#include "pbd/windows_special_dirs.h"
#else
#include <sys/utsname.h>
#endif

#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/pthread_utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/rc_configuration.h"

#include "ardour_http.h"
#include "pingback.h"
#include "ui_config.h"
#include "utils.h"

using std::string;
using namespace ARDOUR;

struct ping_call {
    std::string version;
    std::string announce_path;

    ping_call (const std::string& v, const std::string& a)
	    : version (v), announce_path (a) {}
};

#ifndef MIXBUS /* Ardour */

/* As of October 2022, Ardour only sends system (OS) name */

#ifdef PLATFORM_WINDOWS

static std::string
build_query_string (ArdourCurl::HttpGet const & h)
{
# if ( defined(__x86_64__) || defined(_M_X64) )
	return  "s=Windows64";
# else
	return "s=Windows32";
# endif
}

#else /* POSIX: use uname */

static std::string
build_query_string (ArdourCurl::HttpGet const & h)
{
	string qs;
	struct utsname utb;

	if (uname (&utb)) {
		return qs;
	}

	char* query;

	query = h.escape (utb.sysname, strlen (utb.sysname));
	qs = string_compose ("s=%1", query);
	h.free (query);

	return qs;
}

#endif // ! Windows
#endif // ! MIXBUS

static void*
_pingback (void *arg)
{
	ArdourCurl::HttpGet h;

	//initialize curl

#ifdef MIXBUS
	curl_easy_setopt (h.curl (), CURLOPT_FOLLOWLOCATION, 1);
	/* do not check cert */
	curl_easy_setopt (h.curl (), CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt (h.curl (), CURLOPT_SSL_VERIFYHOST, 0);
#endif

	ping_call* cm = static_cast<ping_call*> (arg);
	string return_str;
	string qs;
	string url;

	url = Config->get_pingback_url ();

	if (url.compare (0, 4, "http") != 0) {
		delete cm;
		return 0;
	}

	char* v = h.escape (cm->version.c_str(), cm->version.length());
	url += v;
	h.free (v);

	qs = build_query_string (h);

	if (qs.empty()) {
		delete cm;
		return 0;
	}

	url += '?';
	url += qs;

	return_str = h.get (url, false);

	if (!return_str.empty ()) {
		if ( return_str.length() > 140 ) { // like a tweet :)
			std::cerr << "Announcement string is too long (probably behind a proxy)." << std::endl;
		} else {
			std::cout << "Announcement is: " << return_str << std::endl;

			//write announcements to local file, even if the
			//announcement is empty

			FILE* fout = g_fopen (cm->announce_path.c_str(), "wb");

			if (fout) {
				fwrite (return_str.c_str(), sizeof(char), return_str.length (), fout);
				fclose (fout);
			}
		}
	} else {
#ifndef NDEBUG
		std::cerr << "pingback: " << h.error () << std::endl;
#endif
	}

	delete cm;
	return 0;
}

namespace ARDOUR {

void pingback (const string& version, const string& announce_path)
{
	if (ARDOUR_UI_UTILS::running_from_source_tree ()) {
		/* we don't ping under these conditions, because the user is
		   probably just paul or robin :)
		*/
		return;
	}

	ping_call* cm = new ping_call (version, announce_path);
	pthread_t thread;

	pthread_create_and_store ("Pingback", &thread, _pingback, cm);
}

}
