/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#include <glib/gstdio.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/downloader.h"
#include "pbd/error.h"
#include "pbd/i18n.h"
#include "pbd/pthread_utils.h"

using namespace PBD;
using std::string;

static size_t
CurlWrite_CallbackFunc_Downloader(void *contents, size_t size, size_t nmemb, Downloader* dl)
{
	return dl->write (contents, size, nmemb);
}

size_t
Downloader::write (void *ptr, size_t size, size_t nmemb)
{
	if (_cancel) {
		fclose (file);
		file = 0;
		::g_unlink (file_path.c_str());

		_downloaded = 0;
		_download_size = 0;

		return 0;
	}

	size_t nwritten = fwrite (ptr, size, nmemb, file);

	_downloaded += nwritten;

	return nwritten;
}

Downloader::Downloader (string const & u, string const & dir)
	: url (u)
	, destdir (dir)
	, file (0)
	, _cancel (false)
	, _download_size (0)
	, _downloaded (0)
{
}

Downloader::~Downloader ()
{
	cleanup();
}

int
Downloader::start ()
{
	file_path = Glib::build_filename (destdir, Glib::path_get_basename (url));

	if (!(file = fopen (file_path.c_str(), "w"))) {
		return -1;
	}

	_cancel = false;
	_status = 0; /* unknown at this point */
	return 0 != (thread = PBD::Thread::create (boost::bind (&Downloader::download, this)));
}

void
Downloader::cleanup ()
{
	thread->join ();
}

void
Downloader::cancel ()
{
	_cancel = true;
}

double
Downloader::progress () const
{
	if (_download_size == 0) {
		return 0.;
	}

	return (double) _downloaded / _download_size;
}

void
Downloader::download ()
{
	char curl_error[CURL_ERROR_SIZE];

	{
		/* First curl fetch to get the data size so that we can offer a
		 * progress meter
		 */

		curl = curl_easy_init ();
		if (!curl) {
			_status = -1;
			return;
		}

		/* get size */

		curl_easy_setopt (curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
		curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);

		CURLcode res = curl_easy_perform (curl);

		if (res == CURLE_OK) {
			double dsize;
			curl_easy_getinfo (curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dsize);
			_download_size = dsize;
		}

		curl_easy_cleanup (curl);

		if (res != CURLE_OK ) {
			error << string_compose (_("Download failed, error code %1 (%2)"), curl_easy_strerror (res), curl_error) << endmsg;
			_status = -2;
			return;
		}
	}

	curl = curl_easy_init ();
	if (!curl) {
		_status = -1;
		return;
	}

	curl_easy_setopt (curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc_Downloader);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
	CURLcode res = curl_easy_perform (curl);
	curl_easy_cleanup (curl);

	if (res == CURLE_OK) {
		_status = 1;
	} else {
		_status = -1;
	}

	if (file) {
		fclose (file);
		file = 0;
	}
}

std::string
Downloader::download_path() const
{
	/* Can only return the download path if we completed, and completed successfully */
	if (_status > 0) {
		return file_path;
	}
	return std::string();
}
