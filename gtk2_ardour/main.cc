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
#include <cerrno>
#include <vector>

#include <signal.h>
#include <locale.h>

#include <sigc++/bind.h>
#include <gtkmm/settings.h>

#ifdef HAVE_FFTW35F
#include <fftw3.h>
#endif

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/textreceiver.h"
#include "pbd/failed_constructor.h"
#include "pbd/pathexpand.h"
#include "pbd/pthread_utils.h"
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
#include "pbd/boost_debug.h"
#endif

#include "ardour/revision.h"
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/session_utils.h"
#include "ardour/filesystem_paths.h"

#include <gtkmm/main.h>
#include <gtkmm2ext/application.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/utils.h>

#include "ardour_ui.h"
#include "ui_config.h"
#include "opts.h"
#include "enums.h"
#include "bundle_env.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#include <fcntl.h> // Needed for '_fmode'
#include <shellapi.h> // console
#endif

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

using namespace std;
using namespace Gtk;
using namespace ARDOUR_COMMAND_LINE;
using namespace ARDOUR;
using namespace PBD;

TextReceiver text_receiver ("ardour");

extern int curvetest (string);

static ARDOUR_UI  *ui = 0;
static string localedir (LOCALEDIR);

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

#ifndef NDEBUG
static void ardour_g_log (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
	switch (log_level) {
		case G_LOG_FLAG_FATAL:
		case G_LOG_LEVEL_CRITICAL:
			fatal << "g_log: " << message << endmsg;
			break;
		case G_LOG_LEVEL_ERROR:
			error << "g_log: " << message << endmsg;
			break;
		case G_LOG_LEVEL_WARNING:
			warning << "g_log: " << message << endmsg;
			break;
		case G_LOG_LEVEL_MESSAGE:
		case G_LOG_LEVEL_INFO:
		default:
			info << "g_log: " << message << endmsg;
			break;
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

#ifndef PLATFORM_WINDOWS
static void
sigpipe_handler (int /*signal*/)
{
	/* XXX fix this so that we do this again after a reconnect to the backend */

	static bool done_the_backend_thing = false;

	if (!done_the_backend_thing) {
		AudioEngine::instance()->died ();
		g_idle_add (tell_about_backend_death, 0);
		done_the_backend_thing =  true;
	}
}
#endif

#if (!defined COMPILER_MSVC && defined PLATFORM_WINDOWS)

static FILE* pStdOut = 0;
static FILE* pStdErr = 0;
static BOOL  bConsole;
static HANDLE hStdOut;

static bool
IsAConsolePort (HANDLE handle)
{
	DWORD mode;
	return (GetConsoleMode(handle, &mode) != 0);
}

static void
console_madness_begin ()
{
	bConsole = AttachConsole(ATTACH_PARENT_PROCESS);
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	/* re-attach to the console so we can see 'printf()' output etc.
	 * for MSVC see  gtk2_ardour/msvc/winmain.cc
	 */

	if ((bConsole) && (IsAConsolePort(hStdOut))) {
		pStdOut = freopen( "CONOUT$", "w", stdout );
		pStdErr = freopen( "CONOUT$", "w", stderr );
	}
}

static void
console_madness_end ()
{
	if (pStdOut) {
		fclose (pStdOut);
	}
	if (pStdErr) {
		fclose (pStdErr);
	}

	if (bConsole) {
		// Detach and free the console from our application
		INPUT_RECORD input_record;

		input_record.EventType = KEY_EVENT;
		input_record.Event.KeyEvent.bKeyDown = TRUE;
		input_record.Event.KeyEvent.dwControlKeyState = 0;
		input_record.Event.KeyEvent.uChar.UnicodeChar = VK_RETURN;
		input_record.Event.KeyEvent.wRepeatCount      = 1;
		input_record.Event.KeyEvent.wVirtualKeyCode   = VK_RETURN;
		input_record.Event.KeyEvent.wVirtualScanCode  = MapVirtualKey( VK_RETURN, 0 );

		DWORD written = 0;
		WriteConsoleInput( GetStdHandle( STD_INPUT_HANDLE ), &input_record, 1, &written );

		FreeConsole();
	}
}

static void command_line_parse_error (int *argc, char** argv[]) {}

#elif (defined(COMPILER_MSVC) && defined(NDEBUG) && !defined(RDC_BUILD))

// these are not used here. for MSVC see  gtk2_ardour/msvc/winmain.cc
static void console_madness_begin () {}
static void console_madness_end () {}

static void command_line_parse_error (int *argc, char** argv[]) {
	// Since we don't ordinarily have access to stdout and stderr with
	// an MSVC app, let the user know we encountered a parsing error.
	Gtk::Main app(argc, argv); // Calls 'gtk_init()'

	Gtk::MessageDialog dlgReportParseError (string_compose (_("\n   %1 could not understand your command line      "), PROGRAM_NAME),
			false, MESSAGE_ERROR, BUTTONS_CLOSE, true);
	dlgReportParseError.set_title (string_compose (_("An error was encountered while launching %1"), PROGRAM_NAME));
	dlgReportParseError.run ();
}

#else
static void console_madness_begin () {}
static void console_madness_end () {}
static void command_line_parse_error (int *argc, char** argv[]) {}
#endif

#if (defined(COMPILER_MSVC) && defined(NDEBUG) && !defined(RDC_BUILD))
/*
 *  Release build with MSVC uses ardour_main()
 */
int ardour_main (int argc, char *argv[])

#elif (defined WINDOWS_VST_SUPPORT && !defined PLATFORM_WINDOWS)

// prototype for function in windows_vst_plugin_ui.cc
extern int windows_vst_gui_init (int* argc, char** argv[]);

/* this is called from the entry point of a wine-compiled
   executable that is linked against gtk2_ardour built
   as a shared library.
*/
extern "C" {

int ardour_main (int argc, char *argv[])

#elif defined NOMAIN
int nomain (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif
{
	ARDOUR::check_for_old_configuration_files();

	fixup_bundle_environment (argc, argv, localedir);

	load_custom_fonts(); /* needs to happen before any gtk and pango init calls */

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

#ifdef HAVE_FFTW35F
	fftwf_make_planner_thread_safe ();
#endif

#ifdef ENABLE_NLS
	/* initialize C and C++ locales to user preference */
	char* l_msg = NULL;
	char* l_num = NULL;
	if (ARDOUR::translations_are_enabled ()) {
		setlocale (LC_ALL, "");
		try {
			std::locale::global (std::locale (setlocale (LC_ALL, 0)));
		} catch (...) {
			std::cerr << "Cannot set C++ locale\n";
		}
#ifndef COMPILER_MSVC
		// LC_MESSAGES isn't a supported locale setting when building
		// with MSVC (in fact, I doubt if it's valid for Windows at all)
		l_msg = setlocale (LC_MESSAGES, NULL);
#endif
		l_num = setlocale (LC_NUMERIC, NULL);
		if (l_msg) { l_msg = strdup (l_msg); }
		if (l_num) { l_num = strdup (l_num); }
	}
#endif

	console_madness_begin();

#if (defined WINDOWS_VST_SUPPORT && !defined PLATFORM_WINDOWS)
	/* this does some magic that is needed to make GTK and X11 client interact properly.
	 * the platform dependent code is in windows_vst_plugin_ui.cc
	 */
	windows_vst_gui_init (&argc, &argv);
#endif

#ifdef ENABLE_NLS
	cerr << "bind txt domain [" << PACKAGE << "] to " << localedir << endl;

	(void) bindtextdomain (PACKAGE, localedir.c_str());
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
		command_line_parse_error (&argc, &argv);
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
		cerr << _("Copyright (C) 1999-2015 Paul Davis") << endl
		     << _("Some portions Copyright (C) Steve Harris, Ari Johnson, Brett Viren, Joel Baker, Robin Gareus") << endl
		     << endl
		     << string_compose (_("%1 comes with ABSOLUTELY NO WARRANTY"), PROGRAM_NAME) << endl
		     <<	_("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.") << endl
		     << _("This is free software, and you are welcome to redistribute it ") << endl
		     << _("under certain conditions; see the source for copying conditions.")
		     << endl;
	}

	if (!ARDOUR::init (ARDOUR_COMMAND_LINE::use_vst, ARDOUR_COMMAND_LINE::try_hw_optimization, localedir.c_str())) {
		error << string_compose (_("could not initialize %1."), PROGRAM_NAME) << endmsg;
		exit (1);
	}

	if (curvetest_file) {
		return curvetest (curvetest_file);
	}

#ifdef ENABLE_NLS
	ARDOUR::LocaleMode locale_mode = UIConfiguration::instance().get_locale_mode ();
	if (l_msg && l_num && locale_mode != ARDOUR::SET_LC_ALL) {
		try {
			std::locale cpp_locale (std::locale::classic ());
			cpp_locale = std::locale (cpp_locale, l_msg, std::locale::messages);
			if (ARDOUR::SET_LC_MESSAGES_AND_LC_NUMERIC == locale_mode) {
				cpp_locale = std::locale (cpp_locale, l_num, std::locale::numeric);
			}
			std::locale::global (cpp_locale);
		} catch (...) {
			std::cerr << "Cannot override C++ locale\n";
		}
		info << "LC_ALL: " << setlocale (LC_ALL, NULL) << endmsg;
	}
	free (l_msg);
	free (l_num);
#endif

#ifndef PLATFORM_WINDOWS
	if (::signal (SIGPIPE, sigpipe_handler)) {
		cerr << _("Cannot xinstall SIGPIPE error handler") << endl;
	}
#endif

	DEBUG_TRACE (DEBUG::Locale, string_compose ("main() locale '%1'\n", setlocale (LC_NUMERIC, NULL)));

	if (UIConfiguration::instance().pre_gui_init ()) {
		error << _("Could not complete pre-GUI initialization") << endmsg;
		exit (1);
	}

	try {
		ui = new ARDOUR_UI (&argc, &argv, localedir.c_str());
	} catch (failed_constructor& err) {
		error << string_compose (_("could not create %1 GUI"), PROGRAM_NAME) << endmsg;
		exit (1);
	}

#ifndef NDEBUG
	g_log_set_handler (NULL,
			GLogLevelFlags (G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL |  G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_RECURSION),
			&ardour_g_log, NULL);
#endif

	ui->run (text_receiver);
	Gtkmm2ext::Application::instance()->cleanup();
	delete ui;
	ui = 0;

	ARDOUR::cleanup ();
#ifndef NDEBUG
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		Glib::usleep(100000);
		sched_yield();
	}
#endif

	pthread_cancel_all ();

#ifndef NDEBUG
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		Glib::usleep(100000);
		sched_yield();
	}
#endif

	console_madness_end ();

	return 0;
}
#if (defined WINDOWS_VST_SUPPORT && !defined PLATFORM_WINDOWS)
} // end of extern "C" block
#endif
