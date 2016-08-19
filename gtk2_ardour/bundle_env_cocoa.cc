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

#include "pbd/i18n.h"

#include <asl.h>
#include <Carbon/Carbon.h>
#include <mach-o/dyld.h>
#include <sys/param.h>

using namespace PBD;
using namespace ARDOUR;
using namespace std;

extern void set_language_preference (); // cocoacarbon.mm
extern void no_app_nap (); // cocoacarbon.mm

static void
setup_logging(void)
{
        /* The ASL API has evolved since it was introduced in 10.4. If ASL_LOG_DESCRIPTOR_WRITE is not available,
           then we're not interested in doing any of this, since its only purpose is to get stderr/stdout to
           appear in the Console.
        */
#ifdef ASL_LOG_DESCRIPTOR_WRITE
        aslmsg msg;
        aslclient c = asl_open (PROGRAM_NAME, "com.apple.console", 0);

        msg = asl_new(ASL_TYPE_MSG);
        asl_set(msg, ASL_KEY_FACILITY, "com.apple.console");
        asl_set(msg, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
        asl_set(msg, ASL_KEY_READ_UID, "-1");

        int fd = dup(2);
        //asl_set_filter(c, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
        asl_add_log_file(c, fd);
        asl_log(c, NULL, ASL_LEVEL_INFO, string_compose ("Hello world from %1", PROGRAM_NAME).c_str());
        asl_log_descriptor(c, msg, ASL_LEVEL_INFO, 1,  ASL_LOG_DESCRIPTOR_WRITE);
        asl_log_descriptor(c, msg, ASL_LEVEL_INFO, 2, ASL_LOG_DESCRIPTOR_WRITE);
#else
#warning This build host has an older ASL API, so no console logging in this build.
#endif
}

void
fixup_bundle_environment (int argc, char* argv[], string & localedir)
{
	/* do this even for non-bundle runtimes */

	no_app_nap ();

	if (!g_getenv ("ARDOUR_BUNDLED")) {
		return;
	}

	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true, "PREBUNDLE_ENV"));

	set_language_preference ();

        setup_logging ();

	char execpath[MAXPATHLEN+1];
	uint32_t pathsz = sizeof (execpath);

	_NSGetExecutablePath (execpath, &pathsz);

	std::string path;
	std::string exec_dir = Glib::path_get_dirname (execpath);
	std::string bundle_dir;
	std::string userconfigdir = user_config_directory();

	bundle_dir = Glib::path_get_dirname (exec_dir);

#ifdef ENABLE_NLS
	if (!ARDOUR::translations_are_enabled ()) {
		localedir = "/this/cannot/exist";
	} else {
		/* force localedir into the bundle */

		vector<string> lpath;
		lpath.push_back (bundle_dir);
		lpath.push_back ("Resources");
		lpath.push_back ("locale");
		localedir = Glib::build_filename (lpath).c_str();
	}
#endif

	export_search_path (bundle_dir, "ARDOUR_DLL_PATH", "/lib");

	/* inside an OS X .app bundle, there is no difference
	   between DATA and CONFIG locations, since OS X doesn't
	   attempt to do anything to expose the notion of
	   machine-independent shared data.
	*/

	export_search_path (bundle_dir, "ARDOUR_DATA_PATH", "/Resources");
	export_search_path (bundle_dir, "ARDOUR_CONFIG_PATH", "/Resources");
	export_search_path (bundle_dir, "ARDOUR_INSTANT_XML_PATH", "/Resources");
	export_search_path (bundle_dir, "LADSPA_PATH", "/Plugins");
	export_search_path (bundle_dir, "VAMP_PATH", "/lib");
	export_search_path (bundle_dir, "GTK_PATH", "/lib/gtkengines");

	g_setenv ("SUIL_MODULE_DIR", (bundle_dir + "/lib").c_str(), 1);
	g_setenv ("PATH", (bundle_dir + "/MacOS:" + std::string(g_getenv ("PATH"))).c_str(), 1);

	/* unset GTK2_RC_FILES so that we only load the RC files that we define
	 */

	g_unsetenv ("GTK2_RC_FILES");
	g_setenv ("CHARSETALIASDIR", bundle_dir.c_str(), 1);
	g_setenv ("FONTCONFIG_FILE", Glib::build_filename (bundle_dir, "Resources/fonts.conf").c_str(), 1);
}

void load_custom_fonts()
{
	/* this code will only compile on OS X 10.6 and above, and we currently do not
	 * need it for earlier versions since we fall back on a non-monospace,
	 * non-custom font.
	 */

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	std::string ardour_mono_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	}

	CFStringRef ttf;
	CFURLRef fontURL;
	CFErrorRef error;
	ttf = CFStringCreateWithBytes(
			kCFAllocatorDefault, (UInt8*) ardour_mono_file.c_str(),
			ardour_mono_file.length(),
			kCFStringEncodingUTF8, FALSE);
	fontURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, ttf, kCFURLPOSIXPathStyle, TRUE);
	if (CTFontManagerRegisterFontsForURL(fontURL, kCTFontManagerScopeProcess, &error) != true) {
		cerr << _("Cannot load ArdourMono TrueType font.") << endl;
	}
#endif
}
