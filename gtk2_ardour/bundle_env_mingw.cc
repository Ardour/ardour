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
}

static __cdecl void unload_custom_fonts() {
	std::string ardour_mono_file;
	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		return;
	}
	RemoveFontResource(ardour_mono_file.c_str());
}

void load_custom_fonts() 
{
	std::string ardour_mono_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
		return;
	}

	// pango with fontconfig backend
	FcConfig *config = FcInitLoadConfigAndFonts();
	FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(ardour_mono_file.c_str()));

	if (ret == FcFalse) {
		cerr << _("Cannot load ArdourMono TrueType font.") << endl;
	}

	ret = FcConfigSetCurrent(config);

	if (ret == FcFalse) {
		cerr << _("Failed to set fontconfig configuration.") << endl;
	}

	// pango with win32 backend
	if (0 == AddFontResource(ardour_mono_file.c_str())) {
		cerr << _("Cannot register ArdourMono TrueType font with windows gdi.") << endl;
	} else {
		atexit (&unload_custom_fonts);
	}
}
