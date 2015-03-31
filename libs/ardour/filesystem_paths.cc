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

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/error.h"

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

static std::string
user_config_directory_name (int version = -1)
{
	if (version < 0) {
		version = atoi (X_(PROGRAM_VERSION));
	}

	const string config_dir_name = string_compose ("%1%2", X_(PROGRAM_NAME), version);

#if defined (__APPLE__) || defined (PLATFORM_WINDOWS)
	return config_dir_name;
#else
	return downcase (config_dir_name);
#endif
}	

std::string
user_config_directory (int version)
{
	std::string p;

#ifdef __APPLE__

	p = Glib::build_filename (Glib::get_home_dir(), "Library/Preferences");

#else

	const char* c = 0;
	/* adopt freedesktop standards, and put .ardour3 into $XDG_CONFIG_HOME or ~/.config */
	if ((c = getenv ("XDG_CONFIG_HOME")) != 0) {
		p = c;
	} else {

#ifdef PLATFORM_WINDOWS
		// Not technically the home dir (since it needs to be a writable folder)
		const string home_dir = Glib::get_user_config_dir();
#else
		const string home_dir = Glib::get_home_dir();
#endif
		if (home_dir.empty ()) {
			error << "Unable to determine home directory" << endmsg;
			exit (1);
		}
		p = home_dir;

#ifndef PLATFORM_WINDOWS
		p = Glib::build_filename (p, ".config");
#endif

	}
#endif // end not __APPLE__

	p = Glib::build_filename (p, user_config_directory_name (version));

	if (version < 0) {
		/* Only create the user config dir if the version was negative,
		   meaning "for the current version.
		*/
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
	 */
	if ((c = getenv ("XDG_CACHE_HOME")) != 0) {
		p = c;
	} else {

#ifdef PLATFORM_WINDOWS
		// Not technically the home dir (since it needs to be a writable folder)
		const string home_dir = Glib::get_user_data_dir();
#else
		const string home_dir = Glib::get_home_dir();
#endif
		if (home_dir.empty ()) {
			error << "Unable to determine home directory" << endmsg;
			exit (1);
		}
		p = home_dir;

#ifndef PLATFORM_WINDOWS
		p = Glib::build_filename (p, ".cache");
#endif

	}
#endif // end not __APPLE__

	p = Glib::build_filename (p, user_config_directory_name ());

#ifdef PLATFORM_WINDOWS
	 /* On Windows Glib::get_user_data_dir is the folder to use for local
		* (as opposed to roaming) application data.
		* See documentation for CSIDL_LOCAL_APPDATA.
		* Glib::get_user_data_dir() == GLib::get_user_config_dir()
		* so we add an extra subdir *below* the config dir.
		*/
	p = Glib::build_filename (p, "cache");
#endif

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
	return Glib::build_filename (dll_dir_path, LIBARDOUR);
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
	return Glib::build_filename (dll_dir_path, LIBARDOUR);
}
#endif

Searchpath
ardour_config_search_path ()
{
	static Searchpath search_path;

	if (search_path.empty()) {
		// Start by adding the user's personal config folder
		search_path += user_config_directory();
#ifdef PLATFORM_WINDOWS
		// On Windows, add am intermediate configuration folder
		// (one that's guaranteed to be writable by all users).
		const gchar* const *all_users_folder = g_get_system_config_dirs();
		// Despite its slightly odd name, the above returns a single entry which
		// corresponds to 'All Users' on Windows (according to the documentation)

		if (all_users_folder) {
			std::string writable_all_users_path = all_users_folder[0];
			writable_all_users_path += "\\";
			writable_all_users_path += PROGRAM_NAME;
			writable_all_users_path += "\\.config";
#ifdef _WIN64
			writable_all_users_path += "\\win64";
#else
			writable_all_users_path += "\\win32";
#endif
			search_path += writable_all_users_path;
		}

		// now add a suitable config path from the bundle
		search_path += windows_search_path ();
#endif
		// finally, add any paths from ARDOUR_CONFIG_PATH if it exists
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

string
been_here_before_path (int version)
{
	if (version < 0) {
		version = atoi (PROGRAM_VERSION);
	}

	return Glib::build_filename (user_config_directory (version), string (".a") + to_string (version, std::dec));
}


} // namespace ARDOUR
