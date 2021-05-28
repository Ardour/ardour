/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_http_h__
#define __gtk_ardour_http_h__

#include <curl/curl.h>
#include <string>
#include <map>

namespace ArdourCurl {

class HttpGet {
	public:
	HttpGet (bool persist = false, bool ssl = true);
	~HttpGet ();

	struct MemStruct {
		MemStruct () : data (0), size (0) {}
		char*  data;
		size_t size;
	};

	struct HeaderInfo {
		std::map<std::string, std::string> h;
	};

	char* get (const char* url, bool with_error_logging = false);

	std::string get (const std::string& url, bool with_error_logging = false) {
		char *rv = get (url.c_str (), with_error_logging);
		return rv ? std::string (rv) : std::string ("");
	}

	char* data () const { return mem.data; }
	size_t data_size () const { return mem.size; }

	long int status () const { return _status; }

	std::map<std::string, std::string> header () const { return nfo.h; }

	char* escape (const char* s, int l) const {
		return curl_easy_escape (_curl, s, l);
	}

	char* unescape (const char* s, int l, int *o) const {
		return curl_easy_unescape (_curl, s, l, o);
	}

	/* this is only to be used for data returned by from
	 * \ref escape() and \ref unescape() */
	void free (void *p) const {
		curl_free (p);
	}

	std::string error () const;

	CURL* curl () const { return _curl; }

	// called from fixup_bundle_environment
	static void setup_certificate_paths ();

	private:
	CURL* _curl;
	bool  persist;

	long int _status;
	long int _result;

	char error_buffer[CURL_ERROR_SIZE];

	struct MemStruct mem;
	struct HeaderInfo nfo;

	static const char* ca_path;
	static const char* ca_info;
};

char* http_get (const char* url, int* status, bool with_error_logging);
std::string http_get (const std::string& url, bool with_error_logging);



} // namespace

#endif /* __gtk_ardour_http_h__ */
