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

#include <cstdlib>
#include <signal.h>
#include <cerrno>
#include <fstream>
#include <vector>

#include <sigc++/bind.h>
#include <gtkmm/settings.h>

#include "pbd/error.h"
#include "pbd/epa.h"
#include "pbd/file_utils.h"
#include "pbd/textreceiver.h"
#include "pbd/failed_constructor.h"
#include "pbd/pathexpand.h"
#include "pbd/pthread_utils.h"
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
#include "pbd/boost_debug.h"
#endif

#include "ardour/revision.h"
#include "ardour/version.h"
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/session_utils.h"
#include "ardour/filesystem_paths.h"

#include <gtkmm/main.h>
#include <gtkmm2ext/application.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/utils.h>

#include <fontconfig/fontconfig.h>

#include "version.h"
#include "utils.h"
#include "ardour_ui.h"
#include "opts.h"
#include "enums.h"

#include "i18n.h"

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

using namespace std;
using namespace Gtk;
using namespace ARDOUR_COMMAND_LINE;
using namespace ARDOUR;
using namespace PBD;

TextReceiver text_receiver ("ardour");

extern int curvetest (string);

static ARDOUR_UI  *ui = 0;
static const char* localedir = LOCALEDIR;

void
gui_jack_error ()
{
	MessageDialog win (string_compose (_("%1 could not connect to the audio backend."), PROGRAM_NAME),
	                   false,
	                   Gtk::MESSAGE_INFO,
	                   Gtk::BUTTONS_NONE);

	win.add_button (Stock::QUIT, RESPONSE_CLOSE);
	win.set_default_response (RESPONSE_CLOSE);

	win.show_all ();
	win.set_position (Gtk::WIN_POS_CENTER);

	if (!no_splash) {
		ui->hide_splash ();
	}

	/* we just don't care about the result, but we want to block */

	win.run ();
}

static void export_search_path (const string& base_dir, const char* varname, const char* dir)
{
	string path;
	const char * cstr = g_getenv (varname);

	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += base_dir;
	path += dir;

	g_setenv (varname, path.c_str(), 1);
}

#ifdef __APPLE__

#include <mach-o/dyld.h>
#include <sys/param.h>

extern void set_language_preference (); // cocoacarbon.mm

void
fixup_bundle_environment (int, char* [])
{
	if (!g_getenv ("ARDOUR_BUNDLED")) {
		return;
	}

	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true, "PREBUNDLE_ENV"));

	set_language_preference ();

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
		localedir = strdup (Glib::build_filename (lpath).c_str());
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

	setenv ("SUIL_MODULE_DIR", (bundle_dir + "/lib").c_str(), 1);
	setenv ("PATH", (bundle_dir + "/MacOS:" + std::string(getenv ("PATH"))).c_str(), 1);

	/* unset GTK_RC_FILES so that we only load the RC files that we define
	 */

	g_unsetenv ("GTK_RC_FILES");

	/* write a pango.rc file and tell pango to use it. we'd love
	   to put this into the PROGRAM_NAME.app bundle and leave it there,
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
				<< Glib::build_filename (bundle_dir, "Resources/pango.modules") 
				<< endl;
			pangorc.close ();
			
			g_setenv ("PANGO_RC_FILE", path.c_str(), 1);
		}
	}
	
	g_setenv ("CHARSETALIASDIR", bundle_dir.c_str(), 1);
	g_setenv ("FONTCONFIG_FILE", Glib::build_filename (bundle_dir, "Resources/fonts.conf").c_str(), 1);
	g_setenv ("GDK_PIXBUF_MODULE_FILE", Glib::build_filename (bundle_dir, "Resources/gdk-pixbuf.loaders").c_str(), 1);
}

static void load_custom_fonts() {
/* this code will only compile on OS X 10.6 and above, and we currently do not
 * need it for earlier versions since we fall back on a non-monospace,
 * non-custom font.
 */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	std::string ardour_mono_file;

	if (!find_file_in_search_path (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
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

#else

void
fixup_bundle_environment (int /*argc*/, char* argv[])
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

	setenv ("SUIL_MODULE_DIR", (dir_path + "/lib").c_str(), 1);
	setenv ("PATH", (dir_path + "/bin:" + std::string(getenv ("PATH"))).c_str(), 1);

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

static void load_custom_fonts() {
	std::string ardour_mono_file;
	if (!find_file_in_search_path (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
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

#endif

static gboolean
tell_about_backend_death (void* /* ignored */)
{
	if (AudioEngine::instance()->processed_frames() == 0) {
		/* died during startup */
		MessageDialog msg (string_compose (_("The audio backend (%1) has failed, or terminated"), AudioEngine::instance()->current_backend_name()), false);
		msg.set_position (Gtk::WIN_POS_CENTER);
		msg.set_secondary_text (string_compose (_(
"%2 exited unexpectedly, and without notifying %1.\n\
\n\
This could be due to misconfiguration or to an error inside %2.\n\
\n\
Click OK to exit %1."), PROGRAM_NAME, AudioEngine::instance()->current_backend_name()));

		msg.run ();
		_exit (0);

	} else {

		/* engine has already run, so this is a mid-session backend death */
			
		MessageDialog msg (string_compose (_("The audio backend (%1) has failed, or terminated"), AudioEngine::instance()->current_backend_name()), false);
		msg.set_secondary_text (string_compose (_("%2 exited unexpectedly, and without notifying %1."),
							 PROGRAM_NAME, AudioEngine::instance()->current_backend_name()));
		msg.present ();
	}
	return false; /* do not call again */
}

static void
sigpipe_handler (int /*signal*/)
{
	/* XXX fix this so that we do this again after a reconnect to the backend
	 */

	static bool done_the_backend_thing = false;

	if (!done_the_backend_thing) {
		AudioEngine::instance()->died ();
		g_idle_add (tell_about_backend_death, 0);
		done_the_backend_thing =  true;
	}
}

#ifdef WINDOWS_VST_SUPPORT

extern int windows_vst_gui_init (int* argc, char** argv[]);

/* this is called from the entry point of a wine-compiled
   executable that is linked against gtk2_ardour built
   as a shared library.
*/
extern "C" {
int ardour_main (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif
{
	fixup_bundle_environment (argc, argv);

	load_custom_fonts(); /* needs to happend before any gtk and pango init calls */

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

#ifdef ENABLE_NLS
	gtk_set_locale ();
#endif

#ifdef WINDOWS_VST_SUPPORT
	/* this does some magic that is needed to make GTK and Wine's own
	   X11 client interact properly.
	*/
	windows_vst_gui_init (&argc, &argv);
#endif

#ifdef ENABLE_NLS
	cerr << "bnd txt domain [" << PACKAGE << "] to " << localedir << endl;

	(void) bindtextdomain (PACKAGE, localedir);
	/* our i18n translations are all in UTF-8, so make sure
	   that even if the user locale doesn't specify UTF-8,
	   we use that when handling them.
	*/
	(void) bind_textdomain_codeset (PACKAGE,"UTF-8");
#endif

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// catch error message system signals ();

	text_receiver.listen_to (error);
	text_receiver.listen_to (info);
	text_receiver.listen_to (fatal);
	text_receiver.listen_to (warning);

#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
	if (g_getenv ("BOOST_DEBUG")) {
		boost_debug_shared_ptr_show_live_debugging (true);
	}
#endif

	if (parse_opts (argc, argv)) {
		exit (1);
	}

	cout << PROGRAM_NAME
	     << VERSIONSTRING
	     << _(" (built using ")
	     << revision
#ifdef __GNUC__
	     << _(" and GCC version ") << __VERSION__
#endif
	     << ')'
	     << endl;

	if (just_version) {
		exit (0);
	}

	if (no_splash) {
		cerr << _("Copyright (C) 1999-2012 Paul Davis") << endl
		     << _("Some portions Copyright (C) Steve Harris, Ari Johnson, Brett Viren, Joel Baker, Robin Gareus") << endl
		     << endl
		     << string_compose (_("%1 comes with ABSOLUTELY NO WARRANTY"), PROGRAM_NAME) << endl
		     <<	_("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.") << endl
		     << _("This is free software, and you are welcome to redistribute it ") << endl
		     << _("under certain conditions; see the source for copying conditions.")
		     << endl;
	}

	/* some GUI objects need this */

	if (!ARDOUR::init (ARDOUR_COMMAND_LINE::use_vst, ARDOUR_COMMAND_LINE::try_hw_optimization, localedir)) {
		error << string_compose (_("could not initialize %1."), PROGRAM_NAME) << endmsg;
		exit (1);
	}

	if (curvetest_file) {
		return curvetest (curvetest_file);
	}

#ifndef PLATFORM_WINDOWS
	if (::signal (SIGPIPE, sigpipe_handler)) {
		cerr << _("Cannot xinstall SIGPIPE error handler") << endl;
	}
#endif

	try {
		ui = new ARDOUR_UI (&argc, &argv, localedir);
	} catch (failed_constructor& err) {
		error << string_compose (_("could not create %1 GUI"), PROGRAM_NAME) << endmsg;
		exit (1);
	}

	ui->run (text_receiver);
	Gtkmm2ext::Application::instance()->cleanup();
	delete ui;
	ui = 0;

	ARDOUR::cleanup ();
	pthread_cancel_all ();

	return 0;
}
#ifdef WINDOWS_VST_SUPPORT
} // end of extern C block
#endif

