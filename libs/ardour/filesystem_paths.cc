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

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"

#ifdef PLATFORM_WINDOWS
#include "shlobj.h"
#include "pbd/windows_special_dirs.h"
#endif

using namespace PBD;

namespace ARDOUR {

using std::string;

std::string
user_config_directory ()
{
	static std::string p;

	if (!p.empty()) return p;

#ifdef __APPLE__
	p = Glib::build_filename (Glib::get_home_dir(), "Library/Preferences");
#else
	const char* c = 0;

	/* adopt freedesktop standards, and put .ardour3 into $XDG_CONFIG_HOME or ~/.config
	 */

	if ((c = getenv ("XDG_CONFIG_HOME")) != 0) {
		p = c;
	} else {
#ifdef PLATFORM_WINDOWS
		std::string home_dir;

		if (0 != PBD::get_win_special_folder(CSIDL_LOCAL_APPDATA)) {
			home_dir = PBD::get_win_special_folder(CSIDL_LOCAL_APPDATA);
			home_dir += "\\";
			home_dir += PROGRAM_NAME;
		}
#else
		const string home_dir = Glib::get_home_dir();
#endif
		if (home_dir.empty ()) {
			error << "Unable to determine home directory" << endmsg;
			exit (1);
		}

		p = home_dir;
		p = Glib::build_filename (p, ".config");
	}
#endif

	p = Glib::build_filename (p, user_config_dir_name);

	if (!Glib::file_test (p, Glib::FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (p.c_str(), 0755)) {
			error << string_compose (_("Cannot create Configuration directory %1 - cannot run"),
						   p) << endmsg;
			exit (1);
		}
	} else if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		error << string_compose (_("Configuration directory %1 already exists and is not a directory/folder - cannot run"),
					   p) << endmsg;
		exit (1);
	}

	return p;
}

std::string
user_cache_directory ()
{
	static std::string p;

	if (!p.empty()) return p;

#ifdef __APPLE__
	p = Glib::build_filename (Glib::get_home_dir(), "Library/Caches");
#else
	const char* c = 0;

	/* adopt freedesktop standards, and put .ardour3 into $XDG_CACHE_HOME
	 * defaulting to or ~/.config
	 *
	 * NB this should work on windows, too, but we may want to prefer
	 * PBD::get_platform_fallback_folder (PBD::FOLDER_VST) or someplace
	 */
	if ((c = getenv ("XDG_CACHE_HOME")) != 0) {
		p = c;
	} else {
#ifdef PLATFORM_WINDOWS
		std::string home_dir;

		if (0 != PBD::get_win_special_folder(CSIDL_LOCAL_APPDATA)) {
			home_dir = PBD::get_win_special_folder(CSIDL_LOCAL_APPDATA);
			home_dir += "\\";
			home_dir += PROGRAM_NAME;
		}
#else
		const string home_dir = Glib::get_home_dir();
#endif

		if (home_dir.empty ()) {
			error << "Unable to determine home directory" << endmsg;
			exit (1);
		}

		p = home_dir;
		p = Glib::build_filename (p, ".cache");
	}
#endif

	p = Glib::build_filename (p, user_config_dir_name);

	if (!Glib::file_test (p, Glib::FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (p.c_str(), 0755)) {
			error << string_compose (_("Cannot create cache directory %1 - cannot run"),
						   p) << endmsg;
			exit (1);
		}
	} else if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		error << string_compose (_("Cache directory %1 already exists and is not a directory/folder - cannot run"),
					   p) << endmsg;
		exit (1);
	}

	return p;
}

std::string
ardour_dll_directory ()
{
#ifdef PLATFORM_WINDOWS
	std::string dll_dir_path(g_win32_get_package_installation_directory_of_module(NULL));
	dll_dir_path = Glib::build_filename (dll_dir_path, "lib");
	return Glib::build_filename (dll_dir_path, "ardour3");
#else
	std::string s = Glib::getenv("ARDOUR_DLL_PATH");
	if (s.empty()) {
		std::cerr << _("ARDOUR_DLL_PATH not set in environment - exiting\n");
		::exit (1);
	}	
	return s;
#endif
}

#ifdef PLATFORM_WINDOWS
Searchpath
windows_search_path ()
{
	std::string dll_dir_path(g_win32_get_package_installation_directory_of_module(NULL));
	dll_dir_path = Glib::build_filename (dll_dir_path, "share");
	return Glib::build_filename (dll_dir_path, "ardour3");
}
#endif

Searchpath
ardour_config_search_path ()
{
	static Searchpath search_path;

	if (search_path.empty()) {
		search_path += user_config_directory();
#ifdef PLATFORM_WINDOWS
		search_path += windows_search_path ();
#endif
		std::string s = Glib::getenv("ARDOUR_CONFIG_PATH");
		if (s.empty()) {
			std::cerr << _("ARDOUR_CONFIG_PATH not set in environment\n");
		} else {
			search_path += Searchpath (s);
		}
	}

	return search_path;
}

Searchpath
ardour_data_search_path ()
{
	static Searchpath search_path;

	if (search_path.empty()) {
		search_path += user_config_directory();
#ifdef PLATFORM_WINDOWS
		search_path += windows_search_path ();
#endif
		std::string s = Glib::getenv("ARDOUR_DATA_PATH");
		if (s.empty()) {
			std::cerr << _("ARDOUR_DATA_PATH not set in environment\n");
		} else {
			search_path += Searchpath (s);
		}
	}

	return search_path;
}

} // namespace ARDOUR
