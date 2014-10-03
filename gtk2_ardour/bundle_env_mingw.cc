/*
    Copyright (C) 2001-2012 Paul Davis

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

#include <stdlib.h>
#include "bundle_env.h"
#include "i18n.h"

#include <glibmm.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangoft2.h>
#include <pango/pangocairo.h>

#include <windows.h>
#include <wingdi.h>
#include <shlobj.h> // CSIDL_*

#include "ardour/ardour.h"
#include "ardour/search_paths.h"
#include "ardour/filesystem_paths.h"

#include "pbd/file_utils.h"
#include "pbd/epa.h"
#include "pbd/windows_special_dirs.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;


/* query top-level Ardour installation path.
 * Typically, this will be somehwere like
 * "C:\Program Files (x86)\Ardour"
 */
const std::string
get_install_path ()
{
	const gchar* pExeRoot = g_win32_get_package_installation_directory_of_module (0);

	if (0 == pExeRoot) {
		HKEY key;
		DWORD size = PATH_MAX;
		char tmp[PATH_MAX+1];
		if (
#ifdef __MINGW64__
				(ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\Ardour\\ardour3\\w64", 0, KEY_READ, &key))
#else
				(ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\Ardour\\ardour3\\w32", 0, KEY_READ, &key))
#endif
				&&(ERROR_SUCCESS == RegQueryValueExA (key, "Install_Dir", 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size))
			 )
		{
			pExeRoot = Glib::locale_to_utf8(tmp).c_str();
		}
	}

	if (0 == pExeRoot) {
		const char *program_files = PBD::get_win_special_folder (CSIDL_PROGRAM_FILES);
		if (program_files) {
			pExeRoot = g_build_filename(program_files, PROGRAM_NAME, NULL);
		}
	}

	if (pExeRoot && Glib::file_test(pExeRoot, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {
		return std::string (pExeRoot);
	}
	return "";
}


void
fixup_bundle_environment (int, char* [], const char** localedir)
{
	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true));
	/* what to do ? */
	// we should at least set ARDOUR_DATA_PATH to prevent the warning message.
	// setting a FONTCONFIG_FILE won't hurt either see bundle_env_msvc.cc
	// (pangocairo prefers the windows gdi backend unless PANGOCAIRO_BACKEND=fc is set)

	// Unset GTK_RC_FILES so that only ardour specific files are loaded
	Glib::unsetenv ("GTK_RC_FILES");


	std::string path;
	const char *cstr;
	cstr = getenv ("VAMP_PATH");
	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += Glib::build_filename(get_install_path(), "lib", "ardour3", "vamp");
	path += G_SEARCHPATH_SEPARATOR;
	path += "%ProgramFiles%\\Vamp Plugins"; // default vamp path
	path += G_SEARCHPATH_SEPARATOR;
	path += "%COMMONPROGRAMFILES%\\Vamp Plugins";
	Glib::setenv ("VAMP_PATH", path, true);
}

static __cdecl void
unload_custom_fonts()
{
	std::string ardour_mono_file;
	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		return;
	}
	RemoveFontResource(ardour_mono_file.c_str());
}

void
load_custom_fonts()
{
	std::string ardour_mono_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
		return;
	}

	if (pango_font_map_get_type() == PANGO_TYPE_FT2_FONT_MAP) {
		FcConfig *config = FcInitLoadConfigAndFonts();
		FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(ardour_mono_file.c_str()));

		if (ret == FcFalse) {
			cerr << _("Cannot load ArdourMono TrueType font.") << endl;
		}

		ret = FcConfigSetCurrent(config);

		if (ret == FcFalse) {
			cerr << _("Failed to set fontconfig configuration.") << endl;
		}
	} else {
		// pango with win32 backend
		if (0 == AddFontResource(ardour_mono_file.c_str())) {
			cerr << _("Cannot register ArdourMono TrueType font with windows gdi.") << endl;
		} else {
			atexit (&unload_custom_fonts);
		}
	}
}
