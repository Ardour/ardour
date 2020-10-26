/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#ifndef PLATFORM_WINDOWS
#include <sys/resource.h>
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <gtkmm/stock.h>

#include "pbd/basename.h"
#include "pbd/file_utils.h"

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"

#include "gtkmm2ext/application.h"

#include "ambiguous_file_dialog.h"
#include "ardour_message.h"
#include "ardour_ui.h"
#include "debug.h"
#include "engine_dialog.h"
#include "keyboard.h"
#include "missing_file_dialog.h"
#include "nsm.h"
#include "opts.h"
#include "pingback.h"
#include "plugin_scan_dialog.h"
#include "public_editor.h"
#include "splash.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;


static bool
_hide_splash (gpointer arg)
{
	((ARDOUR_UI*)arg)->hide_splash();
	return false;
}

bool
ARDOUR_UI::first_idle ()
{
	if (_session) {
		_session->reset_xrun_count ();
		_session->allow_auto_play (true);
	}

	if (editor) {
		editor->first_idle();
	}

	/* in 1 second, hide the splash screen */
	Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (_hide_splash), this), 1000);

	Keyboard::set_can_save_keybindings (true);
	return false;
}

void
ARDOUR_UI::setup_profile ()
{
	if (gdk_screen_width() < 1200 || getenv ("ARDOUR_NARROW_SCREEN")) {
		Profile->set_small_screen ();
	}

	if (g_getenv ("MIXBUS")) {
		Profile->set_mixbus ();
	}
}

int
ARDOUR_UI::missing_file (Session*s, std::string str, DataType type)
{
	MissingFileDialog dialog (s, str, type);

	dialog.show ();
	dialog.present ();

	int result = dialog.run ();
	dialog.hide ();

	switch (result) {
	case RESPONSE_OK:
		break;
	default:
		return 1; // quit entire session load
	}

	result = dialog.get_action ();

	return result;
}

int
ARDOUR_UI::ambiguous_file (std::string file, std::vector<std::string> hits)
{
	AmbiguousFileDialog dialog (file, hits);

	dialog.show ();
	dialog.present ();

	dialog.run ();

	return dialog.get_which ();
}

void
ARDOUR_UI::session_format_mismatch (std::string xml_path, std::string backup_path)
{
	const char* start_big = "<span size=\"x-large\" weight=\"bold\">";
	const char* end_big = "</span>";
	const char* start_mono = "<tt>";
	const char* end_mono = "</tt>";

	ArdourMessageDialog msg (string_compose (_("%4This is a session from an older version of %3%5\n\n"
	                                           "%3 has copied the old session file\n\n%6%1%7\n\nto\n\n%6%2%7\n\n"
	                                           "From now on, use the backup copy with older versions of %3"),
	                                         xml_path, backup_path, PROGRAM_NAME,
	                                         start_big, end_big,
	                                         start_mono, end_mono), true);

	msg.run ();
}


int
ARDOUR_UI::sr_mismatch_dialog (samplecnt_t desired, samplecnt_t actual)
{
	HBox* hbox = new HBox();
	Image* image = new Image (Stock::DIALOG_WARNING, ICON_SIZE_DIALOG);
	ArdourDialog dialog (_("Sample Rate Mismatch"), true);
	Label  message (string_compose (_("\
This session was created with a sample rate of %1 Hz, but\n\
%2 is currently running at %3 Hz.  If you load this session,\n\
audio may be played at the wrong sample rate.\n"), desired, PROGRAM_NAME, actual));

	image->set_alignment(ALIGN_CENTER, ALIGN_TOP);
	hbox->pack_start (*image, PACK_EXPAND_WIDGET, 12);
	hbox->pack_end (message, PACK_EXPAND_PADDING, 12);
	dialog.get_vbox()->pack_start(*hbox, PACK_EXPAND_PADDING, 6);
	dialog.add_button (_("Do not load session"), RESPONSE_REJECT);
	dialog.add_button (_("Load session anyway"), RESPONSE_ACCEPT);
	dialog.set_default_response (RESPONSE_ACCEPT);
	dialog.set_position (WIN_POS_CENTER);
	message.show();
	image->show();
	hbox->show();

	switch (dialog.run()) {
	case RESPONSE_ACCEPT:
		return 0;
	default:
		break;
	}

	return 1;
}

void
ARDOUR_UI::sr_mismatch_message (samplecnt_t desired, samplecnt_t actual)
{
	ArdourMessageDialog msg (string_compose (_("\
This session was created with a sample rate of %1 Hz, but\n\
%2 is currently running at %3 Hz.\n\
Audio will be recorded and played at the wrong sample rate.\n\
Re-Configure the Audio Engine in\n\
Menu > Window > Audio/Midi Setup"),
				desired, PROGRAM_NAME, actual),
			true,
			Gtk::MESSAGE_WARNING);
	msg.run ();
}


XMLNode*
ARDOUR_UI::preferences_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Preferences"));
	} else {
		node = Config->instant_xml(X_("Preferences"));
	}

	if (!node) {
		node = new XMLNode (X_("Preferences"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::mixer_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Mixer"));
	} else {
		node = Config->instant_xml(X_("Mixer"));
	}

	if (!node) {
		node = new XMLNode (X_("Mixer"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::main_window_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Main"));
	} else {
		node = Config->instant_xml(X_("Main"));
	}

	if (!node) {
		if (getenv("ARDOUR_INSTANT_XML_PATH")) {
			node = Config->instant_xml(getenv("ARDOUR_INSTANT_XML_PATH"));
		}
	}

	if (!node) {
		node = new XMLNode (X_("Main"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::editor_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Editor"));
	} else {
		node = Config->instant_xml(X_("Editor"));
	}

	if (!node) {
		if (getenv("ARDOUR_INSTANT_XML_PATH")) {
			node = Config->instant_xml(getenv("ARDOUR_INSTANT_XML_PATH"));
		}
	}

	if (!node) {
		node = new XMLNode (X_("Editor"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::recorder_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Recorder"));
	} else {
		node = Config->instant_xml(X_("Recorder"));
	}

	if (!node) {
		node = new XMLNode (X_("Recorder"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::keyboard_settings () const
{
	XMLNode* node = 0;

	node = Config->extra_xml(X_("Keyboard"));

	if (!node) {
		node = new XMLNode (X_("Keyboard"));
	}

	return node;
}

void
ARDOUR_UI::hide_splash ()
{
	Splash::drop ();
}

void
ARDOUR_UI::check_announcements ()
{
#ifdef PHONE_HOME
	string _annc_filename;

#ifdef __APPLE__
	_annc_filename = PROGRAM_NAME "_announcements_osx_";
#elif defined PLATFORM_WINDOWS
	_annc_filename = PROGRAM_NAME "_announcements_windows_";
#else
	_annc_filename = PROGRAM_NAME "_announcements_linux_";
#endif
	_annc_filename.append (VERSIONSTRING);

	_announce_string = "";

	std::string path = Glib::build_filename (user_config_directory(), _annc_filename);
	FILE* fin = g_fopen (path.c_str(), "rb");
	if (fin) {
		while (!feof (fin)) {
			char tmp[1024];
			size_t len;
			if ((len = fread (tmp, sizeof(char), 1024, fin)) == 0 || ferror (fin)) {
				break;
			}
			_announce_string.append (tmp, len);
		}
		fclose (fin);
	}

	pingback (VERSIONSTRING, path);
#endif
}

int
ARDOUR_UI::nsm_init ()
{
	const char *nsm_url;

	if ((nsm_url = g_getenv ("NSM_URL")) == 0) {
		return 0;
	}

	nsm = new NSM_Client;

	if (nsm->init (nsm_url)) {
		delete nsm;
		nsm = 0;
		error << _("NSM: initialization failed") << endmsg;
		return -1;
	}

	/* the ardour executable may have different names:
	 *
	 * waf's obj.target for distro versions: eg ardour4, ardourvst4
	 * Ardour4, Mixbus3 for bundled versions + full path on OSX & windows
	 * argv[0] does not apply since we need the wrapper-script (not the binary itself)
	 *
	 * The wrapper startup script should set the environment variable 'ARDOUR_SELF'
	 */
	const char *process_name = g_getenv ("ARDOUR_SELF");
	nsm->announce (PROGRAM_NAME, ":dirty:", process_name ? process_name : "ardour6");

	unsigned int i = 0;
	// wait for announce reply from nsm server
	for ( i = 0; i < 5000; ++i) {
		nsm->check ();

		Glib::usleep (i);
		if (nsm->is_active()) {
			break;
		}
	}
	if (i == 5000) {
		error << _("NSM server did not announce itself. Continuing without NSM.") << endmsg;
		delete nsm;
		nsm = 0;
		return 0;
	}

	/* wait for open command from nsm server */
	for (i = 0; i < 5000; ++i) {
		nsm->check ();
		Glib::usleep (1000);
		if (nsm->client_id ()) {
			break;
		}
	}

	if (i == 5000) {
		error << _("NSM: no client ID provided") << endmsg;
		delete nsm;
		nsm = 0;
		return -1;
	}

	if (_session && nsm) {
		_session->set_nsm_state( nsm->is_active() );
	} else {
		error << _("NSM: no session created") << endmsg;
		delete nsm;
		nsm = 0;
		return -1;
	}

	// nsm requires these actions disabled
	vector<string> action_names;
	action_names.push_back("SaveAs");
	action_names.push_back("Rename");
	action_names.push_back("New");
	action_names.push_back("Open");
	action_names.push_back("Recent");
	action_names.push_back("Close");

	for (vector<string>::const_iterator n = action_names.begin(); n != action_names.end(); ++n) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), (*n).c_str());
		if (act) {
			act->set_sensitive (false);
		}
	}

	return 0;
}

void
ARDOUR_UI::sfsm_response (StartupFSM::Result r)
{
	DEBUG_TRACE (DEBUG::GuiStartup, string_compose (X_("startup FSM response %1\n"), r));

	switch (r) {
	case StartupFSM::ExitProgram:
		queue_finish ();
		break;

	case StartupFSM::LoadSession:

		if (load_session_from_startup_fsm () == 0) {
			startup_done ();
			delete startup_fsm;
			startup_fsm = 0;
		} else {
			DEBUG_TRACE (DEBUG::GuiStartup, "FSM reset\n");
			startup_fsm->reset ();
		}

		break;
	}
}

int
ARDOUR_UI::starting ()
{
	Application* app = Application::instance();

	app->ShouldLoad.connect (sigc::mem_fun (*this, &ARDOUR_UI::load_from_application_api));

	if (ARDOUR_COMMAND_LINE::check_announcements) {
		check_announcements ();
	}

	app->ready ();

	/* we need to create this early because it may need to set the
	 *  audio backend end up.
	 */

	EngineControl* amd;

	try {
		amd = dynamic_cast<EngineControl*> (audio_midi_setup.get (true));
	} catch (...) {
		std::cerr << "audio-midi engine setup failed."<< std::endl;
		return -1;
	}

	if (nsm_init ()) {
		return -1;
	} else  {

		if (nsm) {
			return 0;
		}

		startup_fsm = new StartupFSM (*amd);
		startup_fsm->signal_response().connect (sigc::mem_fun (*this, &ARDOUR_UI::sfsm_response));


		/* allow signals to be handled, ShouldLoad() from flush-pending */
		Splash::instance()->pop_front();
		flush_pending ();

		if (!startup_fsm) {
			DEBUG_TRACE (DEBUG::GuiStartup, "Starting: SFSM was driven by flush-pending\n");
			return 0;
		}

		/* Note: entire startup process could happen in this one call
		 * if:
		 *
		 * 1) not a new user
		 * 2) session name provided on command line (and valid)
		 * 3) no audio/MIDI setup required
		 */

		startup_fsm->start ();
	}

	return 0;
}

int
ARDOUR_UI::load_session_from_startup_fsm ()
{
	const string session_path = startup_fsm->session_path;
	const string session_name = startup_fsm->session_name;
	const string session_template = startup_fsm->session_template;
	const bool   session_is_new = startup_fsm->session_is_new;
	const BusProfile bus_profile = startup_fsm->bus_profile;
	const bool   session_was_not_named = (!startup_fsm->session_name_edited && ARDOUR_COMMAND_LINE::session_name.empty());

	std::cout  << " loading from " << session_path << " as " << session_name << " templ " << session_template << " is_new " << session_is_new << " bp " << bus_profile.master_out_channels << std::endl;

	if (session_is_new) {

		if (build_session (session_path, session_name, session_template, bus_profile, true, session_was_not_named)) {
			return -1;
		}
		return 0;
	}

	return load_session (session_path, session_name, session_template);

}

void
ARDOUR_UI::startup_done ()
{
	/* ShouldQuit is a desktop environment mechanism that tells the
	   application it should exit for reasons external to the application
	   itself.

	   During startup, startupFSM handles ShouldQuit. But it is done now,
	   and we have to take over responsibility.
	*/
	Application::instance()->ShouldQuit.connect (sigc::mem_fun (*this, &ARDOUR_UI::queue_finish));
	/* Same story applies for ShouldLoad ... startupFSM will handle it
	   normally, but if it doesn't we (ARDOUR_UI) need to take responsibility
	   for it.
	*/
	Application::instance()->ShouldLoad.connect (sigc::mem_fun (*this, &ARDOUR_UI::load_from_application_api));

	use_config ();

	WM::Manager::instance().show_visible ();

	/* We have to do this here since goto_editor_window() ends up calling show_all() on the
	 * editor window, and we may want stuff to be hidden.
	 */
	_status_bar_visibility.update ();

	BootMessage (string_compose (_("%1 is ready for use"), PROGRAM_NAME));
}

void
ARDOUR_UI::use_config ()
{
	XMLNode* node = Config->extra_xml (X_("TransportControllables"));
	if (node) {
		set_transport_controllable_state (*node);
	}
}

void
ARDOUR_UI::check_memory_locking ()
{
#if defined(__APPLE__) || defined(PLATFORM_WINDOWS)
	/* OS X doesn't support mlockall(2), and so testing for memory locking capability there is pointless */
	return;
#else // !__APPLE__

	XMLNode* memory_warning_node = Config->instant_xml (X_("no-memory-warning"));

	if (AudioEngine::instance()->is_realtime() && memory_warning_node == 0) {

		struct rlimit limits;
		int64_t ram;
		long pages, page_size;
#ifdef __FreeBSD__
		size_t pages_len=sizeof(pages);
		if ((page_size = getpagesize()) < 0 ||
				sysctlbyname("hw.availpages", &pages, &pages_len, NULL, 0))
#else
		if ((page_size = sysconf (_SC_PAGESIZE)) < 0 ||(pages = sysconf (_SC_PHYS_PAGES)) < 0)
#endif
		{
			ram = 0;
		} else {
			ram = (int64_t) pages * (int64_t) page_size;
		}

		if (getrlimit (RLIMIT_MEMLOCK, &limits)) {
			return;
		}

		if (limits.rlim_cur != RLIM_INFINITY) {

			if (ram == 0 || ((double) limits.rlim_cur / ram) < 0.75) {

				ArdourMessageDialog msg (
					string_compose (
						_("WARNING: Your system has a limit for maximum amount of locked memory. "
						  "This might cause %1 to run out of memory before your system "
						  "runs out of memory. \n\n"
						  "You can view the memory limit with 'ulimit -l', "
						  "and it is normally controlled by %2"),
						PROGRAM_NAME,
#if defined(__FreeBSD__) || defined(__NetBSD__)
						X_("/etc/login.conf")
#else
						X_(" /etc/security/limits.conf")
#endif
					).c_str());

				msg.set_default_response (RESPONSE_OK);

				VBox* vbox = msg.get_vbox();
				HBox hbox;
				CheckButton cb (_("Do not show this window again"));
				hbox.pack_start (cb, true, false);
				vbox->pack_start (hbox);
				cb.show();
				vbox->show();
				hbox.show ();

				msg.run ();

				if (cb.get_active()) {
					XMLNode node (X_("no-memory-warning"));
					Config->add_instant_xml (node);
				}
			}
		}
	}
#endif // !__APPLE__
}

void
ARDOUR_UI::load_from_application_api (const std::string& path)
{
	/* OS X El Capitan (and probably later) now somehow passes the command
	   line arguments to an app via the openFile delegate protocol. Ardour
	   already does its own command line processing, and having both
	   pathways active causes crashes. So, if the command line was already
	   set, do nothing here. NSM also uses this code path.
	*/

	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {
		return;
	}


	/* Cancel SessionDialog if it's visible to make OSX delegates work.
	 *
	 * ARDOUR_UI::starting connects app->ShouldLoad signal and then shows a SessionDialog
	 * race-condition:
	 *  - ShouldLoad does not arrive in time, ARDOUR_COMMAND_LINE::session_name is empty:
	 *    -> startupFSM starts a SessionDialog.
	 *  - ShouldLoad signal arrives, this function is called and sets ARDOUR_COMMAND_LINE::session_name
	 *    -> SessionDialog is not displayed
	 */

	if (startup_fsm) {
		/* this will result in the StartupFSM signalling us to load a
		 * session, which if successful will then destroy the
		 * startupFSM and we'll move right along.
		 */

		startup_fsm->handle_path (path);
		return;
	}

	if (nsm) {
		if (!AudioEngine::instance()->set_backend("JACK", "", "")) {
			error << _("NSM: The JACK backend is mandatory and can not be loaded.") << endmsg;
			return;
		}

		if (!AudioEngine::instance()->running()) {
			/* this auto-starts jackd with recent settings
			 * TODO: if Glib::file_test (path, Glib::FILE_TEST_IS_DIR)
			 *  query sample-rate of the session and use it.
			 */
			if (Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
				float sr;
				SampleFormat sf;
				string pv;
				if (0 == Session::get_info_from_path (Glib::build_filename (path, basename_nosuffix (path) + statefile_suffix), sr, sf, pv)) {
					ARDOUR::AudioEngine::instance ()->set_sample_rate (sr);
				}
			}
			if (AudioEngine::instance()->start()) {
				error << string_compose (_("NSM: %1 cannot connect to the JACK server. Please start jackd first."), PROGRAM_NAME) << endmsg;
				return;
			}
		}

		PluginScanDialog psd (true, false);
		psd.start ();

		post_engine ();
	}

	/* the mechanisms that can result is this being called are only
	 * possible for existing sessions.
	 */

	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		if (nsm) {
			BusProfile bus_profile;
			bus_profile.master_out_channels = 2;
			build_session (path, basename_nosuffix (path), "", bus_profile, true, false);
		}
		return;
	}

	ARDOUR_COMMAND_LINE::session_name = path;

	int rv;

	if (Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
		/* /path/to/foo => /path/to/foo, foo */
		rv = load_session (path, basename_nosuffix (path));
	} else {
		/* /path/to/foo/foo.ardour => /path/to/foo, foo */
		rv = load_session (Glib::path_get_dirname (path), basename_nosuffix (path));
	}

	// there was no startupFSM, load_session fails, and there is no existing session ....

	if (rv && !_session) {

		ARDOUR_COMMAND_LINE::session_name = string();

		/* do this again */

		EngineControl* amd;

		try {
			amd = dynamic_cast<EngineControl*> (audio_midi_setup.get (true));
		} catch (...) {
			std::cerr << "audio-midi engine setup failed."<< std::endl;
			return;
		}

		startup_fsm = new StartupFSM (*amd);
		startup_fsm->signal_response().connect (sigc::mem_fun (*this, &ARDOUR_UI::sfsm_response));

		/* Note: entire startup process could happen in this one call
		 * if:
		 *
		 * 1) not a new user
		 * 2) session name provided on command line (and valid)
		 * 3) no audio/MIDI setup required
		 */

		Splash::instance()->pop_front();
		startup_fsm->start ();
	}
}
