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
#include "gtk2ardour-version.h"
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cerrno>

#include <stdarg.h>

#ifndef PLATFORM_WINDOWS
#include <sys/resource.h>
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <stdint.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <gtkmm/messagedialog.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/stock.h>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_archive.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/openuri.h"
#include "pbd/stl_delete.h"
#include "pbd/file_utils.h"
#include "pbd/localtime_r.h"
#include "pbd/pthread_utils.h"
#include "pbd/replace_all.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/xml++.h"

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
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/automation_watch.h"
#include "ardour/diskstream.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/ltc_file_reader.h"
#include "ardour/midi_track.h"
#include "ardour/port.h"
#include "ardour/plugin_manager.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/recent_sessions.h"
#include "ardour/record_enable_control.h"
#include "ardour/session_directory.h"
#include "ardour/session_route.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_utils.h"
#include "ardour/source_factory.h"
#include "ardour/slave.h"
#include "ardour/system_exec.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"
#include "ardour/utils.h"

#include "LuaBridge/LuaBridge.h"

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif
#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#endif

// fix for OSX (nsm.h has a check function, AU/Apple defines check)
#ifdef check
#undef check
#endif

#include "timecode/time.h"

typedef uint64_t microseconds_t;

#include "about.h"
#include "editing.h"
#include "actions.h"
#include "add_route_dialog.h"
#include "ambiguous_file_dialog.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "audio_region_view.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "duplicate_routes_dialog.h"
#include "debug.h"
#include "engine_dialog.h"
#include "export_video_dialog.h"
#include "export_video_infobox.h"
#include "gain_meter.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "idleometer.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "lua_script_manager.h"
#include "luawindow.h"
#include "main_clock.h"
#include "missing_file_dialog.h"
#include "missing_plugin_dialog.h"
#include "mixer_ui.h"
#include "meterbridge.h"
#include "meter_patterns.h"
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
#include "save_as_dialog.h"
#include "script_selector.h"
#include "session_archive_dialog.h"
#include "session_dialog.h"
#include "session_metadata_dialog.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "splash.h"
#include "startup.h"
#include "time_axis_view_item.h"
#include "time_info_box.h"
#include "timers.h"
#include "utils.h"
#include "utils_videotl.h"
#include "video_server_dialog.h"
#include "add_video_dialog.h"
#include "transcode_video_dialog.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;
using namespace Editing;

ARDOUR_UI *ARDOUR_UI::theArdourUI = 0;

sigc::signal<void, framepos_t, bool, framepos_t> ARDOUR_UI::Clock;
sigc::signal<void>      ARDOUR_UI::CloseAllDialogs;

static bool
ask_about_configuration_copy (string const & old_dir, string const & new_dir, int version)
{
	MessageDialog msg (string_compose (_("%1 %2.x has discovered configuration files from %1 %3.x.\n\n"
	                                     "Would you like these files to be copied and used for %1 %2.x?\n\n"
	                                     "(This will require you to restart %1.)"),
	                                   PROGRAM_NAME, PROGRAM_VERSION, version),
	                   false, /* no markup */
	                   Gtk::MESSAGE_INFO,
	                   Gtk::BUTTONS_YES_NO,
	                   true /* modal, though it hardly matters since it is the only window */
		);

	msg.set_default_response (Gtk::RESPONSE_YES);
	msg.show_all ();

	return (msg.run() == Gtk::RESPONSE_YES);
}

static void
libxml_generic_error_func (void* /* parsing_context*/,
                   const char* msg,
                   ...)
{
	va_list ap;
	char buf[2048];

	va_start (ap, msg);
	vsnprintf (buf, sizeof (buf), msg, ap);
	error << buf << endmsg;
	va_end (ap);
}

static void
libxml_structured_error_func (void* /* parsing_context*/,
                              xmlErrorPtr err)
{
	string msg;

	if (err->message)
		msg = err->message;

	replace_all (msg, "\n", "");

	if (!msg.empty()) {
		if (err->file && err->line) {
			error << X_("XML error: ") << msg << " in " << err->file << " at line " << err->line;

			if (err->int2) {
				error << ':' << err->int2;
			}

			error << endmsg;
		} else {
			error << X_("XML error: ") << msg << endmsg;
		}
	}
}


ARDOUR_UI::ARDOUR_UI (int *argcp, char **argvp[], const char* localedir)
	: Gtkmm2ext::UI (PROGRAM_NAME, X_("gui"), argcp, argvp)
	, session_loaded (false)
	, gui_object_state (new GUIObjectState)
	, primary_clock   (new MainClock (X_("primary"),   X_("transport"), true ))
	, secondary_clock (new MainClock (X_("secondary"), X_("secondary"), false))
	, big_clock (new AudioClock (X_("bigclock"), false, "big", true, true, false, false))
	, video_timeline(0)
	, global_actions (X_("global"))
	, ignore_dual_punch (false)
	, main_window_visibility (0)
	, editor (0)
	, mixer (0)
	, nsm (0)
	, _was_dirty (false)
	, _mixer_on_top (false)
	, _initial_verbose_plugin_scan (false)
	, first_time_engine_run (true)
	, secondary_clock_spacer (0)
	, roll_controllable (new TransportControllable ("transport roll", *this, TransportControllable::Roll))
	, stop_controllable (new TransportControllable ("transport stop", *this, TransportControllable::Stop))
	, goto_start_controllable (new TransportControllable ("transport goto start", *this, TransportControllable::GotoStart))
	, goto_end_controllable (new TransportControllable ("transport goto end", *this, TransportControllable::GotoEnd))
	, auto_loop_controllable (new TransportControllable ("transport auto loop", *this, TransportControllable::AutoLoop))
	, play_selection_controllable (new TransportControllable ("transport play selection", *this, TransportControllable::PlaySelection))
	, rec_controllable (new TransportControllable ("transport rec-enable", *this, TransportControllable::RecordEnable))
	, auto_input_button (ArdourButton::led_default_elements)
	, time_info_box (0)
	, auto_return_button (ArdourButton::led_default_elements)
	, follow_edits_button (ArdourButton::led_default_elements)
	, auditioning_alert_button (_("Audition"))
	, solo_alert_button (_("Solo"))
	, feedback_alert_button (_("Feedback"))
	, error_alert_button ( ArdourButton::just_led_default_elements )
	, editor_meter(0)
	, editor_meter_peak_display()
	, _suspend_editor_meter_callbacks (false)
	, _numpad_locate_happening (false)
	, _session_is_new (false)
	, last_key_press_time (0)
	, save_as_dialog (0)
	, meterbridge (0)
	, luawindow (0)
	, rc_option_editor (0)
	, speaker_config_window (X_("speaker-config"), _("Speaker Configuration"))
	, add_route_dialog (X_("add-routes"), _("Add Tracks/Busses"))
	, about (X_("about"), _("About"))
	, location_ui (X_("locations"), S_("Ranges|Locations"))
	, route_params (X_("inspector"), _("Tracks and Busses"))
	, audio_midi_setup (X_("audio-midi-setup"), _("Audio/MIDI Setup"))
	, export_video_dialog (X_("video-export"), _("Video Export Dialog"))
	, lua_script_window (X_("script-manager"), _("Script Manager"))
	, idleometer (X_("idle-o-meter"), _("Idle'o'Meter"))
	, session_option_editor (X_("session-options-editor"), _("Properties"), boost::bind (&ARDOUR_UI::create_session_option_editor, this))
	, add_video_dialog (X_("add-video"), _("Add Video"), boost::bind (&ARDOUR_UI::create_add_video_dialog, this))
	, bundle_manager (X_("bundle-manager"), _("Bundle Manager"), boost::bind (&ARDOUR_UI::create_bundle_manager, this))
	, big_clock_window (X_("big-clock"), _("Big Clock"), boost::bind (&ARDOUR_UI::create_big_clock_window, this))
	, audio_port_matrix (X_("audio-connection-manager"), _("Audio Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::AUDIO))
	, midi_port_matrix (X_("midi-connection-manager"), _("MIDI Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::MIDI))
	, key_editor (X_("key-editor"), _("Keyboard Shortcuts"), boost::bind (&ARDOUR_UI::create_key_editor, this))
	, video_server_process (0)
	, splash (0)
	, have_configure_timeout (false)
	, last_configure_time (0)
	, last_peak_grab (0)
	, have_disk_speed_dialog_displayed (false)
	, _status_bar_visibility (X_("status-bar"))
	, _feedback_exists (false)
	, _log_not_acknowledged (LogLevelNone)
	, duplicate_routes_dialog (0)
	, editor_visibility_button (S_("Window|Editor"))
	, mixer_visibility_button (S_("Window|Mixer"))
	, prefs_visibility_button (S_("Window|Preferences"))
{
	Gtkmm2ext::init (localedir);

	UIConfiguration::instance().post_gui_init ();

	if (ARDOUR::handle_old_configuration_files (boost::bind (ask_about_configuration_copy, _1, _2, _3))) {
		{
			/* "touch" the been-here-before path now that config has been migrated */
			PBD::ScopedFileDescriptor fout (g_open (been_here_before_path ().c_str(), O_CREAT|O_TRUNC|O_RDWR, 0666));
		}
		MessageDialog msg (string_compose (_("Your configuration files were copied. You can now restart %1."), PROGRAM_NAME), true);
		msg.run ();
		/* configuration was modified, exit immediately */
		_exit (0);
	}


	if (string (VERSIONSTRING).find (".pre") != string::npos) {
		/* check this is not being run from ./ardev etc. */
		if (!running_from_source_tree ()) {
			pre_release_dialog ();
		}
	}

	if (theArdourUI == 0) {
		theArdourUI = this;
	}

	/* track main window visibility */

	main_window_visibility = new VisibilityTracker (_main_window);

	/* stop libxml from spewing to stdout/stderr */

	xmlSetGenericErrorFunc (this, libxml_generic_error_func);
	xmlSetStructuredErrorFunc (this, libxml_structured_error_func);

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &ARDOUR_UI::parameter_changed));
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	UIConfiguration::instance().map_parameters (pc);

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

	/* handle sr mismatch with a dialog - cross-thread from engine */
	ARDOUR::Session::NotifyAboutSampleRateMismatch.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::sr_mismatch_message, this, _1, _2), gui_context ());

	/* handle requests to quit (coming from JACK session) */

	ARDOUR::Session::Quit.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::finish, this), gui_context ());

	/* tell the user about feedback */

	ARDOUR::Session::FeedbackDetected.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::feedback_detected, this), gui_context ());
	ARDOUR::Session::SuccessfulGraphSort.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::successful_graph_sort, this), gui_context ());

	/* handle requests to deal with missing files */

	ARDOUR::Session::MissingFile.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::missing_file, this, _1, _2, _3));

	/* and ambiguous files */

	ARDOUR::FileSource::AmbiguousFileName.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::ambiguous_file, this, _1, _2));

	/* also plugin scan messages */
	ARDOUR::PluginScanMessage.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::plugin_scan_dialog, this, _1, _2, _3), gui_context());
	ARDOUR::PluginScanTimeout.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::plugin_scan_timeout, this, _1), gui_context());

	ARDOUR::GUIIdle.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::gui_idle_handler, this), gui_context());

	Config->ParameterChanged.connect ( forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::set_flat_buttons, this), gui_context() );
	set_flat_buttons();

	/* lets get this party started */

	setup_gtk_ardour_enums ();
	setup_profile ();

	SessionEvent::create_per_thread_pool ("GUI", 4096);

	/* we like keyboards */

	keyboard = new ArdourKeyboard(*this);

	XMLNode* node = ARDOUR_UI::instance()->keyboard_settings();
	if (node) {
		keyboard->set_state (*node, Stateful::loading_state_version);
	}

	UIConfiguration::instance().reset_dpi ();

	TimeAxisViewItem::set_constant_heights ();

        /* Set this up so that our window proxies can register actions */

	ActionManager::init ();

	/* The following must happen after ARDOUR::init() so that Config is set up */

	const XMLNode* ui_xml = Config->extra_xml (X_("UI"));

	if (ui_xml) {
		key_editor.set_state (*ui_xml, 0);
		session_option_editor.set_state (*ui_xml, 0);
		speaker_config_window.set_state (*ui_xml, 0);
		about.set_state (*ui_xml, 0);
		add_route_dialog.set_state (*ui_xml, 0);
		add_video_dialog.set_state (*ui_xml, 0);
		route_params.set_state (*ui_xml, 0);
		bundle_manager.set_state (*ui_xml, 0);
		location_ui.set_state (*ui_xml, 0);
		big_clock_window.set_state (*ui_xml, 0);
		audio_port_matrix.set_state (*ui_xml, 0);
		midi_port_matrix.set_state (*ui_xml, 0);
		export_video_dialog.set_state (*ui_xml, 0);
		lua_script_window.set_state (*ui_xml, 0);
		idleometer.set_state (*ui_xml, 0);
	}

	/* Separate windows */

	WM::Manager::instance().register_window (&key_editor);
	WM::Manager::instance().register_window (&session_option_editor);
	WM::Manager::instance().register_window (&speaker_config_window);
	WM::Manager::instance().register_window (&about);
	WM::Manager::instance().register_window (&add_route_dialog);
	WM::Manager::instance().register_window (&add_video_dialog);
	WM::Manager::instance().register_window (&route_params);
	WM::Manager::instance().register_window (&audio_midi_setup);
	WM::Manager::instance().register_window (&export_video_dialog);
	WM::Manager::instance().register_window (&lua_script_window);
	WM::Manager::instance().register_window (&bundle_manager);
	WM::Manager::instance().register_window (&location_ui);
	WM::Manager::instance().register_window (&big_clock_window);
	WM::Manager::instance().register_window (&audio_port_matrix);
	WM::Manager::instance().register_window (&midi_port_matrix);
	WM::Manager::instance().register_window (&idleometer);

	/* do not retain position for add route dialog */
	add_route_dialog.set_state_mask (WindowProxy::Size);

	/* Trigger setting up the color scheme and loading the GTK RC file */

	UIConfiguration::instance().load_rc_file (false);

	_process_thread = new ProcessThread ();
	_process_thread->init ();

	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &ARDOUR_UI::resize_text_widgets));

	attach_to_engine ();
}

void
ARDOUR_UI::pre_release_dialog ()
{
	ArdourDialog d (_("Pre-Release Warning"), true, false);
	d.add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

        Label* label = manage (new Label);
        label->set_markup (string_compose (_("<b>Welcome to this pre-release build of %1 %2</b>\n\n\
There are still several issues and bugs to be worked on,\n\
as well as general workflow improvements, before this can be considered\n\
release software. So, a few guidelines:\n\
\n\
1) Please do <b>NOT</b> use this software with the expectation that it is stable or reliable\n\
   though it may be so, depending on your workflow.\n\
2) Please wait for a helpful writeup of new features.\n\
3) <b>Please do NOT use the forums at ardour.org to report issues</b>.\n\
4) Please <b>DO</b> use the bugtracker at http://tracker.ardour.org/ to report issues\n\
   making sure to note the product version number as 5.0-pre.\n\
5) Please <b>DO</b> use the ardour-users mailing list to discuss ideas and pass on comments.\n\
6) Please <b>DO</b> join us on IRC for real time discussions about %1 %2. You\n\
   can get there directly from within the program via the Help->Chat menu option.\n\
\n\
Full information on all the above can be found on the support page at\n\
\n\
                http://ardour.org/support\n\
"), PROGRAM_NAME, VERSIONSTRING));

        d.get_vbox()->set_border_width (12);
        d.get_vbox()->pack_start (*label, false, false, 12);
        d.get_vbox()->show_all ();

        d.run ();
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
	update_sample_rate (0);
	update_cpu_load ();
}

void
ARDOUR_UI::engine_running ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::engine_running)
	if (first_time_engine_run) {
		post_engine();
		first_time_engine_run = false;
	}

	if (_session) {
		_session->reset_xrun_count ();
	}
	update_disk_space ();
	update_cpu_load ();
	update_xrun_count ();
	update_sample_rate (AudioEngine::instance()->sample_rate());
	update_timecode_format ();
	update_peak_thread_work ();
	ActionManager::set_sensitive (ActionManager::engine_sensitive_actions, true);
	ActionManager::set_sensitive (ActionManager::engine_opposite_sensitive_actions, false);
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
		Gtkmm2ext::UI::instance()->call_slot (MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_halted, this, copy, true));
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

	MessageDialog msg (_main_window, msgstr);
	pop_back_splash (msg);
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
#ifdef AUDIOUNIT_SUPPORT
	std::string au_msg;
	if (AUPluginInfo::au_get_crashlog(au_msg)) {
		popup_error(_("Audio Unit Plugin Scan Failed. Automatic AU scanning has been disabled. Please see the log window for further details."));
		error << _("Audio Unit Plugin Scan Failed:") << endmsg;
		info << au_msg << endmsg;
	}
#endif

	ARDOUR::init_post_engine ();

	/* connect to important signals */

	AudioEngine::instance()->Stopped.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_stopped, this), gui_context());
	AudioEngine::instance()->SampleRateChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate, this, _1), gui_context());
	AudioEngine::instance()->BufferSizeChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate, this, _1), gui_context());
	AudioEngine::instance()->Halted.connect_same_thread (halt_connection, boost::bind (&ARDOUR_UI::engine_halted, this, _1, false));
	AudioEngine::instance()->BecameSilent.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::audioengine_became_silent, this), gui_context());

	if (setup_windows ()) {
		throw failed_constructor ();
	}

	/* Do this after setup_windows (), as that's when the _status_bar_visibility is created */
	XMLNode* n = Config->extra_xml (X_("UI"));
	if (n) {
		_status_bar_visibility.set_state (*n);
	}

	check_memory_locking();

	/* this is the first point at which all the possible actions are
	 * available, because some of the available actions are dependent on
	 * aspects of the engine/backend.
	 */

	if (ARDOUR_COMMAND_LINE::show_key_actions) {


		vector<string> paths;
		vector<string> labels;
		vector<string> tooltips;
		vector<string> keys;
		vector<Glib::RefPtr<Gtk::Action> > actions;

		Gtkmm2ext::ActionMap::get_all_actions (paths, labels, tooltips, keys, actions);

		vector<string>::iterator k;
		vector<string>::iterator p;

		for (p = paths.begin(), k = keys.begin(); p != paths.end(); ++k, ++p) {

			if ((*k).empty()) {
				cout << *p << endl;
			} else {
				cout << *p << " => " << *k << endl;
			}
		}

		halt_connection.disconnect ();
		AudioEngine::instance()->stop ();
		exit (0);
	}

	/* this being a GUI and all, we want peakfiles */

	AudioFileSource::set_build_peakfiles (true);
	AudioFileSource::set_build_missing_peakfiles (true);

	/* set default clock modes */

	primary_clock->set_mode (AudioClock::Timecode);
	secondary_clock->set_mode (AudioClock::BBT);

	/* start the time-of-day-clock */

#ifndef __APPLE__
	/* OS X provides a nearly-always visible wallclock, so don't be stupid */
	update_wall_clock ();
	Glib::signal_timeout().connect_seconds (sigc::mem_fun(*this, &ARDOUR_UI::update_wall_clock), 1);
#endif

	{
		DisplaySuspender ds;
		Config->ParameterChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::parameter_changed, this, _1), gui_context());
		boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
		Config->map_parameters (pc);

		UIConfiguration::instance().map_parameters (pc);
	}
}

ARDOUR_UI::~ARDOUR_UI ()
{
	UIConfiguration::instance().save_state();

	stop_video_server();

	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother at 'real' exit. the OS cleans up for us.
		delete big_clock; big_clock = 0;
		delete primary_clock; primary_clock = 0;
		delete secondary_clock; secondary_clock = 0;
		delete _process_thread; _process_thread = 0;
		delete time_info_box; time_info_box = 0;
		delete meterbridge; meterbridge = 0;
		delete luawindow; luawindow = 0;
		delete editor; editor = 0;
		delete mixer; mixer = 0;
		delete rc_option_editor; rc_option_editor = 0; // failed to wrap object warning
		delete nsm; nsm = 0;
		delete gui_object_state; gui_object_state = 0;
		delete main_window_visibility;
		FastMeter::flush_pattern_cache ();
		PixFader::flush_pattern_cache ();
	}

#ifndef NDEBUG
	/* Small trick to flush main-thread event pool.
	 * Other thread-pools are destroyed at pthread_exit(),
	 * but tmain thread termination is too late to trigger Pool::~Pool()
	 */
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Clear, SessionEvent::Immediate, 0, 0); // get the pool reference, values don't matter since the event is never queued.
	delete ev->event_pool();
#endif
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
	XMLProperty const * prop;

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
		shuttle_box.controllable()->set_id (prop->value());
	}
}

XMLNode&
ARDOUR_UI::get_transport_controllable_state ()
{
	XMLNode* node = new XMLNode(X_("TransportControllables"));

	node->add_property (X_("roll"), roll_controllable->id().to_s());
	node->add_property (X_("stop"), stop_controllable->id().to_s());
	node->add_property (X_("goto_start"), goto_start_controllable->id().to_s());
	node->add_property (X_("goto_end"), goto_end_controllable->id().to_s());
	node->add_property (X_("auto_loop"), auto_loop_controllable->id().to_s());
	node->add_property (X_("play_selection"), play_selection_controllable->id().to_s());
	node->add_property (X_("rec"), rec_controllable->id().to_s());
	node->add_property (X_("shuttle"), shuttle_box.controllable()->id().to_s());

	return *node;
}

void
ARDOUR_UI::save_session_at_its_request (std::string snapshot_name)
{
	if (_session) {
		_session->save_state (snapshot_name);
	}
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
ARDOUR_UI::session_dirty_changed ()
{
	update_autosave ();
	update_title ();
}

void
ARDOUR_UI::update_autosave ()
{
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

static bool
_hide_splash (gpointer arg)
{
	((ARDOUR_UI*)arg)->hide_splash();
	return false;
}

int
ARDOUR_UI::starting ()
{
	Application* app = Application::instance ();
	const char *nsm_url;
	bool brand_new_user = ArdourStartup::required ();

	app->ShouldQuit.connect (sigc::mem_fun (*this, &ARDOUR_UI::queue_finish));
	app->ShouldLoad.connect (sigc::mem_fun (*this, &ARDOUR_UI::load_from_application_api));

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
		std::cerr << "audio-midi engine setup failed."<< std::endl;
		return -1;
	}

	if ((nsm_url = g_getenv ("NSM_URL")) != 0) {
		nsm = new NSM_Client;
		if (!nsm->init (nsm_url)) {
			/* the ardour executable may have different names:
			 *
			 * waf's obj.target for distro versions: eg ardour4, ardourvst4
			 * Ardour4, Mixbus3 for bundled versions + full path on OSX & windows
			 * argv[0] does not apply since we need the wrapper-script (not the binary itself)
			 *
			 * The wrapper startup script should set the environment variable 'ARDOUR_SELF'
			 */
			const char *process_name = g_getenv ("ARDOUR_SELF");
			nsm->announce (PROGRAM_NAME, ":dirty:", process_name ? process_name : "ardour4");

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
			_initial_verbose_plugin_scan = true;
			ArdourStartup s;
			s.present ();
			main().run();
			s.hide ();
			_initial_verbose_plugin_scan = false;
			switch (s.response ()) {
			case Gtk::RESPONSE_OK:
				break;
			default:
				return -1;
			}
		}

		// TODO: maybe IFF brand_new_user
		if (ARDOUR::Profile->get_mixbus () && Config->get_copy_demo_sessions ()) {
			std::string dspd (Config->get_default_session_parent_dir());
			Searchpath ds (ARDOUR::ardour_data_search_path());
			ds.add_subdirectory_to_paths ("sessions");
			vector<string> demos;
			find_files_matching_pattern (demos, ds, "*.tar.xz");

			ARDOUR::RecentSessions rs;
			ARDOUR::read_recent_sessions (rs);

			for (vector<string>::iterator i = demos.begin(); i != demos.end (); ++i) {
				/* "demo-session" must be inside "demo-session.tar.xz"
				 * strip ".tar.xz"
				 */
				std::string name = basename_nosuffix (basename_nosuffix (*i));
				std::string path = Glib::build_filename (dspd, name);
				/* skip if session-dir already exists */
				if (Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR)) {
					continue;
				}
				/* skip sessions that are already in 'recent'.
				 * eg. a new user changed <session-default-dir> shorly after installation
				 */
				for (ARDOUR::RecentSessions::iterator r = rs.begin(); r != rs.end(); ++r) {
					if ((*r).first == name) {
						continue;
					}
				}
				try {
					PBD::FileArchive ar (*i);
					if (0 == ar.inflate (dspd)) {
						store_recent_sessions (name, path);
						info << string_compose (_("Copied Demo Session %1."), name) << endmsg;
					}
				} catch (...) {}
			}
		}

#ifdef NO_PLUGIN_STATE

		ARDOUR::RecentSessions rs;
		ARDOUR::read_recent_sessions (rs);

		string path = Glib::build_filename (user_config_directory(), ".iknowaboutfreeversion");

		if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS) && !rs.empty()) {

			/* already used Ardour, have sessions ... warn about plugin state */

			ArdourDialog d (_("Free/Demo Version Warning"), true);
			Label l;
			Button b (string_compose (_("Subscribe and support development of %1"), PROGRAM_NAME));
			CheckButton c (_("Don't warn me about this again"));

			l.set_markup (string_compose (_("<span weight=\"bold\" size=\"large\">%1</span>\n\n<b>%2</b>\n\n<i>%3</i>\n\n%4"),
			                              string_compose (_("This is a free/demo version of %1"), PROGRAM_NAME),
			                              _("It will not restore OR save any plugin settings"),
			                              _("If you load an existing session with plugin settings\n"
			                                "they will not be used and will be lost."),
			                              _("To get full access to updates without this limitation\n"
			                                "consider becoming a subscriber for a low cost every month.")));
			l.set_justify (JUSTIFY_CENTER);

			b.signal_clicked().connect (mem_fun(*this, &ARDOUR_UI::launch_subscribe));

			d.get_vbox()->pack_start (l, true, true);
			d.get_vbox()->pack_start (b, false, false, 12);
			d.get_vbox()->pack_start (c, false, false, 12);

			d.add_button (_("Quit now"), RESPONSE_CANCEL);
			d.add_button (string_compose (_("Continue using %1"), PROGRAM_NAME), RESPONSE_OK);

			d.show_all ();

			c.signal_toggled().connect (sigc::hide_return (sigc::bind (sigc::ptr_fun (toggle_file_existence), path)));

			if (d.run () != RESPONSE_OK) {
				_exit (0);
			}
		}
#endif

		/* go get a session */

		const bool new_session_required = (ARDOUR_COMMAND_LINE::new_session || brand_new_user);

		if (get_session_parameters (false, new_session_required, ARDOUR_COMMAND_LINE::load_template)) {
			std::cerr << "Cannot get session parameters."<< std::endl;
			return -1;
		}
	}

	use_config ();

	WM::Manager::instance().show_visible ();

	/* We have to do this here since goto_editor_window() ends up calling show_all() on the
	 * editor window, and we may want stuff to be hidden.
	 */
	_status_bar_visibility.update ();

	BootMessage (string_compose (_("%1 is ready for use"), PROGRAM_NAME));

	if (splash && splash->is_visible()) {
		// in 1 second, hide the splash screen
		Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (_hide_splash), this), 1000);
	}

	/* all other dialogs are created conditionally */

	return 0;
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
					MessageDialog msg (_main_window,
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
		fps_connection.disconnect();
	}

	delete ARDOUR_UI::instance()->video_timeline;
	ARDOUR_UI::instance()->video_timeline = NULL;
	stop_video_server();

	/* Save state before deleting the session, as that causes some
	   windows to be destroyed before their visible state can be
	   saved.
	*/
	save_ardour_state ();

	if (key_editor.get (false)) {
		key_editor->disconnect ();
	}

	close_all_dialogs ();

	if (_session) {
		_session->set_clean ();
		_session->remove_pending_capture_state ();
		delete _session;
		_session = 0;
	}

	halt_connection.disconnect ();
	AudioEngine::instance()->stop ();
#ifdef WINDOWS_VST_SUPPORT
	fst_stop_threading();
#endif
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


void
ARDOUR_UI::every_second ()
{
	update_cpu_load ();
	update_xrun_count ();
	update_buffer_load ();
	update_disk_space ();
	update_timecode_format ();
	update_peak_thread_work ();

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
}

void
ARDOUR_UI::every_point_one_seconds ()
{
	// TODO get rid of this..
	// ShuttleControl is updated directly via TransportStateChange signal
}

void
ARDOUR_UI::every_point_zero_something_seconds ()
{
	// august 2007: actual update frequency: 25Hz (40ms), not 100Hz

	if (editor_meter && UIConfiguration::instance().get_show_editor_meter() && editor_meter_peak_display.is_mapped ()) {
		float mpeak = editor_meter->update_meters();
		if (mpeak > editor_meter_max_peak) {
			if (mpeak >= UIConfiguration::instance().get_meter_peak()) {
				editor_meter_peak_display.set_active_state ( Gtkmm2ext::ExplicitActive );
			}
		}
	}
}

void
ARDOUR_UI::set_fps_timeout_connection ()
{
	unsigned int interval = 40;
	if (!_session) return;
	if (_session->timecode_frames_per_second() != 0) {
		/* ideally we'll use a select() to sleep and not accumulate
		 * idle time to provide a regular periodic signal.
		 * See linux_vst_gui_support.cc 'elapsed_time_ms'.
		 * However, that'll require a dedicated thread and cross-thread
		 * signals to the GUI Thread..
		 */
		interval = floor(500. /* update twice per FPS, since Glib::signal_timeout is very irregular */
				* _session->frame_rate() / _session->nominal_frame_rate()
				/ _session->timecode_frames_per_second()
				);
#ifdef PLATFORM_WINDOWS
		// the smallest windows scheduler time-slice is ~15ms.
		// periodic GUI timeouts shorter than that will cause
		// WaitForSingleObject to spinlock (100% of one CPU Core)
		// and gtk never enters idle mode.
		// also changing timeBeginPeriod(1) does not affect that in
		// any beneficial way, so we just limit the max rate for now.
		interval = std::max(30u, interval); // at most ~33Hz.
#else
		interval = std::max(8u, interval); // at most 120Hz.
#endif
	}
	fps_connection.disconnect();
	Timers::set_fps_interval (interval);
}

void
ARDOUR_UI::update_sample_rate (framecnt_t)
{
	char buf[64];

	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::update_sample_rate, ignored)

	if (!AudioEngine::instance()->connected()) {

		snprintf (buf, sizeof (buf), "%s", _("Audio: <span foreground=\"red\">none</span>"));

	} else {

		framecnt_t rate = AudioEngine::instance()->sample_rate();

		if (rate == 0) {
			/* no sample rate available */
			snprintf (buf, sizeof (buf), "%s", _("Audio: <span foreground=\"red\">none</span>"));
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
	case RF64_WAV:
		s << _("RF64/WAV");
		break;
	case MBWF:
		s << _("MBWF");
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
ARDOUR_UI::update_xrun_count ()
{
	char buf[64];

	/* If this text is changed, the set_size_request_to_display_given_text call in ARDOUR_UI::resize_text_widgets
	   should also be changed.
	*/

	if (_session) {
		const unsigned int x = _session->get_xrun_count ();
		if (x > 9999) {
			snprintf (buf, sizeof (buf), _("X: <span foreground=\"%s\">&gt;10K</span>"), X_("red"));
		} else {
			snprintf (buf, sizeof (buf), _("X: <span foreground=\"%s\">%u</span>"), x > 0 ? X_("red") : X_("green"), x);
		}
	} else {
		snprintf (buf, sizeof (buf), _("X: <span foreground=\"%s\">?</span>"), X_("yellow"));
	}
	xrun_label.set_markup (buf);
	set_tip (xrun_label, _("Audio dropouts. Shift+click to reset"));
}

void
ARDOUR_UI::update_cpu_load ()
{
	char buf[64];

	/* If this text is changed, the set_size_request_to_display_given_text call in ARDOUR_UI::resize_text_widgets
	   should also be changed.
	*/

	double const c = AudioEngine::instance()->get_dsp_load ();
	snprintf (buf, sizeof (buf), _("DSP: <span foreground=\"%s\">%5.1f%%</span>"), c >= 90 ? X_("red") : X_("green"), c);
	cpu_load_label.set_markup (buf);
}

void
ARDOUR_UI::update_peak_thread_work ()
{
	char buf[64];
	const int c = SourceFactory::peak_work_queue_length ();
	if (c > 0) {
		snprintf (buf, sizeof (buf), _("PkBld: <span foreground=\"%s\">%d</span>"), c >= 2 ? X_("red") : X_("green"), c);
		peak_thread_work_label.set_markup (buf);
	} else {
		peak_thread_work_label.set_markup (X_(""));
	}
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
	if (track && track->rec_enable_control()->get_value()) {
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
		_session->foreach_route (this, &ARDOUR_UI::count_recenabled_streams, false);

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
ARDOUR_UI::open_recent_session ()
{
	bool can_return = (_session != 0);

	SessionDialog recent_session_dialog;

	while (true) {

		ResponseType r = (ResponseType) recent_session_dialog.run ();

		switch (r) {
		case RESPONSE_ACCEPT:
			break;
		default:
			if (can_return) {
				recent_session_dialog.hide();
				return;
			} else {
				exit (1);
			}
		}

		recent_session_dialog.hide();

		bool should_be_new;

		std::string path = recent_session_dialog.session_folder();
		std::string state = recent_session_dialog.session_name (should_be_new);

		if (should_be_new == true) {
			continue;
		}

		_session_is_new = false;

		if (load_session (path, state) == 0) {
			break;
		}

		can_return = false;
	}
	if (splash && splash->is_visible()) {
		// in 1 second, hide the splash screen
		Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (_hide_splash), this), 1000);
	}
}

bool
ARDOUR_UI::check_audioengine (Gtk::Window& parent)
{
	if (!AudioEngine::instance()->connected()) {
		MessageDialog msg (parent, string_compose (
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
	if (!check_audioengine (_main_window)) {
		return;
	}

	/* ardour sessions are folders */
	Gtk::FileChooserDialog open_session_selector(_("Open Session"), FILE_CHOOSER_ACTION_OPEN);
	open_session_selector.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	open_session_selector.add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
	open_session_selector.set_default_response(Gtk::RESPONSE_ACCEPT);

	if (_session) {
		string session_parent_dir = Glib::path_get_dirname(_session->path());
		open_session_selector.set_current_folder(session_parent_dir);
	} else {
		open_session_selector.set_current_folder(Config->get_default_session_parent_dir());
	}

	Gtkmm2ext::add_volume_shortcuts (open_session_selector);
	try {
		/* add_shortcut_folder throws an exception if the folder being added already has a shortcut */
		string default_session_folder = Config->get_default_session_parent_dir();
		open_session_selector.add_shortcut_folder (default_session_folder);
	}
	catch (Glib::Error & e) {
		std::cerr << "open_session_selector.add_shortcut_folder() threw Glib::Error " << e.what() << std::endl;
	}

	FileFilter session_filter;
	session_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::statefile_suffix));
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	open_session_selector.add_filter (session_filter);

	FileFilter archive_filter;
	archive_filter.add_pattern (X_("*.tar.xz"));
	archive_filter.set_name (_("Session Archives"));

	open_session_selector.add_filter (archive_filter);

	open_session_selector.set_filter (session_filter);

	int response = open_session_selector.run();
	open_session_selector.hide ();

	if (response == Gtk::RESPONSE_CANCEL) {
		return;
	}

	string session_path = open_session_selector.get_filename();
	string path, name;
	bool isnew;

	if (session_path.length() > 0) {
		int rv = ARDOUR::inflate_session (session_path,
				Config->get_default_session_parent_dir(), path, name);
		if (rv == 0) {
			_session_is_new = false;
			load_session (path, name);
		}
		else if (rv < 0) {
			MessageDialog msg (_main_window,
					string_compose (_("Extracting session-archive failed: %1"), inflate_error (rv)));
			msg.run ();
		}
		else if (ARDOUR::find_session (session_path, path, name, isnew) == 0) {
			_session_is_new = isnew;
			load_session (path, name);
		}
	}
}

void
ARDOUR_UI::session_add_vca (const string& name_template, uint32_t n)
{
	if (!_session) {
		return;
	}

	_session->vca_manager().create_vca (n, name_template);
}

void
ARDOUR_UI::session_add_mixed_track (
		const ChanCount& input,
		const ChanCount& output,
		RouteGroup* route_group,
		uint32_t how_many,
		const string& name_template,
		bool strict_io,
		PluginInfoPtr instrument,
		Plugin::PresetRecord* pset,
		ARDOUR::PresentationInfo::order_t order)
{
	if (_session == 0) {
		warning << _("You cannot add a track without a session already loaded.") << endmsg;
		return;
	}

	if (Profile->get_mixbus ()) {
		strict_io = true;
	}

	try {
		list<boost::shared_ptr<MidiTrack> > tracks;
		tracks = _session->new_midi_track (input, output, strict_io, instrument, pset, route_group, how_many, name_template, order, ARDOUR::Normal);

		if (tracks.size() != how_many) {
			error << string_compose(P_("could not create %1 new mixed track", "could not create %1 new mixed tracks", how_many), how_many) << endmsg;
		}
	}

	catch (...) {
		display_insufficient_ports_message ();
		return;
	}
}

void
ARDOUR_UI::session_add_midi_bus (
		RouteGroup* route_group,
		uint32_t how_many,
		const string& name_template,
		bool strict_io,
		PluginInfoPtr instrument,
		Plugin::PresetRecord* pset,
		ARDOUR::PresentationInfo::order_t order)
{
	if (_session == 0) {
		warning << _("You cannot add a track without a session already loaded.") << endmsg;
		return;
	}

	if (Profile->get_mixbus ()) {
		strict_io = true;
	}

	try {
		RouteList routes;
		routes = _session->new_midi_route (route_group, how_many, name_template, strict_io, instrument, pset, PresentationInfo::MidiBus, order);
		if (routes.size() != how_many) {
			error << string_compose(P_("could not create %1 new Midi Bus", "could not create %1 new Midi Busses", how_many), how_many) << endmsg;
		}

	}
	catch (...) {
		display_insufficient_ports_message ();
		return;
	}
}

void
ARDOUR_UI::session_add_midi_route (
		bool disk,
		RouteGroup* route_group,
		uint32_t how_many,
		const string& name_template,
		bool strict_io,
		PluginInfoPtr instrument,
		Plugin::PresetRecord* pset,
		ARDOUR::PresentationInfo::order_t order)
{
	ChanCount one_midi_channel;
	one_midi_channel.set (DataType::MIDI, 1);

	if (disk) {
		session_add_mixed_track (one_midi_channel, one_midi_channel, route_group, how_many, name_template, strict_io, instrument, pset, order);
	} else {
		session_add_midi_bus (route_group, how_many, name_template, strict_io, instrument, pset, order);
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
	string const & name_template,
	bool strict_io,
	ARDOUR::PresentationInfo::order_t order)
{
	list<boost::shared_ptr<AudioTrack> > tracks;
	RouteList routes;

	if (_session == 0) {
		warning << _("You cannot add a track or bus without a session already loaded.") << endmsg;
		return;
	}

	try {
		if (track) {
			tracks = _session->new_audio_track (input_channels, output_channels, route_group, how_many, name_template, order, mode);

			if (tracks.size() != how_many) {
				error << string_compose (P_("could not create %1 new audio track", "could not create %1 new audio tracks", how_many), how_many)
				      << endmsg;
			}

		} else {

			routes = _session->new_audio_route (input_channels, output_channels, route_group, how_many, name_template, PresentationInfo::AudioBus, order);

			if (routes.size() != how_many) {
				error << string_compose (P_("could not create %1 new audio bus", "could not create %1 new audio busses", how_many), how_many)
				      << endmsg;
			}
		}
	}

	catch (...) {
		display_insufficient_ports_message ();
		return;
	}

	if (strict_io) {
		for (list<boost::shared_ptr<AudioTrack> >::iterator i = tracks.begin(); i != tracks.end(); ++i) {
			(*i)->set_strict_io (true);
		}
		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			(*i)->set_strict_io (true);
		}
	}
}

void
ARDOUR_UI::display_insufficient_ports_message ()
{
	MessageDialog msg (_main_window,
			string_compose (_("There are insufficient ports available\n\
to create a new track or bus.\n\
You should save %1, exit and\n\
restart with more ports."), PROGRAM_NAME));
	pop_back_splash (msg);
	msg.run ();
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

		framecnt_t frame_rate = _session->frame_rate();

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

/** Check if any tracks are record enabled. If none are, record enable all of them.
 * @return true if track record-enabled status was changed, false otherwise.
 */
bool
ARDOUR_UI::trx_record_enable_all_tracks ()
{
	if (!_session) {
		return false;
	}

	boost::shared_ptr<RouteList> rl = _session->get_tracks ();
	bool none_record_enabled = true;

	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*r);
		assert (t);

		if (t->rec_enable_control()->get_value()) {
			none_record_enabled = false;
			break;
		}
	}

	if (none_record_enabled) {
		_session->set_controls (route_list_to_control_list (rl, &Stripable::rec_enable_control), 1.0, Controllable::NoGroup);
	}

	return none_record_enabled;
}

void
ARDOUR_UI::transport_record (bool roll)
{
	if (_session) {
		switch (_session->record_status()) {
		case Session::Disabled:
			if (_session->ntracks() == 0) {
				MessageDialog msg (_main_window, _("Please create one or more tracks before trying to record.\nYou can do this with the \"Add Track or Bus\" option in the Session menu."));
				msg.run ();
				return;
			}
			if (Profile->get_trx()) {
				roll = trx_record_enable_all_tracks ();
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

		/* If loop playback is not a mode, then we should cancel
		   it when this action is requested. If it is a mode
		   we just leave it in place.
		*/

		if (!Config->get_loop_is_mode()) {
			/* XXX it is not possible to just leave seamless loop and keep
			   playing at present (nov 4th 2009)
			*/
			if (!Config->get_seamless_loop()) {
				/* stop loop playback and stop rolling */
				_session->request_play_loop (false, true);
			} else if (rolling) {
				/* stop loop playback but keep rolling */
				_session->request_play_loop (false, false);
			}
		}

	} else if (_session->get_play_range () ) {
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
			if (_session->actively_recording()) {

				/* just stop using the loop, then actually stop
				 * below
				 */
				_session->request_play_loop (false, affect_transport);

			} else {
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
				_session->request_play_loop (false, affect_transport);
			}
		} else if (_session->get_play_range ()) {
			affect_transport = false;
			_session->request_play_range (0, true);
		}
	}

	if (affect_transport) {
		if (rolling) {
			_session->request_stop (with_abort, true);

		} else if (!with_abort) { /* with_abort == true means the
		                           * command was intended to stop
		                           * transport, not start.
		                           */

			/* the only external sync condition we can be in here
			 * would be Engine (JACK) sync, in which case we still
			 * want to do this.
			 */

			if (UIConfiguration::instance().get_follow_edits() && ( editor->get_selection().time.front().start == _session->transport_frame() ) ) {  //if playhead is exactly at the start of a range, we can assume it was placed there by follow_edits
				_session->request_play_range (&editor->get_selection().time, true);
				_session->set_requested_return_frame( editor->get_selection().time.front().start );  //force an auto-return here
			}
			_session->request_transport_speed (1.0f);
		}
	}
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (!_session) {
		return;
	}

	Location * looploc = _session->locations()->auto_loop_location();

	if (!looploc) {
		return;
	}

	if (_session->get_play_loop()) {

		/* looping enabled, our job is to disable it */

		_session->request_play_loop (false);

	} else {

		/* looping not enabled, our job is to enable it.

		   loop-is-NOT-mode: this action always starts the transport rolling.
		   loop-IS-mode:     this action simply sets the loop play mechanism, but
		                        does not start transport.
		*/
		if (Config->get_loop_is_mode()) {
			_session->request_play_loop (true, false);
		} else {
			_session->request_play_loop (true, true);
		}
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
ARDOUR_UI::transport_rec_preroll ()
{
	if (!_session) {
		return;
	}
	editor->rec_with_preroll ();
}

void
ARDOUR_UI::transport_rec_count_in ()
{
	if (!_session) {
		return;
	}
	editor->rec_with_count_in ();
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
ARDOUR_UI::toggle_record_enable (uint16_t rid)
{
	if (!_session) {
		return;
	}

	boost::shared_ptr<Route> r;

	if ((r = _session->get_remote_nth_route (rid)) != 0) {

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(r)) != 0) {
			t->rec_enable_control()->set_value (!t->rec_enable_control()->get_value(), Controllable::UseGroup);
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
		layered_button.set_sensitive (false);
		return;
	}

	shuttle_box.map_transport_state ();

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
			if (Config->get_loop_is_mode()) {
				roll_button.set_active (true);
			} else {
				roll_button.set_active (false);
			}

		} else {

			roll_button.set_active (true);
			play_selection_button.set_active (false);
			auto_loop_button.set_active (false);
		}

		if (UIConfiguration::instance().get_follow_edits() && !_session->config.get_external_sync()) {
			/* light up both roll and play-selection if they are joined */
			roll_button.set_active (true);
			play_selection_button.set_active (true);
		}
		layered_button.set_sensitive (!_session->actively_recording ());

		stop_button.set_active (false);

	} else {

		layered_button.set_sensitive (true);
		stop_button.set_active (true);
		roll_button.set_active (false);
		play_selection_button.set_active (false);
		if (Config->get_loop_is_mode ()) {
			auto_loop_button.set_active (_session->get_play_loop());
		} else {
			auto_loop_button.set_active (false);
		}
		update_disk_space ();
	}
}

void
ARDOUR_UI::blink_handler (bool blink_on)
{
	transport_rec_enable_blink (blink_on);
	sync_blink (blink_on);

	if (!UIConfiguration::instance().get_blink_alert_indicators()) {
		blink_on = true;
	}
	error_blink (blink_on);
	solo_blink (blink_on);
	audition_blink (blink_on);
	feedback_blink (blink_on);
}

void
ARDOUR_UI::update_clocks ()
{
	if (!_session) return;

	if (editor && !editor->dragging_playhead()) {
		Clock (_session->audible_frame(), false, editor->get_preferred_edit_position (EDIT_IGNORE_PHEAD)); /* EMIT_SIGNAL */
	}
}

void
ARDOUR_UI::start_clocking ()
{
	if (UIConfiguration::instance().get_super_rapid_clock_update()) {
		clock_signal_connection = Timers::fps_connect (sigc::mem_fun(*this, &ARDOUR_UI::update_clocks));
	} else {
		clock_signal_connection = Timers::rapid_connect (sigc::mem_fun(*this, &ARDOUR_UI::update_clocks));
	}
}

void
ARDOUR_UI::stop_clocking ()
{
	clock_signal_connection.disconnect ();
}

bool
ARDOUR_UI::save_as_progress_update (float fraction, int64_t cnt, int64_t total, Gtk::Label* label, Gtk::ProgressBar* bar)
{
	char buf[256];

	snprintf (buf, sizeof (buf), _("Copied %" PRId64 " of %" PRId64), cnt, total);

	label->set_text (buf);
	bar->set_fraction (fraction);

	/* process events, redraws, etc. */

	while (gtk_events_pending()) {
		gtk_main_iteration ();
	}

	return true; /* continue with save-as */
}

void
ARDOUR_UI::save_session_as ()
{
	if (!_session) {
		return;
	}

	if (!save_as_dialog) {
		save_as_dialog = new SaveAsDialog;
	}

	save_as_dialog->set_name (_session->name());

	int response = save_as_dialog->run ();

	save_as_dialog->hide ();

	switch (response) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}


	Session::SaveAs sa;

	sa.new_parent_folder = save_as_dialog->new_parent_folder ();
	sa.new_name = save_as_dialog->new_name ();
	sa.switch_to = save_as_dialog->switch_to();
	sa.copy_media = save_as_dialog->copy_media();
	sa.copy_external = save_as_dialog->copy_external();
	sa.include_media = save_as_dialog->include_media ();

	/* Only bother with a progress dialog if we're going to copy
	   media into the save-as target. Without that choice, this
	   will be very fast because we're only talking about a few kB's to
	   perhaps a couple of MB's of data.
	*/

	ArdourDialog progress_dialog (_("Save As"), true);
	ScopedConnection c;

	if (sa.include_media && sa.copy_media) {

		Gtk::Label* label = manage (new Gtk::Label());
		Gtk::ProgressBar* progress_bar = manage (new Gtk::ProgressBar ());

		progress_dialog.get_vbox()->pack_start (*label);
		progress_dialog.get_vbox()->pack_start (*progress_bar);
		label->show ();
		progress_bar->show ();

		/* this signal will be emitted from within this, the calling thread,
		 * after every file is copied. It provides information on percentage
		 * complete (in terms of total data to copy), the number of files
		 * copied so far, and the total number to copy.
		 */

		sa.Progress.connect_same_thread (c, boost::bind (&ARDOUR_UI::save_as_progress_update, this, _1, _2, _3, label, progress_bar));

		progress_dialog.show_all ();
		progress_dialog.present ();
	}

	if (_session->save_as (sa)) {
		/* ERROR MESSAGE */
		MessageDialog msg (string_compose (_("Save As failed: %1"), sa.failure_message));
		msg.run ();
	}

	/* the logic here may seem odd: why isn't the condition sa.switch_to ?
	 * the trick is this: if the new session was copy with media included,
	 * then Session::save_as() will have already done a neat trick to avoid
	 * us having to unload and load the new state. But if the media was not
	 * included, then this is required (it avoids us having to otherwise
	 * drop all references to media (sources).
	 */

	if (!sa.include_media && sa.switch_to) {
		unload_session (false);
		load_session (sa.final_session_folder_name, sa.new_name);
		hide_splash ();
	}
}

void
ARDOUR_UI::archive_session ()
{
	if (!_session) {
		return;
	}

	time_t n;
	time (&n);
	Glib::DateTime gdt (Glib::DateTime::create_now_local (n));

	SessionArchiveDialog sad;
	sad.set_name (_session->name() + gdt.format ("_%F_%H%M%S"));
	int response = sad.run ();

	if (response != Gtk::RESPONSE_OK) {
		sad.hide ();
		return;
	}

	if (_session->archive_session (sad.target_folder(), sad.name(), sad.encode_option (), sad.only_used_sources (), &sad)) {
		MessageDialog msg (_("Session Archiving failed."));
		msg.run ();
	}
}

void
ARDOUR_UI::quick_snapshot_session (bool switch_to_it)
{
		char timebuf[128];
		time_t n;
		struct tm local_time;

		time (&n);
		localtime_r (&n, &local_time);
		strftime (timebuf, sizeof(timebuf), "%FT%H.%M.%S", &local_time);

		save_state (timebuf, switch_to_it);
}


bool
ARDOUR_UI::process_snapshot_session_prompter (ArdourPrompter& prompter, bool switch_to_it)
{
	string snapname;

	prompter.get_result (snapname);

	bool do_save = (snapname.length() != 0);

	if (do_save) {
		char illegal = Session::session_name_is_legal(snapname);
		if (illegal) {
			MessageDialog msg (string_compose (_("To ensure compatibility with various systems\n"
					     "snapshot names may not contain a '%1' character"), illegal));
			msg.run ();
			return false;
		}
	}

	vector<std::string> p;
	get_state_files_in_directory (_session->session_directory().root_path(), p);
	vector<string> n = get_file_names_no_extension (p);

	if (find (n.begin(), n.end(), snapname) != n.end()) {

		do_save = overwrite_file_dialog (prompter,
						 _("Confirm Snapshot Overwrite"),
						 _("A snapshot already exists with that name. Do you want to overwrite it?"));
	}

	if (do_save) {
		save_state (snapname, switch_to_it);
	}
	else {
		return false;
	}

	return true;
}


/** Ask the user for the name of a new snapshot and then take it.
 */

void
ARDOUR_UI::snapshot_session (bool switch_to_it)
{
	ArdourPrompter prompter (true);

	prompter.set_name ("Prompter");
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	if (switch_to_it) {
		prompter.set_title (_("Snapshot and switch"));
		prompter.set_prompt (_("New session name"));
	} else {
		prompter.set_title (_("Take Snapshot"));
		prompter.set_prompt (_("Name of new snapshot"));
	}

	if (switch_to_it) {
		prompter.set_initial_text (_session->snap_name());
	} else {
		Glib::DateTime tm (g_date_time_new_now_local ());
		prompter.set_initial_text (tm.format ("%FT%H.%M.%S"));
	}

	bool finished = false;
	while (!finished) {
		switch (prompter.run()) {
		case RESPONSE_ACCEPT:
		{
			finished = process_snapshot_session_prompter (prompter, switch_to_it);
			break;
		}

		default:
			finished = true;
			break;
		}
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
	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	XMLNode* node = new XMLNode (X_("UI"));

	WM::Manager::instance().add_state (*node);

	node->add_child_nocopy (gui_object_state->get_state());

	_session->add_extra_xml (*node);

	if (export_video_dialog) {
		_session->add_extra_xml (export_video_dialog->get_state());
	}

	save_state_canfail (name, switch_to_it);
}

int
ARDOUR_UI::save_state_canfail (string name, bool switch_to_it)
{
	if (_session) {
		int ret;

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
			rec_button.set_active_state (Gtkmm2ext::Off);
		}
	} else if (r == Session::Recording && h) {
		rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		rec_button.unset_active_state ();
	}
}

bool
ARDOUR_UI::process_save_template_prompter (ArdourPrompter& prompter)
{
	string name;

	prompter.get_result (name);

	if (name.length()) {
		int failed = _session->save_template (name);

		if (failed == -2) { /* file already exists. */
			bool overwrite = overwrite_file_dialog (prompter,
								_("Confirm Template Overwrite"),
								_("A template already exists with that name. Do you want to overwrite it?"));

			if (overwrite) {
				_session->save_template (name, true);
			}
			else {
				return false;
			}
		}
	}

	return true;
}

void
ARDOUR_UI::save_template ()
{
	ArdourPrompter prompter (true);

	if (!check_audioengine (_main_window)) {
		return;
	}

	prompter.set_name (X_("Prompter"));
	prompter.set_title (_("Save Template"));
	prompter.set_prompt (_("Name for template:"));
	prompter.set_initial_text(_session->name() + _("-template"));
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);

	bool finished = false;
	while (!finished) {
		switch (prompter.run()) {
		case RESPONSE_ACCEPT:
			finished = process_save_template_prompter (prompter);
			break;

		default:
			finished = true;
			break;
		}
	}
}

void
ARDOUR_UI::edit_metadata ()
{
	SessionMetadataEditor dialog;
	dialog.set_session (_session);
	dialog.grab_focus ();
	dialog.run ();
}

void
ARDOUR_UI::import_metadata ()
{
	SessionMetadataImporter dialog;
	dialog.set_session (_session);
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
	msg.set_position (Gtk::WIN_POS_CENTER);
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

	if (nsm) {

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
ARDOUR_UI::load_from_application_api (const std::string& path)
{
	/* OS X El Capitan (and probably later) now somehow passes the command
	   line arguments to an app via the openFile delegate protocol. Ardour
	   already does its own command line processing, and having both
	   pathways active causes crashes. So, if the command line was already
	   set, do nothing here.
	*/

	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {
		return;
	}

	ARDOUR_COMMAND_LINE::session_name = path;

	/* Cancel SessionDialog if it's visible to make OSX delegates work.
	 *
	 * ARDOUR_UI::starting connects app->ShouldLoad signal and then shows a SessionDialog
	 * race-condition:
	 *  - ShouldLoad does not arrive in time, ARDOUR_COMMAND_LINE::session_name is empty:
	 *    -> ARDOUR_UI::get_session_parameters starts a SessionDialog.
	 *  - ShouldLoad signal arrives, this function is called and sets ARDOUR_COMMAND_LINE::session_name
	 *    -> SessionDialog is not displayed
	 */

	if (_session_dialog) {
		std::string session_name = basename_nosuffix (ARDOUR_COMMAND_LINE::session_name);
		std::string session_path = path;
		if (Glib::file_test (session_path, Glib::FILE_TEST_IS_REGULAR)) {
			session_path = Glib::path_get_dirname (session_path);
		}
		// signal the existing dialog in ARDOUR_UI::get_session_parameters()
		_session_dialog->set_provided_session (session_name, session_path);
		_session_dialog->response (RESPONSE_NONE);
		_session_dialog->hide();
		return;
	}

	int rv;
	if (Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
		/* /path/to/foo => /path/to/foo, foo */
		rv = load_session (path, basename_nosuffix (path));
	} else {
		/* /path/to/foo/foo.ardour => /path/to/foo, foo */
		rv =load_session (Glib::path_get_dirname (path), basename_nosuffix (path));
	}

	// if load_session fails -> pop up SessionDialog.
	if (rv) {
		ARDOUR_COMMAND_LINE::session_name = "";

		if (get_session_parameters (true, false)) {
			exit (1);
		}
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

	_session_dialog = &session_dialog;
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
			case RESPONSE_NONE:
				/* this is used for async * app->ShouldLoad(). */
				continue; // while loop
				break;
			default:
				if (quit_on_cancel) {
					// JE - Currently (July 2014) this section can only get reached if the
					// user quits from the main 'Session Setup' dialog (i.e. reaching this
					// point does NOT indicate an abnormal termination). Therefore, let's
					// behave gracefully (i.e. let's do some cleanup) before we call exit()
					ARDOUR::cleanup ();
					pthread_cancel_all ();

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

		if (!likely_new) {
			int rv = ARDOUR::inflate_session (session_name,
					Config->get_default_session_parent_dir(), session_path, session_name);
			if (rv < 0) {
				MessageDialog msg (session_dialog,
					string_compose (_("Extracting session-archive failed: %1"), inflate_error (rv)));
				msg.run ();
				continue;
			}
			else if (rv == 0) {
				session_dialog.set_provided_session (session_name, session_path);
			}
		}

		// XXX check archive, inflate
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
#ifdef PLATFORM_WINDOWS
		    (session_name.length() > 3 && session_name[1] == ':' && session_name[2] == G_DIR_SEPARATOR)
#else
		    (session_name.length() > 2 && session_name[0] == '.' && session_name[1] == G_DIR_SEPARATOR) ||
		    (session_name.length() > 3 && session_name[0] == '.' && session_name[1] == '.' && session_name[2] == G_DIR_SEPARATOR)
#endif
			 )
		{

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

	_session_dialog = NULL;

	return ret;
}

void
ARDOUR_UI::close_session()
{
	if (!check_audioengine (_main_window)) {
		return;
	}

	if (unload_session (true)) {
		return;
	}

	ARDOUR_COMMAND_LINE::session_name = "";

	if (get_session_parameters (true, false)) {
		exit (1);
	}
	if (splash && splash->is_visible()) {
		// in 1 second, hide the splash screen
		Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (_hide_splash), this), 1000);
	}
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
	catch (SessionException e) {
		MessageDialog msg (string_compose(
			                   _("Session \"%1 (snapshot %2)\" did not load successfully: %3"),
			                   path, snap_name, e.what()),
		                   true,
		                   Gtk::MESSAGE_INFO,
		                   BUTTONS_OK);

		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.present ();

		dump_errors (cerr);

		(void) msg.run ();
		msg.hide ();

		goto out;
	}
	catch (...) {

		MessageDialog msg (string_compose(
			                   _("Session \"%1 (snapshot %2)\" did not load successfully"),
			                   path, snap_name),
		                   true,
		                   Gtk::MESSAGE_INFO,
		                   BUTTONS_OK);

		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.present ();

		dump_errors (cerr);

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

	if (_session) {
		_session->set_clean ();
	}

#ifdef WINDOWS_VST_SUPPORT
	fst_stop_threading();
#endif

	{
		Timers::TimerSuspender t;
		flush_pending (10);
	}

#ifdef WINDOWS_VST_SUPPORT
	fst_start_threading();
#endif
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

	catch (SessionException e) {
		cerr << "Here are the errors associated with this failed session:\n";
		dump_errors (cerr);
		cerr << "---------\n";
		MessageDialog msg (string_compose(_("Could not create session in \"%1\": %2"), path, e.what()));
		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.run ();
		return -1;
	}
	catch (...) {
		cerr << "Here are the errors associated with this failed session:\n";
		dump_errors (cerr);
		cerr << "---------\n";
		MessageDialog msg (string_compose(_("Could not create session in \"%1\""), path));
		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.run ();
		return -1;
	}

	/* Give the new session the default GUI state, if such things exist */

	XMLNode* n;
	n = Config->instant_xml (X_("Editor"));
	if (n) {
		n->remove_nodes_and_delete ("Selection"); // no not apply selection to new sessions.
		new_session->add_instant_xml (*n, false);
	}
	n = Config->instant_xml (X_("Mixer"));
	if (n) {
		new_session->add_instant_xml (*n, false);
	}

	n = Config->instant_xml (X_("Preferences"));
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
	MessageDialog dialog(_("<b>Just ask and wait for an answer.\nIt may take from minutes to hours.</b>"), true);

	dialog.set_title (_("About the Chat"));
	dialog.set_secondary_text (_("When you're inside the chat just ask your question and wait for an answer. The chat is occupied by real people with real lives so many of them are passively online and might not read your question before minutes or hours later.\nSo please be patient and wait for an answer.\n\nYou should just leave the chat window open and check back regularly until someone has answered your question."));

	switch (dialog.run()) {
	case RESPONSE_OK:
#ifdef __APPLE__
		open_uri("http://webchat.freenode.net/?channels=ardour-osx");
#elif defined PLATFORM_WINDOWS
		open_uri("http://webchat.freenode.net/?channels=ardour-windows");
#else
		open_uri("http://webchat.freenode.net/?channels=ardour");
#endif
		break;
	default:
		break;
	}
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
ARDOUR_UI::launch_tracker ()
{
	PBD::open_uri ("http://tracker.ardour.org");
}

void
ARDOUR_UI::launch_subscribe ()
{
	PBD::open_uri ("https://community.ardour.org/s/subscribe");
}

void
ARDOUR_UI::launch_cheat_sheet ()
{
#ifdef __APPLE__
	PBD::open_uri ("http://manual.ardour.org/files/a3_mnemonic_cheat_sheet_osx.pdf");
#else
	PBD::open_uri ("http://manual.ardour.org/files/a3_mnemonic_cheatsheet.pdf");
#endif
}

void
ARDOUR_UI::launch_website ()
{
	PBD::open_uri ("http://ardour.org");
}

void
ARDOUR_UI::launch_website_dev ()
{
	PBD::open_uri ("http://ardour.org/development.html");
}

void
ARDOUR_UI::launch_forums ()
{
	PBD::open_uri ("https://community.ardour.org/forums");
}

void
ARDOUR_UI::launch_howto_report ()
{
	PBD::open_uri ("http://ardour.org/reporting_bugs");
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
		MessageDialog msgd (_main_window,
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
		space_adjusted = floorf((float)rep.space / 1000.0);
	} else if (rep.space < 1000000 * 1000) {
		bprefix = _("mega");
		space_adjusted = floorf((float)rep.space / (1000.0 * 1000.0));
	} else {
		bprefix = _("giga");
		space_adjusted = floorf((float)rep.space / (1000.0 * 1000 * 1000.0));
	}

	if (msg_delete) {
		txt.set_markup (string_compose (P_("\
The following file was deleted from %2,\n\
releasing %3 %4bytes of disk space", "\
The following %1 files were deleted from %2,\n\
releasing %3 %4bytes of disk space", removed),
					removed, Gtkmm2ext::markup_escape_text (dead_directory), space_adjusted, bprefix, PROGRAM_NAME));
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
					removed, Gtkmm2ext::markup_escape_text (dead_directory), space_adjusted, bprefix, PROGRAM_NAME));
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
ARDOUR_UI::cleanup_peakfiles ()
{
	if (_session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}

	if (! _session->can_cleanup_peakfiles ()) {
		return;
	}

	// get all region-views in this session
	RegionSelection rs;
	TrackViewList empty;
	empty.clear();
	editor->get_regions_after(rs, (framepos_t) 0, empty);
	std::list<RegionView*> views = rs.by_layer();

	// remove displayed audio-region-views waveforms
	for (list<RegionView*>::iterator i = views.begin(); i != views.end(); ++i) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*i);
		if (!arv) { continue ; }
		arv->delete_waves();
	}

	// cleanup peak files:
	// - stop pending peakfile threads
	// - close peakfiles if any
	// - remove peak dir in session
	// - setup peakfiles (background thread)
	_session->cleanup_peakfiles ();

	// re-add waves to ARV
	for (list<RegionView*>::iterator i = views.begin(); i != views.end(); ++i) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*i);
		if (!arv) { continue ; }
		arv->create_waves();
	}
}

PresentationInfo::order_t
ARDOUR_UI::translate_order (RouteDialogs::InsertAt place)
{
	if (editor->get_selection().tracks.empty()) {
		return PresentationInfo::max_order;
	}

	PresentationInfo::order_t order_hint = PresentationInfo::max_order;

	/*
	  we want the new routes to have their order keys set starting from
	  the highest order key in the selection + 1 (if available).
	*/

	if (place == RouteDialogs::AfterSelection) {
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView*> (editor->get_selection().tracks.back());
		if (rtav) {
			order_hint = rtav->route()->presentation_info().order();
			order_hint++;
		}
	} else if (place == RouteDialogs::BeforeSelection) {
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView*> (editor->get_selection().tracks.front());
		if (rtav) {
			order_hint = rtav->route()->presentation_info().order();
		}
	} else if (place == RouteDialogs::First) {
		order_hint = 0;
	} else {
		/* leave order_hint at max_order */
	}

	return order_hint;
}

void
ARDOUR_UI::start_duplicate_routes ()
{
	if (!duplicate_routes_dialog) {
		duplicate_routes_dialog = new DuplicateRouteDialog;
	}

	if (duplicate_routes_dialog->restart (_session)) {
		return;
	}

	duplicate_routes_dialog->present ();
}

void
ARDOUR_UI::add_route ()
{
	if (!add_route_dialog.get (false)) {
		add_route_dialog->signal_response().connect (sigc::mem_fun (*this, &ARDOUR_UI::add_route_dialog_finished));
	}

	if (!_session) {
		return;
	}

	if (add_route_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	add_route_dialog->set_position (WIN_POS_MOUSE);
	add_route_dialog->present();
}

void
ARDOUR_UI::add_route_dialog_finished (int r)
{
	int count;

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

	PresentationInfo::order_t order = translate_order (add_route_dialog->insert_at());
	string template_path = add_route_dialog->track_template();
	DisplaySuspender ds;

	if (!template_path.empty()) {
		if (add_route_dialog->name_template_is_default())  {
			_session->new_route_from_template (count, order, template_path, string());
		} else {
			_session->new_route_from_template (count, order, template_path, add_route_dialog->name_template());
		}
		return;
	}

	ChanCount input_chan= add_route_dialog->channels ();
	ChanCount output_chan;
	string name_template = add_route_dialog->name_template ();
	PluginInfoPtr instrument = add_route_dialog->requested_instrument ();
	RouteGroup* route_group = add_route_dialog->route_group ();
	AutoConnectOption oac = Config->get_output_auto_connect();
	bool strict_io = add_route_dialog->use_strict_io ();

	if (oac & AutoConnectMaster) {
		output_chan.set (DataType::AUDIO, (_session->master_out() ? _session->master_out()->n_inputs().n_audio() : input_chan.n_audio()));
		output_chan.set (DataType::MIDI, 0);
	} else {
		output_chan = input_chan;
	}

	/* XXX do something with name template */

	Session::ProcessorChangeBlocker pcb (_session);

	switch (add_route_dialog->type_wanted()) {
	case AddRouteDialog::AudioTrack:
		session_add_audio_track (input_chan.n_audio(), output_chan.n_audio(), add_route_dialog->mode(), route_group, count, name_template, strict_io, order);
		break;
	case AddRouteDialog::MidiTrack:
		session_add_midi_track (route_group, count, name_template, strict_io, instrument, 0, order);
		break;
	case AddRouteDialog::MixedTrack:
		session_add_mixed_track (input_chan, output_chan, route_group, count, name_template, strict_io, instrument, 0, order);
		break;
	case AddRouteDialog::AudioBus:
		session_add_audio_bus (input_chan.n_audio(), output_chan.n_audio(), route_group, count, name_template, strict_io, order);
		break;
	case AddRouteDialog::MidiBus:
		session_add_midi_bus (route_group, count, name_template, strict_io, instrument, 0, order);
		break;
	case AddRouteDialog::VCAMaster:
		session_add_vca (name_template, count);
		break;
	}
}

void
ARDOUR_UI::stop_video_server (bool ask_confirm)
{
	if (!video_server_process && ask_confirm) {
		warning << string_compose (_("Video-Server was not launched by %1. The request to stop it is ignored."), PROGRAM_NAME) << endmsg;
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
			warning << _("Could not connect to the Video Server. Start it or configure its access URL in Preferences.") << endmsg;
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
#ifndef PLATFORM_WINDOWS
		if (icsd_docroot.empty()) {
			icsd_docroot = VideoUtils::video_get_docroot (Config);
		}
#endif

		GStatBuf sb;
#ifdef PLATFORM_WINDOWS
		if (VideoUtils::harvid_version >= 0x000802 && icsd_docroot.empty()) {
			/* OK, allow all drive letters */
		} else
#endif
		if (g_lstat (icsd_docroot.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
			warning << _("Specified docroot is not an existing directory.") << endmsg;
			continue;
		}
#ifndef PLATFORM_WINDOWS
		if ( (g_lstat (icsd_exec.c_str(), &sb) != 0)
		     || (sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#else
		if ( (g_lstat (icsd_exec.c_str(), &sb) != 0)
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

#ifdef PLATFORM_WINDOWS
		if (VideoUtils::harvid_version >= 0x000802 && icsd_docroot.empty()) {
			/* OK, allow all drive letters */
		} else
#endif
		if (icsd_docroot == X_("/") || icsd_docroot == X_("C:\\")) {
			Config->set_video_advanced_setup(false);
		} else {
			std::ostringstream osstream;
			osstream << "http://127.0.0.1:" << video_server_dialog->get_listenport() << "/";
			Config->set_video_server_url(osstream.str());
			Config->set_video_server_docroot(icsd_docroot);
			Config->set_video_advanced_setup(true);
		}

		if (video_server_process) {
			delete video_server_process;
		}

		video_server_process = new ARDOUR::SystemExec(icsd_exec, argp);
		if (video_server_process->start()) {
			warning << _("Cannot launch the video-server") << endmsg;
			continue;
		}
		int timeout = 120; // 6 sec
		while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
			Glib::usleep (50000);
			gui_idle_handler();
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
		warning << _("Could not connect to the Video Server. Start it or configure its access URL in Preferences.") << endmsg;
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

	std::string audio_from_video;
	bool detect_ltc = false;

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

				audio_from_video = transcode_video_dialog->get_audiofile();

				if (!audio_from_video.empty() && transcode_video_dialog->detect_ltc()) {
					detect_ltc = true;
				}
				else if (!audio_from_video.empty()) {
					editor->embed_audio_from_video(
							audio_from_video,
							video_timeline->get_offset(),
							(transcode_video_dialog->import_option() != VTL_IMPORT_NO_VIDEO)
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

		if (!audio_from_video.empty() && detect_ltc) {
			std::vector<LTCFileReader::LTCMap> ltc_seq;

			try {
				/* TODO ask user about TV standard (LTC alignment if any) */
				LTCFileReader ltcr (audio_from_video, video_timeline->get_video_file_fps());
				/* TODO ASK user which channel:  0 .. ltcr->channels() - 1 */

				ltc_seq = ltcr.read_ltc (/*channel*/ 0, /*max LTC frames to decode*/ 15);

				/* TODO seek near end of file, and read LTC until end.
				 * if it fails to find any LTC frames, scan complete file
				 *
				 * calculate drift of LTC compared to video-duration,
				 * ask user for reference (timecode from start/mid/end)
				 */
			} catch (...) {
				// LTCFileReader will have written error messages
			}

			::g_unlink(audio_from_video.c_str());

			if (ltc_seq.size() == 0) {
				PBD::error << _("No LTC detected, video will not be aligned.") << endmsg;
			} else {
				/* the very first TC in the file is somteimes not aligned properly */
				int i = ltc_seq.size() -1;
				ARDOUR::frameoffset_t video_start_offset =
					_session->nominal_frame_rate() * (ltc_seq[i].timecode_sec - ltc_seq[i].framepos_sec);
				PBD::info << string_compose (_("Align video-start to %1 [samples]"), video_start_offset) << endmsg;
				video_timeline->set_offset(video_start_offset);
			}
		}

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
	node = new XMLNode(X_("Videoexport"));
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

void
ARDOUR_UI::export_video (bool range)
{
	if (ARDOUR::Config->get_show_video_export_info()) {
		ExportVideoInfobox infobox (_session);
		Gtk::ResponseType rv = (Gtk::ResponseType) infobox.run();
		if (infobox.show_again()) {
			ARDOUR::Config->set_show_video_export_info(false);
		}
		switch (rv) {
			case GTK_RESPONSE_YES:
				PBD::open_uri (ARDOUR::Config->get_reference_manual_url() + "/video-timeline/operations/#export");
				break;
			default:
				break;
		}
	}
	export_video_dialog->set_session (_session);
	export_video_dialog->apply_state(editor->get_selection().time, range);
	export_video_dialog->run ();
	export_video_dialog->hide ();
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
	if (_session) {
		Location *location = new Location (*_session, where, where, _("xrun"), Location::IsMark, 0);
		_session->locations()->add (location);
	}
}

void
ARDOUR_UI::halt_on_xrun_message ()
{
        cerr << "HALT on xrun\n";
        MessageDialog msg (_main_window, _("Recording was stopped because your system could not keep up."));
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
		MessageDialog* msg = new MessageDialog (_main_window, string_compose (_("\
The disk system on your computer\n\
was not able to keep up with %1.\n\
\n\
Specifically, it failed to write data to disk\n\
quickly enough to keep up with recording.\n"), PROGRAM_NAME));
		msg->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show ();
	}
}


/* TODO: this is getting elaborate enough to warrant being split into a dedicated class */
static MessageDialog *scan_dlg = NULL;
static ProgressBar   *scan_pbar = NULL;
static HBox          *scan_tbox = NULL;
static Gtk::Button   *scan_timeout_button;

void
ARDOUR_UI::cancel_plugin_scan ()
{
	PluginManager::instance().cancel_plugin_scan();
}

void
ARDOUR_UI::cancel_plugin_timeout ()
{
	PluginManager::instance().cancel_plugin_timeout();
	scan_timeout_button->set_sensitive (false);
}

void
ARDOUR_UI::plugin_scan_timeout (int timeout)
{
	if (!scan_dlg || !scan_dlg->is_mapped() || !scan_pbar) {
		return;
	}
	if (timeout > 0) {
		scan_pbar->set_sensitive (false);
		scan_timeout_button->set_sensitive (true);
		scan_pbar->set_fraction ((float) timeout / (float) Config->get_vst_scan_timeout());
		scan_tbox->show();
	} else {
		scan_pbar->set_sensitive (false);
		scan_timeout_button->set_sensitive (false);
	}
	gui_idle_handler();
}

void
ARDOUR_UI::plugin_scan_dialog (std::string type, std::string plugin, bool can_cancel)
{
	if (type == X_("closeme") && !(scan_dlg && scan_dlg->is_mapped())) {
		return;
	}

	const bool cancelled = PluginManager::instance().cancelled();
	if (type != X_("closeme") && (!UIConfiguration::instance().get_show_plugin_scan_window()) && !_initial_verbose_plugin_scan) {
		if (cancelled && scan_dlg->is_mapped()) {
			scan_dlg->hide();
			gui_idle_handler();
			return;
		}
		if (cancelled || !can_cancel) {
			return;
		}
	}

	static Gtk::Button *cancel_button;
	if (!scan_dlg) {
		scan_dlg = new MessageDialog("", false, MESSAGE_INFO, BUTTONS_NONE); // TODO manage
		VBox* vbox = scan_dlg->get_vbox();
		vbox->set_size_request(400,-1);
		scan_dlg->set_title (_("Scanning for plugins"));

		cancel_button = manage(new Gtk::Button(_("Cancel plugin scan")));
		cancel_button->set_name ("EditorGTKButton");
		cancel_button->signal_clicked().connect ( mem_fun (*this, &ARDOUR_UI::cancel_plugin_scan) );
		cancel_button->show();

		scan_dlg->get_vbox()->pack_start ( *cancel_button, PACK_SHRINK);

		scan_tbox = manage( new HBox() );

		scan_timeout_button = manage(new Gtk::Button(_("Stop Timeout")));
		scan_timeout_button->set_name ("EditorGTKButton");
		scan_timeout_button->signal_clicked().connect ( mem_fun (*this, &ARDOUR_UI::cancel_plugin_timeout) );
		scan_timeout_button->show();

		scan_pbar = manage(new ProgressBar());
		scan_pbar->set_orientation(Gtk::PROGRESS_RIGHT_TO_LEFT);
		scan_pbar->set_text(_("Scan Timeout"));
		scan_pbar->show();

		scan_tbox->pack_start (*scan_pbar, PACK_EXPAND_WIDGET, 4);
		scan_tbox->pack_start (*scan_timeout_button, PACK_SHRINK, 4);

		scan_dlg->get_vbox()->pack_start (*scan_tbox, PACK_SHRINK, 4);
	}

	assert(scan_dlg && scan_tbox && cancel_button);

	if (type == X_("closeme")) {
		scan_tbox->hide();
		scan_dlg->hide();
	} else {
		scan_dlg->set_message(type + ": " + Glib::path_get_basename(plugin));
		scan_dlg->show();
	}
	if (!can_cancel || !cancelled) {
		scan_timeout_button->set_sensitive(false);
	}
	cancel_button->set_sensitive(can_cancel && !cancelled);

	gui_idle_handler();
}

void
ARDOUR_UI::gui_idle_handler ()
{
	int timeout = 30;
	/* due to idle calls, gtk_events_pending() may always return true */
	while (gtk_events_pending() && --timeout) {
		gtk_main_iteration ();
	}
}

void
ARDOUR_UI::disk_underrun_handler ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::disk_underrun_handler)

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		MessageDialog* msg = new MessageDialog (
			_main_window, string_compose (_("The disk system on your computer\n\
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

	d = new MessageDialog (msg, false, MESSAGE_INFO, BUTTONS_OK, true);
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

void
ARDOUR_UI::sr_mismatch_message (framecnt_t desired, framecnt_t actual)
{
	MessageDialog msg (string_compose (_("\
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
	if (UIConfiguration::instance().get_primary_clock_delta_edit_cursor()) {
		primary_clock->set (pos, false, editor->get_preferred_edit_position (EDIT_IGNORE_PHEAD));
	} else {
		primary_clock->set (pos);
	}

	if (UIConfiguration::instance().get_secondary_clock_delta_edit_cursor()) {
		secondary_clock->set (pos, false, editor->get_preferred_edit_position (EDIT_IGNORE_PHEAD));
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

	if (!_session) {
		/* why bother - the clock isn't visible */
		return;
	}

	ActionManager::set_sensitive (ActionManager::rec_sensitive_actions, !_session->actively_recording());

	if (big_clock_window) {
		if (_session->record_status () == Session::Recording && _session->have_rec_enabled_track ()) {
			big_clock->set_active (true);
		} else {
			big_clock->set_active (false);
		}
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
ARDOUR_UI::TransportControllable::set_value (double val, PBD::Controllable::GroupControlDisposition /*group_override*/)
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

	if (g_getenv ("TRX")) {
		Profile->set_trx ();
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
					     "From now on, use the backup copy with older versions of %3"),
					   xml_path, backup_path, PROGRAM_NAME,
					   start_big, end_big,
					   start_mono, end_mono), true);

	msg.run ();
}

void
ARDOUR_UI::add_editor_meter_type_item (Menu_Helpers::MenuList& items, RadioMenuItem::Group& group, string const & name, MeterType type)
{
	using namespace Menu_Helpers;

	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (editor_meter, &LevelMeterHBox::set_meter_type), type)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (editor_meter->meter_type () == type);
}

void
ARDOUR_UI::popup_editor_meter_menu (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = manage (new Menu);
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	_suspend_editor_meter_callbacks = true;
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterPeak), MeterPeak);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterPeak0dB), MeterPeak0dB);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterKrms),  MeterKrms);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC1DIN), MeterIEC1DIN);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC1NOR), MeterIEC1NOR);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC2BBC), MeterIEC2BBC);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC2EBU), MeterIEC2EBU);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK20), MeterK20);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK14), MeterK14);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK12), MeterK12);
	add_editor_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterVU),  MeterVU);

	m->popup (ev->button, ev->time);
	_suspend_editor_meter_callbacks = false;
}

bool
ARDOUR_UI::editor_meter_button_press (GdkEventButton* ev)
{
	if (ev->button == 3 && editor_meter) {
		popup_editor_meter_menu (ev);
		return true;
	}
	return false;
}

void
ARDOUR_UI::reset_peak_display ()
{
	if (!_session || !_session->master_out() || !editor_meter) return;
	editor_meter->clear_meters();
	editor_meter_max_peak = -INFINITY;
	editor_meter_peak_display.set_active_state ( Gtkmm2ext::Off );
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

	if (desired_sample_rate != 0) {
		if (Config->get_try_autostart_engine () || getenv ("TRY_AUTOSTART_ENGINE")) {
			audio_midi_setup->try_autostart ();
			if (ARDOUR::AudioEngine::instance()->running()) {
				return 0;
			}
		}
	}

	while (true) {
		int response = audio_midi_setup->run();
		printf("RESPONSE %d\n", response);
		switch (response) {
		case Gtk::RESPONSE_DELETE_EVENT:
			return -1;
		default:
			if (!AudioEngine::instance()->running()) {
				continue;
			}
			audio_midi_setup->hide ();
			return 0;
		}
	}
}


gint
ARDOUR_UI::transport_numpad_timeout ()
{
	_numpad_locate_happening = false;
	if (_numpad_timeout_connection.connected() )
		_numpad_timeout_connection.disconnect();
	return 1;
}

void
ARDOUR_UI::transport_numpad_decimal ()
{
	_numpad_timeout_connection.disconnect();

	if (_numpad_locate_happening) {
		if (editor) editor->goto_nth_marker(_pending_locate_num - 1);
		_numpad_locate_happening = false;
	} else {
		_pending_locate_num = 0;
		_numpad_locate_happening = true;
		_numpad_timeout_connection = Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::transport_numpad_timeout), 2*1000);
	}
}

void
ARDOUR_UI::transport_numpad_event (int num)
{
	if ( _numpad_locate_happening ) {
		_pending_locate_num = _pending_locate_num*10 + num;
	} else {
		switch (num) {
			case 0:  toggle_roll(false, false);		break;
			case 1:  transport_rewind(1);				break;
			case 2:  transport_forward(1);				break;
			case 3:  transport_record(true);			break;
			case 4:  toggle_session_auto_loop();		break;
			case 5:  transport_record(false); toggle_session_auto_loop();	break;
			case 6:  toggle_punch();					break;
			case 7:  toggle_click();				break;
			case 8:  toggle_auto_return();			break;
			case 9:  toggle_follow_edits();		break;
		}
	}
}

void
ARDOUR_UI::set_flat_buttons ()
{
	CairoWidget::set_flat_buttons( UIConfiguration::instance().get_flat_buttons() );
}

void
ARDOUR_UI::audioengine_became_silent ()
{
	MessageDialog msg (string_compose (_("This is a free/demo copy of %1. It has just switched to silent mode."), PROGRAM_NAME),
	                   true,
	                   Gtk::MESSAGE_WARNING,
	                   Gtk::BUTTONS_NONE,
	                   true);

	msg.set_title (string_compose (_("%1 is now silent"), PROGRAM_NAME));

	Gtk::Label pay_label (string_compose (_("Please consider paying for a copy of %1 - you can pay whatever you want."), PROGRAM_NAME));
	Gtk::Label subscribe_label (_("Better yet become a subscriber - subscriptions start at US$1 per month."));
	Gtk::Button pay_button (_("Pay for a copy (via the web)"));
	Gtk::Button subscribe_button (_("Become a subscriber (via the web)"));
	Gtk::HBox pay_button_box;
	Gtk::HBox subscribe_button_box;

	pay_button_box.pack_start (pay_button, true, false);
	subscribe_button_box.pack_start (subscribe_button, true, false);

	bool (*openuri)(const char*) = PBD::open_uri; /* this forces selection of the const char* variant of PBD::open_uri(), which we need to avoid ambiguity below */

	pay_button.signal_clicked().connect (sigc::hide_return (sigc::bind (sigc::ptr_fun (openuri), (const char*) "https://ardour.org/download")));
	subscribe_button.signal_clicked().connect (sigc::hide_return (sigc::bind (sigc::ptr_fun (openuri), (const char*) "https://community.ardour.org/s/subscribe")));

	msg.get_vbox()->pack_start (pay_label);
	msg.get_vbox()->pack_start (pay_button_box);
	msg.get_vbox()->pack_start (subscribe_label);
	msg.get_vbox()->pack_start (subscribe_button_box);

	msg.get_vbox()->show_all ();

	msg.add_button (_("Remain silent"), Gtk::RESPONSE_CANCEL);
	msg.add_button (_("Save and quit"), Gtk::RESPONSE_NO);
	msg.add_button (_("Give me more time"), Gtk::RESPONSE_YES);

	int r = msg.run ();

	switch (r) {
	case Gtk::RESPONSE_YES:
		AudioEngine::instance()->reset_silence_countdown ();
		break;

	case Gtk::RESPONSE_NO:
		/* save and quit */
		save_state_canfail ("");
		exit (0);
		break;

	case Gtk::RESPONSE_CANCEL:
	default:
		/* don't reset, save session and exit */
		break;
	}
}

void
ARDOUR_UI::hide_application ()
{
    Application::instance ()-> hide ();
}

void
ARDOUR_UI::setup_toplevel_window (Gtk::Window& window, const string& name, void* owner)
{
	/* icons, titles, WM stuff */

	static list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;

	if (window_icons.empty()) {
		Glib::RefPtr<Gdk::Pixbuf> icon;
		if ((icon = ::get_icon (PROGRAM_NAME "-icon_16px"))) {
			window_icons.push_back (icon);
		}
		if ((icon = ::get_icon (PROGRAM_NAME "-icon_22px"))) {
			window_icons.push_back (icon);
		}
		if ((icon = ::get_icon (PROGRAM_NAME "-icon_32px"))) {
			window_icons.push_back (icon);
		}
		if ((icon = ::get_icon (PROGRAM_NAME "-icon_48px"))) {
			window_icons.push_back (icon);
		}
	}

	if (!window_icons.empty()) {
		window.set_default_icon_list (window_icons);
	}

	Gtkmm2ext::WindowTitle title (Glib::get_application_name());

	if (!name.empty()) {
		title += name;
	}

	window.set_title (title.get_string());
	window.set_wmclass (string_compose (X_("%1_%1"), downcase (std::string(PROGRAM_NAME)), downcase (name)), PROGRAM_NAME);

	window.set_flags (CAN_FOCUS);
	window.add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	/* This is a hack to ensure that GTK-accelerators continue to
	 * work. Once we switch over to entirely native bindings, this will be
	 * unnecessary and should be removed
	 */
	window.add_accel_group (ActionManager::ui_manager->get_accel_group());

	window.signal_configure_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::configure_handler));
	window.signal_window_state_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbed_window_state_event_handler), owner));
	window.signal_key_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::key_event_handler), &window), false);
	window.signal_key_release_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::key_event_handler), &window), false);
}

bool
ARDOUR_UI::key_event_handler (GdkEventKey* ev, Gtk::Window* event_window)
{
	Gtkmm2ext::Bindings* bindings = 0;
	Gtk::Window* window = 0;

	/* until we get ardour bindings working, we are not handling key
	 * releases yet.
	 */

	if (ev->type != GDK_KEY_PRESS) {
		return false;
	}

	if (event_window == &_main_window) {

		window = event_window;

		/* find current tab contents */

		Gtk::Widget* w = _tabs.get_nth_page (_tabs.get_current_page());

		/* see if it uses the ardour binding system */

		if (w) {
			bindings = reinterpret_cast<Gtkmm2ext::Bindings*>(w->get_data ("ardour-bindings"));
		}

		DEBUG_TRACE (DEBUG::Accelerators, string_compose ("main window key event, bindings = %1, global = %2\n", bindings, &global_bindings));

	} else {

		window = event_window;

		/* see if window uses ardour binding system */

		bindings = reinterpret_cast<Gtkmm2ext::Bindings*>(window->get_data ("ardour-bindings"));
	}

	/* An empty binding set is treated as if it doesn't exist */

	if (bindings && bindings->empty()) {
		bindings = 0;
	}

	return key_press_focus_accelerator_handler (*window, ev, bindings);
}

static Gtkmm2ext::Bindings*
get_bindings_from_widget_heirarchy (GtkWidget** w)
{
	void* p = NULL;

	while (*w) {
		if ((p = g_object_get_data (G_OBJECT(*w), "ardour-bindings")) != 0) {
			break;
		}
		*w = gtk_widget_get_parent (*w);
	}

	return reinterpret_cast<Gtkmm2ext::Bindings*> (p);
}

bool
ARDOUR_UI::key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev, Gtkmm2ext::Bindings* bindings)
{
	GtkWindow* win = window.gobj();
	GtkWidget* focus = gtk_window_get_focus (win);
	GtkWidget* binding_widget = focus;
	bool special_handling_of_unmodified_accelerators = false;
	const guint mask = (Keyboard::RelevantModifierKeyMask & ~(Gdk::SHIFT_MASK|Gdk::LOCK_MASK));

        if (focus) {

		/* some widget has keyboard focus */

		if (GTK_IS_ENTRY(focus) || Keyboard::some_magic_widget_has_focus()) {

			/* A particular kind of focusable widget currently has keyboard
			 * focus. All unmodified key events should go to that widget
			 * first and not be used as an accelerator by default
			 */

			special_handling_of_unmodified_accelerators = true;

		} else {

			Gtkmm2ext::Bindings* focus_bindings = get_bindings_from_widget_heirarchy (&binding_widget);
			if (focus_bindings) {
				bindings = focus_bindings;
				DEBUG_TRACE (DEBUG::Accelerators, string_compose ("Switch bindings based on focus widget, now using %1\n", bindings->name()));
			}
		}
	}

        DEBUG_TRACE (DEBUG::Accelerators, string_compose ("Win = %1 [title = %9] focus = %7 (%8) Key event: code = %2  state = %3 special handling ? %4 magic widget focus ? %5 focus widget %6 named %7 mods ? %8\n",
                                                          win,
                                                          ev->keyval,
							  Gtkmm2ext::show_gdk_event_state (ev->state),
                                                          special_handling_of_unmodified_accelerators,
                                                          Keyboard::some_magic_widget_has_focus(),
							  focus,
                                                          (focus ? gtk_widget_get_name (focus) : "no focus widget"),
                                                          ((ev->state & mask) ? "yes" : "no"),
                                                          window.get_title()));

	/* This exists to allow us to override the way GTK handles
	   key events. The normal sequence is:

	   a) event is delivered to a GtkWindow
	   b) accelerators/mnemonics are activated
	   c) if (b) didn't handle the event, propagate to
	       the focus widget and/or focus chain

	   The problem with this is that if the accelerators include
	   keys without modifiers, such as the space bar or the
	   letter "e", then pressing the key while typing into
	   a text entry widget results in the accelerator being
	   activated, instead of the desired letter appearing
	   in the text entry.

	   There is no good way of fixing this, but this
	   represents a compromise. The idea is that
	   key events involving modifiers (not Shift)
	   get routed into the activation pathway first, then
	   get propagated to the focus widget if necessary.

	   If the key event doesn't involve modifiers,
	   we deliver to the focus widget first, thus allowing
	   it to get "normal text" without interference
	   from acceleration.

	   Of course, this can also be problematic: if there
	   is a widget with focus, then it will swallow
	   all "normal text" accelerators.
	*/


	if (!special_handling_of_unmodified_accelerators || (ev->state & mask)) {

		/* no special handling or there are modifiers in effect: accelerate first */

                DEBUG_TRACE (DEBUG::Accelerators, "\tactivate, then propagate\n");
		DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tevent send-event:%1 time:%2 length:%3 name %7 string:%4 hardware_keycode:%5 group:%6\n",
								  ev->send_event, ev->time, ev->length, ev->string, ev->hardware_keycode, ev->group, gdk_keyval_name (ev->keyval)));

		DEBUG_TRACE (DEBUG::Accelerators, "\tsending to window\n");
		KeyboardKey k (ev->state, ev->keyval);

		while (bindings) {

			DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tusing Ardour bindings %1 @ %2 for this event\n", bindings->name(), bindings));

			if (bindings->activate (k, Bindings::Press)) {
				DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
				return true;
			}

			if (binding_widget) {
				binding_widget = gtk_widget_get_parent (binding_widget);
				if (binding_widget) {
					bindings = get_bindings_from_widget_heirarchy (&binding_widget);
				} else {
					bindings = 0;
				}
			} else {
				bindings = 0;
			}
		}

		DEBUG_TRACE (DEBUG::Accelerators, "\tnot yet handled, try global bindings\n");

		if (global_bindings && global_bindings->activate (k, Bindings::Press)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
			return true;
		}

                DEBUG_TRACE (DEBUG::Accelerators, "\tnot accelerated, now propagate\n");

                if (gtk_window_propagate_key_event (win, ev)) {
	                DEBUG_TRACE (DEBUG::Accelerators, "\tpropagate handled\n");
	                return true;
                }

	} else {

		/* no modifiers, propagate first */

		DEBUG_TRACE (DEBUG::Accelerators, "\tpropagate, then activate\n");

		if (gtk_window_propagate_key_event (win, ev)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\thandled by propagate\n");
			return true;
		}

		DEBUG_TRACE (DEBUG::Accelerators, "\tpropagation didn't handle, so activate\n");
		KeyboardKey k (ev->state, ev->keyval);

		while (bindings) {

			DEBUG_TRACE (DEBUG::Accelerators, "\tusing Ardour bindings for this window\n");


			if (bindings->activate (k, Bindings::Press)) {
				DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
				return true;
			}

			if (binding_widget) {
				binding_widget = gtk_widget_get_parent (binding_widget);
				if (binding_widget) {
					bindings = get_bindings_from_widget_heirarchy (&binding_widget);
				} else {
					bindings = 0;
				}
			} else {
				bindings = 0;
			}
		}

		DEBUG_TRACE (DEBUG::Accelerators, "\tnot yet handled, try global bindings\n");

		if (global_bindings && global_bindings->activate (k, Bindings::Press)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
			return true;
		}
	}

	DEBUG_TRACE (DEBUG::Accelerators, "\tnot handled\n");
	return true;
}

void
ARDOUR_UI::load_bindings ()
{
	if ((global_bindings = Bindings::get_bindings (X_("Global"), global_actions)) == 0) {
		error << _("Global keybindings are missing") << endmsg;
	}
}

void
ARDOUR_UI::cancel_solo ()
{
	if (_session) {
		_session->cancel_all_solo ();
	}
}

void
ARDOUR_UI::reset_focus (Gtk::Widget* w)
{
	/* this resets focus to the first focusable parent of the given widget,
	 * or, if there is no focusable parent, cancels focus in the toplevel
	 * window that the given widget is packed into (if there is one).
	 */

	if (!w) {
		return;
	}

	Gtk::Widget* top = w->get_toplevel();

	if (!top || !top->is_toplevel()) {
		return;
	}

	w = w->get_parent ();

	while (w) {

		if (w->is_toplevel()) {
			/* Setting the focus widget to a Gtk::Window causes all
			 * subsequent calls to ::has_focus() on the nominal
			 * focus widget in that window to return
			 * false. Workaround: never set focus to the toplevel
			 * itself.
			 */
			break;
		}

		if (w->get_can_focus ()) {
			Gtk::Window* win = dynamic_cast<Gtk::Window*> (top);
			win->set_focus (*w);
			return;
		}
		w = w->get_parent ();
	}

	if (top == &_main_window) {

	}

	/* no focusable parent found, cancel focus in top level window.
	   C++ API cannot be used for this. Thanks, references.
	*/

	gtk_window_set_focus (GTK_WINDOW(top->gobj()), 0);

}
