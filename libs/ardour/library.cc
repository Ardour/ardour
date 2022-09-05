#include <curl/curl.h>

#include <glibmm/miscutils.h>

#include "pbd/i18n.h"
#include "pbd/file_archive.h"
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

	curl_easy_setopt (curl, CURLOPT_URL, X_("https://ardour.org/libraries.xml"));
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc_StdString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
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
		string n, d, f, l;
		if (!node->get_property (X_("name"), n) ||
		    !node->get_property (X_("file"), f) ||
		    !node->get_property (X_("license"), l)) {
			continue;
		}
		d = node->content();

		_descriptions.push_back (LibraryDescription (n, d, f, l));
	}

	return 0;
}

int
LibraryFetcher::add (string const & url)
{
	try {
		FileArchive archive (url);
		const string destdir = Config->get_clip_library_dir ();


		if (archive.make_local (destdir)) {
			/* failed */
			return -1;
		}

		std::vector<std::string> contents = archive.contents ();

		std::set<std::string> dirs;

		for (auto const & c : contents) {
			std::string dir = Glib::path_get_dirname (c);
			std::string gp = Glib::path_get_dirname (dir);

			/* if the dirname of this dirname is the destdir, then
			   dirname is a top-level dir that will (should) be
			   created when/if we inflate the archive
			*/

			if (gp == destdir) {
				dirs.insert (dir);
			}
		}

		if (dirs.empty()) {
			return -1;
		}

		if (archive.inflate (destdir)) {
			/* cleanup ? */
			return -1;
		}

		std::string path;

		for (std::set<std::string>::const_iterator d = dirs.begin(); d != dirs.end(); ++d) {
			if (d != dirs.begin()) {
				path += G_SEARCHPATH_SEPARATOR;
			}
			path += Glib::build_filename (destdir, *d);
		}

		assert (!path.empty());

		path += G_SEARCHPATH_SEPARATOR;
		path += Config->get_sample_lib_path ();

		Config->set_sample_lib_path (path);

	} catch (...) {
		return -1;
	}

	return 0;
}
