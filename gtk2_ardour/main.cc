/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2017 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#include <cstdlib>
#include <cerrno>
#include <vector>

#include <signal.h>
#include <locale.h>

#include <sigc++/bind.h>
#include <gtkmm/settings.h>

#include <curl/curl.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/textreceiver.h"
#include "pbd/failed_constructor.h"
#include "pbd/pathexpand.h"
#include "pbd/pthread_utils.h"
#include "pbd/win_console.h"
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
#include "pbd/boost_debug.h"
#endif

#include "ardour/revision.h"
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/session_utils.h"
#include "ardour/filesystem_paths.h"

#include <gtkmm/main.h>
#include <gtkmm/stock.h>

#include <gtkmm2ext/application.h>
#include <gtkmm2ext/utils.h>

#include "ardour_message.h"
#include "ardour_ui.h"
#include "ui_config.h"
#include "opts.h"
#include "enums.h"
#include "bundle_env.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>  // CreateMutex
#include <fcntl.h> // Needed for '_fmode'
#include <shellapi.h> // console
#endif

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

#ifdef LXVST_SUPPORT
#include <gdk/gdkx.h>
#endif

using namespace std;
using namespace Gtk;
using namespace ARDOUR_COMMAND_LINE;
using namespace ARDOUR;
using namespace PBD;

TextReceiver text_receiver (PROGRAM_NAME);

extern int curvetest (string);

static ARDOUR_UI  *ui = 0;
static string localedir (LOCALEDIR);

void
gui_jack_error ()
{
	ArdourMessageDialog win (string_compose (_("%1 could not connect to the audio backend."), PROGRAM_NAME),
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

	g_log_default_handler (log_domain, log_level, message, NULL);

	switch (log_level) {
		case G_LOG_FLAG_FATAL:
			fatal << "g_log: " << message << endmsg;
			break;
		case G_LOG_LEVEL_CRITICAL:
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
	if (AudioEngine::instance()->processed_samples() == 0) {
		/* died during startup */
		ArdourMessageDialog msg (string_compose (_("The audio backend (%1) has failed, or terminated"), AudioEngine::instance()->current_backend_name()), false);
		msg.set_position (Gtk::WIN_POS_CENTER);
		msg.set_secondary_text (string_compose (_(
"%2 exited unexpectedly, and without notifying %1.\n\
\n\
This could be due to misconfiguration or to an error inside %2.\n\
\n\
Click OK to exit %1."), PROGRAM_NAME, AudioEngine::instance()->current_backend_name()));

		msg.run ();
		_exit (EXIT_SUCCESS);

	} else {

		/* engine has already run, so this is a mid-session backend death */

		ArdourMessageDialog msg (string_compose (_("The audio backend (%1) has failed, or terminated"), AudioEngine::instance()->current_backend_name()), false);
		msg.set_secondary_text (string_compose (_("%2 exited unexpectedly, and without notifying %1."),
							 PROGRAM_NAME, AudioEngine::instance()->current_backend_name()));
		msg.run ();
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

static void command_line_parse_error (int *argc, char** argv[]) {}

#elif (defined(COMPILER_MSVC) && defined(NDEBUG) && !defined(RDC_BUILD))

static void command_line_parse_error (int *argc, char** argv[]) {
	// Since we don't ordinarily have access to stdout and stderr with
	// an MSVC app, let the user know we encountered a parsing error.
	Gtk::Main app(argc, argv); // Calls 'gtk_init()'

	ArdourMessageDialog dlgReportParseError (string_compose (_("\n   %1 could not understand your command line      "), PROGRAM_NAME),
			false, MESSAGE_ERROR, BUTTONS_CLOSE, true);
	dlgReportParseError.set_title (string_compose (_("An error was encountered while launching %1"), PROGRAM_NAME));
	dlgReportParseError.run ();
}

#else
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
	console_madness_begin();

	ARDOUR::check_for_old_configuration_files();

	/* global init is not thread safe.*/
	if (curl_global_init (CURL_GLOBAL_DEFAULT)) {
		cerr << "curl_global_init() failed. The web is gone. We're all doomed." << endl;
	}

	fixup_bundle_environment (argc, argv, localedir);

	load_custom_fonts(); /* needs to happen before any gtk and pango init calls */

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

#ifdef LXVST_SUPPORT
	XInitThreads ();
#endif

#if ENABLE_NLS
	/* initialize C locale to user preference */
	if (ARDOUR::translations_are_enabled ()) {
		if (!setlocale (LC_ALL, "")) {
			std::cerr << "localization call failed, " << PROGRAM_NAME << " will not be translated\n";
		}
	}
#endif

#if (defined WINDOWS_VST_SUPPORT && !defined PLATFORM_WINDOWS)
	/* this does some magic that is needed to make GTK and X11 client interact properly.
	 * the platform dependent code is in windows_vst_plugin_ui.cc
	 */
	windows_vst_gui_init (&argc, &argv);
#endif

#if ENABLE_NLS

#ifndef NDEBUG
	cerr << "bind txt domain [" << PACKAGE << "] to " << localedir << endl;
#endif

	(void) bindtextdomain (PACKAGE, localedir.c_str());
	/* our i18n translations are all in UTF-8, so make sure
	   that even if the user locale doesn't specify UTF-8,
	   we use that when handling them.
	*/
	(void) bind_textdomain_codeset (PACKAGE,"UTF-8");
#endif

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// catch error message system signals ();

	text_receiver.listen_to (debug);
	text_receiver.listen_to (info);
	text_receiver.listen_to (warning);
	text_receiver.listen_to (error);
	text_receiver.listen_to (fatal);

#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
	if (g_getenv ("BOOST_DEBUG")) {
		boost_debug_shared_ptr_show_live_debugging (true);
	}
#endif

	if (parse_opts (argc, argv)) {
		command_line_parse_error (&argc, &argv);
		exit (EXIT_FAILURE);
	}

	{
#ifndef NDEBUG
		const char *adf;
		if ((adf = g_getenv ("ARDOUR_DEBUG_FLAGS"))) {
			PBD::parse_debug_options (adf);
		}
#endif /* NDEBUG */
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
		exit (EXIT_SUCCESS);
	}

	if (no_splash) {
		cout << _("Copyright (C) 1999-2022 Paul Davis") << endl
		     << _("Some portions Copyright (C) Steve Harris, Ari Johnson, Brett Viren, Joel Baker, Robin Gareus") << endl
		     << endl
		     << string_compose (_("%1 comes with ABSOLUTELY NO WARRANTY"), PROGRAM_NAME) << endl
		     << _("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.") << endl
		     << _("This is free software, and you are welcome to redistribute it ") << endl
		     << _("under certain conditions; see the source for copying conditions.")
		     << endl;
	}

#ifdef PLATFORM_WINDOWS
	CreateMutexA (0, 1, string_compose ("%1%2", PROGRAM_NAME, PROGRAM_VERSION).c_str ());
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		Gtk::Main main (argc, argv);
		Gtk::MessageDialog msg (string_compose (_("%1 is already running."), PROGRAM_NAME),
				false, Gtk::MESSAGE_ERROR , Gtk::BUTTONS_OK, true);
		msg.run ();
		exit (EXIT_FAILURE);
	}
#endif

#ifdef HAVE_DRMINGW
	/* prevent missing libs popups */
	UINT prev_error_mode = SetErrorMode (SEM_FAILCRITICALERRORS);
	SetErrorMode (prev_error_mode | SEM_FAILCRITICALERRORS);
	HMODULE exchndl = LoadLibraryA ("exchndl.dll");

	if (exchndl) {
		/* %localappdata%\Ardour<X>\CrashLog\ */
		string crash_dir = Glib::build_filename (Glib::get_user_data_dir (), string_compose ("%1%2", PROGRAM_NAME, PROGRAM_VERSION), "CrashLog");
		g_mkdir_with_parents (crash_dir.c_str(), 0700);

		Glib::DateTime tm (g_date_time_new_now_local ());
		string crash_file = string_compose ("%1-%2-crash-%3.txt", PROGRAM_NAME, VERSIONSTRING, tm.format ("%s"));
		string crash_path = Glib::build_filename (crash_dir, crash_file);

		typedef void (*exc_init_fn_t) (void);
		typedef bool (*exc_path_fn_t) (const char *);

		exc_init_fn_t exchndl_init = (exc_init_fn_t) GetProcAddress (exchndl, "ExcHndlInit");
		exc_path_fn_t exchndl_path = (exc_path_fn_t) GetProcAddress (exchndl, "ExcHndlSetLogFileNameA");

		if (exchndl_init && exchndl_path) {
			exchndl_init ();
			exchndl_path (crash_path.c_str());
			cout << "Crash Log: " << crash_path << endl;
		} else {
			cout << "Cannot initialize crash reporter" << endl;
		}
	} else {
		cout << "Crash reporter is not compatible with this system" << endl;
	}
	SetErrorMode (prev_error_mode);
#endif

	if (!ARDOUR::init (ARDOUR_COMMAND_LINE::try_hw_optimization, localedir.c_str(), true)) {
		error << string_compose (_("could not initialize %1."), PROGRAM_NAME) << endmsg;
		Gtk::Main main (argc, argv);
		Gtk::MessageDialog msg (string_compose (_("Could not initialize %1 (likely due to corrupt config files).\n"
		                                          "Run %1 from a commandline for more information."), PROGRAM_NAME),
		                        false, Gtk::MESSAGE_ERROR , Gtk::BUTTONS_OK, true);
		msg.run ();
		exit (EXIT_FAILURE);
	}

#ifndef PLATFORM_WINDOWS
	if (::signal (SIGPIPE, sigpipe_handler)) {
		cerr << _("Cannot xinstall SIGPIPE error handler") << endl;
	}
#endif

	DEBUG_TRACE (DEBUG::Locale, string_compose ("main() locale '%1'\n", setlocale (LC_NUMERIC, NULL)));

	if (UIConfiguration::instance().pre_gui_init ()) {
		error << _("Could not complete pre-GUI initialization") << endmsg;
		exit (EXIT_FAILURE);
	}

	try {
		ui = new ARDOUR_UI (&argc, &argv, localedir.c_str());
	} catch (failed_constructor& err) {
		error << string_compose (_("could not create %1 GUI"), PROGRAM_NAME) << endmsg;
		exit (EXIT_FAILURE);
	}

#ifndef NDEBUG
	g_log_set_default_handler (&ardour_g_log, NULL);
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
