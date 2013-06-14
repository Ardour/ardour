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
/** @file utils_videotl.h
 *  @brief common functions used for video-file im/export
 */

#ifndef __gtk_ardour_video_utils_h__
#define __gtk_ardour_video_utils_h__

#include <string>
#include <gtkmm.h>

#include "ardour/rc_configuration.h"
#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour_dialog.h"

bool confirm_video_outfn (std::string, std::string docroot="");
std::string video_dest_dir (const std::string, const std::string);
std::string video_dest_file (const std::string, const std::string);
std::string strip_file_extension (const std::string infile);
std::string get_file_extension (const std::string infile);

void ParseCSV(const std::string &csv, std::vector<std::vector<std::string> > &lines);
std::string video_map_path (std::string server_docroot, std::string filepath);
void video_draw_cross (Glib::RefPtr<Gdk::Pixbuf> img);
std::string video_get_server_url (ARDOUR::RCConfiguration* config);
std::string video_get_docroot (ARDOUR::RCConfiguration* config);

bool video_query_info (
		std::string video_server_url,
		std::string filepath,
		double &video_file_fps,
		long long int &video_duration,
		double &video_start_offset,
		double &video_aspect_ratio
		);

extern "C" {
	char *curl_http_get (const char *u, int *status);
}

#endif /* __gtk_ardour_video_utils_h__ */
