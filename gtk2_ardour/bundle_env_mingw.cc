/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <stdlib.h>
#include <string>
#include "bundle_env.h"
#include "pbd/i18n.h"

#include <glibmm.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangoft2.h>
#include <pango/pangocairo.h>

#include <windows.h>
#include <wingdi.h>

#include "ardour/ardour.h"
#include "ardour/search_paths.h"
#include "ardour/filesystem_paths.h"

#include "pbd/file_utils.h"
#include "pbd/epa.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;


enum MY_PROCESS_DPI_AWARENESS {
  PROCESS_DPI_UNAWARE,
  PROCESS_SYSTEM_DPI_AWARE,
  PROCESS_PER_MONITOR_DPI_AWARE
};

typedef HRESULT (WINAPI* SetProcessDpiAwareness_t)(MY_PROCESS_DPI_AWARENESS);

void
fixup_bundle_environment (int, char* [], string & localedir)
{
	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true));
	/* what to do ? */
	// we should at least set ARDOUR_DATA_PATH to prevent the warning message.
	// setting a FONTCONFIG_FILE won't hurt either see bundle_env_msvc.cc
	// (pangocairo prefers the windows gdi backend unless PANGOCAIRO_BACKEND=fc is set)

	// Unset GTK2_RC_FILES so that only ardour specific files are loaded
	Glib::unsetenv ("GTK2_RC_FILES");

	std::string path;

	if (ARDOUR::translations_are_enabled ()) {
		path = windows_search_path().to_string();
		path += "\\locale";
		Glib::setenv ("GTK_LOCALEDIR", path, true);

		// and return the same path to our caller
		localedir = path;
	}

	const char *cstr;
	cstr = getenv ("VAMP_PATH");
	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += Glib::build_filename(ardour_dll_directory(), "vamp");
	path += G_SEARCHPATH_SEPARATOR;
	path += "%ProgramFiles%\\Vamp Plugins"; // default vamp path
	path += G_SEARCHPATH_SEPARATOR;
	path += "%COMMONPROGRAMFILES%\\Vamp Plugins";
	Glib::setenv ("VAMP_PATH", path, true);

	Glib::setenv ("SUIL_MODULE_DIR", Glib::build_filename(ardour_dll_directory(), "suil"), true);

	/* XXX this should really be PRODUCT_EXE see tools/x-win/package.sh
	 * ardour on windows does not have a startup wrapper script.
	 *
	 * then again, there's probably nobody using NSM on windows.
	 * because neither nsmd nor the GUI is currently available for windows.
	 * furthermore it'll be even less common for derived products.
	 */
	Glib::setenv ("ARDOUR_SELF", Glib::build_filename(ardour_dll_directory(), "ardour.exe"), true);

	/* https://docs.microsoft.com/en-us/windows/win32/api/shellscalingapi/nf-shellscalingapi-setprocessdpiawareness */
	HMODULE module = LoadLibraryA ("Shcore.dll");
	if (module) {
		SetProcessDpiAwareness_t setProcessDpiAwareness = reinterpret_cast<SetProcessDpiAwareness_t> (GetProcAddress (module, "SetProcessDpiAwareness"));
		if (setProcessDpiAwareness) {
			setProcessDpiAwareness (PROCESS_SYSTEM_DPI_AWARE);
		}
		FreeLibrary (module);
	}
}

static __cdecl void
unload_custom_fonts()
{
	std::string font_file;
	if (find_file (ardour_data_search_path(), "ArdourMono.ttf", font_file)) {
		RemoveFontResource(font_file.c_str());
	}
	if (find_file (ardour_data_search_path(), "ArdourSans.ttf", font_file)) {
		RemoveFontResource(font_file.c_str());
	}
}

void
load_custom_fonts()
{
	std::string ardour_mono_file;
	std::string ardour_sans_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	}

	if (!find_file (ardour_data_search_path(), "ArdourSans.ttf", ardour_sans_file)) {
		cerr << _("Cannot find ArdourSans TrueType font") << endl;
	}

	if (ardour_mono_file.empty () && ardour_sans_file.empty ()) {
		return;
	}

	if (pango_font_map_get_type() == PANGO_TYPE_FT2_FONT_MAP) {
		FcConfig *config = FcInitLoadConfigAndFonts();

		if (!ardour_mono_file.empty () && FcFalse == FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(ardour_mono_file.c_str()))) {
			cerr << _("Cannot load ArdourMono TrueType font.") << endl;
		}

		if (!ardour_sans_file.empty () && FcFalse == FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(ardour_sans_file.c_str()))) {
			cerr << _("Cannot load ArdourSans TrueType font.") << endl;
		}

		if (FcFalse == FcConfigSetCurrent(config)) {
			cerr << _("Failed to set fontconfig configuration.") << endl;
		}
	} else {
		// pango with win32 backend
		if (0 == AddFontResource(ardour_mono_file.c_str())) {
			cerr << _("Cannot register ArdourMono TrueType font with windows gdi.") << endl;
		}
		if (0 == AddFontResource(ardour_sans_file.c_str())) {
			cerr << _("Cannot register ArdourSans TrueType font with windows gdi.") << endl;
		}
		atexit (&unload_custom_fonts);
	}
}
