/*
    Copyright (C) 2010-2013 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#include <cstdio>
#include <string>
#include <cerrno>
#include <gtkmm.h>
#include <curl/curl.h>

#include "pbd/error.h"
#include "ardour/ardour.h"
#include "ardour/session_directory.h"
#include "video_image_frame.h"
#include "utils_videotl.h"

#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

bool
confirm_video_outfn (std::string outfn, std::string docroot)
{
	/* replace docroot's '/' to G_DIR_SEPARATOR for the comparison */
	size_t look_here = 0;
	size_t found_here;
	const char ds = G_DIR_SEPARATOR;
	while((found_here = docroot.find('/', look_here)) != string::npos) {
		docroot.replace(found_here, 1, std::string(&ds, 1));
		look_here = found_here + 1;
	}

	if (!docroot.empty() && docroot.compare(0, docroot.length(), outfn, 0, docroot.length())) {
		ArdourDialog confirm (_("Destination is outside Video Server's docroot. "), true);
		Label m (_("The destination file path is outside of the Video Server's docroot. The file will not be readable by the Video Server. Do you still want to continue?"));
		confirm.get_vbox()->pack_start (m, true, true);
		confirm.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		confirm.add_button (_("Continue"), Gtk::RESPONSE_ACCEPT);
		confirm.show_all ();
		if (confirm.run() == RESPONSE_CANCEL) { return false; }
	}

	if (Glib::file_test(outfn, Glib::FILE_TEST_EXISTS)) {
		ArdourDialog confirm (_("Confirm Overwrite"), true);
		Label m (_("A file with the same name already exists.  Do you want to overwrite it?"));
		confirm.get_vbox()->pack_start (m, true, true);
		confirm.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		confirm.add_button (_("Overwrite"), Gtk::RESPONSE_ACCEPT);
		confirm.show_all ();
		if (confirm.run() == RESPONSE_CANCEL) { return false; }
	}

	std::string dir = Glib::path_get_dirname (outfn);
	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Cannot create video folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return false;
	}
	return true;
}

std::string
video_dest_dir (const std::string sessiondir, const std::string docroot)
{
	std::string dir = docroot;
	if (dir.empty() || !dir.compare(0, dir.length(), sessiondir, 0, dir.length())) {
		dir=sessiondir;
	}
	if ((dir.empty() || dir.at(dir.length()-1) != G_DIR_SEPARATOR)) { dir += G_DIR_SEPARATOR; }

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Cannot create video folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
	}
	return dir;
}

std::string
video_get_docroot (ARDOUR::RCConfiguration* config)
{
	if (config->get_video_advanced_setup()) {
		return config->get_video_server_docroot();
	}
	return X_("/");
}

std::string
video_get_server_url (ARDOUR::RCConfiguration* config)
{
	if (config->get_video_advanced_setup()) {
		return config->get_video_server_url();
	}
	return X_("http://localhost:1554");
}


std::string
strip_file_extension (const std::string infile)
{
	std::string rv;
	char *ext, *bn = strdup(infile.c_str());
	if ((ext=strrchr(bn, '.'))) {
		if (!strchr(ext, G_DIR_SEPARATOR)) {
			*ext = 0;
		}
	}
	rv = std::string(bn);
	free(bn);
	return rv;
}

std::string
get_file_extension (const std::string infile)
{
	std::string rv = "";
	char *ext, *bn = strdup(infile.c_str());
	if ((ext=strrchr(bn, '.'))) {
		if (!strchr(ext, G_DIR_SEPARATOR)) {
			rv=std::string(ext+1);
		}
	}
	free(bn);
	return rv;
}

std::string
video_dest_file (const std::string dir, const std::string infile)
{
	return dir + "a3_" + strip_file_extension(Glib::path_get_basename(infile)) + ".avi";
}

std::string
video_map_path (std::string server_docroot, std::string filepath)
{
	std::string rv = filepath;

	/* replace all G_DIR_SEPARATOR with '/' */
	size_t look_here = 0;
	size_t found_here;
	while((found_here = rv.find(G_DIR_SEPARATOR, look_here)) != string::npos) {
		rv.replace(found_here, 1, "/");
		look_here = found_here + 1;
	}

	/* strip docroot */
	if (server_docroot.length() > 0) {
		if (rv.compare(0, server_docroot.length(), server_docroot) == 0 ) {
			rv = rv.substr(server_docroot.length());
		}
	}

	CURL *curl;
	char *ue;
	curl = curl_easy_init();
	ue = curl_easy_escape(curl, rv.c_str(),rv.length());
	if (ue) {
		rv = std::string(ue);
		curl_free(ue);
	}
	curl_easy_cleanup(curl);

	return rv;
}

void
ParseCSV (const std::string &csv, std::vector<std::vector<std::string> > &lines)
{
	bool inQuote(false);
	bool newLine(false);
	std::string field;
	lines.clear();
	std::vector<std::string> line;

	std::string::const_iterator aChar = csv.begin();
	while (aChar != csv.end()) {
		switch (*aChar) {
		case '"':
		 newLine = false;
		 inQuote = !inQuote;
		 break;

		case ',':
		 newLine = false;
		 if (inQuote == true) {
				field += *aChar;
		 } else {
				line.push_back(field);
				field.clear();
		 }
		 break;

		case '\n':
		case '\r':
		 if (inQuote == true) {
				field += *aChar;
		 } else {
				if (newLine == false) {
					 line.push_back(field);
					 lines.push_back(line);
					 field.clear();
					 line.clear();
					 newLine = true;
				}
		 }
		 break;

		default:
			 newLine = false;
			 field.push_back(*aChar);
			 break;
		}
		aChar++;
	}

	 if (field.size())
		line.push_back(field);

	 if (line.size())
		lines.push_back(line);
}

bool
video_query_info (
		std::string video_server_url,
		std::string filepath,
		double &video_file_fps,
		long long int &video_duration,
		double &video_start_offset,
		double &video_aspect_ratio
		)
{
	char url[2048];

	snprintf(url, sizeof(url), "%s%sinfo/?file=%s&format=plain"
			, video_server_url.c_str()
			, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/"
			, filepath.c_str());
	char *res = curl_http_get(url, NULL);
	int pid=0;
#ifndef COMPILER_MINGW
	if (res) {
		char *pch, *pst;
		int version;
		pch = strtok_r(res, "\n", &pst);
		while (pch) {
#if 0 /* DEBUG */
			printf("VideoFileInfo [%i] -> '%s'\n", pid, pch);
#endif
			switch (pid) {
				case 0:
				  version = atoi(pch);
					if (version != 1) break;
				case 1:
				  video_file_fps = atof(pch);
					break;
				case 2:
					video_duration = atoll(pch);
					break;
				case 3:
					video_start_offset = atof(pch);
					break;
				case 4:
					video_aspect_ratio = atof(pch);
					break;
				default:
					break;
			}
			pch = strtok_r(NULL,"\n", &pst);
			++pid;
		}
	  free(res);
	}
#endif
	if (pid!=5) {
		return false;
	}
	return true;
}

void
video_draw_cross (Glib::RefPtr<Gdk::Pixbuf> img)
{

	int rowstride = img->get_rowstride();
	int n_channels = img->get_n_channels();
	guchar *pixels, *p;
	pixels = img->get_pixels();

	int x,y;
	int clip_width = img->get_width();
	int clip_height = img->get_height();

	for (x=0;x<clip_width;x++) {
		y = clip_height * x / clip_width;
		p = pixels + y * rowstride + x * n_channels;
		p[0] = 192; p[1] = 192; p[2] = 192;
		if (n_channels>3) p[3] = 255;
		p = pixels + y * rowstride + (clip_width-x-1) * n_channels;
		p[0] = 192; p[1] = 192; p[2] = 192;
		if (n_channels>3) p[3] = 255;
	}
}


extern "C" {
#include <curl/curl.h>

	struct MemoryStruct {
		char *data;
		size_t size;
	};

	static size_t
	WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data) {
		size_t realsize = size * nmemb;
		struct MemoryStruct *mem = (struct MemoryStruct *)data;

		mem->data = (char *)realloc(mem->data, mem->size + realsize + 1);
		if (mem->data) {
			memcpy(&(mem->data[mem->size]), ptr, realsize);
			mem->size += realsize;
			mem->data[mem->size] = 0;
		}
		return realsize;
	}

	char *curl_http_get (const char *u, int *status) {
		CURL *curl;
		CURLcode res;
		struct MemoryStruct chunk;
		long int httpstatus;
		if (status) *status = 0;
		//usleep(500000); return NULL; // TEST & DEBUG
		if (strncmp("http://", u, 7)) return NULL;

		chunk.data=NULL;
		chunk.size=0;

		curl = curl_easy_init();
		if(!curl) return NULL;
		curl_easy_setopt(curl, CURLOPT_URL, u);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ARDOUR_USER_AGENT);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, ARDOUR_CURL_TIMEOUT);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
#ifdef CURLERRORDEBUG
		char curlerror[CURL_ERROR_SIZE] = "";
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerror);
#endif

		res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpstatus);
		curl_easy_cleanup(curl);
		if (status) *status = httpstatus;
		if (res) {
#ifdef CURLERRORDEBUG
			printf("curl_http_get() failed: %s\n", curlerror);
#endif
			return NULL;
		}
		if (httpstatus != 200) {
			free (chunk.data);
			chunk.data = NULL;
		}
		return (chunk.data);
	}

} /* end extern "C" */
