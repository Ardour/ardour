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

#include <fstream>
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

#include "bundle_env.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;

void
fixup_bundle_environment (int /*argc*/, char* argv[], const char** localedir)
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
	std::string userconfigdir = user_config_directory();

#ifdef ENABLE_NLS
	if (!ARDOUR::translations_are_enabled ()) {
		(*localedir) = "/this/cannot/exist";
	} else {
		/* force localedir into the bundle */
		vector<string> lpath;
		lpath.push_back (dir_path);
		lpath.push_back ("share");
		lpath.push_back ("locale");
		(*localedir) = canonical_path (Glib::build_filename (lpath)).c_str();
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

	/* unset GTK_RC_FILES so that we only load the RC files that we define
	 */

	g_unsetenv ("GTK_RC_FILES");

	/* Tell fontconfig where to find fonts.conf. Use the system version
	   if it exists, otherwise use the stuff we included in the bundle
	*/

	if (Glib::file_test ("/etc/fonts/fonts.conf", Glib::FILE_TEST_EXISTS)) {
		g_setenv ("FONTCONFIG_FILE", "/etc/fonts/fonts.conf", 1);
		g_setenv ("FONTCONFIG_PATH", "/etc/fonts", 1);
	} else {
		error << _("No fontconfig file found on your system. Things may looked very odd or ugly") << endmsg;
	}

	/* write a pango.rc file and tell pango to use it. we'd love
	   to put this into the Ardour.app bundle and leave it there,
	   but the user may not have write permission. so ...

	   we also have to make sure that the user ardour directory
	   actually exists ...
	*/

	if (g_mkdir_with_parents (userconfigdir.c_str(), 0755) < 0) {
		error << string_compose (_("cannot create user %3 folder %1 (%2)"), userconfigdir, strerror (errno), PROGRAM_NAME)
		      << endmsg;
	} else {
		
		path = Glib::build_filename (userconfigdir, "pango.rc");
		std::ofstream pangorc (path.c_str());
		if (!pangorc) {
			error << string_compose (_("cannot open pango.rc file %1") , path) << endmsg;
		} else {
			pangorc << "[Pango]\nModuleFiles="
				<< Glib::build_filename (userconfigdir, "pango.modules")
				<< endl;
			pangorc.close ();
		}
		
		g_setenv ("PANGO_RC_FILE", path.c_str(), 1);
		
		/* similar for GDK pixbuf loaders, but there's no RC file required
		   to specify where it lives.
		*/
		
		g_setenv ("GDK_PIXBUF_MODULE_FILE", Glib::build_filename (userconfigdir, "gdk-pixbuf.loaders").c_str(), 1);
	}

        /* this doesn't do much but setting it should prevent various parts of the GTK/GNU stack
           from looking outside the bundle to find the charset.alias file.
        */
        g_setenv ("CHARSETALIASDIR", dir_path.c_str(), 1);

}

void 
load_custom_fonts() 
{
	std::string ardour_mono_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	}

	FcConfig *config = FcInitLoadConfigAndFonts();
	FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(ardour_mono_file.c_str()));

	if (ret == FcFalse) {
		cerr << _("Cannot load ArdourMono TrueType font.") << endl;
	}

	ret = FcConfigSetCurrent(config);

	if (ret == FcFalse) {
		cerr << _("Failed to set fontconfig configuration.") << endl;
	}
}
