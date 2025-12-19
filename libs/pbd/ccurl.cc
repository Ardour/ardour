/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#include <cassert>
#include <cstring>
#include <iostream>

#include <glibmm.h>

#include "pbd/ccurl.h"
#include "pbd/compose.h"

const char* PBD::CCurl::ca_path = NULL;
const char* PBD::CCurl::ca_info = NULL;

#ifndef ARDOUR_CURL_TIMEOUT
#define ARDOUR_CURL_TIMEOUT (60)
#endif

#ifdef ARDOURCURLDEBUG
#define CCERR(msg)                                                                           \
  do {                                                                                       \
    if (cc != CURLE_OK) {                                                                    \
      std::cerr << string_compose ("curl_easy_setopt(%1) failed: %2", msg, cc) << std::endl; \
    }                                                                                        \
  } while (0)
#else
#define CCERR(msg) (void)cc;
#endif

#ifdef ARDOURCURLTRACE
// inspired by https://curl.se/libcurl/c/debug.html
// Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
// SPDX-License-Identifier: curl
static void
dump (const char* text, FILE* stream, unsigned char* ptr, size_t size)
{
	unsigned int width = 0x40;

	fprintf (stream, "%s, %10.10lu bytes (0x%8.8lx)\n", text, (unsigned long)size, (unsigned long)size);

	for (size_t i = 0; i < size; i += width) {
		fprintf (stream, "%4.4lx: ", (unsigned long)i);

		for (size_t c = 0; (c < width) && (i + c < size); c++) {
			/* check for 0D0A; if found, skip past and start a new line of output */
			if ((i + c + 1 < size) && ptr[i + c] == 0x0D && ptr[i + c + 1] == 0x0A) {
				i += (c + 2 - width);
				break;
			}

			fprintf (stream, "%c", (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80) ? ptr[i + c] : '.');
			/* check again for 0D0A, to avoid an extra \n if it is at width */
			if ((i + c + 2 < size) && ptr[i + c + 1] == 0x0D && ptr[i + c + 2] == 0x0A) {
				i += (c + 3 - width);
				break;
			}
		}
		fputc ('\n', stream); /* newline */
	}
	fflush (stream);
}

static int
curl_trace (CURL* curl, curl_infotype type, char* data, size_t size, void*)
{
	const char* text;
	(void)curl;

	switch (type) {
		case CURLINFO_TEXT:
			fprintf (stderr, "== Info: %s", data);
			return 0;
		case CURLINFO_HEADER_OUT:
			text = "=> Send header";
			break;
		case CURLINFO_DATA_OUT:
			text = "=> Send data";
			break;
		case CURLINFO_SSL_DATA_OUT:
			text = "=> Send SSL data";
			break;
		case CURLINFO_HEADER_IN:
			text = "<= Recv header";
			break;
		case CURLINFO_DATA_IN:
			text = "<= Recv data";
			break;
		case CURLINFO_SSL_DATA_IN:
			text = "<= Recv SSL data";
			break;
		default: /* in case a new one is introduced to shock us */
			return 0;
	}

	dump (text, stderr, (unsigned char*)data, size);
	return 0;
}
#endif

using namespace PBD;

CCurl::CCurl ()
	: _curl (0)
{
}

CCurl::~CCurl ()
{
	if (_curl) {
		curl_easy_cleanup (_curl);
	}
}

void
CCurl::reset ()
{
	if (_curl) {
		curl_easy_cleanup (_curl);
	}
	_curl = NULL;
}

CURL*
CCurl::curl () const
{
	if (_curl) {
		return _curl;
	}

	_curl = curl_easy_init ();

	if (!_curl) {
		std::cerr << "CCurl curl_easy_init() failed." << std::endl;
		return NULL;
	}

	CURLcode cc;
	cc = curl_easy_setopt (_curl, CURLOPT_USERAGENT, PROGRAM_NAME VERSIONSTRING);
	CCERR ("CURLOPT_USERAGENT");
	cc = curl_easy_setopt (_curl, CURLOPT_TIMEOUT, ARDOUR_CURL_TIMEOUT);
	CCERR ("CURLOPT_TIMEOUT");
	cc = curl_easy_setopt (_curl, CURLOPT_NOSIGNAL, 1);
	CCERR ("CURLOPT_NOSIGNAL");

#ifdef ARDOURCURLTRACE
	cc = curl_easy_setopt (_curl, CURLOPT_DEBUGFUNCTION, curl_trace);
	CCERR ("CURLOPT_TRACE");
	cc = curl_easy_setopt (_curl, CURLOPT_VERBOSE, 1L);
	CCERR ("CURLOPT_VERBOSE");
#endif

	ca_setopt (_curl);

	return _curl;
}

void
CCurl::ca_setopt (CURL* c)
{
#if defined PLATFORM_WINDOWS || defined __APPLE__
	/* winSSL and DarwinSSL does not need this, use defaults w/VERIFYHOST */
	return;
#endif
	if (ca_info) {
		curl_easy_setopt (c, CURLOPT_CAINFO, ca_info);
	}
	if (ca_path) {
		curl_easy_setopt (c, CURLOPT_CAPATH, ca_path);
	}
	if (ca_info || ca_path) {
		curl_easy_setopt (c, CURLOPT_SSL_VERIFYPEER, 1);
	} else {
		curl_easy_setopt (c, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt (c, CURLOPT_SSL_VERIFYHOST, 0);
	}
}

void
CCurl::setup_certificate_paths ()
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
	 * Alternatively, ship the Mozilla CA list, perhaps using https://mkcert.org/ .
	 */
	assert (!ca_path && !ca_info); // call once

	if (Glib::file_test ("/etc/pki/tls/certs/ca-bundle.crt", Glib::FILE_TEST_IS_REGULAR)) {
		// Fedora / RHEL, Arch
		ca_info = "/etc/pki/tls/certs/ca-bundle.crt";
	} else if (Glib::file_test ("/etc/ssl/certs/ca-certificates.crt", Glib::FILE_TEST_IS_REGULAR)) {
		// Debian and derivatives
		ca_info = "/etc/ssl/certs/ca-certificates.crt";
	} else if (Glib::file_test ("/etc/pki/tls/cert.pem", Glib::FILE_TEST_IS_REGULAR)) {
		// GNU/TLS can keep extra stuff here
		ca_info = "/etc/pki/tls/cert.pem";
	}
	// else NULL: use default (currently) "/etc/ssl/certs/ca-certificates.crt" if it exists

	/* If we don't set anything, defaults are used. At the time of writing we compile bundled curl on debian
	 * and it'll default to ca_path = /etc/ssl/certs and ca_info = /etc/ssl/certs/ca-certificates.crt .
	 * That works on Debian and derivs + openSUSE. It has historically not
	 * worked on RHEL / Fedora, but worst case the directory exists and doesn't
	 * prevent ca_info from working. https://bugzilla.redhat.com/show_bug.cgi?id=1053882
	 */
}
