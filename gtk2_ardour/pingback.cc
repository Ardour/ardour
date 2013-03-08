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
#include <iostream>
#include <fstream>
#include <cstring>

#include <sys/utsname.h>
#include <curl/curl.h>

#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/pthread_utils.h"
#include "ardour/filesystem_paths.h"

#include "pingback.h"

using std::string;

static size_t
curl_write_data (char *bufptr, size_t size, size_t nitems, void *ptr)
{
        /* we know its a string */

        string* sptr = (string*) ptr;

        for (size_t i = 0; i < nitems; ++i) {
                for (size_t n = 0; n < size; ++n) {
                        if (*bufptr == '\n') {
                                break;
                        }

                        (*sptr) += *bufptr++;
                }
        }

        return size * nitems;
}

struct ping_call {
    std::string version;
    std::string announce_path;

    ping_call (const std::string& v, const std::string& a)
	    : version (v), announce_path (a) {}
};

static void*
_pingback (void *arg)
{
	ping_call* cm = static_cast<ping_call*> (arg);
	CURL* c;
	struct utsname utb;
	string return_str;

	if (uname (&utb)) {
		return 0;
	}

	//initialize curl

	curl_global_init (CURL_GLOBAL_NOTHING);
	c = curl_easy_init ();
	
	curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, curl_write_data); 
	curl_easy_setopt (c, CURLOPT_WRITEDATA, &return_str); 
	char errbuf[CURL_ERROR_SIZE];
	curl_easy_setopt (c, CURLOPT_ERRORBUFFER, errbuf); 
	/* we really would prefer to be able to authenticate the certificate
	   but this has issues that right now (march 2013), i don't understand.
	*/
	curl_easy_setopt (c, CURLOPT_SSL_VERIFYPEER, 0);

	//get announcements from our server
	std::cerr << "Checking for Announcements from ardour.org  ...\n";

	string url;

#ifdef __APPLE__
	url = "https://community.ardour.org/pingback/osx/";
#else
	url = "https://community.ardour.org/pingback/linux/";
#endif

	char* v = curl_easy_escape (c, cm->version.c_str(), cm->version.length());
	url += v;
	url += '?';
	free (v);

	string uts = string_compose ("%1 %2 %3 %4", utb.sysname, utb.release, utb.version, utb.machine);
	string s;
	char* query;

	query = curl_easy_escape (c, utb.sysname, strlen (utb.sysname));
	s = string_compose ("s=%1", query);
	url += s;
	url += '&';
	free (query);

	query = curl_easy_escape (c, utb.release, strlen (utb.release));
	s = string_compose ("r=%1", query);
	url += s;
	url += '&';
	free (query);

	query = curl_easy_escape (c, utb.machine, strlen (utb.machine));
	s = string_compose ("m=%1", query);
	url += s;
	free (query);

	curl_easy_setopt (c, CURLOPT_URL, url.c_str());

	return_str = "";

	if (curl_easy_perform (c) == 0) {
		int http_status; 

		curl_easy_getinfo (c, CURLINFO_RESPONSE_CODE, &http_status);

		if (http_status != 200) {
			std::cerr << "Bad HTTP status" << std::endl;
			return 0;
		}

		if ( return_str.length() > 140 ) { // like a tweet :)
			std::cerr << "Announcement string is too long (probably behind a proxy)." << std::endl;
		} else {
			std::cerr << "Announcement is: " << return_str << std::endl;
			
			//write announcements to local file, even if the
			//announcement is empty
				
			std::ofstream annc_file (cm->announce_path.c_str());
			
			if (annc_file) {
				annc_file << return_str;
			}
		}
	} else {
		std::cerr << "curl failed: " << errbuf << std::endl;
	}

	curl_easy_cleanup (c);
	delete cm;

	return 0;
}

namespace ARDOUR {

void pingback (const string& version, const string& announce_path) 
{
	ping_call* cm = new ping_call (version, announce_path);
	pthread_t thread;

	pthread_create_and_store ("pingback", &thread, _pingback, cm);
}

}
