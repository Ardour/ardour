/*
    Copyright (C) 1999-2013 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cerrno>
#include <fstream>

#ifndef WIN32
#include <sys/resource.h>
#endif

#include <stdint.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gtkmm/messagedialog.h>
#include <gtkmm/accelmap.h>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/openuri.h"
#include "pbd/file_utils.h"
#include "pbd/localtime_r.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/fastmeter.h"
#include "gtkmm2ext/popup.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/ardour.h"
#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/automation_watch.h"
#include "ardour/diskstream.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/recent_sessions.h"
#include "ardour/session_directory.h"
#include "ardour/session_route.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_utils.h"
#include "ardour/slave.h"

#include "timecode/time.h"

typedef uint64_t microseconds_t;

#include "about.h"
#include "actions.h"
#include "add_route_dialog.h"
#include "ambiguous_file_dialog.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "engine_dialog.h"
#include "gain_meter.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "main_clock.h"
#include "missing_file_dialog.h"
#include "missing_plugin_dialog.h"
#include "mixer_ui.h"
#include "mouse_cursors.h"
#include "nsm.h"
#include "opts.h"
#include "pingback.h"
#include "processor_box.h"
#include "prompter.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "route_time_axis.h"
#include "route_params_ui.h"
#include "session_dialog.h"
#include "session_metadata_dialog.h"
#include "session_option_editor.h"
#include "shuttle_control.h"
#include "speaker_dialog.h"
#include "splash.h"
#include "startup.h"
#include "theme_manager.h"
#include "time_axis_view_item.h"
#include "utils.h"
#include "video_server_dialog.h"
#include "add_video_dialog.h"
#include "transcode_video_dialog.h"
#include "system_exec.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

ARDOUR_UI *ARDOUR_UI::theArdourUI = 0;
UIConfiguration *ARDOUR_UI::ui_config = 0;

sigc::signal<void,bool> ARDOUR_UI::Blink;
sigc::signal<void>      ARDOUR_UI::RapidScreenUpdate;
sigc::signal<void>      ARDOUR_UI::SuperRapidScreenUpdate;
sigc::signal<void, framepos_t, bool, framepos_t> ARDOUR_UI::Clock;
sigc::signal<void>      ARDOUR_UI::CloseAllDialogs;

ARDOUR_UI::ARDOUR_UI (int *argcp, char **argvp[], const char* localedir)

	: Gtkmm2ext::UI (PROGRAM_NAME, argcp, argvp)
	
	, gui_object_state (new GUIObjectState)

	, primary_clock (new MainClock (X_("primary"), false, X_("transport"), true, true, true, false, true))
	, secondary_clock (new MainClock (X_("secondary"), false, X_("secondary"), true, true, false, false, true))

	  /* big clock */

	, big_clock (new AudioClock (X_("bigclock"), false, "big", true, true, false, false))
	, video_timeline(0)

	  /* start of private members */

	, nsm (0)
	, _was_dirty (false)
	, _mixer_on_top (false)
	, first_time_engine_run (true)

	  /* transport */

	, roll_controllable (new TransportControllable ("transport roll", *this, TransportControllable::Roll))
	, stop_controllable (new TransportControllable ("transport stop", *this, TransportControllable::Stop))
	, goto_start_controllable (new TransportControllable ("transport goto start", *this, TransportControllable::GotoStart))
	, goto_end_controllable (new TransportControllable ("transport goto end", *this, TransportControllable::GotoEnd))
	, auto_loop_controllable (new TransportControllable ("transport auto loop", *this, TransportControllable::AutoLoop))
	, play_selection_controllable (new TransportControllable ("transport play selection", *this, TransportControllable::PlaySelection))
	, rec_controllable (new TransportControllable ("transport rec-enable", *this, TransportControllable::RecordEnable))

	, auto_return_button (ArdourButton::led_default_elements)
	, follow_edits_button (ArdourButton::led_default_elements)
	, auto_input_button (ArdourButton::led_default_elements)

	, auditioning_alert_button (_("audition"))
	, solo_alert_button (_("solo"))
	, feedback_alert_button (_("feedback"))

	, editor_meter(0)
	, editor_meter_peak_display()

	, speaker_config_window (X_("speaker-config"), _("Speaker Configuration"))
	, theme_manager (X_("theme-manager"), _("Theme Manager"))
	, key_editor (X_("key-editor"), _("Key Bindings"))
	, rc_option_editor (X_("rc-options-editor"), _("Preferences"))
	, add_route_dialog (X_("add-routes"), _("Add Tracks/Busses"))
	, about (X_("about"), _("About"))
	, location_ui (X_("locations"), _("Locations"))
	, route_params (X_("inspector"), _("Tracks and Busses"))
	, audio_midi_setup (X_("audio-midi-setup"), _("Audio/MIDI Setup"))
	, session_option_editor (X_("session-options-editor"), _("Properties"), boost::bind (&ARDOUR_UI::create_session_option_editor, this))
	, add_video_dialog (X_("add-video"), _("Add Tracks/Busses"), boost::bind (&ARDOUR_UI::create_add_video_dialog, this))
	, bundle_manager (X_("bundle-manager"), _("Bundle Manager"), boost::bind (&ARDOUR_UI::create_bundle_manager, this))
	, big_clock_window (X_("big-clock"), _("Big Clock"), boost::bind (&ARDOUR_UI::create_big_clock_window, this))
	, audio_port_matrix (X_("audio-connection-manager"), _("Audio Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::AUDIO))
	, midi_port_matrix (X_("midi-connection-manager"), _("MIDI Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::MIDI))

	, error_log_button (_("Errors"))

	, _status_bar_visibility (X_("status-bar"))
	, _feedback_exists (false)
{
	Gtkmm2ext::init(localedir);

	splash = 0;

	if (theArdourUI == 0) {
		theArdourUI = this;
	}

	ui_config = new UIConfiguration();

	editor = 0;
	mixer = 0;
	meterbridge = 0;
	editor = 0;
	_session_is_new = false;
	session_selector_window = 0;
	last_key_press_time = 0;
	video_server_process = 0;
	open_session_selector = 0;
	have_configure_timeout = false;
	have_disk_speed_dialog_displayed = false;
	session_loaded = false;
	ignore_dual_punch = false;

	roll_button.set_controllable (roll_controllable);
	stop_button.set_controllable (stop_controllable);
	goto_start_button.set_controllable (goto_start_controllable);
	goto_end_button.set_controllable (goto_end_controllable);
	auto_loop_button.set_controllable (auto_loop_controllable);
	play_selection_button.set_controllable (play_selection_controllable);
	rec_button.set_controllable (rec_controllable);

	roll_button.set_name ("transport button");
	stop_button.set_name ("transport button");
	goto_start_button.set_name ("transport button");
	goto_end_button.set_name ("transport button");
	auto_loop_button.set_name ("transport button");
	play_selection_button.set_name ("transport button");
	rec_button.set_name ("transport recenable button");
	midi_panic_button.set_name ("transport button");

	goto_start_button.set_tweaks (ArdourButton::ShowClick);
	goto_end_button.set_tweaks (ArdourButton::ShowClick);
	midi_panic_button.set_tweaks (ArdourButton::ShowClick);
	
	last_configure_time= 0;
	last_peak_grab = 0;

	ARDOUR::Diskstream::DiskOverrun.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::disk_overrun_handler, this), gui_context());
	ARDOUR::Diskstream::DiskUnderrun.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::disk_underrun_handler, this), gui_context());

	ARDOUR::Session::VersionMismatch.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_format_mismatch, this, _1, _2), gui_context());

	/* handle dialog requests */

	ARDOUR::Session::Dialog.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_dialog, this, _1), gui_context());

	/* handle pending state with a dialog (PROBLEM: needs to return a value and thus cannot be x-thread) */

	ARDOUR::Session::AskAboutPendingState.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::pending_state_dialog, this));

	/* handle Audio/MIDI setup when session requires it */

	ARDOUR::Session::AudioEngineSetupRequired.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::do_audio_midi_setup, this, _1));

	/* handle sr mismatch with a dialog (PROBLEM: needs to return a value and thus cannot be x-thread) */

	ARDOUR::Session::AskAboutSampleRateMismatch.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::sr_mismatch_dialog, this, _1, _2));

	/* handle requests to quit (coming from JACK session) */

	ARDOUR::Session::Quit.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::finish, this), gui_context ());

	/* tell the user about feedback */

	ARDOUR::Session::FeedbackDetected.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::feedback_detected, this), gui_context ());
	ARDOUR::Session::SuccessfulGraphSort.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::successful_graph_sort, this), gui_context ());

	/* handle requests to deal with missing files */

	ARDOUR::Session::MissingFile.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::missing_file, this, _1, _2, _3));

	/* and ambiguous files */

	ARDOUR::FileSource::AmbiguousFileName.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::ambiguous_file, this, _1, _2));

	/* lets get this party started */

	setup_gtk_ardour_enums ();
	setup_profile ();

	SessionEvent::create_per_thread_pool ("GUI", 512);

	/* we like keyboards */

	keyboard = new ArdourKeyboard(*this);

	XMLNode* node = ARDOUR_UI::instance()->keyboard_settings();
	if (node) {
		keyboard->set_state (*node, Stateful::loading_state_version);
	}

	/* we don't like certain modifiers */
	Bindings::set_ignored_state (GDK_LOCK_MASK|GDK_MOD2_MASK|GDK_MOD3_MASK);

	reset_dpi();

	TimeAxisViewItem::set_constant_heights ();

        /* Set this up so that our window proxies can register actions */

	ActionManager::init ();

	/* The following must happen after ARDOUR::init() so that Config is set up */

	const XMLNode* ui_xml = Config->extra_xml (X_("UI"));

	if (ui_xml) {
		theme_manager.set_state (*ui_xml);
		key_editor.set_state (*ui_xml);
		rc_option_editor.set_state (*ui_xml);
		session_option_editor.set_state (*ui_xml);
		speaker_config_window.set_state (*ui_xml);
		about.set_state (*ui_xml);
		add_route_dialog.set_state (*ui_xml);
		add_video_dialog.set_state (*ui_xml);
		route_params.set_state (*ui_xml);
		bundle_manager.set_state (*ui_xml);
		location_ui.set_state (*ui_xml);
		big_clock_window.set_state (*ui_xml);
		audio_port_matrix.set_state (*ui_xml);
		midi_port_matrix.set_state (*ui_xml);
	}

	WM::Manager::instance().register_window (&theme_manager);
	WM::Manager::instance().register_window (&key_editor);
	WM::Manager::instance().register_window (&rc_option_editor);
	WM::Manager::instance().register_window (&session_option_editor);
	WM::Manager::instance().register_window (&speaker_config_window);
	WM::Manager::instance().register_window (&about);
	WM::Manager::instance().register_window (&add_route_dialog);
	WM::Manager::instance().register_window (&add_video_dialog);
	WM::Manager::instance().register_window (&route_params);
	WM::Manager::instance().register_window (&audio_midi_setup);
	WM::Manager::instance().register_window (&bundle_manager);
	WM::Manager::instance().register_window (&location_ui);
	WM::Manager::instance().register_window (&big_clock_window);
	WM::Manager::instance().register_window (&audio_port_matrix);
	WM::Manager::instance().register_window (&midi_port_matrix);

	/* We need to instantiate the theme manager because it loads our
	   theme files. This should really change so that its window
	   and its functionality are separate 
	*/
	
	(void) theme_manager.get (true);
	
	_process_thread = new ProcessThread ();
	_process_thread->init ();

	DPIReset.connect (sigc::mem_fun (*this, &ARDOUR_UI::resize_text_widgets));

	attach_to_engine ();
}

GlobalPortMatrixWindow*
ARDOUR_UI::create_global_port_matrix (ARDOUR::DataType type)
{
	if (!_session) {
		return 0;
	}
	return new GlobalPortMatrixWindow (_session, type);
}

void
ARDOUR_UI::attach_to_engine ()
{
	AudioEngine::instance()->Running.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_running, this), gui_context());
	ARDOUR::Port::set_connecting_blocked (ARDOUR_COMMAND_LINE::no_connect_ports);
}

void
ARDOUR_UI::engine_stopped ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::engine_stopped)
	ActionManager::set_sensitive (ActionManager::engine_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::engine_opposite_sensitive_actions, true);
}

void
ARDOUR_UI::engine_running ()
{
	if (first_time_engine_run) {
		post_engine();
		first_time_engine_run = false;
	} 
	
	update_disk_space ();
	update_cpu_load ();
	update_sample_rate (AudioEngine::instance()->sample_rate());
	update_timecode_format ();
}

void
ARDOUR_UI::engine_halted (const char* reason, bool free_reason)
{
	if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {
		/* we can't rely on the original string continuing to exist when we are called
		   again in the GUI thread, so make a copy and note that we need to
		   free it later.
		*/
		char *copy = strdup (reason);
		Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&ARDOUR_UI::engine_halted, this, copy, true));
		return;
	}

	ActionManager::set_sensitive (ActionManager::engine_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::engine_opposite_sensitive_actions, true);

	update_sample_rate (0);

	string msgstr;

	/* if the reason is a non-empty string, it means that the backend was shutdown
	   rather than just Ardour.
	*/

	if (strlen (reason)) {
		msgstr = string_compose (_("The audio backend was shutdown because:\n\n%1"), reason);
	} else {
		msgstr = string_compose (_("\
The audio backend has either been shutdown or it\n\
disconnected %1 because %1\n\
was not fast enough. Try to restart\n\
the audio backend and save the session."), PROGRAM_NAME);
	}

	MessageDialog msg (*editor, msgstr);
	pop_back_splash (msg);
	msg.set_keep_above (true);
	msg.run ();
	
	if (free_reason) {
		free (const_cast<char*> (reason));
	}
}

void
ARDOUR_UI::post_engine ()
{
	/* Things to be done once (and once ONLY) after we have a backend running in the AudioEngine
	 */

	ARDOUR::init_post_engine ();
	
	/* connect to important signals */

	AudioEngine::instance()->Stopped.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_stopped, this), gui_context());
	AudioEngine::instance()->SampleRateChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate, this, _1), gui_context());
	AudioEngine::instance()->Halted.connect_same_thread (halt_connection, boost::bind (&ARDOUR_UI::engine_halted, this, _1, false));

	_tooltips.enable();

	ActionManager::load_menus ();

	if (setup_windows ()) {
		throw failed_constructor ();
	}

	/* Do this after setup_windows (), as that's when the _status_bar_visibility is created */
	XMLNode* n = Config->extra_xml (X_("UI"));
	if (n) {
		_status_bar_visibility.set_state (*n);
	}
	
	check_memory_locking();

	/* this is the first point at which all the keybindings are available */

	if (ARDOUR_COMMAND_LINE::show_key_actions) {
		vector<string> names;
		vector<string> paths;
		vector<string> tooltips;
		vector<string> keys;
		vector<AccelKey> bindings;

		ActionManager::get_all_actions (names, paths, tooltips, keys, bindings);

		vector<string>::iterator n;
		vector<string>::iterator k;
		for (n = names.begin(), k = keys.begin(); n != names.end(); ++n, ++k) {
			cout << "Action: " << (*n) << " bound to " << (*k) << endl;
		}

		exit (0);
	}

	blink_timeout_tag = -1;

	/* this being a GUI and all, we want peakfiles */

	AudioFileSource::set_build_peakfiles (true);
	AudioFileSource::set_build_missing_peakfiles (true);

	/* set default clock modes */

	if (Profile->get_sae()) {
		primary_clock->set_mode (AudioClock::BBT);
		secondary_clock->set_mode (AudioClock::MinSec);
	}  else {
		primary_clock->set_mode (AudioClock::Timecode);
		secondary_clock->set_mode (AudioClock::BBT);
	}

	/* start the time-of-day-clock */

#ifndef GTKOSX
	/* OS X provides a nearly-always visible wallclock, so don't be stupid */
	update_wall_clock ();
	Glib::signal_timeout().connect_seconds (sigc::mem_fun(*this, &ARDOUR_UI::update_wall_clock), 1);
#endif

	Config->ParameterChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::parameter_changed, this, _1), gui_context());
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	Config->map_parameters (pc);
}

ARDOUR_UI::~ARDOUR_UI ()
{
	if (ui_config->dirty()) {
		ui_config->save_state();
	}

	delete keyboard;
	delete editor;
	delete mixer;
	delete meterbridge;

	stop_video_server();
}

void
ARDOUR_UI::pop_back_splash (Gtk::Window& win)
{
	if (Splash::instance()) {
		Splash::instance()->pop_back_for (win);
	}
}

gint
ARDOUR_UI::configure_timeout ()
{
	if (last_configure_time == 0) {
		/* no configure events yet */
		return true;
	}

	/* force a gap of 0.5 seconds since the last configure event
	 */

	if (get_microseconds() - last_configure_time < 500000) {
		return true;
	} else {
		have_configure_timeout = false;
		save_ardour_state ();
		return false;
	}
}

gboolean
ARDOUR_UI::configure_handler (GdkEventConfigure* /*conf*/)
{
	if (have_configure_timeout) {
		last_configure_time = get_microseconds();
	} else {
		Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::configure_timeout), 100);
		have_configure_timeout = true;
	}

	return FALSE;
}

void
ARDOUR_UI::set_transport_controllable_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("roll")) != 0) {
		roll_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("stop")) != 0) {
		stop_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("goto-start")) != 0) {
		goto_start_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("goto-end")) != 0) {
		goto_end_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("auto-loop")) != 0) {
		auto_loop_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("play-selection")) != 0) {
		play_selection_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("rec")) != 0) {
		rec_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("shuttle")) != 0) {
		shuttle_box->controllable()->set_id (prop->value());
	}
}

XMLNode&
ARDOUR_UI::get_transport_controllable_state ()
{
	XMLNode* node = new XMLNode(X_("TransportControllables"));
	char buf[64];

	roll_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("roll"), buf);
	stop_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("stop"), buf);
	goto_start_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("goto_start"), buf);
	goto_end_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("goto_end"), buf);
	auto_loop_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("auto_loop"), buf);
	play_selection_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("play_selection"), buf);
	rec_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("rec"), buf);
	shuttle_box->controllable()->id().print (buf, sizeof (buf));
	node->add_property (X_("shuttle"), buf);

	return *node;
}


gint
ARDOUR_UI::autosave_session ()
{
	if (g_main_depth() > 1) {
		/* inside a recursive main loop,
		   give up because we may not be able to
		   take a lock.
		*/
		return 1;
	}

	if (!Config->get_periodic_safety_backups()) {
		return 1;
	}

	if (_session) {
		_session->maybe_write_autosave();
	}

	return 1;
}

void
ARDOUR_UI::update_autosave ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::update_autosave)

	if (_session && _session->dirty()) {
		if (_autosave_connection.connected()) {
			_autosave_connection.disconnect();
		}

		_autosave_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &ARDOUR_UI::autosave_session),
				Config->get_periodic_safety_backup_interval() * 1000);

	} else {
		if (_autosave_connection.connected()) {
			_autosave_connection.disconnect();
		}
	}
}

void
ARDOUR_UI::check_announcements ()
{
#ifdef PHONE_HOME
	string _annc_filename;

#ifdef __APPLE__
	_annc_filename = PROGRAM_NAME "_announcements_osx_";
#else
	_annc_filename = PROGRAM_NAME "_announcements_linux_";
#endif
	_annc_filename.append (VERSIONSTRING);

	std::string path = Glib::build_filename (user_config_directory(), _annc_filename);
	std::ifstream announce_file (path.c_str());
	if ( announce_file.fail() )
		_announce_string = "";
	else {
		std::stringstream oss;
		oss << announce_file.rdbuf();
		_announce_string = oss.str();
	}

	pingback (VERSIONSTRING, path);
#endif
}

int
ARDOUR_UI::starting ()
{
	Application* app = Application::instance ();
	const char *nsm_url;
	bool brand_new_user = ArdourStartup::required ();

	app->ShouldQuit.connect (sigc::mem_fun (*this, &ARDOUR_UI::queue_finish));
	app->ShouldLoad.connect (sigc::mem_fun (*this, &ARDOUR_UI::idle_load));

	if (ARDOUR_COMMAND_LINE::check_announcements) {
		check_announcements ();
	}

	app->ready ();

	/* we need to create this early because it may need to set the
	 *  audio backend end up.
	 */
	
	try {
		audio_midi_setup.get (true);
	} catch (...) {
		return -1;
	}

	if ((nsm_url = g_getenv ("NSM_URL")) != 0) {
		nsm = new NSM_Client;
		if (!nsm->init (nsm_url)) {
			nsm->announce (PROGRAM_NAME, ":dirty:", "ardour3");

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
				error << _("NSM server did not announce itself") << endmsg;
				return -1;
			}
			// wait for open command from nsm server
			for ( i = 0; i < 5000; ++i) {
				nsm->check ();
				Glib::usleep (1000);
				if (nsm->client_id ()) {
					break;
				}
			}

			if (i == 5000) {
				error << _("NSM: no client ID provided") << endmsg;
				return -1;
			}

			if (_session && nsm) {
				_session->set_nsm_state( nsm->is_active() );
			} else {
				error << _("NSM: no session created") << endmsg;
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

		} else {
			delete nsm;
			nsm = 0;
			error << _("NSM: initialization failed") << endmsg;
			return -1;
		}

	} else  {
		
		if (brand_new_user) {
			ArdourStartup s;
			s.present ();
			main().run();
			s.hide ();
			switch (s.response ()) {
			case Gtk::RESPONSE_OK:
				break;
			default:
				return -1;
			}
		}

		/* go get a session */

		const bool new_session_required = (ARDOUR_COMMAND_LINE::new_session || brand_new_user);

		if (get_session_parameters (false, new_session_required, ARDOUR_COMMAND_LINE::load_template)) {
			return -1;
		}
	}

	use_config ();

	goto_editor_window ();

	WM::Manager::instance().show_visible ();

	/* We have to do this here since goto_editor_window() ends up calling show_all() on the
	 * editor window, and we may want stuff to be hidden.
	 */
	_status_bar_visibility.update ();

	BootMessage (string_compose (_("%1 is ready for use"), PROGRAM_NAME));
	return 0;
}

void
ARDOUR_UI::check_memory_locking ()
{
#if defined(__APPLE__) || defined(WIN32)
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

				MessageDialog msg (
					string_compose (
						_("WARNING: Your system has a limit for maximum amount of locked memory. "
						  "This might cause %1 to run out of memory before your system "
						  "runs out of memory. \n\n"
						  "You can view the memory limit with 'ulimit -l', "
						  "and it is normally controlled by %2"),
						PROGRAM_NAME, 
#ifdef __FreeBSD__
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

				pop_back_splash (msg);

				editor->ensure_float (msg);
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
ARDOUR_UI::queue_finish ()
{
	Glib::signal_idle().connect (mem_fun (*this, &ARDOUR_UI::idle_finish));
}

bool
ARDOUR_UI::idle_finish ()
{
	finish ();
	return false; /* do not call again */
}

void
ARDOUR_UI::finish()
{
	if (_session) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();

		if (_session->dirty()) {
			vector<string> actions;
			actions.push_back (_("Don't quit"));
			actions.push_back (_("Just quit"));
			actions.push_back (_("Save and quit"));
			switch (ask_about_saving_session(actions)) {
			case -1:
				return;
				break;
			case 1:
				/* use the default name */
				if (save_state_canfail ("")) {
					/* failed - don't quit */
					MessageDialog msg (*editor,
							   string_compose (_("\
%1 was unable to save your session.\n\n\
If you still wish to quit, please use the\n\n\
\"Just quit\" option."), PROGRAM_NAME));
					pop_back_splash(msg);
					msg.run ();
					return;
				}
				break;
			case 0:
				break;
			}
		}

		second_connection.disconnect ();
		point_one_second_connection.disconnect ();
		point_zero_something_second_connection.disconnect();
	}

	delete ARDOUR_UI::instance()->video_timeline;
	ARDOUR_UI::instance()->video_timeline = NULL;
	stop_video_server();

	/* Save state before deleting the session, as that causes some
	   windows to be destroyed before their visible state can be
	   saved.
	*/
	save_ardour_state ();

	close_all_dialogs ();

	loading_message (string_compose (_("Please wait while %1 cleans up..."), PROGRAM_NAME));

	if (_session) {
		// _session->set_deletion_in_progress ();
		_session->set_clean ();
		_session->remove_pending_capture_state ();
		delete _session;
		_session = 0;
	}

	halt_connection.disconnect ();
	AudioEngine::instance()->stop ();
	quit ();
}

int
ARDOUR_UI::ask_about_saving_session (const vector<string>& actions)
{
	ArdourDialog window (_("Unsaved Session"));
	Gtk::HBox dhbox;  // the hbox for the image and text
	Gtk::Label  prompt_label;
	Gtk::Image* dimage = manage (new Gtk::Image(Stock::DIALOG_WARNING,  Gtk::ICON_SIZE_DIALOG));

	string msg;

	assert (actions.size() >= 3);

	window.add_button (actions[0], RESPONSE_REJECT);
	window.add_button (actions[1], RESPONSE_APPLY);
	window.add_button (actions[2], RESPONSE_ACCEPT);

	window.set_default_response (RESPONSE_ACCEPT);

	Gtk::Button noquit_button (msg);
	noquit_button.set_name ("EditorGTKButton");

	string prompt;

	if (_session->snap_name() == _session->name()) {
		prompt = string_compose(_("The session \"%1\"\nhas not been saved.\n\nAny changes made this time\nwill be lost unless you save it.\n\nWhat do you want to do?"),
					_session->snap_name());
	} else {
		prompt = string_compose(_("The snapshot \"%1\"\nhas not been saved.\n\nAny changes made this time\nwill be lost unless you save it.\n\nWhat do you want to do?"),
					_session->snap_name());
	}

	prompt_label.set_text (prompt);
	prompt_label.set_name (X_("PrompterLabel"));
	prompt_label.set_alignment(ALIGN_LEFT, ALIGN_TOP);

	dimage->set_alignment(ALIGN_CENTER, ALIGN_TOP);
	dhbox.set_homogeneous (false);
	dhbox.pack_start (*dimage, false, false, 5);
	dhbox.pack_start (prompt_label, true, false, 5);
	window.get_vbox()->pack_start (dhbox);

	window.set_name (_("Prompter"));
	window.set_modal (true);
	window.set_resizable (false);

	dhbox.show();
	prompt_label.show();
	dimage->show();
	window.show();
	window.set_keep_above (true);
	window.present ();

	ResponseType r = (ResponseType) window.run();

	window.hide ();

	switch (r) {
	case RESPONSE_ACCEPT: // save and get out of here
		return 1;
	case RESPONSE_APPLY:  // get out of here
		return 0;
	default:
		break;
	}

	return -1;
}


gint
ARDOUR_UI::every_second ()
{
	update_cpu_load ();
	update_buffer_load ();
	update_disk_space ();
	update_timecode_format ();

	if (nsm && nsm->is_active ()) {
		nsm->check ();

		if (!_was_dirty && _session->dirty ()) {
			nsm->is_dirty ();
			_was_dirty = true;
		}
		else if (_was_dirty && !_session->dirty ()){
			nsm->is_clean ();
			_was_dirty = false;
		}
	}
	return TRUE;
}

gint
ARDOUR_UI::every_point_one_seconds ()
{
	shuttle_box->update_speed_display ();
	RapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

gint
ARDOUR_UI::every_point_zero_something_seconds ()
{
	// august 2007: actual update frequency: 25Hz (40ms), not 100Hz

	SuperRapidScreenUpdate(); /* EMIT_SIGNAL */
	if (editor_meter && Config->get_show_editor_meter()) {
		float mpeak = editor_meter->update_meters();
		if (mpeak > editor_meter_max_peak) {
			if (mpeak >= Config->get_meter_peak()) {
				editor_meter_peak_display.set_name ("meterbridge peakindicator on");
				editor_meter_peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
			}
		}
	}
	return TRUE;
}

void
ARDOUR_UI::update_sample_rate (framecnt_t)
{
	char buf[64];

	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::update_sample_rate, ignored)

	if (!AudioEngine::instance()->connected()) {

		snprintf (buf, sizeof (buf), _("Audio: <span foreground=\"red\">none</span>"));

	} else {

		framecnt_t rate = AudioEngine::instance()->sample_rate();

		if (rate == 0) {
			/* no sample rate available */
			snprintf (buf, sizeof (buf), _("Audio: <span foreground=\"red\">none</span>"));
		} else {

			if (fmod (rate, 1000.0) != 0.0) {
				snprintf (buf, sizeof (buf), _("Audio: <span foreground=\"green\">%.1f kHz / %4.1f ms</span>"),
					  (float) rate / 1000.0f,
					  (AudioEngine::instance()->usecs_per_cycle() / 1000.0f));
			} else {
				snprintf (buf, sizeof (buf), _("Audio: <span foreground=\"green\">%" PRId64 " kHz / %4.1f ms</span>"),
					  rate/1000,
					  (AudioEngine::instance()->usecs_per_cycle() / 1000.0f));
			}
		}
	}
	sample_rate_label.set_markup (buf);
}

void
ARDOUR_UI::update_format ()
{
	if (!_session) {
		format_label.set_text ("");
		return;
	}

	stringstream s;
	s << _("File:") << X_(" <span foreground=\"green\">");

	switch (_session->config.get_native_file_header_format ()) {
	case BWF:
		s << _("BWF");
		break;
	case WAVE:
		s << _("WAV");
		break;
	case WAVE64:
		s << _("WAV64");
		break;
	case CAF:
		s << _("CAF");
		break;
	case AIFF:
		s << _("AIFF");
		break;
	case iXML:
		s << _("iXML");
		break;
	case RF64:
		s << _("RF64");
		break;
	}

	s << " ";
	
	switch (_session->config.get_native_file_data_format ()) {
	case FormatFloat:
		s << _("32-float");
		break;
	case FormatInt24:
		s << _("24-int");
		break;
	case FormatInt16:
		s << _("16-int");
		break;
	}

	s << X_("</span>");

	format_label.set_markup (s.str ());
}

void
ARDOUR_UI::update_cpu_load ()
{
	char buf[64];

	/* If this text is changed, the set_size_request_to_display_given_text call in ARDOUR_UI::resize_text_widgets
	   should also be changed.
	*/

	float const c = AudioEngine::instance()->get_dsp_load ();
	snprintf (buf, sizeof (buf), _("DSP: <span foreground=\"%s\">%5.1f%%</span>"), c >= 90 ? X_("red") : X_("green"), c);
	cpu_load_label.set_markup (buf);
}

void
ARDOUR_UI::update_buffer_load ()
{
	char buf[256];

	uint32_t const playback = _session ? _session->playback_load () : 100;
	uint32_t const capture = _session ? _session->capture_load () : 100;

	/* If this text is changed, the set_size_request_to_display_given_text call in ARDOUR_UI::resize_text_widgets
	   should also be changed.
	*/
	
	if (_session) {
		snprintf (
			buf, sizeof (buf),
			_("Buffers: <span foreground=\"green\">p:</span><span foreground=\"%s\">%" PRIu32 "%%</span> "
			           "<span foreground=\"green\">c:</span><span foreground=\"%s\">%" PRIu32 "%%</span>"),
			playback <= 5 ? X_("red") : X_("green"),
			playback,
			capture <= 5 ? X_("red") : X_("green"),
			capture
			);

		buffer_load_label.set_markup (buf);
	} else {
		buffer_load_label.set_text ("");
	}
}

void
ARDOUR_UI::count_recenabled_streams (Route& route)
{
	Track* track = dynamic_cast<Track*>(&route);
	if (track && track->record_enabled()) {
		rec_enabled_streams += track->n_inputs().n_total();
	}
}

void
ARDOUR_UI::update_disk_space()
{
	if (_session == 0) {
		return;
	}

	boost::optional<framecnt_t> opt_frames = _session->available_capture_duration();
	char buf[64];
	framecnt_t fr = _session->frame_rate();

	if (fr == 0) {
		/* skip update - no SR available */
		return;
	}

	if (!opt_frames) {
		/* Available space is unknown */
		snprintf (buf, sizeof (buf), "%s", _("Disk: <span foreground=\"green\">Unknown</span>"));
	} else if (opt_frames.get_value_or (0) == max_framecnt) {
		snprintf (buf, sizeof (buf), "%s", _("Disk: <span foreground=\"green\">24hrs+</span>"));
	} else {
		rec_enabled_streams = 0;
		_session->foreach_route (this, &ARDOUR_UI::count_recenabled_streams);

		framecnt_t frames = opt_frames.get_value_or (0);

		if (rec_enabled_streams) {
			frames /= rec_enabled_streams;
		}

		int hrs;
		int mins;
		int secs;

		hrs  = frames / (fr * 3600);

		if (hrs > 24) {
			snprintf (buf, sizeof (buf), "%s", _("Disk: <span foreground=\"green\">&gt;24 hrs</span>"));
		} else {
			frames -= hrs * fr * 3600;
			mins = frames / (fr * 60);
			frames -= mins * fr * 60;
			secs = frames / fr;
			
			bool const low = (hrs == 0 && mins <= 30);
			
			snprintf (
				buf, sizeof(buf),
				_("Disk: <span foreground=\"%s\">%02dh:%02dm:%02ds</span>"),
				low ? X_("red") : X_("green"),
				hrs, mins, secs
				);
		}
	}

	disk_space_label.set_markup (buf);
}

void
ARDOUR_UI::update_timecode_format ()
{
	char buf[64];

	if (_session) {
		bool matching;
		TimecodeSlave* tcslave;
		SyncSource sync_src = Config->get_sync_source();

		if ((sync_src == LTC || sync_src == MTC) && (tcslave = dynamic_cast<TimecodeSlave*>(_session->slave())) != 0) {
			matching = (tcslave->apparent_timecode_format() == _session->config.get_timecode_format());
		} else {
			matching = true;
		}
			
		snprintf (buf, sizeof (buf), S_("Timecode|TC: <span foreground=\"%s\">%s</span>"),
			  matching ? X_("green") : X_("red"),
			  Timecode::timecode_format_name (_session->config.get_timecode_format()).c_str());
	} else {
		snprintf (buf, sizeof (buf), "TC: n/a");
	}

	timecode_format_label.set_markup (buf);
}	

gint
ARDOUR_UI::update_wall_clock ()
{
	time_t now;
	struct tm *tm_now;
	static int last_min = -1;

	time (&now);
	tm_now = localtime (&now);
	if (last_min != tm_now->tm_min) {
		char buf[16];
		sprintf (buf, "%02d:%02d", tm_now->tm_hour, tm_now->tm_min);
		wall_clock_label.set_text (buf);
		last_min = tm_now->tm_min;
	}

	return TRUE;
}

void
ARDOUR_UI::redisplay_recent_sessions ()
{
	std::vector<std::string> session_directories;
	RecentSessionsSorter cmp;

	recent_session_display.set_model (Glib::RefPtr<TreeModel>(0));
	recent_session_model->clear ();

	ARDOUR::RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		recent_session_display.set_model (recent_session_model);
		return;
	}

	// sort them alphabetically
	sort (rs.begin(), rs.end(), cmp);

	for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		session_directories.push_back ((*i).second);
	}

	for (vector<std::string>::const_iterator i = session_directories.begin();
			i != session_directories.end(); ++i)
	{
		std::vector<std::string> state_file_paths;

		// now get available states for this session

		get_state_files_in_directory (*i, state_file_paths);

		vector<string*>* states;
		vector<const gchar*> item;
		string fullpath = *i;

		/* remove any trailing / */

		if (fullpath[fullpath.length() - 1] == '/') {
			fullpath = fullpath.substr (0, fullpath.length() - 1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(fullpath.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			continue;
		}

		/* now get available states for this session */

		if ((states = Session::possible_states (fullpath)) == 0) {
			/* no state file? */
			continue;
		}

		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		Gtk::TreeModel::Row row = *(recent_session_model->append());

		row[recent_session_columns.visible_name] = Glib::path_get_basename (fullpath);
		row[recent_session_columns.fullpath] = fullpath;
		row[recent_session_columns.tip] = Glib::Markup::escape_text (fullpath);

		if (state_file_names.size() > 1) {

			// add the children

			for (std::vector<std::string>::iterator i2 = state_file_names.begin();
					i2 != state_file_names.end(); ++i2)
			{

				Gtk::TreeModel::Row child_row = *(recent_session_model->append (row.children()));

				child_row[recent_session_columns.visible_name] = *i2;
				child_row[recent_session_columns.fullpath] = fullpath;
				child_row[recent_session_columns.tip] = Glib::Markup::escape_text (fullpath);
			}
		}
	}

	recent_session_display.set_tooltip_column(1); // recent_session_columns.tip
	recent_session_display.set_model (recent_session_model);
}

void
ARDOUR_UI::build_session_selector ()
{
	session_selector_window = new ArdourDialog (_("Recent Sessions"));

	Gtk::ScrolledWindow *scroller = manage (new Gtk::ScrolledWindow);

	session_selector_window->add_button (Stock::CANCEL, RESPONSE_CANCEL);
	session_selector_window->add_button (Stock::OPEN, RESPONSE_ACCEPT);
	session_selector_window->set_default_response (RESPONSE_ACCEPT);
	recent_session_model = TreeStore::create (recent_session_columns);
	recent_session_display.set_model (recent_session_model);
	recent_session_display.append_column (_("Recent Sessions"), recent_session_columns.visible_name);
	recent_session_display.set_headers_visible (false);
	recent_session_display.get_selection()->set_mode (SELECTION_BROWSE);
	recent_session_display.signal_row_activated().connect (sigc::mem_fun (*this, &ARDOUR_UI::recent_session_row_activated));

	scroller->add (recent_session_display);
	scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	session_selector_window->set_name ("SessionSelectorWindow");
	session_selector_window->set_size_request (200, 400);
	session_selector_window->get_vbox()->pack_start (*scroller);

	recent_session_display.show();
	scroller->show();
}

void
ARDOUR_UI::recent_session_row_activated (const TreePath& /*path*/, TreeViewColumn* /*col*/)
{
	session_selector_window->response (RESPONSE_ACCEPT);
}

void
ARDOUR_UI::open_recent_session ()
{
	bool can_return = (_session != 0);

	if (session_selector_window == 0) {
		build_session_selector ();
	}

	redisplay_recent_sessions ();

	while (true) {

		ResponseType r = (ResponseType) session_selector_window->run ();

		switch (r) {
		case RESPONSE_ACCEPT:
			break;
		default:
			if (can_return) {
				session_selector_window->hide();
				return;
			} else {
				exit (1);
			}
		}

		if (recent_session_display.get_selection()->count_selected_rows() == 0) {
			continue;
		}

		session_selector_window->hide();

		Gtk::TreeModel::iterator i = recent_session_display.get_selection()->get_selected();

		if (i == recent_session_model->children().end()) {
			return;
		}

		std::string path = (*i)[recent_session_columns.fullpath];
		std::string state = (*i)[recent_session_columns.visible_name];

		_session_is_new = false;

		if (load_session (path, state) == 0) {
			break;
		}

		can_return = false;
	}
}

bool
ARDOUR_UI::check_audioengine ()
{
	if (!AudioEngine::instance()->connected()) {
		MessageDialog msg (string_compose (
					   _("%1 is not connected to any audio backend.\n"
					     "You cannot open or close sessions in this condition"),
					   PROGRAM_NAME));
		pop_back_splash (msg);
		msg.run ();
		return false;
	}
	return true;
}

void
ARDOUR_UI::open_session ()
{
	if (!check_audioengine()) {
		return;

	}

	/* popup selector window */

	if (open_session_selector == 0) {

		/* ardour sessions are folders */

		open_session_selector = new Gtk::FileChooserDialog (_("Open Session"), FILE_CHOOSER_ACTION_OPEN);
		open_session_selector->add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		open_session_selector->add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
		open_session_selector->set_default_response(Gtk::RESPONSE_ACCEPT);
		
		if (_session) {
			string session_parent_dir = Glib::path_get_dirname(_session->path());
			string::size_type last_dir_sep = session_parent_dir.rfind(G_DIR_SEPARATOR);
			session_parent_dir = session_parent_dir.substr(0, last_dir_sep);
			open_session_selector->set_current_folder(session_parent_dir);
		} else {
			open_session_selector->set_current_folder(Config->get_default_session_parent_dir());
		}

		string default_session_folder = Config->get_default_session_parent_dir();
		try {
			/* add_shortcut_folder throws an exception if the folder being added already has a shortcut */
			open_session_selector->add_shortcut_folder (default_session_folder);
		}
		catch (Glib::Error & e) {
			std::cerr << "open_session_selector->add_shortcut_folder (" << default_session_folder << ") threw Glib::Error " << e.what() << std::endl;
		}

		FileFilter session_filter;
		session_filter.add_pattern ("*.ardour");
		session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
		open_session_selector->add_filter (session_filter);
		open_session_selector->set_filter (session_filter);
  	}

	int response = open_session_selector->run();
	open_session_selector->hide ();

	switch (response) {
	case RESPONSE_ACCEPT:
		break;
	default:
		open_session_selector->hide();
		return;
	}

	open_session_selector->hide();
	string session_path = open_session_selector->get_filename();
	string path, name;
	bool isnew;

	if (session_path.length() > 0) {
		if (ARDOUR::find_session (session_path, path, name, isnew) == 0) {
			_session_is_new = isnew;
			load_session (path, name);
		}
	}
}


void
ARDOUR_UI::session_add_mixed_track (const ChanCount& input, const ChanCount& output, RouteGroup* route_group, 
				    uint32_t how_many, const string& name_template, PluginInfoPtr instrument)
{
	list<boost::shared_ptr<MidiTrack> > tracks;

	if (_session == 0) {
		warning << _("You cannot add a track without a session already loaded.") << endmsg;
		return;
	}

	try {
		tracks = _session->new_midi_track (input, output, instrument, ARDOUR::Normal, route_group, how_many, name_template);
		
		if (tracks.size() != how_many) {
			error << string_compose(P_("could not create %1 new mixed track", "could not create %1 new mixed tracks", how_many), how_many) << endmsg;
		}
	}

	catch (...) {
		MessageDialog msg (*editor,
				   string_compose (_("There are insufficient JACK ports available\n\
to create a new track or bus.\n\
You should save %1, exit and\n\
restart JACK with more ports."), PROGRAM_NAME));
		msg.run ();
	}
}
	

void
ARDOUR_UI::session_add_midi_route (bool disk, RouteGroup* route_group, uint32_t how_many, const string& name_template, PluginInfoPtr instrument)
{
	ChanCount one_midi_channel;
	one_midi_channel.set (DataType::MIDI, 1);

	if (disk) {
		session_add_mixed_track (one_midi_channel, one_midi_channel, route_group, how_many, name_template, instrument);
	}
}

void
ARDOUR_UI::session_add_audio_route (
	bool track,
	int32_t input_channels,
	int32_t output_channels,
	ARDOUR::TrackMode mode,
	RouteGroup* route_group,
	uint32_t how_many,
	string const & name_template
	)
{
	list<boost::shared_ptr<AudioTrack> > tracks;
	RouteList routes;

	if (_session == 0) {
		warning << _("You cannot add a track or bus without a session already loaded.") << endmsg;
		return;
	}

	try {
		if (track) {
			tracks = _session->new_audio_track (input_channels, output_channels, mode, route_group, how_many, name_template);

			if (tracks.size() != how_many) {
				error << string_compose (P_("could not create %1 new audio track", "could not create %1 new audio tracks", how_many), how_many) 
				      << endmsg;
			}

		} else {

			routes = _session->new_audio_route (input_channels, output_channels, route_group, how_many, name_template);

			if (routes.size() != how_many) {
				error << string_compose (P_("could not create %1 new audio bus", "could not create %1 new audio busses", how_many), how_many)
				      << endmsg;
			}
		}
	}

	catch (...) {
		MessageDialog msg (*editor,
				   string_compose (_("There are insufficient JACK ports available\n\
to create a new track or bus.\n\
You should save %1, exit and\n\
restart JACK with more ports."), PROGRAM_NAME));
		pop_back_splash (msg);
		msg.run ();
	}
}

void
ARDOUR_UI::transport_goto_start ()
{
	if (_session) {
		_session->goto_start();

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (_session->current_start_frame ());
		}
	}
}

void
ARDOUR_UI::transport_goto_zero ()
{
	if (_session) {
		_session->request_locate (0);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->reset_x_origin (0);
		}
	}
}

void
ARDOUR_UI::transport_goto_wallclock ()
{
	if (_session && editor) {

		time_t now;
		struct tm tmnow;
		framepos_t frames;

		time (&now);
		localtime_r (&now, &tmnow);
		
		int frame_rate = _session->frame_rate();
		
		if (frame_rate == 0) {
			/* no frame rate available */
			return;
		}

		frames = tmnow.tm_hour * (60 * 60 * frame_rate);
		frames += tmnow.tm_min * (60 * frame_rate);
		frames += tmnow.tm_sec * frame_rate;

		_session->request_locate (frames, _session->transport_rolling ());

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (frames);
		}
	}
}

void
ARDOUR_UI::transport_goto_end ()
{
	if (_session) {
		framepos_t const frame = _session->current_end_frame();
		_session->request_locate (frame);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (frame);
		}
	}
}

void
ARDOUR_UI::transport_stop ()
{
	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		return;
	}

	_session->request_stop (false, true);
}

void
ARDOUR_UI::transport_record (bool roll)
{

	if (_session) {
		switch (_session->record_status()) {
		case Session::Disabled:
			if (_session->ntracks() == 0) {
				MessageDialog msg (*editor, _("Please create one or more tracks before trying to record.\nYou can do this with the \"Add Track or Bus\" option in the Session menu."));
				msg.run ();
				return;
			}
			_session->maybe_enable_record ();
			if (roll) {
				transport_roll ();
			}
			break;
		case Session::Recording:
			if (roll) {
				_session->request_stop();
			} else {
				_session->disable_record (false, true);
			}
			break;

		case Session::Enabled:
			_session->disable_record (false, true);
		}
	}
}

void
ARDOUR_UI::transport_roll ()
{
	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		return;
	}

#if 0
	if (_session->config.get_external_sync()) {
		switch (Config->get_sync_source()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}
#endif

	bool rolling = _session->transport_rolling();

	if (_session->get_play_loop()) {
		/* XXX it is not possible to just leave seamless loop and keep
		   playing at present (nov 4th 2009)
		*/
		if (!Config->get_seamless_loop()) {
			_session->request_play_loop (false, true);
		}
	} else if (_session->get_play_range () && !Config->get_always_play_range()) {
		/* stop playing a range if we currently are */
		_session->request_play_range (0, true);
	}

	if (!rolling) {
		_session->request_transport_speed (1.0f);
	}
}

bool
ARDOUR_UI::get_smart_mode() const
{
	return ( editor->get_smart_mode() );
}


void
ARDOUR_UI::toggle_roll (bool with_abort, bool roll_out_of_bounded_mode)
{

	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		return;
	}

	if (_session->config.get_external_sync()) {
		switch (Config->get_sync_source()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = _session->transport_rolling();
	bool affect_transport = true;

	if (rolling && roll_out_of_bounded_mode) {
		/* drop out of loop/range playback but leave transport rolling */
		if (_session->get_play_loop()) {
			if (Config->get_seamless_loop()) {
				/* the disk buffers contain copies of the loop - we can't
				   just keep playing, so stop the transport. the user
				   can restart as they wish.
				*/
				affect_transport = true;
			} else {
				/* disk buffers are normal, so we can keep playing */
				affect_transport = false;
			}
			_session->request_play_loop (false, true);
		} else if (_session->get_play_range ()) {
			affect_transport = false;
			_session->request_play_range (0, true);
		}
	}

	if (affect_transport) {
		if (rolling) {
			_session->request_stop (with_abort, true);
		} else {
			if ( Config->get_always_play_range() ) {
				_session->request_play_range (&editor->get_selection().time, true);
			}

			_session->request_transport_speed (1.0f);
		}
	}
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	Location * looploc = _session->locations()->auto_loop_location();

	if (!_session || !looploc) {
		return;
	}

	if (_session->get_play_loop()) {

		if (_session->transport_rolling()) {

			_session->request_locate (looploc->start(), true);
			_session->request_play_loop (false);

		} else {
			_session->request_play_loop (false);
		}
	} else {
		_session->request_play_loop (true);
	}
	
	//show the loop markers
	looploc->set_hidden (false, this);
}

void
ARDOUR_UI::transport_play_selection ()
{
	if (!_session) {
		return;
	}

	editor->play_selection ();
}

void
ARDOUR_UI::transport_play_preroll ()
{
	if (!_session) {
		return;
	}
	editor->play_with_preroll ();
}

void
ARDOUR_UI::transport_rewind (int option)
{
	float current_transport_speed;

       	if (_session) {
		current_transport_speed = _session->transport_speed();

		if (current_transport_speed >= 0.0f) {
			switch (option) {
			case 0:
				_session->request_transport_speed (-1.0f);
				break;
			case 1:
				_session->request_transport_speed (-4.0f);
				break;
			case -1:
				_session->request_transport_speed (-0.5f);
				break;
			}
		} else {
			/* speed up */
			_session->request_transport_speed (current_transport_speed * 1.5f);
		}
	}
}

void
ARDOUR_UI::transport_forward (int option)
{
	if (!_session) {
		return;
	}
	
	float current_transport_speed = _session->transport_speed();
	
	if (current_transport_speed <= 0.0f) {
		switch (option) {
		case 0:
			_session->request_transport_speed (1.0f);
			break;
		case 1:
			_session->request_transport_speed (4.0f);
			break;
		case -1:
			_session->request_transport_speed (0.5f);
			break;
		}
	} else {
		/* speed up */
		_session->request_transport_speed (current_transport_speed * 1.5f);
	}
}

void
ARDOUR_UI::toggle_record_enable (uint32_t rid)
{
	if (!_session) {
		return;
	}

	boost::shared_ptr<Route> r;

	if ((r = _session->route_by_remote_id (rid)) != 0) {

		Track* t;

		if ((t = dynamic_cast<Track*>(r.get())) != 0) {
			t->set_record_enabled (!t->record_enabled(), this);
		}
	}
}

void
ARDOUR_UI::map_transport_state ()
{
	if (!_session) {
		auto_loop_button.unset_active_state ();
		play_selection_button.unset_active_state ();
		roll_button.unset_active_state ();
		stop_button.set_active_state (Gtkmm2ext::ExplicitActive);
		return;
	}

	shuttle_box->map_transport_state ();

	float sp = _session->transport_speed();

	if (sp != 0.0f) {

		/* we're rolling */

		if (_session->get_play_range()) {

			play_selection_button.set_active_state (Gtkmm2ext::ExplicitActive);
			roll_button.unset_active_state ();
			auto_loop_button.unset_active_state ();

		} else if (_session->get_play_loop ()) {

			auto_loop_button.set_active (true);
			play_selection_button.set_active (false);
			roll_button.set_active (false);

		} else {

			roll_button.set_active (true);
			play_selection_button.set_active (false);
			auto_loop_button.set_active (false);
		}

		if (Config->get_always_play_range()) {
			/* light up both roll and play-selection if they are joined */
			roll_button.set_active (true);
			play_selection_button.set_active (true);
		}

		stop_button.set_active (false);

	} else {

		stop_button.set_active (true);
		roll_button.set_active (false);
		play_selection_button.set_active (false);
		auto_loop_button.set_active (false);
		update_disk_space ();
	}
}

void
ARDOUR_UI::update_clocks ()
{
	if (!editor || !editor->dragging_playhead()) {
		Clock (_session->audible_frame(), false, editor->get_preferred_edit_position()); /* EMIT_SIGNAL */
	}
}

void
ARDOUR_UI::start_clocking ()
{
	if (Config->get_super_rapid_clock_update()) {
		clock_signal_connection = SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &ARDOUR_UI::update_clocks));
	} else {
		clock_signal_connection = RapidScreenUpdate.connect (sigc::mem_fun(*this, &ARDOUR_UI::update_clocks));
	}
}

void
ARDOUR_UI::stop_clocking ()
{
	clock_signal_connection.disconnect ();
}

gint
ARDOUR_UI::_blink (void *arg)
{
	((ARDOUR_UI *) arg)->blink ();
	return TRUE;
}

void
ARDOUR_UI::blink ()
{
	Blink (blink_on = !blink_on); /* EMIT_SIGNAL */
}

void
ARDOUR_UI::start_blinking ()
{
	/* Start the blink signal. Everybody with a blinking widget
	   uses Blink to drive the widget's state.
	*/

	if (blink_timeout_tag < 0) {
		blink_on = false;
		blink_timeout_tag = g_timeout_add (240, _blink, this);
	}
}

void
ARDOUR_UI::stop_blinking ()
{
	if (blink_timeout_tag >= 0) {
		g_source_remove (blink_timeout_tag);
		blink_timeout_tag = -1;
	}
}


/** Ask the user for the name of a new snapshot and then take it.
 */

void
ARDOUR_UI::snapshot_session (bool switch_to_it)
{
	ArdourPrompter prompter (true);
	string snapname;

	prompter.set_name ("Prompter");
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	if (switch_to_it) {
		prompter.set_title (_("Save as..."));
		prompter.set_prompt (_("New session name"));
	} else {
		prompter.set_title (_("Take Snapshot"));
		prompter.set_prompt (_("Name of new snapshot"));
	}

	if (!switch_to_it) {
		char timebuf[128];
		time_t n;
		struct tm local_time;

		time (&n);
		localtime_r (&n, &local_time);
		strftime (timebuf, sizeof(timebuf), "%FT%H.%M.%S", &local_time);
		prompter.set_initial_text (timebuf);
	}

  again:
	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
	{
		prompter.get_result (snapname);

		bool do_save = (snapname.length() != 0);

		if (do_save) {
			char illegal = Session::session_name_is_legal(snapname);
			if (illegal) {
				MessageDialog msg (string_compose (_("To ensure compatibility with various systems\n"
				                     "snapshot names may not contain a '%1' character"), illegal));
				msg.run ();
				goto again;
			}
		}

		vector<std::string> p;
		get_state_files_in_directory (_session->session_directory().root_path(), p);
		vector<string> n = get_file_names_no_extension (p);
		if (find (n.begin(), n.end(), snapname) != n.end()) {

			ArdourDialog confirm (_("Confirm Snapshot Overwrite"), true);
			Label m (_("A snapshot already exists with that name.  Do you want to overwrite it?"));
			confirm.get_vbox()->pack_start (m, true, true);
			confirm.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
			confirm.add_button (_("Overwrite"), Gtk::RESPONSE_ACCEPT);
			confirm.show_all ();
			switch (confirm.run()) {
			case RESPONSE_CANCEL:
				do_save = false;
			}
		}

		if (do_save) {
			save_state (snapname, switch_to_it);
		}
		break;
	}

	default:
		break;
	}
}

/** Ask the user for a new session name and then rename the session to it.
 */

void
ARDOUR_UI::rename_session ()
{
	if (!_session) {
		return;
	}

	ArdourPrompter prompter (true);
	string name;

	prompter.set_name ("Prompter");
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	prompter.set_title (_("Rename Session"));
	prompter.set_prompt (_("New session name"));

  again:
	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
	{
		prompter.get_result (name);

		bool do_rename = (name.length() != 0);

		if (do_rename) {
			char illegal = Session::session_name_is_legal (name);

			if (illegal) {
				MessageDialog msg (string_compose (_("To ensure compatibility with various systems\n"
								     "session names may not contain a '%1' character"), illegal));
				msg.run ();
				goto again;
			}

			switch (_session->rename (name)) {
			case -1: {
				MessageDialog msg (_("That name is already in use by another directory/folder. Please try again."));
				msg.set_position (WIN_POS_MOUSE);
				msg.run ();
				goto again;
				break;
			}
			case 0:
				break;
			default: {
				MessageDialog msg (_("Renaming this session failed.\nThings could be seriously messed up at this point"));
				msg.set_position (WIN_POS_MOUSE);
				msg.run ();
				break;
			}
			}
		}
		
		break;
	}

	default:
		break;
	}
}

void
ARDOUR_UI::save_state (const string & name, bool switch_to_it)
{
	XMLNode* node = new XMLNode (X_("UI"));

	WM::Manager::instance().add_state (*node);

	node->add_child_nocopy (gui_object_state->get_state());

	_session->add_extra_xml (*node);

	save_state_canfail (name, switch_to_it);
}

int
ARDOUR_UI::save_state_canfail (string name, bool switch_to_it)
{
	if (_session) {
		int ret;

		if (name.length() == 0) {
			name = _session->snap_name();
		}

		if ((ret = _session->save_state (name, false, switch_to_it)) != 0) {
			return ret;
		}
	}

	save_ardour_state (); /* XXX cannot fail? yeah, right ... */
	return 0;
}

void
ARDOUR_UI::primary_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (primary_clock->current_time ());
	}
}

void
ARDOUR_UI::big_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (big_clock->current_time ());
	}
}

void
ARDOUR_UI::secondary_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (secondary_clock->current_time ());
	}
}

void
ARDOUR_UI::transport_rec_enable_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->step_editing()) {
		return;
	}

	Session::RecordState const r = _session->record_status ();
	bool const h = _session->have_rec_enabled_track ();

	if (r == Session::Enabled || (r == Session::Recording && !h)) {
		if (onoff) {
			rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			rec_button.set_active_state (Gtkmm2ext::ImplicitActive);
		}
	} else if (r == Session::Recording && h) {
		rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		rec_button.unset_active_state ();
	}
}

void
ARDOUR_UI::save_template ()
{
	ArdourPrompter prompter (true);
	string name;

	if (!check_audioengine()) {
		return;
	}

	prompter.set_name (X_("Prompter"));
	prompter.set_title (_("Save Template"));
	prompter.set_prompt (_("Name for template:"));
	prompter.set_initial_text(_session->name() + _("-template"));
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);

	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
		prompter.get_result (name);

		if (name.length()) {
			_session->save_template (name);
		}
		break;

	default:
		break;
	}
}

void
ARDOUR_UI::edit_metadata ()
{
	SessionMetadataEditor dialog;
	dialog.set_session (_session);
	editor->ensure_float (dialog);
	dialog.run ();
}

void
ARDOUR_UI::import_metadata ()
{
	SessionMetadataImporter dialog;
	dialog.set_session (_session);
	editor->ensure_float (dialog);
	dialog.run ();
}

bool
ARDOUR_UI::ask_about_loading_existing_session (const std::string& session_path)
{
	std::string str = string_compose (_("This session\n%1\nalready exists. Do you want to open it?"), session_path);

	MessageDialog msg (str,
			   false,
			   Gtk::MESSAGE_WARNING,
			   Gtk::BUTTONS_YES_NO,
			   true);


	msg.set_name (X_("OpenExistingDialog"));
	msg.set_title (_("Open Existing Session"));
	msg.set_wmclass (X_("existing_session"), PROGRAM_NAME);
	msg.set_position (Gtk::WIN_POS_MOUSE);
	pop_back_splash (msg);

	switch (msg.run()) {
	case RESPONSE_YES:
		return true;
		break;
	}
	return false;
}

int
ARDOUR_UI::build_session_from_dialog (SessionDialog& sd, const std::string& session_path, const std::string& session_name)
{
	BusProfile bus_profile;

	if (nsm || Profile->get_sae()) {

		bus_profile.master_out_channels = 2;
		bus_profile.input_ac = AutoConnectPhysical;
		bus_profile.output_ac = AutoConnectMaster;
		bus_profile.requested_physical_in = 0; // use all available
		bus_profile.requested_physical_out = 0; // use all available

	} else {

		/* get settings from advanced section of NSD */

		if (sd.create_master_bus()) {
			bus_profile.master_out_channels = (uint32_t) sd.master_channel_count();
		} else {
			bus_profile.master_out_channels = 0;
		}

		if (sd.connect_inputs()) {
			bus_profile.input_ac = AutoConnectPhysical;
		} else {
			bus_profile.input_ac = AutoConnectOption (0);
		}

		bus_profile.output_ac = AutoConnectOption (0);

		if (sd.connect_outputs ()) {
			if (sd.connect_outs_to_master()) {
				bus_profile.output_ac = AutoConnectMaster;
			} else if (sd.connect_outs_to_physical()) {
				bus_profile.output_ac = AutoConnectPhysical;
			}
		}

		bus_profile.requested_physical_in = (uint32_t) sd.input_limit_count();
		bus_profile.requested_physical_out = (uint32_t) sd.output_limit_count();
	}

	if (build_session (session_path, session_name, bus_profile)) {
		return -1;
	}

	return 0;
}

void
ARDOUR_UI::idle_load (const std::string& path)
{
	if (_session) {
		if (Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
			/* /path/to/foo => /path/to/foo, foo */
			load_session (path, basename_nosuffix (path));
		} else {
			/* /path/to/foo/foo.ardour => /path/to/foo, foo */
			load_session (Glib::path_get_dirname (path), basename_nosuffix (path));
		}

	} else {
		ARDOUR_COMMAND_LINE::session_name = path;
	}
}

/** @param quit_on_cancel true if exit() should be called if the user clicks `cancel' in the new session dialog */
int
ARDOUR_UI::get_session_parameters (bool quit_on_cancel, bool should_be_new, string load_template)
{
	string session_name;
	string session_path;
	string template_name;
	int ret = -1;
	bool likely_new = false;
	bool cancel_not_quit;

	/* deal with any existing DIRTY session now, rather than later. don't
	 * treat a non-dirty session this way, so that it stays visible 
	 * as we bring up the new session dialog.
	 */

	if (_session && ARDOUR_UI::instance()->video_timeline) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();
	}

	/* if there is already a session, relabel the button
	   on the SessionDialog so that we don't Quit directly
	*/
	cancel_not_quit = (_session != 0);

	if (_session && _session->dirty()) {
		if (unload_session (false)) {
			/* unload cancelled by user */
			return 0;
		}
		ARDOUR_COMMAND_LINE::session_name = "";
	}

	if (!load_template.empty()) {
		should_be_new = true;
		template_name = load_template;
	}

	session_name = basename_nosuffix (ARDOUR_COMMAND_LINE::session_name);
	session_path = ARDOUR_COMMAND_LINE::session_name;
	
	if (!session_path.empty()) {
		if (Glib::file_test (session_path.c_str(), Glib::FILE_TEST_EXISTS)) {
			if (Glib::file_test (session_path.c_str(), Glib::FILE_TEST_IS_REGULAR)) {
				/* session/snapshot file, change path to be dir */
				session_path = Glib::path_get_dirname (session_path);
			}
		}
	}

	SessionDialog session_dialog (should_be_new, session_name, session_path, load_template, cancel_not_quit);

	while (ret != 0) {

		if (!ARDOUR_COMMAND_LINE::session_name.empty()) {

			/* if they named a specific statefile, use it, otherwise they are
			   just giving a session folder, and we want to use it as is
			   to find the session.
			*/

			string::size_type suffix = ARDOUR_COMMAND_LINE::session_name.find (statefile_suffix);

			if (suffix != string::npos) {
				session_path = Glib::path_get_dirname (ARDOUR_COMMAND_LINE::session_name);
				session_name = ARDOUR_COMMAND_LINE::session_name.substr (0, suffix);
				session_name = Glib::path_get_basename (session_name);
			} else {
				session_path = ARDOUR_COMMAND_LINE::session_name;
				session_name = Glib::path_get_basename (ARDOUR_COMMAND_LINE::session_name);
			}
		} else {
			session_path = "";
			session_name = "";
			session_dialog.clear_given ();
		}
		
		if (should_be_new || session_name.empty()) {
			/* need the dialog to get info from user */

			cerr << "run dialog\n";

			switch (session_dialog.run()) {
			case RESPONSE_ACCEPT:
				break;
			default:
				if (quit_on_cancel) {
					exit (1);
				} else {
					return ret;
				}
			}

			session_dialog.hide ();
		}

		/* if we run the startup dialog again, offer more than just "new session" */
		
		should_be_new = false;
		
		session_name = session_dialog.session_name (likely_new);
		session_path = session_dialog.session_folder ();

		if (nsm) {
		        likely_new = true;
		}

		string::size_type suffix = session_name.find (statefile_suffix);
		
		if (suffix != string::npos) {
			session_name = session_name.substr (0, suffix);
		}
		
		/* this shouldn't happen, but we catch it just in case it does */
		
		if (session_name.empty()) {
			continue;
		}
		
		if (session_dialog.use_session_template()) {
			template_name = session_dialog.session_template_name();
			_session_is_new = true;
		}
		
		if (session_name[0] == G_DIR_SEPARATOR ||
		    (session_name.length() > 2 && session_name[0] == '.' && session_name[1] == G_DIR_SEPARATOR) ||
		    (session_name.length() > 3 && session_name[0] == '.' && session_name[1] == '.' && session_name[2] == G_DIR_SEPARATOR)) {
			
			/* absolute path or cwd-relative path specified for session name: infer session folder
			   from what was given.
			*/
			
			session_path = Glib::path_get_dirname (session_name);
			session_name = Glib::path_get_basename (session_name);
			
		} else {

			session_path = session_dialog.session_folder();
			
			char illegal = Session::session_name_is_legal (session_name);
			
			if (illegal) {
				MessageDialog msg (session_dialog,
						   string_compose (_("To ensure compatibility with various systems\n"
								     "session names may not contain a '%1' character"),
								   illegal));
				msg.run ();
				ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
				continue;
			}
		}
	
		if (Glib::file_test (session_path, Glib::FileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {


			if (likely_new && !nsm) {

				std::string existing = Glib::build_filename (session_path, session_name);

				if (!ask_about_loading_existing_session (existing)) {
					ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
					continue;
				}
			}

			_session_is_new = false;

		} else {

			if (!likely_new) {
				pop_back_splash (session_dialog);
				MessageDialog msg (string_compose (_("There is no existing session at \"%1\""), session_path));
				msg.run ();
				ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
				continue;
			}

			char illegal = Session::session_name_is_legal(session_name);

			if (illegal) {
				pop_back_splash (session_dialog);
				MessageDialog msg (session_dialog, string_compose(_("To ensure compatibility with various systems\n"
										    "session names may not contain a '%1' character"), illegal));
				msg.run ();
				ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
				continue;
			}

			_session_is_new = true;
		}

		if (likely_new && template_name.empty()) {

			ret = build_session_from_dialog (session_dialog, session_path, session_name);

		} else {

			ret = load_session (session_path, session_name, template_name);

			if (ret == -2) {
				/* not connected to the AudioEngine, so quit to avoid an infinite loop */
				exit (1);
			}

			if (!ARDOUR_COMMAND_LINE::immediate_save.empty()) {
				_session->save_state (ARDOUR_COMMAND_LINE::immediate_save, false);
				exit (1);
			}

			/* clear this to avoid endless attempts to load the
			   same session.
			*/

			ARDOUR_COMMAND_LINE::session_name = "";
		}
	}

	return ret;
}

void
ARDOUR_UI::close_session()
{
	if (!check_audioengine()) {
		return;
	}

	if (unload_session (true)) {
		return;
	}

	ARDOUR_COMMAND_LINE::session_name = "";

	if (get_session_parameters (true, false)) {
		exit (1);
	}

	goto_editor_window ();
}

/** @param snap_name Snapshot name (without .ardour suffix).
 *  @return -2 if the load failed because we are not connected to the AudioEngine.
 */
int
ARDOUR_UI::load_session (const std::string& path, const std::string& snap_name, std::string mix_template)
{
	Session *new_session;
	int unload_status;
	int retval = -1;

	if (_session) {
		unload_status = unload_session ();
		
		if (unload_status < 0) {
			goto out;
		} else if (unload_status > 0) {
			retval = 0;
			goto out;
		}
	}

	session_loaded = false;

	loading_message (string_compose (_("Please wait while %1 loads your session"), PROGRAM_NAME));

	try {
		new_session = new Session (*AudioEngine::instance(), path, snap_name, 0, mix_template);
	}

	/* this one is special */

	catch (AudioEngine::PortRegistrationFailure& err) {

		MessageDialog msg (err.what(),
				   true,
				   Gtk::MESSAGE_INFO,
				   Gtk::BUTTONS_CLOSE);

		msg.set_title (_("Port Registration Error"));
		msg.set_secondary_text (_("Click the Close button to try again."));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.present ();

		int response = msg.run ();

		msg.hide ();

		switch (response) {
		case RESPONSE_CANCEL:
			exit (1);
		default:
			break;
		}
		goto out;
	}

	catch (...) {

		MessageDialog msg (string_compose(
			                   _("Session \"%1 (snapshot %2)\" did not load successfully"),
			                   path, snap_name),
		                   true,
		                   Gtk::MESSAGE_INFO,
		                   BUTTONS_OK);

		msg.set_keep_above (true);
		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.present ();
		(void) msg.run ();
		msg.hide ();

		goto out;
	}

	{
		list<string> const u = new_session->unknown_processors ();
		if (!u.empty()) {
			MissingPluginDialog d (_session, u);
			d.run ();
		}
	}

	if (!new_session->writable()) {
		MessageDialog msg (_("This session has been opened in read-only mode.\n\nYou will not be able to record or save."),
				   true,
				   Gtk::MESSAGE_INFO,
				   BUTTONS_OK);
		
		msg.set_keep_above (true);
		msg.set_title (_("Read-only Session"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.present ();
		(void) msg.run ();
		msg.hide ();
	}
	

	/* Now the session been created, add the transport controls */
	new_session->add_controllable(roll_controllable);
	new_session->add_controllable(stop_controllable);
	new_session->add_controllable(goto_start_controllable);
	new_session->add_controllable(goto_end_controllable);
	new_session->add_controllable(auto_loop_controllable);
	new_session->add_controllable(play_selection_controllable);
	new_session->add_controllable(rec_controllable);

	set_session (new_session);

	session_loaded = true;

	goto_editor_window ();

	if (_session) {
		_session->set_clean ();
	}

	flush_pending ();
	retval = 0;

  out:
	return retval;
}

int
ARDOUR_UI::build_session (const std::string& path, const std::string& snap_name, BusProfile& bus_profile)
{
	Session *new_session;
	int x;

	session_loaded = false;
	x = unload_session ();

	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 0;
	}

	_session_is_new = true;

	try {
		new_session = new Session (*AudioEngine::instance(), path, snap_name, &bus_profile);
	}

	catch (...) {

		MessageDialog msg (string_compose(_("Could not create session in \"%1\""), path));
		pop_back_splash (msg);
		msg.run ();
		return -1;
	}

	/* Give the new session the default GUI state, if such things exist */

	XMLNode* n;
	n = Config->instant_xml (X_("Editor"));
	if (n) {
		new_session->add_instant_xml (*n, false);
	}
	n = Config->instant_xml (X_("Mixer"));
	if (n) {
		new_session->add_instant_xml (*n, false);
	}

	/* Put the playhead at 0 and scroll fully left */
	n = new_session->instant_xml (X_("Editor"));
	if (n) {
		n->add_property (X_("playhead"), X_("0"));
		n->add_property (X_("left-frame"), X_("0"));
	}

	set_session (new_session);

	session_loaded = true;

	new_session->save_state(new_session->name());

	return 0;
}

void
ARDOUR_UI::launch_chat ()
{
#ifdef __APPLE__
	open_uri("http://webchat.freenode.net/?channels=ardour-osx");
#else
	open_uri("http://webchat.freenode.net/?channels=ardour");
#endif
}

void
ARDOUR_UI::launch_manual ()
{
	PBD::open_uri (Config->get_tutorial_manual_url());
}

void
ARDOUR_UI::launch_reference ()
{
	PBD::open_uri (Config->get_reference_manual_url());
}

void
ARDOUR_UI::loading_message (const std::string& msg)
{
	if (ARDOUR_COMMAND_LINE::no_splash) {
		return;
	}

	if (!splash) {
		show_splash ();
	}

	splash->message (msg);
}

void
ARDOUR_UI::show_splash ()
{
	if (splash == 0) {
		try {
			splash = new Splash;
		} catch (...) {
			return;
		}
	}

	splash->display ();
}

void
ARDOUR_UI::hide_splash ()
{
        delete splash;
        splash = 0;
}

void
ARDOUR_UI::display_cleanup_results (ARDOUR::CleanupReport& rep, const gchar* list_title, const bool msg_delete)
{
	size_t removed;

	removed = rep.paths.size();

	if (removed == 0) {
		MessageDialog msgd (*editor,
				    _("No files were ready for clean-up"),
				    true,
				    Gtk::MESSAGE_INFO,
				    Gtk::BUTTONS_OK);
		msgd.set_title (_("Clean-up"));
		msgd.set_secondary_text (_("If this seems suprising, \n\
check for any existing snapshots.\n\
These may still include regions that\n\
require some unused files to continue to exist."));

		msgd.run ();
		return;
	}

	ArdourDialog results (_("Clean-up"), true, false);

	struct CleanupResultsModelColumns : public Gtk::TreeModel::ColumnRecord {
	    CleanupResultsModelColumns() {
		    add (visible_name);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<std::string> visible_name;
	    Gtk::TreeModelColumn<std::string> fullpath;
	};


	CleanupResultsModelColumns results_columns;
	Glib::RefPtr<Gtk::ListStore> results_model;
	Gtk::TreeView results_display;

	results_model = ListStore::create (results_columns);
	results_display.set_model (results_model);
	results_display.append_column (list_title, results_columns.visible_name);

	results_display.set_name ("CleanupResultsList");
	results_display.set_headers_visible (true);
	results_display.set_headers_clickable (false);
	results_display.set_reorderable (false);

	Gtk::ScrolledWindow list_scroller;
	Gtk::Label txt;
	Gtk::VBox dvbox;
	Gtk::HBox dhbox;  // the hbox for the image and text
	Gtk::HBox ddhbox; // the hbox we eventually pack into the dialog's vbox
	Gtk::Image* dimage = manage (new Gtk::Image(Stock::DIALOG_INFO,  Gtk::ICON_SIZE_DIALOG));

	dimage->set_alignment(ALIGN_LEFT, ALIGN_TOP);

	const string dead_directory = _session->session_directory().dead_path();

	/* subst:
	   %1 - number of files removed
	   %2 - location of "dead"
	   %3 - size of files affected
	   %4 - prefix for "bytes" to produce sensible results (e.g. mega, kilo, giga)
	*/

	const char* bprefix;
	double space_adjusted = 0;

	if (rep.space < 1000) {
		bprefix = X_("");
		space_adjusted = rep.space;
	} else if (rep.space < 1000000) {
		bprefix = _("kilo");
		space_adjusted = truncf((float)rep.space / 1000.0);
	} else if (rep.space < 1000000 * 1000) {
		bprefix = _("mega");
		space_adjusted = truncf((float)rep.space / (1000.0 * 1000.0));
	} else {
		bprefix = _("giga");
		space_adjusted = truncf((float)rep.space / (1000.0 * 1000 * 1000.0));
	}

	if (msg_delete) {
		txt.set_markup (string_compose (P_("\
The following file was deleted from %2,\n\
releasing %3 %4bytes of disk space", "\
The following %1 files were deleted from %2,\n\
releasing %3 %4bytes of disk space", removed),
					removed, Glib::Markup::escape_text (dead_directory), space_adjusted, bprefix, PROGRAM_NAME));
	} else {
		txt.set_markup (string_compose (P_("\
The following file was not in use and \n\
has been moved to: %2\n\n\
After a restart of %5\n\n\
<span face=\"mono\">Session -> Clean-up -> Flush Wastebasket</span>\n\n\
will release an additional %3 %4bytes of disk space.\n", "\
The following %1 files were not in use and \n\
have been moved to: %2\n\n\
After a restart of %5\n\n\
<span face=\"mono\">Session -> Clean-up -> Flush Wastebasket</span>\n\n\
will release an additional %3 %4bytes of disk space.\n", removed),
					removed, Glib::Markup::escape_text (dead_directory), space_adjusted, bprefix, PROGRAM_NAME));
	}

	dhbox.pack_start (*dimage, true, false, 5);
	dhbox.pack_start (txt, true, false, 5);

	for (vector<string>::iterator i = rep.paths.begin(); i != rep.paths.end(); ++i) {
		TreeModel::Row row = *(results_model->append());
		row[results_columns.visible_name] = *i;
		row[results_columns.fullpath] = *i;
	}

	list_scroller.add (results_display);
	list_scroller.set_size_request (-1, 150);
	list_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	dvbox.pack_start (dhbox, true, false, 5);
	dvbox.pack_start (list_scroller, true, false, 5);
	ddhbox.pack_start (dvbox, true, false, 5);

	results.get_vbox()->pack_start (ddhbox, true, false, 5);
	results.add_button (Stock::CLOSE, RESPONSE_CLOSE);
	results.set_default_response (RESPONSE_CLOSE);
	results.set_position (Gtk::WIN_POS_MOUSE);

	results_display.show();
	list_scroller.show();
	txt.show();
	dvbox.show();
	dhbox.show();
	ddhbox.show();
	dimage->show();

	//results.get_vbox()->show();
	results.set_resizable (false);

	results.run ();

}

void
ARDOUR_UI::cleanup ()
{
	if (_session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}


	MessageDialog checker (_("Are you sure you want to clean-up?"),
				true,
				Gtk::MESSAGE_QUESTION,
				Gtk::BUTTONS_NONE);

	checker.set_title (_("Clean-up"));

	checker.set_secondary_text(_("Clean-up is a destructive operation.\n\
ALL undo/redo information will be lost if you clean-up.\n\
Clean-up will move all unused files to a \"dead\" location."));

	checker.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	checker.add_button (_("Clean-up"), RESPONSE_ACCEPT);
	checker.set_default_response (RESPONSE_CANCEL);

	checker.set_name (_("CleanupDialog"));
	checker.set_wmclass (X_("ardour_cleanup"), PROGRAM_NAME);
	checker.set_position (Gtk::WIN_POS_MOUSE);

	switch (checker.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	ARDOUR::CleanupReport rep;

	editor->prepare_for_cleanup ();

	/* do not allow flush until a session is reloaded */

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("FlushWastebasket"));
	if (act) {
		act->set_sensitive (false);
	}

	if (_session->cleanup_sources (rep)) {
		editor->finish_cleanup ();
		return;
	}

	editor->finish_cleanup ();

	checker.hide();
	display_cleanup_results (rep, _("Cleaned Files"), false);
}

void
ARDOUR_UI::flush_trash ()
{
	if (_session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}

	ARDOUR::CleanupReport rep;

	if (_session->cleanup_trash_sources (rep)) {
		return;
	}

	display_cleanup_results (rep, _("deleted file"), true);
}

void
ARDOUR_UI::setup_order_hint ()
{
	uint32_t order_hint = 0;

	/*
	  we want the new routes to have their order keys set starting from 
	  the highest order key in the selection + 1 (if available).
	*/
	if (add_route_dialog->get_transient_for () == mixer->get_toplevel()) {
		for (RouteUISelection::iterator s = mixer->selection().routes.begin(); s != mixer->selection().routes.end(); ++s) {
			if ((*s)->route()->order_key() > order_hint) {
				order_hint = (*s)->route()->order_key();
			}
		}

		if (!mixer->selection().routes.empty()) {
			order_hint++;
		}

	} else {
		for (TrackSelection::iterator s = editor->get_selection().tracks.begin(); s != editor->get_selection().tracks.end(); ++s) {
			RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (*s);
			if (tav->route()->order_key() > order_hint) {
				order_hint = tav->route()->order_key();
			}
		}

		if (!editor->get_selection().tracks.empty()) {
			order_hint++;
		}
	}

	_session->set_order_hint (order_hint);

	/* create a gap in the existing route order keys to accomodate new routes.*/

	boost::shared_ptr <RouteList> rd = _session->get_routes();
	for (RouteList::iterator ri = rd->begin(); ri != rd->end(); ++ri) {
		boost::shared_ptr<Route> rt (*ri);
			
		if (rt->is_monitor()) {
			continue;
		}

		if (rt->order_key () >= order_hint) {
			rt->set_order_key (rt->order_key () + add_route_dialog->count());
		}
	}
}

void
ARDOUR_UI::add_route (Gtk::Window* float_window)
{
	int count;

	if (!_session) {
		return;
	}

	if (add_route_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	if (float_window) {
		add_route_dialog->unset_transient_for ();
		add_route_dialog->set_transient_for (*float_window);
	}

	ResponseType r = (ResponseType) add_route_dialog->run ();

	add_route_dialog->hide();

	switch (r) {
		case RESPONSE_ACCEPT:
			break;
		default:
			return;
			break;
	}

	if ((count = add_route_dialog->count()) <= 0) {
		return;
	}

	setup_order_hint();

	PBD::ScopedConnection idle_connection;

	if (count > 8) {
		ARDOUR::GUIIdle.connect (idle_connection, MISSING_INVALIDATOR, boost::bind (&Gtkmm2ext::UI::flush_pending, this), gui_context());
	}

	string template_path = add_route_dialog->track_template();

	if (!template_path.empty()) {
		if (add_route_dialog->name_template_is_default())  {
			_session->new_route_from_template (count, template_path, string());
		} else {
			_session->new_route_from_template (count, template_path, add_route_dialog->name_template());
		}
		return;
	}

	ChanCount input_chan= add_route_dialog->channels ();
	ChanCount output_chan;
	string name_template = add_route_dialog->name_template ();
	PluginInfoPtr instrument = add_route_dialog->requested_instrument ();
	RouteGroup* route_group = add_route_dialog->route_group ();
	AutoConnectOption oac = Config->get_output_auto_connect();

	if (oac & AutoConnectMaster) {
		output_chan.set (DataType::AUDIO, (_session->master_out() ? _session->master_out()->n_inputs().n_audio() : input_chan.n_audio()));
		output_chan.set (DataType::MIDI, 0);
	} else {
		output_chan = input_chan;
	}

	/* XXX do something with name template */

	switch (add_route_dialog->type_wanted()) {
	case AddRouteDialog::AudioTrack:
		session_add_audio_track (input_chan.n_audio(), output_chan.n_audio(), add_route_dialog->mode(), route_group, count, name_template);
		break;
	case AddRouteDialog::MidiTrack:
		session_add_midi_track (route_group, count, name_template, instrument);
		break;
	case AddRouteDialog::MixedTrack:
		session_add_mixed_track (input_chan, output_chan, route_group, count, name_template, instrument);
		break;
	case AddRouteDialog::AudioBus:
		session_add_audio_bus (input_chan.n_audio(), output_chan.n_audio(), route_group, count, name_template);
		break;
	}

	/* idle connection will end at scope end */
}

void
ARDOUR_UI::stop_video_server (bool ask_confirm)
{
	if (!video_server_process && ask_confirm) {
		warning << _("Video-Server was not launched by Ardour. The request to stop it is ignored.") << endmsg;
	}
	if (video_server_process) {
		if(ask_confirm) {
			ArdourDialog confirm (_("Stop Video-Server"), true);
			Label m (_("Do you really want to stop the Video Server?"));
			confirm.get_vbox()->pack_start (m, true, true);
			confirm.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
			confirm.add_button (_("Yes, Stop It"), Gtk::RESPONSE_ACCEPT);
			confirm.show_all ();
			if (confirm.run() == RESPONSE_CANCEL) {
				return;
			}
		}
		delete video_server_process;
		video_server_process =0;
	}
}

void
ARDOUR_UI::start_video_server_menu (Gtk::Window* float_window)
{
  ARDOUR_UI::start_video_server( float_window, true);
}

bool
ARDOUR_UI::start_video_server (Gtk::Window* float_window, bool popup_msg)
{
	if (!_session) {
		return false;
	}
	if (popup_msg) {
		if (ARDOUR_UI::instance()->video_timeline->check_server()) {
			if (video_server_process) {
				popup_error(_("The Video Server is already started."));
			} else {
				popup_error(_("An external Video Server is configured and can be reached. Not starting a new instance."));
			}
		}
	}

	int firsttime = 0;
	while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
		if (firsttime++) {
			warning << _("Could not connect to the Video Server. Start it or configure its access URL in Edit -> Preferences.") << endmsg;
		}
		VideoServerDialog *video_server_dialog = new VideoServerDialog (_session);
		if (float_window) {
			video_server_dialog->set_transient_for (*float_window);
		}

		if (!Config->get_show_video_server_dialog() && firsttime < 2) {
			video_server_dialog->hide();
		} else {
			ResponseType r = (ResponseType) video_server_dialog->run ();
			video_server_dialog->hide();
			if (r != RESPONSE_ACCEPT) { return false; }
			if (video_server_dialog->show_again()) {
				Config->set_show_video_server_dialog(false);
			}
		}

		std::string icsd_exec = video_server_dialog->get_exec_path();
		std::string icsd_docroot = video_server_dialog->get_docroot();
		if (icsd_docroot.empty()) {icsd_docroot = X_("/");}

		struct stat sb;
		if (!g_lstat (icsd_docroot.c_str(), &sb) == 0 || !S_ISDIR(sb.st_mode)) {
			warning << _("Specified docroot is not an existing directory.") << endmsg;
			continue;
		}
#ifndef WIN32
		if ( (!g_lstat (icsd_exec.c_str(), &sb) == 0)
		     || (sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#else
		if ( (!g_lstat (icsd_exec.c_str(), &sb) == 0)
		     || (sb.st_mode & (S_IXUSR)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#endif

		char **argp;
		argp=(char**) calloc(9,sizeof(char*));
		argp[0] = strdup(icsd_exec.c_str());
		argp[1] = strdup("-P");
		argp[2] = (char*) calloc(16,sizeof(char)); snprintf(argp[2], 16, "%s", video_server_dialog->get_listenaddr().c_str());
		argp[3] = strdup("-p");
		argp[4] = (char*) calloc(6,sizeof(char)); snprintf(argp[4], 6, "%i", video_server_dialog->get_listenport());
		argp[5] = strdup("-C");
		argp[6] = (char*) calloc(6,sizeof(char)); snprintf(argp[6], 6, "%i", video_server_dialog->get_cachesize());
		argp[7] = strdup(icsd_docroot.c_str());
		argp[8] = 0;
		stop_video_server();

		if (icsd_docroot == X_("/")) {
			Config->set_video_advanced_setup(false);
		} else {
			std::ostringstream osstream;
			osstream << "http://localhost:" << video_server_dialog->get_listenport() << "/";
			Config->set_video_server_url(osstream.str());
			Config->set_video_server_docroot(icsd_docroot);
			Config->set_video_advanced_setup(true);
		}

		if (video_server_process) {
			delete video_server_process;
		}

		video_server_process = new SystemExec(icsd_exec, argp);
		if (video_server_process->start()) {
			warning << _("Cannot launch the video-server") << endmsg;
			continue;
		}
		int timeout = 120; // 6 sec
		while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
			Glib::usleep (50000);
			if (--timeout <= 0 || !video_server_process->is_running()) break;
		}
		if (timeout <= 0) {
			warning << _("Video-server was started but does not respond to requests...") << endmsg;
		} else {
			if (!ARDOUR_UI::instance()->video_timeline->check_server_docroot()) {
				delete video_server_process;
				video_server_process = 0;
			}
		}
	}
	return true;
}

void
ARDOUR_UI::add_video (Gtk::Window* float_window)
{
	if (!_session) {
		return;
	}

	if (!start_video_server(float_window, false)) {
		warning << _("Could not connect to the Video Server. Start it or configure its access URL in Edit -> Preferences.") << endmsg;
		return;
	}

	if (float_window) {
		add_video_dialog->set_transient_for (*float_window);
	}

	if (add_video_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	ResponseType r = (ResponseType) add_video_dialog->run ();
	add_video_dialog->hide();
	if (r != RESPONSE_ACCEPT) { return; }

	bool local_file, orig_local_file;
	std::string path = add_video_dialog->file_name(local_file);

	std::string orig_path = path;
	orig_local_file = local_file;

	bool auto_set_session_fps = add_video_dialog->auto_set_session_fps();

	if (local_file && !Glib::file_test(path, Glib::FILE_TEST_EXISTS)) {
		warning << string_compose(_("could not open %1"), path) << endmsg;
		return;
	}
	if (!local_file && path.length() == 0) {
		warning << _("no video-file selected") << endmsg;
		return;
	}

	switch (add_video_dialog->import_option()) {
		case VTL_IMPORT_TRANSCODE:
			{
				TranscodeVideoDialog *transcode_video_dialog;
				transcode_video_dialog = new TranscodeVideoDialog (_session, path);
				ResponseType r = (ResponseType) transcode_video_dialog->run ();
				transcode_video_dialog->hide();
				if (r != RESPONSE_ACCEPT) {
					delete transcode_video_dialog;
					return;
				}
				if (!transcode_video_dialog->get_audiofile().empty()) {
					editor->embed_audio_from_video(
							transcode_video_dialog->get_audiofile(),
							video_timeline->get_offset()
							);
				}
				switch (transcode_video_dialog->import_option()) {
					case VTL_IMPORT_TRANSCODED:
						path = transcode_video_dialog->get_filename();
						local_file = true;
						break;
					case VTL_IMPORT_REFERENCE:
						break;
					default:
						delete transcode_video_dialog;
						return;
				}
				delete transcode_video_dialog;
			}
			break;
		default:
		case VTL_IMPORT_NONE:
			break;
	}

	/* strip _session->session_directory().video_path() from video file if possible */
	if (local_file && !path.compare(0, _session->session_directory().video_path().size(), _session->session_directory().video_path())) {
		 path=path.substr(_session->session_directory().video_path().size());
		 if (path.at(0) == G_DIR_SEPARATOR) {
			 path=path.substr(1);
		 }
	}

	video_timeline->set_update_session_fps(auto_set_session_fps);
	if (video_timeline->video_file_info(path, local_file)) {
		XMLNode* node = new XMLNode(X_("Videotimeline"));
		node->add_property (X_("Filename"), path);
		node->add_property (X_("AutoFPS"), auto_set_session_fps?X_("1"):X_("0"));
		node->add_property (X_("LocalFile"), local_file?X_("1"):X_("0"));
		if (orig_local_file) {
			node->add_property (X_("OriginalVideoFile"), orig_path);
		} else {
			node->remove_property (X_("OriginalVideoFile"));
		}
		_session->add_extra_xml (*node);
		_session->set_dirty ();

		_session->maybe_update_session_range(
			std::max(video_timeline->get_offset(), (ARDOUR::frameoffset_t) 0),
			std::max(video_timeline->get_offset() + video_timeline->get_duration(), (ARDOUR::frameoffset_t) 0));


		if (add_video_dialog->launch_xjadeo() && local_file) {
			editor->set_xjadeo_sensitive(true);
			editor->toggle_xjadeo_proc(1);
		} else {
			editor->toggle_xjadeo_proc(0);
		}
		editor->toggle_ruler_video(true);
	}
}

void
ARDOUR_UI::remove_video ()
{
	video_timeline->close_session();
	editor->toggle_ruler_video(false);

	/* reset state */
	video_timeline->set_offset_locked(false);
	video_timeline->set_offset(0);

	/* delete session state */
	XMLNode* node = new XMLNode(X_("Videotimeline"));
	_session->add_extra_xml(*node);
	node = new XMLNode(X_("Videomonitor"));
	_session->add_extra_xml(*node);
	stop_video_server();
}

void
ARDOUR_UI::flush_videotimeline_cache (bool localcacheonly)
{
	if (localcacheonly) {
		video_timeline->vmon_update();
	} else {
		video_timeline->flush_cache();
	}
	editor->queue_visual_videotimeline_update();
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
ARDOUR_UI::create_xrun_marker (framepos_t where)
{
	editor->mouse_add_new_marker (where, false, true);
}

void
ARDOUR_UI::halt_on_xrun_message ()
{
	MessageDialog msg (*editor,
			   _("Recording was stopped because your system could not keep up."));
	msg.run ();
}

void
ARDOUR_UI::xrun_handler (framepos_t where)
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::xrun_handler, where)

	if (_session && Config->get_create_xrun_marker() && _session->actively_recording()) {
		create_xrun_marker(where);
	}

	if (_session && Config->get_stop_recording_on_xrun() && _session->actively_recording()) {
		halt_on_xrun_message ();
	}
}

void
ARDOUR_UI::disk_overrun_handler ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::disk_overrun_handler)

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		MessageDialog* msg = new MessageDialog (*editor, string_compose (_("\
The disk system on your computer\n\
was not able to keep up with %1.\n\
\n\
Specifically, it failed to write data to disk\n\
quickly enough to keep up with recording.\n"), PROGRAM_NAME));
		msg->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show ();
	}
}

void
ARDOUR_UI::disk_underrun_handler ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::disk_underrun_handler)

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		MessageDialog* msg = new MessageDialog (
			*editor, string_compose (_("The disk system on your computer\n\
was not able to keep up with %1.\n\
\n\
Specifically, it failed to read data from disk\n\
quickly enough to keep up with playback.\n"), PROGRAM_NAME));
		msg->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show ();
	}
}

void
ARDOUR_UI::disk_speed_dialog_gone (int /*ignored_response*/, MessageDialog* msg)
{
	have_disk_speed_dialog_displayed = false;
	delete msg;
}

void
ARDOUR_UI::session_dialog (std::string msg)
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::session_dialog, msg)

	MessageDialog* d;

	if (editor) {
		d = new MessageDialog (*editor, msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	} else {
		d = new MessageDialog (msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	}

	d->show_all ();
	d->run ();
	delete d;
}

int
ARDOUR_UI::pending_state_dialog ()
{
	HBox* hbox = manage (new HBox());
	Image* image = manage (new Image (Stock::DIALOG_QUESTION, ICON_SIZE_DIALOG));
	ArdourDialog dialog (_("Crash Recovery"), true);
	Label  message (string_compose (_("\
This session appears to have been in the\n\
middle of recording when %1 or\n\
the computer was shutdown.\n\
\n\
%1 can recover any captured audio for\n\
you, or it can ignore it. Please decide\n\
what you would like to do.\n"), PROGRAM_NAME));
	image->set_alignment(ALIGN_CENTER, ALIGN_TOP);
	hbox->pack_start (*image, PACK_EXPAND_WIDGET, 12);
	hbox->pack_end (message, PACK_EXPAND_PADDING, 12);
	dialog.get_vbox()->pack_start(*hbox, PACK_EXPAND_PADDING, 6);
	dialog.add_button (_("Ignore crash data"), RESPONSE_REJECT);
	dialog.add_button (_("Recover from crash"), RESPONSE_ACCEPT);
	dialog.set_default_response (RESPONSE_ACCEPT);
	dialog.set_position (WIN_POS_CENTER);
	message.show();
	image->show();
	hbox->show();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		return 1;
	default:
		return 0;
	}
}

int
ARDOUR_UI::sr_mismatch_dialog (framecnt_t desired, framecnt_t actual)
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

int
ARDOUR_UI::disconnect_from_engine ()
{
	/* drop connection to AudioEngine::Halted so that we don't act
	 *  as if the engine unexpectedly shut down
	 */

	halt_connection.disconnect ();
	
	if (AudioEngine::instance()->stop ()) {
		MessageDialog msg (*editor, _("Could not disconnect from Audio/MIDI engine"));
		msg.run ();
		return -1;
	} else {
		AudioEngine::instance()->Halted.connect_same_thread (halt_connection, boost::bind (&ARDOUR_UI::engine_halted, this, _1, false));
	}
	
	update_sample_rate (0);
	return 0;
}

int
ARDOUR_UI::reconnect_to_engine ()
{
	if (AudioEngine::instance()->start ()) {
		if (editor) {
			MessageDialog msg (*editor,  _("Could not reconnect to the Audio/MIDI engine"));
			msg.run ();
		} else {
			MessageDialog msg (_("Could not reconnect to the Audio/MIDI engine"));
			msg.run ();
		}
		return -1;
	}
	
	update_sample_rate (0);
	return 0;
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
ARDOUR_UI::update_transport_clocks (framepos_t pos)
{
	if (Config->get_primary_clock_delta_edit_cursor()) {
		primary_clock->set (pos, false, editor->get_preferred_edit_position());
	} else {
		primary_clock->set (pos);
	}

	if (Config->get_secondary_clock_delta_edit_cursor()) {
		secondary_clock->set (pos, false, editor->get_preferred_edit_position());
	} else {
		secondary_clock->set (pos);
	}

	if (big_clock_window) {
		big_clock->set (pos);
	}
	ARDOUR_UI::instance()->video_timeline->manual_seek_video_monitor(pos);
}

void
ARDOUR_UI::step_edit_status_change (bool yn)
{
	// XXX should really store pre-step edit status of things
	// we make insensitive

	if (yn) {
		rec_button.set_active_state (Gtkmm2ext::ImplicitActive);
		rec_button.set_sensitive (false);
	} else {
		rec_button.unset_active_state ();;
		rec_button.set_sensitive (true);
	}
}

void
ARDOUR_UI::record_state_changed ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::record_state_changed);

	if (!_session || !big_clock_window) {
		/* why bother - the clock isn't visible */
		return;
	}

	if (_session->record_status () == Session::Recording && _session->have_rec_enabled_track ()) {
		big_clock->set_active (true);
	} else {
		big_clock->set_active (false);
	}
}

bool
ARDOUR_UI::first_idle ()
{
	if (_session) {
		_session->allow_auto_play (true);
	}

	if (editor) {
		editor->first_idle();
	}

	Keyboard::set_can_save_keybindings (true);
	return false;
}

void
ARDOUR_UI::store_clock_modes ()
{
	XMLNode* node = new XMLNode(X_("ClockModes"));

	for (vector<AudioClock*>::iterator x = AudioClock::clocks.begin(); x != AudioClock::clocks.end(); ++x) {
		XMLNode* child = new XMLNode (X_("Clock"));
		
		child->add_property (X_("name"), (*x)->name());
		child->add_property (X_("mode"), enum_2_string ((*x)->mode()));
		child->add_property (X_("on"), ((*x)->off() ? X_("no") : X_("yes")));

		node->add_child_nocopy (*child);
	}

	_session->add_extra_xml (*node);
	_session->set_dirty ();
}

ARDOUR_UI::TransportControllable::TransportControllable (std::string name, ARDOUR_UI& u, ToggleType tp)
	: Controllable (name), ui (u), type(tp)
{

}

void
ARDOUR_UI::TransportControllable::set_value (double val)
{
	if (val < 0.5) {
		/* do nothing: these are radio-style actions */
		return;
	}

	const char *action = 0;

	switch (type) {
	case Roll:
		action = X_("Roll");
		break;
	case Stop:
		action = X_("Stop");
		break;
	case GotoStart:
		action = X_("GotoStart");
		break;
	case GotoEnd:
		action = X_("GotoEnd");
		break;
	case AutoLoop:
		action = X_("Loop");
		break;
	case PlaySelection:
		action = X_("PlaySelection");
		break;
	case RecordEnable:
		action = X_("Record");
		break;
	default:
		break;
	}

	if (action == 0) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", action);

	if (act) {
		act->activate ();
	}
}

double
ARDOUR_UI::TransportControllable::get_value (void) const
{
	float val = 0.0;

	switch (type) {
	case Roll:
		break;
	case Stop:
		break;
	case GotoStart:
		break;
	case GotoEnd:
		break;
	case AutoLoop:
		break;
	case PlaySelection:
		break;
	case RecordEnable:
		break;
	default:
		break;
	}

	return val;
}

void
ARDOUR_UI::setup_profile ()
{
	if (gdk_screen_width() < 1200 || getenv ("ARDOUR_NARROW_SCREEN")) {
		Profile->set_small_screen ();
	}

	if (getenv ("ARDOUR_SAE")) {
		Profile->set_sae ();
		Profile->set_single_package ();
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

/** Allocate our thread-local buffers */
void
ARDOUR_UI::get_process_buffers ()
{
	_process_thread->get_buffers ();
}

/** Drop our thread-local buffers */
void
ARDOUR_UI::drop_process_buffers ()
{
	_process_thread->drop_buffers ();
}

void
ARDOUR_UI::feedback_detected ()
{
	_feedback_exists = true;
}

void
ARDOUR_UI::successful_graph_sort ()
{
	_feedback_exists = false;
}

void
ARDOUR_UI::midi_panic ()
{
	if (_session) {
		_session->midi_panic();
	}
}

void
ARDOUR_UI::session_format_mismatch (std::string xml_path, std::string backup_path)
{
	const char* start_big = "<span size=\"x-large\" weight=\"bold\">";
	const char* end_big = "</span>";
	const char* start_mono = "<tt>";
	const char* end_mono = "</tt>";

	MessageDialog msg (string_compose (_("%4This is a session from an older version of %3%5\n\n"
					     "%3 has copied the old session file\n\n%6%1%7\n\nto\n\n%6%2%7\n\n"
					     "From now on, use the -2000 version with older versions of %3"),
					   xml_path, backup_path, PROGRAM_NAME,
					   start_big, end_big,
					   start_mono, end_mono), true);

	msg.run ();
}


void
ARDOUR_UI::reset_peak_display ()
{
	if (!_session || !_session->master_out() || !editor_meter) return;
	editor_meter->clear_meters();
	editor_meter_max_peak = -INFINITY;
	editor_meter_peak_display.set_name ("meterbridge peakindicator");
	editor_meter_peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
}

void
ARDOUR_UI::reset_group_peak_display (RouteGroup* group)
{
	if (!_session || !_session->master_out()) return;
	if (group == _session->master_out()->route_group()) {
		reset_peak_display ();
	}
}

void
ARDOUR_UI::reset_route_peak_display (Route* route)
{
	if (!_session || !_session->master_out()) return;
	if (_session->master_out().get() == route) {
		reset_peak_display ();
	}
}

int
ARDOUR_UI::do_audio_midi_setup (uint32_t desired_sample_rate)
{
	audio_midi_setup->set_desired_sample_rate (desired_sample_rate);
	audio_midi_setup->set_position (WIN_POS_CENTER);

	switch (audio_midi_setup->run()) {
	case Gtk::RESPONSE_OK:
		return 0;
	case Gtk::RESPONSE_APPLY:
		return 0;
	default:
		return -1;
	}
}


