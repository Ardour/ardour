/*
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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
#include <cstdio>
#include <string>
#include <cerrno>
#include <gtkmm.h>

#include "pbd/error.h"
#include "pbd/string_convert.h"

#include "ardour/ardour.h"
#include "ardour/session_directory.h"

#include "ardour_http.h"
#include "utils.h"
#include "utils_videotl.h"
#include "video_image_frame.h"

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

#ifndef ARDOUR_CURL_TIMEOUT
#define ARDOUR_CURL_TIMEOUT (60)
#endif
#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace VideoUtils;

unsigned int VideoUtils::harvid_version = 0x0;

bool
VideoUtils::confirm_video_outfn (Gtk::Window& parent, std::string outfn, std::string docroot)
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
		switch (confirm.run ()) {
			case Gtk::RESPONSE_ACCEPT:
				break;
			default:
				return false;
		}
	}

	if (Glib::file_test(outfn, Glib::FILE_TEST_EXISTS)) {
		bool overwrite = ARDOUR_UI_UTILS::overwrite_file_dialog (parent,
		                                                         _("Confirm Overwrite"),
		                                                         _("A file with the same name already exists. Do you want to overwrite it?"));

		if (!overwrite) {
			return false;
		}
	}

	std::string dir = Glib::path_get_dirname (outfn);
	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Cannot create video folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return false;
	}
	return true;
}

std::string
VideoUtils::video_dest_dir (const std::string sessiondir, const std::string docroot)
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
VideoUtils::video_get_docroot (ARDOUR::RCConfiguration* config)
{
	if (config->get_video_advanced_setup()) {
		return config->get_video_server_docroot();
	}
#ifndef PLATFORM_WINDOWS
	return X_("/");
#else
	if (harvid_version >= 0x000802) { // 0.8.2
		return X_("");
	} else {
		return X_("C:\\");
	}
#endif
}

std::string
VideoUtils::video_get_server_url (ARDOUR::RCConfiguration* config)
{
	if (config->get_video_advanced_setup()) {
		return config->get_video_server_url();
	}
	return X_("http://127.0.0.1:1554");
}


std::string
VideoUtils::strip_file_extension (const std::string infile)
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
VideoUtils::get_file_extension (const std::string infile)
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
VideoUtils::video_dest_file (const std::string dir, const std::string infile)
{
	return Glib::build_filename(dir, strip_file_extension(Glib::path_get_basename(infile)) + ".avi");
}

std::string
VideoUtils::video_map_path (std::string server_docroot, std::string filepath)
{
	std::string rv = filepath;

	/* strip docroot */
	if (server_docroot.length() > 0) {
		if (rv.compare(0, server_docroot.length(), server_docroot) == 0 ) {
			rv = rv.substr(server_docroot.length());
		}
	}

	/* replace all G_DIR_SEPARATOR with '/' */
	size_t look_here = 0;
	size_t found_here;
	while((found_here = rv.find(G_DIR_SEPARATOR, look_here)) != string::npos) {
		rv.replace(found_here, 1, "/");
		look_here = found_here + 1;
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
VideoUtils::ParseCSV (const std::string &csv, std::vector<std::vector<std::string> > &lines)
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
VideoUtils::video_query_info (
		std::string video_server_url,
		std::string filepath,
		double &video_file_fps,
		long long int &video_duration,
		double &video_start_offset,
		double &video_aspect_ratio
		)
{
	char url[2048];

	snprintf(url, sizeof(url), "%s%sinfo/?file=%s&format=csv"
			, video_server_url.c_str()
			, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/"
			, filepath.c_str());
	std::string res = ArdourCurl::http_get (url, false);
	if (res.empty ()) {
		return false;
	}

	std::vector<std::vector<std::string> > lines;
	ParseCSV(res, lines);

	if (lines.empty() || lines.at(0).empty() || lines.at(0).size() != 6) {
		return false;
	}
	if (atoi(lines.at(0).at(0)) != 1) return false; // version
	video_start_offset = 0.0;
	video_aspect_ratio = string_to<double>(lines.at(0).at(3));
	video_file_fps = string_to<double>(lines.at(0).at(4));
	video_duration = string_to<int64_t>(lines.at(0).at(5));

	if (video_aspect_ratio < 0.01 || video_file_fps < 0.01) {
		/* catch errors early, aspect == 0 or fps == 0 will
		 * wreak havoc down the road */
		return false;
	}
	return true;
}

void
VideoUtils::video_draw_cross (Glib::RefPtr<Gdk::Pixbuf> img)
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

