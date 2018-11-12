/*
    Copyright (C) 2012 Paul Davis
    Inspired by code from Ben Loftis @ Harrison Consoles

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
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <glibmm.h>
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
#include "utils.h"

using std::string;
using namespace ARDOUR;

struct ping_call {
    std::string version;
    std::string announce_path;

    ping_call (const std::string& v, const std::string& a)
	    : version (v), announce_path (a) {}
};

#ifdef PLATFORM_WINDOWS
static bool
_query_registry (const char *regkey, const char *regval, std::string &rv) {
	HKEY key;
	DWORD size = PATH_MAX;
	char tmp[PATH_MAX+1];

	if (   (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, regkey, 0, KEY_READ, &key))
	    && (ERROR_SUCCESS == RegQueryValueExA (key, regval, 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size))
		 )
	{
		rv = Glib::locale_to_utf8 (tmp);
		return true;
	}

	if (   (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, regkey, 0, KEY_READ | KEY_WOW64_32KEY, &key))
	    && (ERROR_SUCCESS == RegQueryValueExA (key, regval, 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size))
		 )
	{
		rv = Glib::locale_to_utf8 (tmp);
		return true;
	}

	return false;
}
#endif


static void*
_pingback (void *arg)
{
	ArdourCurl::HttpGet h;

	ping_call* cm = static_cast<ping_call*> (arg);
	string return_str;
	//initialize curl

	string url;

#ifdef __APPLE__
	url = Config->get_osx_pingback_url ();
#elif defined PLATFORM_WINDOWS
	url = Config->get_windows_pingback_url ();
#else
	url = Config->get_linux_pingback_url ();
#endif

	if (url.compare (0, 4, "http") != 0) {
		delete cm;
		return 0;
	}

	char* v = h.escape (cm->version.c_str(), cm->version.length());
	url += v;
	url += '?';
	h.free (v);

#ifndef PLATFORM_WINDOWS
	struct utsname utb;

	if (uname (&utb)) {
		delete cm;
		return 0;
	}

	//string uts = string_compose ("%1 %2 %3 %4", utb.sysname, utb.release, utb.version, utb.machine);
	string s;
	char* query;

	query = h.escape (utb.sysname, strlen (utb.sysname));
	s = string_compose ("s=%1", query);
	url += s;
	url += '&';
	h.free (query);

	query = h.escape (utb.release, strlen (utb.release));
	s = string_compose ("r=%1", query);
	url += s;
	url += '&';
	h.free (query);

	query = h.escape (utb.machine, strlen (utb.machine));
	s = string_compose ("m=%1", query);
	url += s;
	h.free (query);
#else
	std::string val;
	if (_query_registry("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", val)) {
		char* query = h.escape (val.c_str(), strlen (val.c_str()));
		url += "r=";
		url += query;
		url += '&';
		h.free (query);
	} else {
		url += "r=&";
	}

	if (_query_registry("Hardware\\Description\\System\\CentralProcessor\\0", "Identifier", val)) {
		// remove "Family X Model YY Stepping Z" tail
		size_t cut = val.find (" Family ");
		if (string::npos != cut) {
			val = val.substr (0, cut);
		}
		char* query = h.escape (val.c_str(), strlen (val.c_str()));
		url += "m=";
		url += query;
		url += '&';
		h.free (query);
	} else {
		url += "m=&";
	}

# if ( defined(__x86_64__) || defined(_M_X64) )
	url += "s=Windows64";
# else
	url += "s=Windows32";
# endif

#endif /* PLATFORM_WINDOWS */

	return_str = h.get (url, false);

	if (!return_str.empty ()) {
		if ( return_str.length() > 140 ) { // like a tweet :)
			std::cerr << "Announcement string is too long (probably behind a proxy)." << std::endl;
		} else {
			std::cerr << "Announcement is: " << return_str << std::endl;

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

	pthread_create_and_store ("pingback", &thread, _pingback, cm);
}

}
