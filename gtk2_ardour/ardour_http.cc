/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Paul Davis <paul@linuxaudiosystems.com>
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

//#define ARDOURCURLDEBUG

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>

#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour_http.h"

#include "pbd/i18n.h"

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

#ifndef ARDOUR_CURL_TIMEOUT
#define ARDOUR_CURL_TIMEOUT (60)
#endif

#ifdef ARDOURCURLDEBUG
#define CCERR(msg) do { if (cc != CURLE_OK) { std::cerr << string_compose ("curl_easy_setopt(%1) failed: %2", msg, cc) << std::endl; } } while (0)
#else
#define CCERR(msg)
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

static size_t headerCallback (char* ptr, size_t size, size_t nmemb, void* data)
{
	size_t realsize = size * nmemb;
#ifdef ARDOURCURLDEBUG
		std::cerr << string_compose ("ArdourCurl HTTP-header recv %1 bytes", realsize) << std::endl;
#endif
	struct HttpGet::HeaderInfo *nfo = (struct HttpGet::HeaderInfo*)data;
	std::string header (static_cast<const char*>(ptr), realsize);
	std::string::size_type index = header.find (':', 0);
	if (index != std::string::npos) {
		std::string k = header.substr (0, index);
		std::string v = header.substr (index + 2);
		k.erase(k.find_last_not_of (" \n\r\t")+1);
		v.erase(v.find_last_not_of (" \n\r\t")+1);
		nfo->h[k] = v;
#ifdef ARDOURCURLDEBUG
		std::cerr << string_compose ("ArdourCurl HTTP-header  '%1' = '%2'", k, v) << std::endl;
#endif
	}

	return realsize;
}

HttpGet::HttpGet (bool p, bool ssl)
	: persist (p)
	, _status (-1)
	, _result (-1)
{
	error_buffer[0] = 0;
	_curl = curl_easy_init ();

	if (!_curl) {
		std::cerr << "HttpGet::HttpGet curl_easy_init() failed." << std::endl;
		return;
	}

	CURLcode cc;

	cc = curl_easy_setopt (_curl, CURLOPT_WRITEDATA, (void *)&mem); CCERR ("CURLOPT_WRITEDATA");
	cc = curl_easy_setopt (_curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); CCERR ("CURLOPT_WRITEFUNCTION");
	cc = curl_easy_setopt (_curl, CURLOPT_HEADERDATA, (void *)&nfo); CCERR ("CURLOPT_HEADERDATA");
	cc = curl_easy_setopt (_curl, CURLOPT_HEADERFUNCTION, headerCallback); CCERR ("CURLOPT_HEADERFUNCTION");
	cc = curl_easy_setopt (_curl, CURLOPT_USERAGENT, PROGRAM_NAME VERSIONSTRING); CCERR ("CURLOPT_USERAGENT");
	cc = curl_easy_setopt (_curl, CURLOPT_TIMEOUT, ARDOUR_CURL_TIMEOUT); CCERR ("CURLOPT_TIMEOUT");
	cc = curl_easy_setopt (_curl, CURLOPT_NOSIGNAL, 1); CCERR ("CURLOPT_NOSIGNAL");
	cc = curl_easy_setopt (_curl, CURLOPT_ERRORBUFFER, error_buffer); CCERR ("CURLOPT_ERRORBUFFER");
	// cc= curl_easy_setopt (_curl, CURLOPT_FOLLOWLOCATION, 1); CCERR ("CURLOPT_FOLLOWLOCATION");

	// by default use curl's default.
	if (ssl && ca_info) {
		curl_easy_setopt (_curl, CURLOPT_CAINFO, ca_info);
	}
	if (ssl && ca_path) {
		curl_easy_setopt (_curl, CURLOPT_CAPATH, ca_path);
	}

	if (ssl && (ca_info || ca_path)) {
		curl_easy_setopt (_curl, CURLOPT_SSL_VERIFYPEER, 1);
	}
}

HttpGet::~HttpGet ()
{
	if (_curl) {
		curl_easy_cleanup (_curl);
	}
	if (!persist) {
		::free (mem.data);
	}
}

char*
HttpGet::get (const char* url, bool with_error_logging)
{
#ifdef ARDOURCURLDEBUG
	std::cerr << "HttpGet::get() ---- new request ---"<< std::endl;
#endif
	_status = _result = -1;
	if (!_curl || !url) {
		if (with_error_logging) {
			PBD::error << "HttpGet::get() not initialized (or NULL url)"<< endmsg;
		}
#ifdef ARDOURCURLDEBUG
		std::cerr << "HttpGet::get() not initialized (or NULL url)"<< std::endl;
#endif
		return NULL;
	}

	if (strncmp ("http://", url, 7) && strncmp ("https://", url, 8)) {
		if (with_error_logging) {
			PBD::error << "HttpGet::get() not a http[s] URL"<< endmsg;
		}
#ifdef ARDOURCURLDEBUG
		std::cerr << "HttpGet::get() not a http[s] URL"<< std::endl;
#endif
		return NULL;
	}

	if (!persist) {
		::free (mem.data);
	} // otherwise caller is expected to have free()d or re-used it.

	error_buffer[0] = 0;
	mem.data = NULL;
	mem.size = 0;

	CURLcode cc;

	cc = curl_easy_setopt (_curl, CURLOPT_URL, url);
	CCERR ("CURLOPT_URL");
	_result = curl_easy_perform (_curl);
	cc = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &_status);
	CCERR ("CURLINFO_RESPONSE_CODE,");

	if (_result) {
		if (with_error_logging) {
			PBD::error << string_compose (_("HTTP request failed: (%1) %2"), _result, error_buffer) << endmsg;
		}
#ifdef ARDOURCURLDEBUG
		std::cerr << string_compose (_("HTTP request failed: (%1) %2"), _result, error_buffer) << std::endl;
#endif
		return NULL;
	}
	if (_status != 200) {
		if (with_error_logging) {
		PBD::error << string_compose (_("HTTP request status: %1"), _status) << endmsg;
	}
#ifdef ARDOURCURLDEBUG
		std::cerr << string_compose (_("HTTP request status: %1"), _status) << std::endl;
#endif
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
ArdourCurl::http_get (const char* url, int* status, bool with_error_logging) {
	HttpGet h (true);
	char* rv = h.get (url, with_error_logging);
	if (status) {
		*status = h.status ();
	}
	return rv;
}

std::string
ArdourCurl::http_get (const std::string& url, bool with_error_logging) {
	return HttpGet (false).get (url, with_error_logging);
}
