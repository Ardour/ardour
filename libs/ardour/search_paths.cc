/*
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 Ben Loftis <ben@harrisonconsoles.com>
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

#include <glib.h>
#include <glibmm.h>
#include <string.h>

#include "pbd/pathexpand.h"

#include "ardour/search_paths.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <shlobj.h> // CSIDL_*
#include "pbd/windows_special_dirs.h"
#endif

namespace {
	const char * const backend_env_variable_name = "ARDOUR_BACKEND_PATH";
	const char * const surfaces_env_variable_name = "ARDOUR_SURFACES_PATH";
	const char * const export_env_variable_name = "ARDOUR_EXPORT_FORMATS_PATH";
	const char * const theme_env_variable_name = "ARDOUR_THEMES_PATH";
	const char * const ladspa_env_variable_name = "LADSPA_PATH";
	const char * const midi_patch_env_variable_name = "ARDOUR_MIDI_PATCH_PATH";
	const char * const panner_env_variable_name = "ARDOUR_PANNER_PATH";
} // anonymous

using namespace PBD;

namespace ARDOUR {

Searchpath
backend_search_path ()
{
	Searchpath spath(user_config_directory ());
	spath += ardour_dll_directory ();
	spath.add_subdirectory_to_paths(backend_dir_name);

	spath += Searchpath(Glib::getenv(backend_env_variable_name));
	return spath;
}

Searchpath
control_protocol_search_path ()
{
	Searchpath spath(user_config_directory ());
	spath += ardour_dll_directory ();
	spath.add_subdirectory_to_paths (surfaces_dir_name);

	spath += Searchpath(Glib::getenv(surfaces_env_variable_name));
	return spath;
}

Searchpath
theme_search_path ()
{
	Searchpath spath (ardour_data_search_path ());
	spath.add_subdirectory_to_paths (theme_dir_name);

	spath += Searchpath(Glib::getenv(theme_env_variable_name));
	return spath;
}

Searchpath
export_formats_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths (export_formats_dir_name);

	bool export_formats_path_defined = false;
	Searchpath spath_env (Glib::getenv(export_env_variable_name, export_formats_path_defined));

	if (export_formats_path_defined) {
		spath += spath_env;
	}

	return spath;
}

Searchpath
ladspa_search_path ()
{
	Searchpath spath_env (Glib::getenv(ladspa_env_variable_name));

	Searchpath spath (user_config_directory ());

	spath += ardour_dll_directory ();
	spath.add_subdirectory_to_paths (ladspa_dir_name);

#ifndef PLATFORM_WINDOWS
	spath.push_back ("/usr/local/lib64/ladspa");
	spath.push_back ("/usr/local/lib/ladspa");
	spath.push_back ("/usr/lib64/ladspa");
	spath.push_back ("/usr/lib/ladspa");
#endif

#ifdef __APPLE__
	spath.push_back (path_expand ("~/Library/Audio/Plug-Ins/LADSPA"));
	spath.push_back ("/Library/Audio/Plug-Ins/LADSPA");
#endif

	return spath_env + spath;
}

Searchpath
lv2_bundled_search_path ()
{
	Searchpath spath( ardour_dll_directory () );
	spath.add_subdirectory_to_paths ("LV2");

	return spath;
}

Searchpath
midi_patch_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(midi_patch_dir_name);

	bool midi_patch_path_defined = false;
	Searchpath spath_env (Glib::getenv(midi_patch_env_variable_name, midi_patch_path_defined));

	if (midi_patch_path_defined) {
		spath += spath_env;
	}

	return spath;
}

Searchpath
panner_search_path ()
{
	Searchpath spath(user_config_directory ());

	spath += ardour_dll_directory ();
	spath.add_subdirectory_to_paths(panner_dir_name);
	spath += Searchpath(Glib::getenv(panner_env_variable_name));

	return spath;
}

Searchpath
template_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(templates_dir_name);
	return spath;
}

Searchpath
plugin_metadata_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(plugin_metadata_dir_name);
	return spath;
}

Searchpath
route_template_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(route_templates_dir_name);
	return spath;
}

Searchpath
lua_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(lua_dir_name);

	return spath;
}

#ifdef PLATFORM_WINDOWS

const char*
vst_search_path ()
{
	DWORD dwType = REG_SZ;
	HKEY hKey;
	DWORD dwSize = PATH_MAX;
	char* p = 0;
	char* user_home = 0;
	char tmp[PATH_MAX+1];

	if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_CURRENT_USER, "Software\\VST", 0, KEY_READ, &hKey)) {
		// Look for the user's VST Registry entry
		if (ERROR_SUCCESS == RegQueryValueExA (hKey, "VSTPluginsPath", 0, &dwType, (LPBYTE)tmp, &dwSize))
			p = g_build_filename (Glib::locale_to_utf8(tmp).c_str(), NULL);

		RegCloseKey (hKey);
	}

	if (p == 0) {
		if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\VST", 0, KEY_READ, &hKey))
		{
			// Look for a global VST Registry entry
			if (ERROR_SUCCESS == RegQueryValueExA (hKey, "VSTPluginsPath", 0, &dwType, (LPBYTE)tmp, &dwSize))
				p = g_build_filename (Glib::locale_to_utf8(tmp).c_str(), NULL);

			RegCloseKey (hKey);
		}
	}

	if (p == 0) {
#if ( (defined __i386__) || (defined _M_IX86) )
		char *pVSTx86 = 0;
		std::string pProgFilesX86 = PBD::get_win_special_folder_path (CSIDL_PROGRAM_FILESX86);

		if (!pProgFilesX86.empty()) {
			// Look for a VST folder under C:\Program Files (x86)
			if ((pVSTx86 = g_build_filename (pProgFilesX86.c_str(), "Steinberg", "VSTPlugins", NULL)))
			{
				if (Glib::file_test (pVSTx86, Glib::FILE_TEST_EXISTS))
					if (Glib::file_test (pVSTx86, Glib::FILE_TEST_IS_DIR))
						p = g_build_filename (pVSTx86, NULL);

				g_free (pVSTx86);
			}
		}
#else
		// Look for a VST folder under C:\Program Files
		char *pVST = 0;
		std::string pProgFiles = PBD::get_win_special_folder_path (CSIDL_PROGRAM_FILES);

		if (!pProgFiles.empty()) {
			if ((pVST = g_build_filename (pProgFiles.c_str(), "Steinberg", "VSTPlugins", NULL))) {
				if (Glib::file_test (pVST, Glib::FILE_TEST_EXISTS))
					if (Glib::file_test (pVST, Glib::FILE_TEST_IS_DIR))
						p = g_build_filename (pVST, NULL);

				g_free (pVST);
			}
		}
#endif
	}

	if (p == 0) {
		// If all else failed, assume the plugins are under "My Documents"
		user_home = (char*) g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
		if (user_home) {
			p = g_build_filename (user_home, "Plugins", "VST", NULL);
		} else {
			user_home = g_build_filename(g_get_home_dir(), "My Documents", NULL);
			if (user_home)
				p = g_build_filename (user_home, "Plugins", "VST", NULL);
		}
	} else {
		// Concatenate the registry path with the user's personal path
		user_home = (char*) g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);

		if (user_home) {
			p = g_build_path (";", p, g_build_filename(user_home, "Plugins", "VST", NULL), NULL);
		} else {
			user_home = g_build_filename(g_get_home_dir(), "My Documents", NULL);

			if (user_home) {
				p = g_build_path (";", p, g_build_filename (user_home, "Plugins", "VST", NULL), NULL);
			}
		}
	}

	return p;
}

#else

/* Unix-like. Probably require some OS X specific breakdown if we ever add VST
 * support on that platform.
 */

const char *
vst_search_path ()
{
	return "/usr/local/lib/vst:/usr/lib/vst";
}

#endif // PLATFORM_WINDOWS

} // namespace ARDOUR
