/*
    Copyright (C) 2008 John Emmas

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

#include <glib.h>
#include <glibmm.h>
#include <string.h>

#include "ardour/vst_search_path.h"

#ifdef PLATFORM_WINDOWS

#include "pbd/windows_special_dirs.h"

namespace ARDOUR {

char*
vst_search_path ()
{
	DWORD dwType = REG_SZ;  
	HKEY hKey;
	DWORD dwSize = PATH_MAX;  
	char* p = 0;
	gchar  *user_home = 0;

	if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_CURRENT_USER, "Software\\VST", 0, KEY_READ, &hKey)) {
		// Look for the user's VST Registry entry
		if (ERROR_SUCCESS == RegQueryValueExA (hKey, "VSTPluginsPath", 0, &dwType, (LPBYTE)tmp, &dwSize)) {
			p = g_build_filename (Glib::locale_to_utf8(tmp).c_str(), 0);
		}
		RegCloseKey (hKey);
		
		if (p == 0) {
			if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\VST", 0, KEY_READ, &hKey)) {
				// Look for a global VST Registry entry
				if (ERROR_SUCCESS == RegQueryValueExA (hKey, "VSTPluginsPath", 0, &dwType, (LPBYTE)tmp, &dwSize))
					p = g_build_filename (Glib::locale_to_utf8(tmp).c_str(), 0);
				
				RegCloseKey (hKey);
			}
			
			if (p == 0) {
				char *pVSTx86 = 0;
				char *pProgFilesX86 = get_win_special_folder (CSIDL_PROGRAM_FILESX86);
				
				if (pProgFilesX86) {
					// Look for a VST folder under C:\Program Files (x86)
					if (pVSTx86 = g_build_filename (pProgFilesX86, "Steinberg", "VSTPlugins", 0)) {
						if (Glib::file_test (pVSTx86, Glib::FILE_TEST_EXISTS))
							if (Glib::file_test (pVSTx86, Glib::FILE_TEST_IS_DIR))
								p = g_build_filename (pVSTx86, 0);
						
						g_free (pVSTx86);
					}
					
					g_free (pProgFilesX86);
				}
			}

			if (p == 0) {
				// Look for a VST folder under C:\Program Files
				char *pVST = 0;
				char *pProgFiles = get_win_special_folder (CSIDL_PROGRAM_FILES);
				
				if (pProgFiles) {
					if (pVST = g_build_filename (pProgFiles, "Steinberg", "VSTPlugins", 0)) {
						if (Glib::file_test (pVST, Glib::FILE_TEST_EXISTS))
							if (Glib::file_test (pVST, Glib::FILE_TEST_IS_DIR))
								p = g_build_filename (pVST, 0);
						g_free (pVST);
					}
					
					g_free (pProgFiles);
				}
			}
		}
		
		if (p == 0) {
			// If all else failed, assume the plugins are under "My Documents"
			user_home = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
			if (user_home) {
				p = g_build_filename (user_home, "Plugins", "VST", 0);
			} else {
				user_home = g_build_filename(g_get_home_dir(), "My Documents", 0);
				if (user_home)
					p = g_build_filename (user_home, "Plugins", "VST", 0);
			}
		} else {
			// Concatenate the registry path with the user's personal path

			user_home = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
			
			if (user_home) {
				p = g_build_path (";", p, g_build_filename(user_home, "Plugins", "VST", 0), 0);
			} else {
				user_home = g_build_filename(g_get_home_dir(), "My Documents", 0);
				if (user_home) {
					p = g_build_path (";", p, g_build_filename (user_home, "Plugins", "VST", 0), 0);
				}
			}
		}
	}

	return p;
}

}  // namespace ARDOUR

#else 

/* Unix-like. Probably require some OS X specific breakdown if we ever add VST
 * support on that platform.
 */

namespace ARDOUR {

const char *
vst_search_path ()
{
	return "/usr/local/lib/vst:/usr/lib/vst";
}

} // namespace ARDOUR

#endif /* PLATFORM_WINDOWS */

