/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012 Todd Naugle <toddn@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#include <boost/scoped_ptr.hpp>
#include <string>
#include <glibmm/spawn.h>

#include "pbd/epa.h"
#include "pbd/openuri.h"

#ifdef __APPLE__
#include <curl/curl.h>
	extern bool cocoa_open_url (const char*);
#endif

#ifdef PLATFORM_WINDOWS
# include <windows.h>
# include <shellapi.h>
#else
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

bool
PBD::open_uri (const char* uri)
{
#ifdef PLATFORM_WINDOWS
	gunichar2* wuri = g_utf8_to_utf16 (uri, -1, NULL, NULL, NULL);
	ShellExecuteW(NULL, L"open", (LPCWSTR)wuri, NULL, NULL, SW_SHOWNORMAL);
	g_free (wuri);
	return true;
#elif __APPLE__
	return cocoa_open_url (uri);
#else
	EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
	boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;

	/* revert all environment settings back to whatever they were when ardour started
	 */

	if (global_epa) {
		current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
		global_epa->restore ();
	}

	std::string s(uri);
	while (s.find("\\") != std::string::npos)
		s.replace(s.find("\\"), 1, "\\\\");
	while (s.find("\"") != std::string::npos)
		s.replace(s.find("\\"), 1, "\\\"");

	char const* arg = s.c_str();

	pid_t pid = ::vfork ();

	if (pid == 0) {
		::execlp ("xdg-open", "xdg-open", arg, (char*)NULL);
		_exit (EXIT_SUCCESS);
	} else if (pid > 0) {
		/* wait until started, keep std::string s in scope */
		::waitpid (pid, 0, 0);
	} else {
		return false;
	}

#endif /* not PLATFORM_WINDOWS and not __APPLE__ */
	return true;
}

bool
PBD::open_uri (const std::string& uri)
{
	return open_uri (uri.c_str());
}

bool
PBD::open_folder (const std::string& d)
{
#ifdef __APPLE__
	CURL *curl = curl_easy_init ();
	bool rv = false;
	if (curl) {
		char * e = curl_easy_escape (curl, d.c_str(), d.size());
		std::string url = "file:///" + std::string(e);
		rv = PBD::open_uri (url);
		curl_free (e);
	}
	return rv;
#else
	return PBD::open_uri (d);
#endif
}
