/*
    Copyright (C) 2007 Tim Mayberry

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
#include <cstdlib>
#include <iostream>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/strsplit.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"

using namespace PBD;

namespace ARDOUR {

using std::string;

sys::path
user_config_directory ()
{
	sys::path p;

#ifdef __APPLE__
	p = Glib::get_home_dir();
	p /= "Library/Preferences";

#else
	const char* c = 0;

	/* adopt freedesktop standards, and put .ardour3 into $XDG_CONFIG_HOME or ~/.config
	 */

	if ((c = getenv ("XDG_CONFIG_HOME")) != 0) {
		p = c;
	} else {
		const string home_dir = Glib::get_home_dir();

		if (home_dir.empty ()) {
			const string error_msg = "Unable to determine home directory";

			// log the error
			error << error_msg << endmsg;

			throw sys::filesystem_error(error_msg);
		}

		p = home_dir;
		p /= ".config";
	}
#endif

	p /= user_config_dir_name;

	std::string ps (p.to_string());

	if (!Glib::file_test (ps, Glib::FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (ps.c_str(), 0755)) {
			error << string_compose (_("Cannot create Configuration directory %1 - cannot run"),
						   ps) << endmsg;
			exit (1);
		}
	} else if (!Glib::file_test (ps, Glib::FILE_TEST_IS_DIR)) {
		error << string_compose (_("Configuration directory %1 already exists and is not a directory/folder - cannot run"),
					   ps) << endmsg;
		exit (1);
	}

	return p;
}

sys::path
ardour_dll_directory ()
{
	std::string s = Glib::getenv("ARDOUR_DLL_PATH");
	if (s.empty()) {
		std::cerr << _("ARDOUR_DLL_PATH not set in environment - exiting\n");
		::exit (1);
	}	
	return sys::path (s);
}

SearchPath
ardour_config_search_path ()
{
	static bool have_path = false;
	static SearchPath search_path;

	if (!have_path) {
		SearchPath sp (user_config_directory());
		
		std::string s = Glib::getenv("ARDOUR_CONFIG_PATH");
		if (s.empty()) {
			std::cerr << _("ARDOUR_CONFIG_PATH not set in environment - exiting\n");
			::exit (1);
		}
		
		std::vector<string> ss;
		split (s, ss, ':');
		for (std::vector<string>::iterator i = ss.begin(); i != ss.end(); ++i) {
			sp += sys::path (*i);
		}
		
		search_path = sp;
		have_path = true;
		info << "CONFIG PATH: " << search_path.to_string() << endmsg;
	}

	return search_path;
}

SearchPath
ardour_data_search_path ()
{
	static bool have_path = false;
	static SearchPath search_path;

	if (!have_path) {
		SearchPath sp (user_config_directory());
		
		std::string s = Glib::getenv("ARDOUR_DATA_PATH");
		if (s.empty()) {
			std::cerr << _("ARDOUR_DATA_PATH not set in environment - exiting\n");
			::exit (1);
		}
		
		std::vector<string> ss;
		split (s, ss, ':');
		for (std::vector<string>::iterator i = ss.begin(); i != ss.end(); ++i) {
			sp += sys::path (*i);
		}
		
		search_path = sp;
		have_path = true;
		info << "DATA PATH: " << search_path.to_string() << endmsg;
	}

	return search_path;
}

} // namespace ARDOUR
