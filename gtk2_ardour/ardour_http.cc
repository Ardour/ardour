/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>

#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/i18n.h"
#include "pbd/error.h"

#include "ardour_http.h"

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

#ifndef ARDOUR_CURL_TIMEOUT
#define ARDOUR_CURL_TIMEOUT (60)
#endif

using namespace ArdourCurl;

const char* HttpGet::ca_path = NULL;
const char* HttpGet::ca_info = NULL;

void
HttpGet::setup_certificate_paths ()
{
	/* this is only needed for Linux Bundles.
	 * (on OSX, Windows, we use system-wide ssl (darwinssl, winssl)
	 * Gnu/Linux distro will link against system-wide libcurl.
	 *
	 * but for linux-bundles we get to enjoy:
	 * https://www.happyassassin.net/2015/01/12/a-note-about-ssltls-trusted-certificate-stores-and-platforms/
	 *
	 * (we do ship curl + nss + nss-pem)
	 *
	 * Short of this mess: we could simply bundle a .crt of
	 * COMODO (ardour) and ghandi (freesound) and be done with it.
	 */
	assert (!ca_path && !ca_info); // call once

	curl_global_init (CURL_GLOBAL_DEFAULT);

	if (Glib::file_test ("/etc/pki/tls/certs/ca-bundle.crt", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		// Fedora / RHEL, Arch
		ca_info = "/etc/pki/tls/certs/ca-bundle.crt";
	}
	else if (Glib::file_test ("/etc/ssl/certs/ca-certificates.crt", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		// Debian and derivatives
		ca_info = "/etc/ssl/certs/ca-certificates.crt";
	}
	else if (Glib::file_test ("/etc/pki/tls/cert.pem", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		// GNU/TLS can keep extra stuff here
		ca_info = "/etc/pki/tls/cert.pem";
	}
	// else NULL: use default (currently) "/etc/ssl/certs/ca-certificates.crt" if it exists

	if (Glib::file_test ("/etc/pki/tls/certs/ca-bundle.crt", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {
		// we're on RHEL // https://bugzilla.redhat.com/show_bug.cgi?id=1053882
		ca_path = "/nonexistent_path"; // don't try "/etc/ssl/certs" in case it's curl's default
	}
	else if (Glib::file_test ("/etc/ssl/certs", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {
		// Debian and derivs + OpenSuSe
		ca_path = "/etc/ssl/certs";
	} else {
		ca_path = "/nonexistent_path"; // don't try -- just in case:
	}

	/* If we don't set anything defaults are used. at the time of writing we compile bundled curl on debian
	 * and it'll default to  /etc/ssl/certs and /etc/ssl/certs/ca-certificates.crt
	 */
}

static size_t
WriteMemoryCallback (void *ptr, size_t size, size_t nmemb, void *data) {
	size_t realsize = size * nmemb;
	struct HttpGet::MemStruct *mem = (struct HttpGet::MemStruct*)data;

	mem->data = (char *)realloc (mem->data, mem->size + realsize + 1);
	if (mem->data) {
		memcpy (&(mem->data[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->data[mem->size] = 0;
	}
	return realsize;
}


HttpGet::HttpGet (bool p, bool ssl)
	: persist (p)
	, _status (-1)
	, _result (-1)
{
	error_buffer[0] = '\0';
	_curl = curl_easy_init ();

	curl_easy_setopt (_curl, CURLOPT_WRITEDATA, (void *)&mem);
	curl_easy_setopt (_curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt (_curl, CURLOPT_USERAGENT, PROGRAM_NAME VERSIONSTRING);
	curl_easy_setopt (_curl, CURLOPT_TIMEOUT, ARDOUR_CURL_TIMEOUT);
	curl_easy_setopt (_curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt (_curl, CURLOPT_ERRORBUFFER, error_buffer);

	// by default use curl's default.
	if (ssl && ca_info) {
		curl_easy_setopt (_curl, CURLOPT_CAINFO, ca_info);
	}
	if (ssl && ca_path) {
		curl_easy_setopt (_curl, CURLOPT_CAPATH, ca_path);
	}
}

HttpGet::~HttpGet ()
{
	curl_easy_cleanup (_curl);
	if (!persist) {
		free (mem.data);
	}
}

char*
HttpGet::get (const char* url)
{
	_status = _result = -1;
	if (!_curl || !url) {
		return NULL;
	}

	if (strncmp ("http://", url, 7) && strncmp ("https://", url, 8)) {
		return NULL;
	}

	if (!persist) {
		free (mem.data);
	} // otherwise caller is expected to have free()d or re-used it.

	mem.data = NULL;
	mem.size = 0;

	curl_easy_setopt (_curl, CURLOPT_URL, url);
	_result = curl_easy_perform (_curl);
	curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &_status);

	if (_result) {
		PBD::error << string_compose (_("HTTP request failed: (%1) %2"), _result, error_buffer);
		return NULL;
	}
	if (_status != 200) {
		PBD::error << string_compose (_("HTTP request status: %1"), _status);
		return NULL;
	}

	return mem.data;
}

std::string
HttpGet::error () const {
	if (_result != 0) {
		return string_compose (_("HTTP request failed: (%1) %2"), _result, error_buffer);
	}
	if (_status != 200) {
		return string_compose (_("HTTP request status: %1"), _status);
	}
	return "No Error";
}

char*
ArdourCurl::http_get (const char* url, int* status) {
	HttpGet h (true);
	char* rv = h.get (url);
	if (status) {
		*status = h.status ();
	}
	return rv;
}

std::string
ArdourCurl::http_get (const std::string& url) {
	return HttpGet (false).get (url);
}
