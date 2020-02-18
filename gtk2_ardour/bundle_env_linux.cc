/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#include <string>
#include <vector>
#include <cerrno>
#include <cstring>

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <fontconfig/fontconfig.h>

#include "ardour/ardour.h"
#include "ardour/filesystem_paths.h"

#include "pbd/epa.h"
#include "pbd/search_path.h"
#include "pbd/pathexpand.h"
#include "pbd/file_utils.h"

#include "ardour_http.h"
#include "bundle_env.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;

void
fixup_bundle_environment (int /*argc*/, char* argv[], string & localedir)
{
	/* THIS IS FOR LINUX - its just about the only place where its
	 * acceptable to build paths directly using '/'.
	 */

	if (!g_getenv ("ARDOUR_BUNDLED")) {
		return;
	}

	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true, "PREBUNDLE_ENV"));

	std::string path;
	std::string dir_path = Glib::path_get_dirname (Glib::path_get_dirname (argv[0]));
#if defined WINDOWS_VST_SUPPORT
	// argv[0] will be "wine"
	if (g_getenv ("INSTALL_DIR")) {
		dir_path = g_getenv ("INSTALL_DIR");
	}
#endif

#if ENABLE_NLS
	if (!ARDOUR::translations_are_enabled ()) {
		localedir = "/this/cannot/exist";
	} else {
		/* force localedir into the bundle */
		vector<string> lpath;
		lpath.push_back (dir_path);
		lpath.push_back ("share");
		lpath.push_back ("locale");
		localedir = canonical_path (Glib::build_filename (lpath)).c_str();
	}
#endif

	/* note that this function is POSIX/Linux specific, so using / as
	   a dir separator in this context is just fine.
	*/

	export_search_path (dir_path, "ARDOUR_DLL_PATH", "/lib");
	export_search_path (dir_path, "ARDOUR_CONFIG_PATH", "/etc");
	export_search_path (dir_path, "ARDOUR_INSTANT_XML_PATH", "/share");
	export_search_path (dir_path, "ARDOUR_DATA_PATH", "/share");
	export_search_path (dir_path, "LADSPA_PATH", "/plugins");
	export_search_path (dir_path, "VAMP_PATH", "/lib");
	export_search_path (dir_path, "GTK_PATH", "/lib/gtkengines");

	g_setenv ("SUIL_MODULE_DIR", (dir_path + "/lib").c_str(), 1);
	g_setenv ("PATH", (dir_path + "/bin:" + std::string(g_getenv ("PATH"))).c_str(), 1);

	/* unset GTK2_RC_FILES so that we only load the RC files that we define
	 */

	g_unsetenv ("GTK2_RC_FILES");

	/* Tell fontconfig where to find fonts.conf. Use the system version
	   if it exists, otherwise use the stuff we included in the bundle
	*/

	if (Glib::file_test ("/etc/fonts/fonts.conf", Glib::FILE_TEST_EXISTS)) {
		g_setenv ("FONTCONFIG_FILE", "/etc/fonts/fonts.conf", 1);
		g_setenv ("FONTCONFIG_PATH", "/etc/fonts", 1);
	} else {
		error << _("No fontconfig file found on your system. Things may looked very odd or ugly") << endmsg;
	}

	/* this doesn't do much but setting it should prevent various parts of the GTK/GNU stack
		 from looking outside the bundle to find the charset.alias file.
		 */
	g_setenv ("CHARSETALIASDIR", dir_path.c_str(), 1);

	ArdourCurl::HttpGet::setup_certificate_paths ();
}

void
load_custom_fonts()
{
	FcConfig* config = FcInitLoadConfigAndFonts();

	std::string font_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", font_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	} else {
		FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(font_file.c_str()));
		if (ret == FcFalse) {
			cerr << _("Cannot load ArdourMono TrueType font.") << endl;
		}
	}

	if (!find_file (ardour_data_search_path(), "ArdourSans.ttf", font_file)) {
		cerr << _("Cannot find ArdourSans TrueType font") << endl;
	} else {
		FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(font_file.c_str()));
		if (ret == FcFalse) {
			cerr << _("Cannot load ArdourSans TrueType font.") << endl;
		}
	}

	if (FcFalse == FcConfigSetCurrent(config)) {
		cerr << _("Failed to set fontconfig configuration.") << endl;
	}
}
