/*
    Copyright (C) 2001-2007 Paul Davis
    
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

#include <sigc++/bind.h>
#include <gtkmm/settings.h>

#include <pbd/error.h>
#include <pbd/textreceiver.h>
#include <pbd/failed_constructor.h>
#include <pbd/pthread_utils.h>
#include <pbd/epa.h>

#include <jack/jack.h>

#include <ardour/svn_revision.h>
#include <ardour/version.h>
#include <ardour/ardour.h>
#include <ardour/audioengine.h>

#include <gtkmm/main.h>

#include <gtkmm2ext/application.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/utils.h>

#include "version.h"
#include "ardour_ui.h"
#include "opts.h"
#include "enums.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR_COMMAND_LINE;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

TextReceiver text_receiver ("ardour");

extern int curvetest (string);

static ARDOUR_UI  *ui = 0;
static const char* localedir = LOCALEDIR;

#ifdef __APPLE__

#include <mach-o/dyld.h>
#include <sys/param.h>

extern void set_language_preference (); // cocoacarbon.mm

void
fixup_bundle_environment (int argc, char* argv[])
{
	if (!getenv ("ARDOUR_BUNDLED")) {
		return;
	}
	
        EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true));

	set_language_preference ();

	char execpath[MAXPATHLEN+1];
	uint32_t pathsz = sizeof (execpath);

	_NSGetExecutablePath (execpath, &pathsz);

	std::string exec_path (execpath);
	std::string dir_path = Glib::path_get_dirname (exec_path);
	std::string path;
	const char *cstr = getenv ("PATH");

	/* ensure that we find any bundled executables (e.g. JACK),
	   and find them before any instances of the same name
	   elsewhere in PATH
	*/

	path = dir_path;

	/* JACK is often in /usr/local/bin and since Info.plist refuses to 
	   set PATH, we have to force this in order to discover a running
	   instance of JACK ...
	*/
	
	path += ':';
	path += "/usr/local/bin";

	if (cstr) {
		path += ':';
		path += cstr;
	}
	setenv ("PATH", path.c_str(), 1);

	path = dir_path;
	path += "/../Resources:";
	path += dir_path;
	path += "/../Resources/Surfaces:";
	path += dir_path;
	path += "/../Resources/Panners:";

	setenv ("ARDOUR_MODULE_PATH", path.c_str(), 1);

        path = get_user_ardour_path ();
        path += ':';
	path += dir_path;
	path += "/../Resources/icons:";
	path += dir_path;
	path += "/../Resources/pixmaps:";
	path += dir_path;
	path += "/../Resources/share:";
	path += dir_path;
	path += "/../Resources";

	setenv ("ARDOUR_PATH", path.c_str(), 1);
	setenv ("ARDOUR_CONFIG_PATH", path.c_str(), 1);
	setenv ("ARDOUR_DATA_PATH", path.c_str(), 1);

	path = dir_path;
	path += "/../Resources";
	setenv ("ARDOUR_INSTANT_XML_PATH", path.c_str(), 1);

	cstr = getenv ("LADSPA_PATH");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/../Plugins";
	
	setenv ("LADSPA_PATH", path.c_str(), 1);

	cstr = getenv ("VAMP_PATH");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/../Frameworks";
	
	setenv ("VAMP_PATH", path.c_str(), 1);

	path += dir_path;
	path += "/../Frameworks";
	setenv ("SUIL_MODULE_DIR", path.c_str(), 1);

	cstr = getenv ("ARDOUR_CONTROL_SURFACE_PATH");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/../Surfaces";
	
	setenv ("ARDOUR_CONTROL_SURFACE_PATH", path.c_str(), 1);

	path = dir_path;
	path += "/../Frameworks/clearlooks";

	setenv ("GTK_PATH", path.c_str(), 1);

        /* unset GTK_RC_FILES so that we only load the RC files that we define
         */

        unsetenv ("GTK_RC_FILES");

	if (!ARDOUR::translations_are_disabled ()) {

		path = dir_path;
		path += "/../Resources/locale";
		
		localedir = strdup (path.c_str());
		setenv ("GTK_LOCALEDIR", localedir, 1);
	}

	/* write a pango.rc file and tell pango to use it. we'd love
	   to put this into the Ardour.app bundle and leave it there,
	   but the user may not have write permission. so ... 

	   we also have to make sure that the user ardour directory
	   actually exists ...
	*/

	if (g_mkdir_with_parents (ARDOUR::get_user_ardour_path().c_str(), 0755) < 0) {
		error << string_compose (_("cannot create user ardour folder %1 (%2)"), ARDOUR::get_user_ardour_path(), strerror (errno))
		      << endmsg;
	} else {

		path = Glib::build_filename (ARDOUR::get_user_ardour_path(), "pango.rc");
		
		std::ofstream pangorc (path.c_str());
		if (!pangorc) {
			error << string_compose (_("cannot open pango.rc file %1") , path) << endmsg;
		} else {
			pangorc << "[Pango]\nModuleFiles=";
			std::string mpath;

			mpath = dir_path;
			mpath += "/../Resources/pango.modules";

			pangorc << mpath << endl;
			
			pangorc.close ();
			setenv ("PANGO_RC_FILE", path.c_str(), 1);
		}
	}

	// gettext charset aliases

	setenv ("CHARSETALIASDIR", path.c_str(), 1);

	// font config
	
	path = dir_path;
	path += "/../Resources/fonts.conf";

	setenv ("FONTCONFIG_FILE", path.c_str(), 1);

	// GDK Pixbuf loader module file

	path = dir_path;
	path += "/../Resources/gdk-pixbuf.loaders";

	setenv ("GDK_PIXBUF_MODULE_FILE", path.c_str(), 1);

	if (getenv ("ARDOUR_WITH_JACK")) {
		// JACK driver dir
		
		path = dir_path;
		path += "/../Frameworks";
		
		setenv ("JACK_DRIVER_DIR", path.c_str(), 1);
	}
}

#else

void
fixup_bundle_environment (int argc, char* argv[])
{
	if (!getenv ("ARDOUR_BUNDLED")) {
		return;
	}

        EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true, "PREBUNDLE_ENV"));
	
	std::string exec_path = argv[0];
	std::string dir_path = Glib::path_get_dirname (Glib::path_get_dirname (exec_path));
	std::string path;
	const char *cstr = getenv ("PATH");

	/* ensure that we find any bundled executables (e.g. JACK),
	   and find them before any instances of the same name
	   elsewhere in PATH
	*/

        /* note that this function is POSIX/Linux specific, so using / as 
           a dir separator in this context is just fine.
        */

	path = dir_path;
	path += "/etc:";
	path += dir_path;
	path += "/lib/surfaces:";
	path += dir_path;
	path += "/lib/panners:";

	setenv ("ARDOUR_MODULE_PATH", path.c_str(), 1);

        path = get_user_ardour_path ();
        path += ':';
	path += dir_path;
	path += "/etc/icons:";
	path += dir_path;
	path += "/etc/pixmaps:";
	path += dir_path;
	path += "/share:";
	path += dir_path;
	path += "/etc";

	setenv ("ARDOUR_PATH", path.c_str(), 1);
	setenv ("ARDOUR_CONFIG_PATH", path.c_str(), 1);
	setenv ("ARDOUR_DATA_PATH", path.c_str(), 1);

	path = dir_path;
	path += "/etc";
	setenv ("ARDOUR_INSTANT_XML_PATH", path.c_str(), 1);

	cstr = getenv ("LADSPA_PATH");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/lib/plugins";
	
	setenv ("LADSPA_PATH", path.c_str(), 1);

	cstr = getenv ("VAMP_PATH");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/lib";
	
	setenv ("VAMP_PATH", path.c_str(), 1);

	cstr = getenv ("SUIL_MODULE_DIR");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/lib";
	
	setenv ("SUIL_MODULE_DIR", path.c_str(), 1);

	cstr = getenv ("ARDOUR_CONTROL_SURFACE_PATH");
	if (cstr) {
		path = cstr;
		path += ':';
	} else {
		path = "";
	}
	path += dir_path;
	path += "/lib/surfaces";
	
	setenv ("ARDOUR_CONTROL_SURFACE_PATH", path.c_str(), 1);

	path = dir_path;
	path += "/lib/clearlooks";

	setenv ("GTK_PATH", path.c_str(), 1);

        /* unset GTK_RC_FILES so that we only load the RC files that we define
         */

        unsetenv ("GTK_RC_FILES");

	/* Tell fontconfig where to find fonts.conf. Use the system version
	   if it exists, otherwise use the stuff we included in the bundle
	*/

	if (Glib::file_test ("/etc/fonts/fonts.conf", Glib::FILE_TEST_EXISTS)) {
		setenv ("FONTCONFIG_FILE", "/etc/fonts/fonts.conf", 1);
		setenv ("FONTCONFIG_PATH", "/etc/fonts", 1);
	} else {
		/* use the one included in the bundle */
		
		path = Glib::build_filename (dir_path, "etc/fonts/fonts.conf");
		setenv ("FONTCONFIG_FILE", path.c_str(), 1);
		setenv ("FONTCONFIG_PATH", Glib::build_filename (dir_path, "etc/fonts").c_str(), 1);
	}

	if (!ARDOUR::translations_are_disabled ()) {
                path = dir_path;
                path += "/share/locale";
                
                localedir = strdup (path.c_str());
                setenv ("GTK_LOCALEDIR", localedir, 1);
        }

	/* write a pango.rc file and tell pango to use it. we'd love
	   to put this into the Ardour.app bundle and leave it there,
	   but the user may not have write permission. so ... 

	   we also have to make sure that the user ardour directory
	   actually exists ...
	*/

	if (g_mkdir_with_parents (ARDOUR::get_user_ardour_path().c_str(), 0755) < 0) {
		error << string_compose (_("cannot create user ardour folder %1 (%2)"), ARDOUR::get_user_ardour_path(), strerror (errno))
		      << endmsg;
	} else {

                std::string mpath;

		path = Glib::build_filename (ARDOUR::get_user_ardour_path(), "pango.rc");

		std::ofstream pangorc (path.c_str());
		if (!pangorc) {
			error << string_compose (_("cannot open pango.rc file %1") , path) << endmsg;
                } else {
                        mpath = Glib::build_filename (ARDOUR::get_user_ardour_path(), "pango.modules");
                        
			pangorc << "[Pango]\nModuleFiles=";
			pangorc << mpath << endl;
			pangorc.close ();
                }

                setenv ("PANGO_RC_FILE", path.c_str(), 1);

                /* similar for GDK pixbuf loaders, but there's no RC file required
                   to specify where it lives.
                */

                mpath = Glib::build_filename (ARDOUR::get_user_ardour_path(), "gdk-pixbuf.loaders");
                setenv ("GDK_PIXBUF_MODULE_FILE", mpath.c_str(), 1);
	}
}

#endif

static gboolean
tell_about_jack_death (void* /* ignored */)
{
	if (AudioEngine::instance()->processed_frames() == 0) {
		/* died during startup */
		MessageDialog msg (_("JACK exited"), false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK);
		msg.set_position (Gtk::WIN_POS_CENTER);
		msg.set_secondary_text (string_compose (_(
"JACK exited unexpectedly, and without notifying %1.\n\
\n\
This could be due to misconfiguration or to an error inside JACK.\n\
\n\
Click OK to exit %1."), PROGRAM_NAME));
    
		msg.run ();
		_exit (0);
		
	} else {

		/* engine has already run, so this is a mid-session JACK death */
		
		MessageDialog* msg = manage (new MessageDialog (_("JACK exited"), false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_NONE));
		msg->set_secondary_text (string_compose (_(
"JACK exited unexpectedly, and without notifying %1.\n\
\n\
This is probably due to an error inside JACK. You should restart JACK\n\
and reconnect %1 to it, or exit %1 now. You cannot save your\n\
session at this time, because we would lose your connection information.\n"), PROGRAM_NAME));
		msg->present ();
	}
	return false; /* do not call again */
}

static void
sigpipe_handler (int sig)
{
        static bool done_the_jack_thing = false;
	
	if (!done_the_jack_thing) {
		AudioEngine::instance()->died ();
		g_idle_add (tell_about_jack_death, 0);
		done_the_jack_thing =  true;
	}
}

#ifdef VST_SUPPORT

extern int gui_init (int* argc, char** argv[]);

/* this is called from the entry point of a wine-compiled
   executable that is linked against gtk2_ardour built
   as a shared library.
*/

extern "C" {
int ardour_main (int argc, char *argv[])
#else
int main (int argc, char* argv[])
#endif
{
	vector<std::string> null_file_list;

	fixup_bundle_environment (argc, argv);

        Glib::thread_init();

#ifdef VST_SUPPORT
	/* this does some magic that is needed to make GTK and Wine's own
	   X11 client interact properly.
	*/
	gui_init (&argc, &argv);
#endif

	(void) bindtextdomain (PACKAGE, localedir);
	/* our i18n translations are all in UTF-8, so make sure
	   that even if the user locale doesn't specify UTF-8,
	   we use that when handling them.
	*/
	(void) bind_textdomain_codeset (PACKAGE,"UTF-8");

	/* this really does nothing since we don't get gettext(),
	   only dgettext(), but whatever ...
	*/
	(void) textdomain (PACKAGE);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// catch error message system signals ();

	text_receiver.listen_to (error);
	text_receiver.listen_to (info);
	text_receiver.listen_to (fatal);
	text_receiver.listen_to (warning);

	if (parse_opts (argc, argv)) {
		exit (1);
	}

	if (curvetest_file) {
		return curvetest (curvetest_file);
	}
	
	cout << PROGRAM_NAME << ' '
	     << VERSIONSTRING
	     << _("\n   (built using ")
	     << svn_revision
#ifdef __GNUC__
	     << _(" and GCC version ") << __VERSION__ 
#endif
	     << ')'
	     << endl;
	
	if (just_version) {
		exit (0);
	}

	if (no_splash) {
		cerr << _("Copyright (C) 1999-2008 Paul Davis") << endl
		     << _("Some portions Copyright (C) Steve Harris, Ari Johnson, Brett Viren, Joel Baker") << endl
		     << endl
		     << string_compose (_("%1 comes with ABSOLUTELY NO WARRANTY"), PROGRAM_NAME) << endl
		     <<	_("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.") << endl
		     << _("This is free software, and you are welcome to redistribute it ") << endl
		     << _("under certain conditions; see the source for copying conditions.")
		     << endl;
	}

	/* some GUI objects need this */

	PBD::ID::init ();

	if (::signal (SIGPIPE, sigpipe_handler)) {
		cerr << _("Cannot install SIGPIPE error handler") << endl;
	}

        try { 
		ui = new ARDOUR_UI (&argc, &argv);
	} catch (failed_constructor& err) {
		error << _("could not create ARDOUR GUI") << endmsg;
		exit (1);
	}

	ui->run (text_receiver);
	Gtkmm2ext::Application::instance()->cleanup();
	ui = 0;

	ARDOUR::cleanup ();
	pthread_cancel_all ();

	return 0;
}
#ifdef VST_SUPPORT
} // end of extern C block
#endif

