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

	ca_setopt (_curl);

	return _curl;
}

void
CCurl::ca_setopt (CURL* c)
{
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
