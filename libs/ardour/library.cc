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
#include "pbd/pthread_utils.h"
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

LibraryFetcher::LibraryFetcher ()
{
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

	if (root.name() != X_("Resources")) {
		return -4;
	}

	XMLNode* libraries = 0;

	for (auto const & node : root.children()) {
		if (node->name() == X_("Libraries")) {
			libraries = node;
			break;
		}
	}

	if (!libraries) {
		return -5;
	}

	for (auto const & node : libraries->children()) {
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
LibraryFetcher::add (std::string const & root_dir)
{
	std::string newpath;

	/* just add the root dir to the relevant search path. The user can
	 * expand the rest in the browser
	 */

	if (Config->get_sample_lib_path().find (root_dir) == string::npos) {
		newpath = root_dir;
		newpath += G_SEARCHPATH_SEPARATOR;
		newpath += Config->get_sample_lib_path ();
		Config->set_sample_lib_path (newpath);
		Config->save_state ();
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

