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

#include <curl/curl.h>
#include <glib/gstdio.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/i18n.h"
#include "pbd/file_archive.h"
#include "pbd/replace_all.h"
#include "pbd/whitespace.h"
#include "pbd/xml++.h"

#include "ardour/rc_configuration.h"
#include "ardour/library.h"

using namespace PBD;
using namespace ARDOUR;
using std::string;

static size_t
CurlWrite_CallbackFunc_StdString(void *contents, size_t size, size_t nmemb, std::string *s)
{
    size_t newLength = size*nmemb;

    try  {
        s->append ((char*)contents, newLength);

    }  catch (std::bad_alloc &e) {
        //handle memory problem
        return 0;
    }

    return newLength;
}

int
LibraryFetcher::get_descriptions ()
{
	CURL* curl;

	curl = curl_easy_init ();
	if (!curl) {
		return -1;
	}
	std::string buf;

	curl_easy_setopt (curl, CURLOPT_URL, Config->get_resource_index_url().c_str());
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc_StdString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
        CURLcode res = curl_easy_perform (curl);
	curl_easy_cleanup (curl);

	if (res != CURLE_OK) {
		return -2;
	}

	XMLTree tree;
	if (!tree.read_buffer (buf.c_str())) {
		return -3;
	}

	XMLNode const & root (*tree.root());

	for (auto const & node : root.children()) {
		string n, d, u, l, td, a, sz;
		if (!node->get_property (X_("name"), n) ||
		    !node->get_property (X_("author"), a) ||
		    !node->get_property (X_("url"), u) ||
		    !node->get_property (X_("license"), l) ||
		    !node->get_property (X_("toplevel"), td) ||
		    !node->get_property (X_("size"), sz)) {
			continue;
		}

		for (auto const & cnode : node->children()) {
			if (cnode->is_content()) {
				d = cnode->content();
				break;
			}
		}

		string ds;
		remove_extra_whitespace (d, ds);
		strip_whitespace_edges (ds);
		replace_all (ds, "\n", "");

		_descriptions.push_back (LibraryDescription (n, a, ds, u, l, td, sz));
		_descriptions.back().set_installed (installed (_descriptions.back()));
	}

	return 0;
}

int
LibraryFetcher::add (std::string const & path)
{
	try {
		FileArchive archive (path);
		std::vector<std::string> contents = archive.contents ();
		std::set<std::string> dirs;

		for (auto const & c : contents) {
			string::size_type slash = c.find (G_DIR_SEPARATOR);
			if (slash == string::npos || slash == c.length() - 1) {
				/* no slash or slash at end ... directory ? */
				dirs.insert (c);
			}
		}

		if (dirs.empty()) {
			return -1;
		}

		/* Unpack the archive. Likely should have a thread for this */

		std::string destdir = Glib::path_get_dirname (path);

		{
			std::string pwd (Glib::get_current_dir ());

			if (g_chdir (destdir.c_str ())) {
				error << string_compose (_("cannot chdir to '%1' to unpack library archive (%2)\n"), destdir, strerror (errno)) << endmsg;
				return -1;
			}

			if (archive.inflate (destdir)) {
				/* cleanup ? */
				return -1;
			}

			g_chdir (pwd.c_str());
		}

		std::string newpath;

		for (std::set<std::string>::const_iterator d = dirs.begin(); d != dirs.end(); ++d) {

			std::string installed_path = Glib::build_filename (destdir, *d);

			if (Config->get_sample_lib_path().find (installed_path) == string::npos) {
				if (d != dirs.begin()) {
					newpath += G_SEARCHPATH_SEPARATOR;
				}
				newpath += installed_path;
			}
		}

		if (!newpath.empty()) {
			newpath += G_SEARCHPATH_SEPARATOR;
			newpath += Config->get_sample_lib_path ();

			Config->set_sample_lib_path (newpath);
		}

	} catch (...) {
		return -1;
	}

	return 0;
}

void
LibraryFetcher::foreach_description (boost::function<void (LibraryDescription)> f)
{
	for (auto ld : _descriptions) {
		f (ld);
	}
}

std::string
LibraryFetcher::install_path_for (LibraryDescription const & desc)
{
	return Glib::build_filename (Config->get_clip_library_dir(), desc.toplevel_dir());
}

bool
LibraryFetcher::installed (LibraryDescription const & desc)
{
	std::string path = install_path_for (desc);
	if (Glib::file_test (path, Glib::FILE_TEST_EXISTS) && Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
		return true;
	}
	return false;
}

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
	thr = std::thread (&Downloader::download, this);
	return 0;
}

void
Downloader::cleanup ()
{
	thr.join ();
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
