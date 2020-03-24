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

#include <unistd.h>

#include <string>
#include <vector>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <fontconfig/fontconfig.h>

#include "ardour/ardour.h"
#include "ardour/filesystem_paths.h"

#include "pbd/epa.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/pathexpand.h"

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
extern int query_darwin_version (); // cocoacarbon.mm

static void
setup_logging (void)
{
	char path[PATH_MAX+1];
	snprintf (path, sizeof (path), "%s/stderr.log", user_config_directory().c_str());

	int efd = ::open (path, O_CREAT|O_WRONLY|O_TRUNC, 0644);

	if (efd >= 0) {
		if (dup2 (efd, STDERR_FILENO) < 0) {
			::exit (12);
		}
	} else {
		::exit (11);
	}

	snprintf (path, sizeof (path), "%s/stdout.log", user_config_directory().c_str());

	int ofd = ::open (path, O_CREAT|O_WRONLY|O_TRUNC, 0644);

	if (ofd >= 0) {
		if (dup2 (ofd, STDOUT_FILENO) < 0) {
			::exit (14);
		}
	} else {
		::exit (13);
	}
}

void
fixup_bundle_environment (int argc, char* argv[], string & localedir)
{
	/* if running from a bundle, stdout/stderr will be redirect to null by
	 * launchd. That's not useful for anyone, so fix that. Use the same
	 * mechanism is not running from a bundle, but ARDOUR_LOGGING is
	 * set. This allows us to test the stderr/stdout redirects directly
	 * from ./ardev.
	 */

	if (g_getenv ("ARDOUR_BUNDLED") || g_getenv ("ARDOUR_LOGGING")) {
		setup_logging ();
	}

	if (query_darwin_version () >= 19) {
		/* on Catalina, do not use NSGLView */
		g_setenv ("ARDOUR_NSGL", "0", 0);
	} else {
		g_setenv ("ARDOUR_NSGL", "1", 0);
	}

	no_app_nap ();

	if (!g_getenv ("ARDOUR_BUNDLED")) {
		return;
	}

	if (g_getenv ("ARDOUR_SELF")) {
		g_setenv ("ARDOUR_SELF", argv[0], 1);
	}
	if (g_getenv ("PREBUNDLE_ENV")) {
		EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true, "PREBUNDLE_ENV"));
	}

	set_language_preference ();

	char execpath[MAXPATHLEN+1];
	uint32_t pathsz = sizeof (execpath);

	_NSGetExecutablePath (execpath, &pathsz);

	std::string path;
	std::string exec_dir = Glib::path_get_dirname (execpath);
	std::string bundle_dir;
	std::string userconfigdir = user_config_directory();

	bundle_dir = Glib::path_get_dirname (exec_dir);

#if ENABLE_NLS
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
	std::string font_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", font_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	} else {
		CFStringRef ttf;
		CFURLRef fontURL;
		CFErrorRef error;
		ttf = CFStringCreateWithBytes(
				kCFAllocatorDefault, (const UInt8*) font_file.c_str(),
				font_file.length(),
				kCFStringEncodingUTF8, FALSE);
		fontURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, ttf, kCFURLPOSIXPathStyle, TRUE);
		if (CTFontManagerRegisterFontsForURL(fontURL, kCTFontManagerScopeProcess, &error) != true) {
			cerr << _("Cannot load ArdourMono TrueType font.") << endl;
		}
	}

	if (!find_file (ardour_data_search_path(), "ArdourSans.ttf", font_file)) {
		cerr << _("Cannot find ArdourSans TrueType font") << endl;
	} else {
		CFStringRef ttf;
		CFURLRef fontURL;
		CFErrorRef error;
		ttf = CFStringCreateWithBytes(
				kCFAllocatorDefault, (const UInt8*) font_file.c_str(),
				font_file.length(),
				kCFStringEncodingUTF8, FALSE);
		fontURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, ttf, kCFURLPOSIXPathStyle, TRUE);
		if (CTFontManagerRegisterFontsForURL(fontURL, kCTFontManagerScopeProcess, &error) != true) {
			cerr << _("Cannot load ArdourSans TrueType font.") << endl;
		}
	}
#endif
}
