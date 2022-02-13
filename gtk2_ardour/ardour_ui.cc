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

#include <gtkmm/accelmap.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>
#include <gtkmm/uimanager.h>

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
#include "pbd/types_convert.h"
#include "pbd/unwind.h"
#include "pbd/file_utils.h"
#include "pbd/localtime_r.h"
#include "pbd/pthread_utils.h"
#include "pbd/replace_all.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/xml++.h"

#include "temporal/tempo.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/fastmeter.h"
#include "widgets/prompter.h"
#include "widgets/tooltips.h"

#include "ardour/ardour.h"
#include "ardour/audio_backend.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/automation_watch.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/ltc_file_reader.h"
#include "ardour/monitor_control.h"
#include "ardour/midi_track.h"
#include "ardour/port.h"
#include "ardour/plugin_manager.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/recent_sessions.h"
#include "ardour/record_enable_control.h"
#include "ardour/revision.h"
#include "ardour/session_directory.h"
#include "ardour/session_route.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_utils.h"
#include "ardour/source_factory.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/triggerbox.h"
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

#include "temporal/time.h"

#include "about.h"
#include "editing.h"
#include "enums_convert.h"
#include "actions.h"
#include "add_route_dialog.h"
#include "ambiguous_file_dialog.h"
#include "ardour_message.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "audio_region_view.h"
#include "big_clock_window.h"
#include "big_transport_window.h"
#include "bundle_manager.h"
#include "dsp_stats_window.h"
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
#include "plugin_dspload_window.h"
#include "plugin_manager_ui.h"
#include "processor_box.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "recorder_ui.h"
#include "route_time_axis.h"
#include "route_params_ui.h"
#include "save_as_dialog.h"
#include "save_template_dialog.h"
#include "script_selector.h"
#include "session_archive_dialog.h"
#include "session_dialog.h"
#include "session_metadata_dialog.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "splash.h"
#include "template_dialog.h"
#include "time_axis_view_item.h"
#include "time_info_box.h"
#include "timers.h"
#include "transport_masters_dialog.h"
#include "trigger_page.h"
#include "triggerbox_ui.h"
#include "utils.h"
#include "utils_videotl.h"
#include "video_server_dialog.h"
#include "virtual_keyboard_window.h"
#include "add_video_dialog.h"
#include "transcode_video_dialog.h"
#include "plugin_selector.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;
using namespace Editing;

ARDOUR_UI *ARDOUR_UI::theArdourUI = 0;

sigc::signal<void, timepos_t> ARDOUR_UI::Clock;
sigc::signal<void> ARDOUR_UI::CloseAllDialogs;

static bool
ask_about_configuration_copy (string const & old_dir, string const & new_dir, int version)
{
	ArdourMessageDialog msg (string_compose (
	                          _("%1 %2.x has discovered configuration files from %1 %3.x.\n\n"
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
	, session_load_in_progress (false)
	, gui_object_state (new GUIObjectState)
	, primary_clock   (new MainClock (X_("primary"),   X_("transport"), true ))
	, secondary_clock (new MainClock (X_("secondary"), X_("secondary"), false))
	, big_clock (new AudioClock (X_("bigclock"), false, "big", true, true, false, false))
	, video_timeline(0)
	, ignore_dual_punch (false)
	, main_window_visibility (0)
	, editor (0)
	, mixer (0)
	, recorder (0)
	, trigger_page (0)
	, nsm (0)
	, _was_dirty (false)
	, _mixer_on_top (false)
	, _shared_popup_menu (0)
	, startup_fsm (0)
	, secondary_clock_spacer (0)
	, latency_disable_button (ArdourButton::led_default_elements)

	, _cue_rec_enable (_("Rec Cues"), ArdourButton::led_default_elements)
	, _cue_play_enable (_("Play Cues"), ArdourButton::led_default_elements)

	, time_info_box (0)
	, auto_return_button (ArdourButton::led_default_elements)
	, follow_edits_button (ArdourButton::led_default_elements)
	, auditioning_alert_button (_("Audition"))
	, solo_alert_button (_("Solo"))
	, feedback_alert_button (_("Feedback"))
	, error_alert_button ( ArdourButton::just_led_default_elements )
	, editor_meter_peak_display()
	, editor_meter(0)
	, _clear_editor_meter( true)
	, _editor_meter_peaked (false)
	, _numpad_locate_happening (false)
	, _session_is_new (false)
	, last_key_press_time (0)
	, save_as_dialog (0)
	, meterbridge (0)
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
	, plugin_manager_ui (X_("plugin-manager"), _("Plugin Manager"))
	, plugin_dsp_load_window (X_("plugin-dsp-load"), _("Plugin DSP Load"))
	, dsp_statistics_window (X_("dsp-statistics"), _("Performance Meters"))
	, transport_masters_window (X_("transport-masters"), _("Transport Masters"))
	, session_option_editor (X_("session-options-editor"), _("Properties"), boost::bind (&ARDOUR_UI::create_session_option_editor, this))
	, add_video_dialog (X_("add-video"), _("Add Video"), boost::bind (&ARDOUR_UI::create_add_video_dialog, this))
	, bundle_manager (X_("bundle-manager"), _("Bundle Manager"), boost::bind (&ARDOUR_UI::create_bundle_manager, this))
	, big_clock_window (X_("big-clock"), _("Big Clock"), boost::bind (&ARDOUR_UI::create_big_clock_window, this))
	, big_transport_window (X_("big-transport"), _("Transport Controls"), boost::bind (&ARDOUR_UI::create_big_transport_window, this))
	, virtual_keyboard_window (X_("virtual-keyboard"), _("Virtual Keyboard"), boost::bind (&ARDOUR_UI::create_virtual_keyboard_window, this))
	, audio_port_matrix (X_("audio-connection-manager"), _("Audio Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::AUDIO))
	, midi_port_matrix (X_("midi-connection-manager"), _("MIDI Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::MIDI))
	, key_editor (X_("key-editor"), _("Keyboard Shortcuts"), boost::bind (&ARDOUR_UI::create_key_editor, this))
	, luawindow (X_("luawindow"), S_("Window|Scripting"), boost::bind (&ARDOUR_UI::create_luawindow, this))
	, video_server_process (0)
	, have_configure_timeout (false)
	, last_configure_time (0)
	, last_peak_grab (0)
	, have_disk_speed_dialog_displayed (false)
	, _status_bar_visibility (X_("status-bar"))
	, _feedback_exists (false)
	, _ambiguous_latency (false)
	, _log_not_acknowledged (LogLevelNone)
	, duplicate_routes_dialog (0)
	, editor_visibility_button (S_("Window|Edit"))
	, mixer_visibility_button (S_("Window|Mix"))
	, prefs_visibility_button (S_("Window|Prefs"))
	, recorder_visibility_button (S_("Window|Rec"))
	, trigger_page_visibility_button (S_("Window|Cue"))
	, nsm_first_session_opened (false)
{
	Gtkmm2ext::init (localedir);

	UIConfiguration::instance().post_gui_init ();

	if (ARDOUR::handle_old_configuration_files (boost::bind (ask_about_configuration_copy, _1, _2, _3))) {
		{
			/* "touch" the been-here-before path now that config has been migrated */
			PBD::ScopedFileDescriptor fout (g_open (been_here_before_path ().c_str(), O_CREAT|O_TRUNC|O_RDWR, 0666));
		}
		ArdourMessageDialog msg (string_compose (_("Your configuration files were copied. You can now restart %1."), PROGRAM_NAME), true);
		msg.run ();
		/* configuration was modified, exit immediately */
		_exit (EXIT_SUCCESS);
	}


	if (theArdourUI == 0) {
		theArdourUI = this;
	}

	/* track main window visibility */

	main_window_visibility = new VisibilityTracker (_main_window);

	/* stop libxml from spewing to stdout/stderr */

	xmlSetGenericErrorFunc (this, libxml_generic_error_func);
	xmlSetStructuredErrorFunc (this, libxml_structured_error_func);

	/* Set this up early */

	ActionManager::init ();

	/* we like keyboards */

	keyboard = new ArdourKeyboard(*this);

	XMLNode* node = ARDOUR_UI::instance()->keyboard_settings();
	if (node) {
		keyboard->set_state (*node, Stateful::loading_state_version);
	}

	/* actions do not need to be defined when we load keybindings. They
	 * will be lazily discovered. But bindings do need to exist when we
	 * create windows/tabs with their own binding sets.
	 */

	keyboard->setup_keybindings ();

	if ((global_bindings = Bindings::get_bindings (X_("Global"))) == 0) {
		error << _("Global keybindings are missing") << endmsg;
	}

	install_actions ();

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &ARDOUR_UI::parameter_changed));
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	UIConfiguration::instance().map_parameters (pc);

	transport_ctrl.setup (this);

	ARDOUR::DiskWriter::Overrun.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::disk_overrun_handler, this), gui_context());
	ARDOUR::DiskReader::Underrun.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::disk_underrun_handler, this), gui_context());

	ARDOUR::Session::VersionMismatch.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_format_mismatch, this, _1, _2), gui_context());

	TriggerBox::CueRecordingChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::cue_rec_state_changed, this), gui_context ());
	cue_rec_state_changed();

	/* handle dialog requests */

	ARDOUR::Session::Dialog.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_dialog, this, _1), gui_context());

	/* handle pending state with a dialog (PROBLEM: needs to return a value and thus cannot be x-thread) */

	ARDOUR::Session::AskAboutPendingState.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::pending_state_dialog, this));

	/* handle sr mismatch with a dialog (PROBLEM: needs to return a value and thus cannot be x-thread) */

	ARDOUR::Session::AskAboutSampleRateMismatch.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::sr_mismatch_dialog, this, _1, _2));

	/* handle sr mismatch with a dialog - cross-thread from engine */
	ARDOUR::Session::NotifyAboutSampleRateMismatch.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::sr_mismatch_message, this, _1, _2), gui_context ());

	/* handle requests to quit (coming from JACK session) */

	ARDOUR::Session::Quit.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::finish, this), gui_context ());

	/* tell the user about feedback */

	ARDOUR::Session::FeedbackDetected.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::feedback_detected, this), gui_context ());
	ARDOUR::Session::SuccessfulGraphSort.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::successful_graph_sort, this), gui_context ());

	/* indicate global latency compensation en/disable */
	ARDOUR::Latent::DisableSwitchChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::latency_switch_changed, this), gui_context ());

	/* handle requests to deal with missing files */

	ARDOUR::Session::MissingFile.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::missing_file, this, _1, _2, _3));

	/* and ambiguous files */

	ARDOUR::FileSource::AmbiguousFileName.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::ambiguous_file, this, _1, _2));

	ARDOUR::GUIIdle.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::gui_idle_handler, this), gui_context());

	Config->ParameterChanged.connect ( forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::set_flat_buttons, this), gui_context() );
	set_flat_buttons();

	theme_changed.connect (sigc::mem_fun(*this, &ARDOUR_UI::on_theme_changed));
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ARDOUR_UI::on_theme_changed));
	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &ARDOUR_UI::on_theme_changed));

	/* lets get this party started */

	setup_gtk_ardour_enums ();
	setup_profile ();

	SessionEvent::create_per_thread_pool ("GUI", 4096);

	UIConfiguration::instance().reset_dpi ();

	TimeAxisViewItem::set_constant_heights ();

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
		big_transport_window.set_state (*ui_xml, 0);
		virtual_keyboard_window.set_state (*ui_xml, 0);
		audio_port_matrix.set_state (*ui_xml, 0);
		midi_port_matrix.set_state (*ui_xml, 0);
		luawindow.set_state (*ui_xml, 0);
		export_video_dialog.set_state (*ui_xml, 0);
		lua_script_window.set_state (*ui_xml, 0);
		idleometer.set_state (*ui_xml, 0);
		plugin_manager_ui.set_state (*ui_xml, 0);
		plugin_dsp_load_window.set_state (*ui_xml, 0);
		dsp_statistics_window.set_state (*ui_xml, 0);
		transport_masters_window.set_state (*ui_xml, 0);
	}

	/* set default parent for dialogs and windows */
	WM::Manager::instance().set_transient_for (&_main_window);

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
	WM::Manager::instance().register_window (&big_transport_window);
	WM::Manager::instance().register_window (&virtual_keyboard_window);
	WM::Manager::instance().register_window (&audio_port_matrix);
	WM::Manager::instance().register_window (&midi_port_matrix);
	WM::Manager::instance().register_window (&luawindow);
	WM::Manager::instance().register_window (&idleometer);
	WM::Manager::instance().register_window (&plugin_manager_ui);
	WM::Manager::instance().register_window (&plugin_dsp_load_window);
	WM::Manager::instance().register_window (&dsp_statistics_window);
	WM::Manager::instance().register_window (&transport_masters_window);

	/* session-sensitive windows */
	ActionManager::session_sensitive_actions.push_back (ActionManager::get_action (X_("Window"), X_("toggle-session-options-editor")));
	ActionManager::session_sensitive_actions.push_back (ActionManager::get_action (X_("Window"), X_("toggle-transport-masters")));

	/* do not retain position for add route dialog */
	add_route_dialog.set_state_mask (WindowProxy::Size);

	/* Trigger setting up the color scheme and loading the GTK RC file */

	UIConfiguration::instance().load_rc_file (false);

	_process_thread = new ProcessThread ();

	ARDOUR::Port::set_connecting_blocked (ARDOUR_COMMAND_LINE::no_connect_ports);
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
	AudioEngine::instance()->Running.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_running, this, _1), gui_context());
	engine_running (0);
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
ARDOUR_UI::engine_running (uint32_t cnt)
{
	if (cnt == 0) {
		post_engine();
	}

	update_disk_space ();
	update_cpu_load ();
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

	ArdourMessageDialog msg (_main_window, msgstr);
	msg.run ();

	if (free_reason) {
		free (const_cast<char*> (reason));
	}
}

void
ARDOUR_UI::post_engine ()
{
	/* Things to be done once (and once ONLY) after we have a backend running in the AudioEngine */

#ifdef AUDIOUNIT_SUPPORT
	string aucrsh = Glib::build_filename (ARDOUR::user_cache_directory(), "au_crash");
	if (Glib::file_test (aucrsh, Glib::FILE_TEST_EXISTS)) {
		popup_error (_("Indexing Audio Unit Plugin Failed.\nAutomatic AU scanning has been disabled\n(check with 'auval', then re-enable scanning the in preferences)."));
		::g_unlink (aucrsh.c_str());
	}
#endif

	/* connect to important signals */

	AudioEngine::instance()->Stopped.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_stopped, this), gui_context());
	AudioEngine::instance()->SampleRateChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate, this, _1), gui_context());
	AudioEngine::instance()->BufferSizeChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate, this, _1), gui_context());
	AudioEngine::instance()->Halted.connect_same_thread (halt_connection, boost::bind (&ARDOUR_UI::engine_halted, this, _1, false));
	AudioEngine::instance()->BecameSilent.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::audioengine_became_silent, this), gui_context());

	if (setup_windows ()) {
		throw failed_constructor (); // TODO catch me if you can
	}

	transport_ctrl.map_actions ();

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
		stringstream sstr;
		Bindings::save_all_bindings_as_html (sstr);

		if (sstr.str().empty()) {
			return;
		}
		gchar* file_name;
		GError *err = NULL;
		gint fd;

		if ((fd = g_file_open_tmp ("akprintXXXXXX.html", &file_name, &err)) < 0) {
			if (err) {
				error << string_compose (_("Could not open temporary file to print bindings (%1)"), err->message) << endmsg;
				g_error_free (err);
			}
			return;
		}

#ifdef PLATFORM_WINDOWS
		::close (fd);
#endif

		err = NULL;

		if (!g_file_set_contents (file_name, sstr.str().c_str(), sstr.str().size(), &err)) {
#ifndef PLATFORM_WINDOWS
			::close (fd);
#endif
			g_unlink (file_name);
			if (err) {
				error << string_compose (_("Could not save bindings to file (%1)"), err->message) << endmsg;
				g_error_free (err);
			}
			return;
		}

#ifndef PLATFORM_WINDOWS
		::close (fd);
#endif

		PBD::open_uri (string_compose ("file:///%1", file_name));

		halt_connection.disconnect ();
		AudioEngine::instance()->stop ();
		exit (EXIT_SUCCESS);

	}

	if (ARDOUR_COMMAND_LINE::show_actions) {


		vector<string> paths;
		vector<string> labels;
		vector<string> tooltips;
		vector<string> keys;
		vector<Glib::RefPtr<Gtk::Action> > actions;
		string ver_in = revision;
		string ver = ver_in.substr(0, ver_in.find("-"));

		stringstream output;
		output << "\n<h2>Menu actions</h2>" << endl;
		output << "<p>\n  Every single menu item in " << PROGRAM_NAME << "'s GUI is accessible by control" << endl;
		output << "  surfaces or scripts.\n</p>\n" << endl;
		output << "<p>\n  The list below shows all available values of <em>action-name</em> as of" << endl;
		output << "  " << PROGRAM_NAME << " " << ver << ". You can get the current list at any" << endl;
		output << "  time by running " << PROGRAM_NAME << " with the -A flag.\n</p>\n" << endl;
		output << "<table class=\"dl\">\n  <thead>" << endl;
		output << "      <tr><th>Action Name</th><th>Menu Name</th></tr>" << endl;
		output << "  </thead>\n  <tbody>" << endl;

		ActionManager::get_all_actions (paths, labels, tooltips, keys, actions);

		vector<string>::iterator p;
		vector<string>::iterator l;

		for (p = paths.begin(), l = labels.begin(); p != paths.end(); ++p, ++l) {
			output << " <tr><th><kbd class=\"osc\">" << *p << "</kbd></th><td>" << *l << "</td></tr>" << endl;
		}
		output << "  </tbody>\n  </table>" << endl;

		// output this mess to a browser for easiest X-platform use
		// it is not pretty HTML, but it works and it's main purpose
		// is to create raw html to fit in Ardour's manual with no editing
		gchar* file_name;
		GError *err = NULL;
		gint fd;

		if ((fd = g_file_open_tmp ("list-of-menu-actionsXXXXXX.html", &file_name, &err)) < 0) {
			if (err) {
				error << string_compose (_("Could not open temporary file to print bindings (%1)"), err->message) << endmsg;
				g_error_free (err);
			}
			return;
		}

#ifdef PLATFORM_WINDOWS
		::close (fd);
#endif

		err = NULL;

		if (!g_file_set_contents (file_name, output.str().c_str(), output.str().size(), &err)) {
#ifndef PLATFORM_WINDOWS
			::close (fd);
#endif
			g_unlink (file_name);
			if (err) {
				error << string_compose (_("Could not save bindings to file (%1)"), err->message) << endmsg;
				g_error_free (err);
			}
			return;
		}

#ifndef PLATFORM_WINDOWS
		::close (fd);
#endif

		PBD::open_uri (string_compose ("file:///%1", file_name));

		halt_connection.disconnect ();
		AudioEngine::instance()->stop ();
		exit (EXIT_SUCCESS);
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

	ARDOUR_UI_UTILS::inhibit_screensaver (false);

	stop_video_server();

	/* unsubscribe from AudioEngine::Stopped */
	if (recorder) {
		recorder->cleanup ();
	}

	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother at 'real' exit. the OS cleans up for us.
		delete big_clock; big_clock = 0;
		delete primary_clock; primary_clock = 0;
		delete secondary_clock; secondary_clock = 0;
		delete _process_thread; _process_thread = 0;
		delete time_info_box; time_info_box = 0;
		delete meterbridge; meterbridge = 0;
		delete duplicate_routes_dialog; duplicate_routes_dialog = 0;
		delete trigger_page; trigger_page = 0;
		delete recorder; recorder = 0;
		delete editor; editor = 0;
		delete mixer; mixer = 0;
		delete rc_option_editor; rc_option_editor = 0; // failed to wrap object warning
		delete nsm; nsm = 0;
		delete gui_object_state; gui_object_state = 0;
		delete _shared_popup_menu ; _shared_popup_menu = 0;
		delete main_window_visibility;
		FastMeter::flush_pattern_cache ();
		ArdourFader::flush_pattern_cache ();
	} else if (mixer) {
		/* drop references to any PluginInfoPtr */
		delete mixer->plugin_selector ();
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
	std::string str;

	if (node.get_property ("roll", str)){
		roll_controllable->set_id (str);
	}
	if (node.get_property ("stop", str)) {
		stop_controllable->set_id (str);
	}
	if (node.get_property ("goto-start", str)) {
		goto_start_controllable->set_id (str);
	}
	if (node.get_property ("goto-end", str)) {
		goto_end_controllable->set_id (str);
	}
	if (node.get_property ("auto-loop", str)) {
		auto_loop_controllable->set_id (str);
	}
	if (node.get_property ("play-selection", str)) {
		play_selection_controllable->set_id (str);
	}
	if (node.get_property ("rec", str)) {
		rec_controllable->set_id (str);
	}
	if (node.get_property ("shuttle", str)) {
		shuttle_box.controllable()->set_id (str);
	}
}

XMLNode&
ARDOUR_UI::get_transport_controllable_state ()
{
	XMLNode* node = new XMLNode(X_("TransportControllables"));

	node->set_property (X_("roll"), roll_controllable->id());
	node->set_property (X_("stop"), stop_controllable->id());
	node->set_property (X_("goto-start"), goto_start_controllable->id());
	node->set_property (X_("goto-end"), goto_end_controllable->id());
	node->set_property (X_("auto-loop"), auto_loop_controllable->id());
	node->set_property (X_("play-selection"), play_selection_controllable->id());
	node->set_property (X_("rec"), rec_controllable->id());
	node->set_property (X_("shuttle"), shuttle_box.controllable()->id());

	return *node;
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
	bool delete_unnamed_session = false;

	if (_session) {
		const bool unnamed = _session->unnamed();

		ARDOUR_UI::instance()->video_timeline->sync_session_state();

		if (_session->dirty() || unnamed) {
			vector<string> actions;
			actions.push_back (_("Don't quit"));

			if (_session->unnamed()) {
				actions.push_back (_("Discard session"));
				actions.push_back (_("Name session and quit"));
			} else {
				actions.push_back (_("Just quit"));
				actions.push_back (_("Save and quit"));
			}

			switch (ask_about_saving_session (actions)) {
			case -1:
				return;
				break;
			case 1:
				if (unnamed) {
					rename_session (true);
				}
				/* use the default name */
				if (save_state_canfail ("")) {
					/* failed - don't quit */
					ArdourMessageDialog msg (_main_window,
							   string_compose (_("\
%1 was unable to save your session.\n\n\
If you still wish to quit, please use the\n\n\
\"Just quit\" option."), PROGRAM_NAME));
					msg.run ();
					return;
				}
				break;
			case 0:
				if (unnamed) {
					delete_unnamed_session = true;
				}
				break;
			}
		}

		second_connection.disconnect ();
		point_one_second_connection.disconnect ();
		point_zero_something_second_connection.disconnect();
		fps_connection.disconnect();
	}

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

		if (delete_unnamed_session) {

			/* This may run a recursive dialog, which will allow
			 * for the GTK idle handler to do things. Not a problem
			 * in itself, but something to keep in mind since it
			 * isn't visually apparent that this will allow a
			 * recursive main loop to execute.
			 */

			ask_about_scratch_deletion ();
		}

		_session->set_clean ();
		delete _session;
		_session = 0;

	}

	delete ARDOUR_UI::instance()->video_timeline;
	ARDOUR_UI::instance()->video_timeline = NULL;
	stop_video_server();

	halt_connection.disconnect ();
	AudioEngine::instance()->stop ();
	quit ();
}

void
ARDOUR_UI::ask_about_scratch_deletion ()
{
	if (!_session) {
		return;
	}

	string path = _session->path();

	ArdourMessageDialog msg (_main_window,
	                         _("DANGER!"),
	                         true,
	                         Gtk::MESSAGE_WARNING,
	                         Gtk::BUTTONS_NONE, true);

	msg.set_secondary_text (string_compose (_("You have not named this session yet.\n"
	                                          "You can continue to use it as\n\n"
	                                          "%1\n\n"
	                                          "or it will be deleted.\n\n"
	                                          "Deletion is permanent and irreversible."), _session->name()));

	msg.set_title (_("SCRATCH SESSION - DANGER!"));
	msg.add_button (_("Delete this session (IRREVERSIBLE!)"), RESPONSE_OK);
	msg.add_button (_("Do not delete"), RESPONSE_CANCEL);
	msg.set_default_response (RESPONSE_CANCEL);
	msg.set_position (Gtk::WIN_POS_MOUSE);

	int r = msg.run ();

	if (r == Gtk::RESPONSE_OK) {
		PBD::remove_directory (path);
	} else {
		_session->end_unnamed_status ();
	}
}

void
ARDOUR_UI::every_second ()
{
	update_cpu_load ();
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
	if (editor) editor->build_region_boundary_cache();
}

void
ARDOUR_UI::every_point_zero_something_seconds ()
{
	// august 2007: actual update frequency: 25Hz (40ms), not 100Hz

	if (editor_meter && UIConfiguration::instance().get_show_editor_meter() && editor_meter_peak_display.is_mapped ()) {

		if (_clear_editor_meter) {
			editor_meter->clear_meters();
			editor_meter_peak_display.set_active_state (Gtkmm2ext::Off);
			_clear_editor_meter = false;
			_editor_meter_peaked = false;
		}

		const float mpeak = editor_meter->update_meters();
		const bool peaking = mpeak > UIConfiguration::instance().get_meter_peak();

		if (!_editor_meter_peaked && peaking) {
			editor_meter_peak_display.set_active_state ( Gtkmm2ext::ExplicitActive );
			_editor_meter_peaked = true;
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
				* _session->sample_rate() / _session->nominal_sample_rate()
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
ARDOUR_UI::update_sample_rate (samplecnt_t)
{
	std::string label = string_compose (X_("<span weight=\"ultralight\">%1</span>: "), _("Audio"));

	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::update_sample_rate, ignored)

	if (!AudioEngine::instance()->running()) {

		sample_rate_label.set_markup (label + _("none"));

	} else {

		samplecnt_t rate = AudioEngine::instance()->sample_rate();

		if (rate == 0) {

			/* no sample rate available */
			sample_rate_label.set_markup (label + _("none"));

		} else {
			char buf[64];

			if (fmod (rate, 1000.0) != 0.0) {
				snprintf (buf, sizeof (buf), "%.1f %s / %4.1f %s",
					  (float) rate / 1000.0f, _("kHz"),
					  (AudioEngine::instance()->usecs_per_cycle() / 1000.0f), _("ms"));
			} else {
				snprintf (buf, sizeof (buf), "%" PRId64 " %s / %4.1f %s",
					  rate / 1000, _("kHz"),
					  (AudioEngine::instance()->usecs_per_cycle() / 1000.0f), _("ms"));
			}
			sample_rate_label.set_markup (label + buf);
		}
	}
}

void
ARDOUR_UI::update_format ()
{
	if (!_session) {
		format_label.set_text ("");
		return;
	}

	stringstream s;
	s << X_("<span weight=\"ultralight\">") << _("File") << X_("</span>: ");

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
	case FLAC:
		s << _("FLAC");
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

	format_label.set_markup (s.str ());
}

void
ARDOUR_UI::update_path_label ()
{
	stringstream s;
	s << X_("<span weight=\"ultralight\">") << _("Path") << X_("</span>: ");
	if (_session) {
		s << Gtkmm2ext::markup_escape_text (_session->path());
	} else {
		s << "-";
	}
	session_path_label.set_markup (s.str ());
}

void
ARDOUR_UI::update_cpu_load ()
{
	const unsigned int x = _session ? _session->get_xrun_count () : 0;
	const bool fw = AudioEngine::instance()->freewheeling ();
	double const c = AudioEngine::instance()->get_dsp_load ();

	std::string label = string_compose (X_("<span weight=\"ultralight\">%1</span>: "), _("DSP"));
	const char* const bg = (c > 90 && !fw) ? " background=\"red\" foreground=\"white\"" : "";

	char buf[256];
	if (x > 9999) {
		snprintf (buf, sizeof (buf), "<span face=\"monospace\"%s>%2.0f%%</span> (>10k)", bg, c);
	} else if (x > 0) {
		snprintf (buf, sizeof (buf), "<span face=\"monospace\"%s>%2.0f%%</span> (%d)", bg, c, x);
	} else {
		snprintf (buf, sizeof (buf), "<span face=\"monospace\"%s>%2.0f%%</span>", bg, c);
	}

	dsp_load_label.set_markup (label + buf);

	if (x > 9999) {
		snprintf (buf, sizeof (buf), "%.1f%% X: >10k\n%s", c, _("Shift+Click to clear xruns."));
	} else if (x > 0) {
		snprintf (buf, sizeof (buf), "%.1f%% X: %u\n%s", c, x, _("Shift+Click to clear xruns."));
	} else {
		snprintf (buf, sizeof (buf), "%.1f%%", c);
	}

	ArdourWidgets::set_tooltip (dsp_load_label, label + buf);
}

void
ARDOUR_UI::update_peak_thread_work ()
{
	char buf[64];
	const int c = SourceFactory::peak_work_queue_length ();
	if (c > 0) {
		std::string label = string_compose (X_("<span weight=\"ultralight\">%1</span>: "), _("PkBld"));
		const char* const bg = c > 2 ? " background=\"red\" foreground=\"white\"" : "";
		snprintf (buf, sizeof (buf), "<span %s>%d</span>", bg, c);
		peak_thread_work_label.set_markup (label + buf);
	} else {
		peak_thread_work_label.set_markup (X_(""));
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
ARDOUR_UI::format_disk_space_label (float remain_sec)
{
	if (remain_sec < 0) {
		disk_space_label.set_text (_("N/A"));
		ArdourWidgets::set_tooltip (disk_space_label, _("Unknown"));
		return;
	}

	char buf[64];

	int sec = floor (remain_sec);
	int hrs  = sec / 3600;
	int mins = (sec / 60) % 60;
	int secs = sec % 60;
	snprintf (buf, sizeof(buf), _("%02dh:%02dm:%02ds"), hrs, mins, secs);
	ArdourWidgets::set_tooltip (disk_space_label, string_compose ("%1: %2", _("Available record time"), buf));

	std::string label = string_compose (X_("<span weight=\"ultralight\">%1</span>: "), _("Rec"));

	if (remain_sec > 86400) {
		disk_space_label.set_markup (label + _(">24h"));
	} else if (remain_sec > 32400 /* 9 hours */) {
		snprintf (buf, sizeof (buf), "%.0f", remain_sec / 3600.f);
		disk_space_label.set_markup (label + buf + S_("hours|h"));
	} else if (remain_sec > 5940 /* 99 mins */) {
		snprintf (buf, sizeof (buf), "%.1f", remain_sec / 3600.f);
		disk_space_label.set_markup (label + buf + S_("hours|h"));
	} else {
		snprintf (buf, sizeof (buf), "%.0f", remain_sec / 60.f);
		disk_space_label.set_markup (label + buf + S_("minutes|m"));
	}

}

void
ARDOUR_UI::update_disk_space()
{
	if (_session == 0) {
		format_disk_space_label (-1);
		return;
	}

	boost::optional<samplecnt_t> opt_samples = _session->available_capture_duration();
	samplecnt_t fr = _session->sample_rate();

	if (fr == 0) {
		/* skip update - no SR available */
		format_disk_space_label (-1);
		return;
	}

	if (!opt_samples) {
		/* Available space is unknown */
		format_disk_space_label (-1);
	} else if (opt_samples.value_or (0) == max_samplecnt) {
		format_disk_space_label (max_samplecnt);
	} else {
		rec_enabled_streams = 0;
		_session->foreach_route (this, &ARDOUR_UI::count_recenabled_streams, false);

		samplecnt_t samples = opt_samples.value_or (0);

		if (rec_enabled_streams) {
			samples /= rec_enabled_streams;
		}

		format_disk_space_label (samples / (float)fr);
	}

}

void
ARDOUR_UI::update_timecode_format ()
{
	std::string label = string_compose (X_("<span weight=\"ultralight\">%1</span>: "), S_("Timecode|TC"));

	if (_session) {
		bool matching;
		boost::shared_ptr<TimecodeTransportMaster> tcmaster;
		boost::shared_ptr<TransportMaster> tm = TransportMasterManager::instance().current();

		if ((tm->type() == LTC || tm->type() == MTC) && (tcmaster = boost::dynamic_pointer_cast<TimecodeTransportMaster>(tm)) != 0 && tm->locked()) {
			matching = (tcmaster->apparent_timecode_format() == _session->config.get_timecode_format());
		} else {
			matching = true;
		}

		const char* const bg = matching ? "" : " background=\"red\" foreground=\"white\"";

		timecode_format_label.set_markup (string_compose ("%1<span%2>%3</span>",
					label, bg,
					Timecode::timecode_format_name (_session->config.get_timecode_format()).c_str()));
	} else {
		timecode_format_label.set_markup (label + _("n/a"));
	}

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
ARDOUR_UI::session_add_midi_route (
		bool disk,
		RouteGroup* route_group,
		uint32_t how_many,
		const string& name_template,
		bool strict_io,
		PluginInfoPtr instrument,
		Plugin::PresetRecord* pset,
		ARDOUR::PresentationInfo::order_t order,
		bool trigger_visibility)
{
	if (_session == 0) {
		warning << _("You cannot add a track without a session already loaded.") << endmsg;
		return;
	}

	if (Profile->get_mixbus ()) {
		strict_io = true;
	}

	try {
		if (disk) {

			ChanCount one_midi_channel;
			one_midi_channel.set (DataType::MIDI, 1);

			list<boost::shared_ptr<MidiTrack> > tracks;
			tracks = _session->new_midi_track (one_midi_channel, one_midi_channel, strict_io, instrument, pset, route_group, how_many, name_template, order, ARDOUR::Normal, true, trigger_visibility);

			if (tracks.size() != how_many) {
				error << string_compose(P_("could not create %1 new mixed track", "could not create %1 new mixed tracks", how_many), how_many) << endmsg;
			}

		} else {

			RouteList routes;
			routes = _session->new_midi_route (route_group, how_many, name_template, strict_io, instrument, pset, PresentationInfo::MidiBus, order);

			if (routes.size() != how_many) {
				error << string_compose(P_("could not create %1 new Midi Bus", "could not create %1 new Midi Busses", how_many), how_many) << endmsg;
			}

		}
	}
	catch (...) {
		display_insufficient_ports_message ();
		return;
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
	ARDOUR::PresentationInfo::order_t order,
	bool trigger_visibility)
{
	list<boost::shared_ptr<AudioTrack> > tracks;
	RouteList routes;

	assert (_session);

	try {
		if (track) {
			tracks = _session->new_audio_track (input_channels, output_channels, route_group, how_many, name_template, order, mode, true, trigger_visibility);

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
ARDOUR_UI::session_add_foldback_bus (int32_t channels, uint32_t how_many, string const & name_template)
{
	RouteList routes;

	assert (_session);

	try {
		routes = _session->new_audio_route (channels, channels, 0, how_many, name_template, PresentationInfo::FoldbackBus, -1);

		if (routes.size() != how_many) {
			error << string_compose (P_("could not create %1 new foldback bus", "could not create %1 new foldback busses", how_many), how_many)
			      << endmsg;
		}
	}

	catch (...) {
		display_insufficient_ports_message ();
		return;
	}
}

void
ARDOUR_UI::display_insufficient_ports_message ()
{
	ArdourMessageDialog msg (_main_window,
			string_compose (_("There are insufficient ports available\n\
to create a new track or bus.\n\
You should save %1, exit and\n\
restart with more ports."), PROGRAM_NAME));
	msg.run ();
}

void
ARDOUR_UI::transport_goto_start ()
{
	if (_session) {
		_session->goto_start ();

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (_session->current_start_sample ());
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
		samplepos_t samples;

		time (&now);
		localtime_r (&now, &tmnow);

		samplecnt_t sample_rate = _session->sample_rate();

		if (sample_rate == 0) {
			/* no frame rate available */
			return;
		}

		samples = tmnow.tm_hour * (60 * 60 * sample_rate);
		samples += tmnow.tm_min * (60 * sample_rate);
		samples += tmnow.tm_sec * sample_rate;

		_session->request_locate (samples);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (samples);
		}
	}
}

void
ARDOUR_UI::transport_goto_end ()
{
	if (_session) {
		samplepos_t const sample = _session->current_end_sample();
		_session->request_locate (sample);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (sample);
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
				ArdourMessageDialog msg (_main_window, _("Please create one or more tracks before trying to record.\nYou can do this with the \"Add Track or Bus\" option in the Session menu."));
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
			if (roll) {
				transport_roll();
			} else {
				_session->disable_record (false, true);
			}
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

	if (_session->config.get_external_sync()) {
		switch (TransportMasterManager::instance().current()->type()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = _session->transport_rolling();

	if (_session->get_play_loop()) {

		/* If loop playback is not a mode, then we should cancel
		   it when this action is requested. If it is a mode
		   we just leave it in place.
		*/

		if (!Config->get_loop_is_mode()) {
			/* stop loop playback but keep transport state */
			_session->request_play_loop (false, false);
		}

	} else if (_session->get_play_range () ) {
		/* stop playing a range if we currently are */
		_session->request_play_range (0, true);
	}

	if (!rolling) {
		_session->request_roll ();
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
		switch (TransportMasterManager::instance().current()->type()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = _session->transport_rolling();

	if (rolling) {

		if (roll_out_of_bounded_mode) {
			/* drop out of loop/range playback but leave transport rolling */

			if (_session->get_play_loop()) {

				if (_session->actively_recording()) {
					/* actually stop transport because
					   otherwise the captured data will make
					   no sense.
					*/
					_session->request_play_loop (false, true);

				} else {
					_session->request_play_loop (false, false);
				}

			} else if (_session->get_play_range ()) {

				_session->request_cancel_play_range ();
			}

		} else {
			_session->request_stop (with_abort, true);
		}

	} else { /* not rolling */

		if (with_abort) { /* with_abort == true means the command was intended to stop transport, not start. */
			return;
		}

		if (_session->get_play_loop() && Config->get_loop_is_mode()) {
			_session->request_locate (_session->locations()->auto_loop_location()->start().samples(), MustRoll);
		} else {
			if (UIConfiguration::instance().get_follow_edits()) {
				list<TimelineRange>& range = editor->get_selection().time;
				if (range.front().start().samples() == _session->transport_sample()) { // if playhead is exactly at the start of a range, we assume it was placed there by follow_edits
					_session->request_play_range (&range, true);
					_session->set_requested_return_sample (range.front().start().samples());  //force an auto-return here
				}
			}
			_session->request_roll ();
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
ARDOUR_UI::transport_ffwd_rewind (bool fwd)
{
	if (!_session) {
		return;
	}
	// incrementally increase speed by semitones
	// (keypress auto-repeat is 100ms)
	const float maxspeed = Config->get_shuttle_max_speed();
	float semitone_ratio = exp2f (1.0f/12.0f);
	const float octave_down = powf (1.f / semitone_ratio, 12.f);
	float transport_speed = _session->actual_speed();
	float speed;

	if (Config->get_rewind_ffwd_like_tape_decks()) {

		if (fwd) {
			if (transport_speed <= 0) {
				_session->request_transport_speed (1.0);
				_session->request_roll (TRS_UI);
				return;
			}
		} else {
			if (transport_speed >= 0) {
				_session->request_transport_speed (-1.0);
				_session->request_roll (TRS_UI);
				return;
			}
		}


	} else {

		if (fabs (transport_speed) <= 0.1) {

			/* close to zero, maybe flip direction */

			if (fwd) {
				if (transport_speed <= 0) {
					_session->request_transport_speed (1.0);
					_session->request_roll (TRS_UI);
				}
			} else {
				if (transport_speed >= 0) {
					_session->request_transport_speed (-1.0);
					_session->request_roll (TRS_UI);
				}
			}

			/* either we've just started, or we're moving as slowly as we
			 * ever should
			 */

			return;
		}

		if (fwd) {
			if (transport_speed < 0.f) {
				if (fabs (transport_speed) < octave_down) {
					/* we need to move the speed back towards zero */
					semitone_ratio = powf (1.f / semitone_ratio, 4.f);
				} else {
					semitone_ratio = 1.f / semitone_ratio;
				}
			} else {
				if (fabs (transport_speed) < octave_down) {
					/* moving very slowly, use 4 semitone steps */
					semitone_ratio = powf (semitone_ratio, 4.f);
				}
			}
		} else {
			if (transport_speed > 0.f) {
				/* we need to move the speed back towards zero */

				if (transport_speed < octave_down) {
					semitone_ratio = powf (1.f / semitone_ratio, 4.f);
				} else {
					semitone_ratio = 1.f / semitone_ratio;
				}
			} else {
				if (fabs (transport_speed) < octave_down) {
					/* moving very slowly, use 4 semitone steps */
					semitone_ratio = powf (semitone_ratio, 4.f);
				}
			}
		}

	}

	speed = semitone_ratio * transport_speed;
	speed = std::max (-maxspeed, std::min (maxspeed, speed));
	_session->request_transport_speed (speed);
	_session->request_roll (TRS_UI);

}

void
ARDOUR_UI::transport_rewind ()
{
	transport_ffwd_rewind (false);
}

void
ARDOUR_UI::transport_forward ()
{
	transport_ffwd_rewind (true);
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
		layered_button.set_sensitive (false);
		if (UIConfiguration::instance().get_screen_saver_mode () == InhibitWhileRecording) {
			inhibit_screensaver (false);
		}
		return;
	}

	shuttle_box.map_transport_state ();

	float sp = _session->transport_speed();

	if (sp != 0.0f) {
		layered_button.set_sensitive (!_session->actively_recording ());
	} else {
		layered_button.set_sensitive (true);
		update_disk_space ();
	}
	if (UIConfiguration::instance().get_screen_saver_mode () == InhibitWhileRecording) {
		inhibit_screensaver (_session->actively_recording ());
	}
}

void
ARDOUR_UI::blink_handler (bool blink_on)
{
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

	if (editor && !editor->dragging_playhead() && !editor->pending_locate_request()) {
		Clock (timepos_t (_session->audible_sample())); /* EMIT_SIGNAL */
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

void
ARDOUR_UI::save_state (const string & name, bool switch_to_it)
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	if (_session->unnamed()) {
		rename_session (true);
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
		_session->request_locate (primary_clock->current_time ().samples());
	}
}

void
ARDOUR_UI::big_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (big_clock->current_time ().samples());
	}
}

void
ARDOUR_UI::secondary_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (secondary_clock->current_time ().samples());
	}
}
void
ARDOUR_UI::save_template_dialog_response (int response, SaveTemplateDialog* d)
{
	if (response == RESPONSE_ACCEPT) {
		const string name = d->get_template_name ();
		const string desc = d->get_description ();

		int failed = _session->save_template (name, desc);

		if (failed == -2) { /* file already exists. */
			bool overwrite = overwrite_file_dialog (*d,
								_("Confirm Template Overwrite"),
								_("A template already exists with that name. Do you want to overwrite it?"));

			if (overwrite) {
				_session->save_template (name, desc, true);
			}
			else {
				d->show ();
				return;
			}
		}
	}
	delete d;
}

void
ARDOUR_UI::save_template ()
{
	const std::string desc = SessionMetadata::Metadata()->description ();
	SaveTemplateDialog* d = new SaveTemplateDialog (_session->name (), desc);
	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::save_template_dialog_response), d));
	d->show ();
}

void ARDOUR_UI::manage_templates ()
{
	TemplateDialog td;
	td.run();
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

static void _lua_print (std::string s) {
#ifndef NDEBUG
	std::cout << "LuaTemplate: " << s << "\n";
#endif
	PBD::info << "LuaTemplate: " << s << endmsg;
}

std::map<std::string, std::string>
ARDOUR_UI::route_setup_info (const std::string& script_path)
{
	std::map<std::string, std::string> rv;

	if (!Glib::file_test (script_path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
		return rv;
	}

	LuaState lua;
	lua.Print.connect (&_lua_print);
	lua.sandbox (true);

	lua_State* L = lua.getState();
	LuaInstance::register_classes (L);
	LuaBindings::set_session (L, _session);
	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");

	lua.do_command ("function ardour () end");
	lua.do_file (script_path);

	try {
		luabridge::LuaRef fn = luabridge::getGlobal (L, "route_setup");
		if (!fn.isFunction ()) {
			return rv;
		}
		luabridge::LuaRef rs = fn ();
		if (!rs.isTable ()) {
			return rv;
		}
		for (luabridge::Iterator i(rs); !i.isNil (); ++i) {
			if (!i.key().isString()) {
				continue;
			}
			std::string key = i.key().tostring();
			if (i.value().isString() || i.value().isNumber() || i.value().isBoolean()) {
				rv[key] = i.value().tostring();
			}
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
	return rv;
}

void
ARDOUR_UI::meta_route_setup (const std::string& script_path)
{
	if (!Glib::file_test (script_path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
		return;
	}
	assert (add_route_dialog);

	int count;
	if ((count = add_route_dialog->count()) <= 0) {
		return;
	}

	LuaState lua;
	lua.Print.connect (&_lua_print);
	lua.sandbox (true);

	lua_State* L = lua.getState();
	LuaInstance::register_classes (L);
	LuaBindings::set_session (L, _session);
	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");

	lua.do_command ("function ardour () end");
	lua.do_file (script_path);

	luabridge::LuaRef args (luabridge::newTable (L));

	args["name"]       = add_route_dialog->name_template ();
	args["insert_at"]  = translate_order (add_route_dialog->insert_at());
	args["group"]      = add_route_dialog->route_group ();
	args["strict_io"]  = add_route_dialog->use_strict_io ();
	args["instrument"] = add_route_dialog->requested_instrument ();
	args["track_mode"] = add_route_dialog->mode ();
	args["channels"]   = add_route_dialog->channel_count ();
	args["how_many"]   = count;

	try {
		luabridge::LuaRef fn = luabridge::getGlobal (L, "factory");
		if (fn.isFunction()) {
			fn (args)();
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) {
		display_insufficient_ports_message ();
	}
}

void
ARDOUR_UI::meta_session_setup (const std::string& script_path)
{
	if (!Glib::file_test (script_path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
		return;
	}

	LuaState lua;
	lua.Print.connect (&_lua_print);
	lua.sandbox (true);

	lua_State* L = lua.getState();
	LuaInstance::register_classes (L);
	LuaBindings::set_session (L, _session);
	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");

	lua.do_command ("function ardour () end");
	lua.do_file (script_path);

	try {
		luabridge::LuaRef fn = luabridge::getGlobal (L, "factory");
		if (fn.isFunction()) {
			fn ()();
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) {
		display_insufficient_ports_message ();
	}
}

void
ARDOUR_UI::display_cleanup_results (ARDOUR::CleanupReport const& rep, const gchar* list_title, const bool msg_delete)
{
	size_t removed;

	removed = rep.paths.size();

	if (removed == 0) {
		ArdourMessageDialog msgd (_main_window,
		                          _("No files were ready for clean-up"),
		                          true,
		                          Gtk::MESSAGE_INFO,
		                          Gtk::BUTTONS_OK);
		msgd.set_title (_("Clean-up"));
		msgd.set_secondary_text (_("If this seems surprising, \n\
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

	for (vector<string>::const_iterator i = rep.paths.begin(); i != rep.paths.end(); ++i) {
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


	ArdourMessageDialog checker (_("Are you sure you want to clean-up?"),
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
	checker.hide();

	ARDOUR::CleanupReport rep;

	editor->prepare_for_cleanup ();

	/* do not allow flush until a session is reloaded */
	ActionManager::get_action (X_("Main"), X_("FlushWastebasket"))->set_sensitive (false);

	if (_session->cleanup_sources (rep)) {
		editor->finish_cleanup ();
		return;
	}

	editor->finish_cleanup ();

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
	editor->get_regions_after(rs, timepos_t (), empty);
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

	ArdourWaveView::WaveView::clear_cache ();

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
		return place == RouteDialogs::First ? 0 : PresentationInfo::max_order;
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
	if (!_session || !_session->writable() || _session->actively_recording()) {
		return;
	}

	if (!add_route_dialog.get (false)) {
		add_route_dialog->signal_response().connect (sigc::mem_fun (*this, &ARDOUR_UI::add_route_dialog_response));
	}

	if (add_route_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	add_route_dialog->set_position (WIN_POS_MOUSE);
	add_route_dialog->present();
}

void
ARDOUR_UI::add_route_dialog_response (int r)
{
	if (!_session) {
		warning << _("You cannot add tracks or busses without a session already loaded.") << endmsg;
		return;
	}

	if (!AudioEngine::instance()->running ()) {
		switch (r) {
			case AddRouteDialog::Add:
			case AddRouteDialog::AddAndClose:
				break;
			default:
				return;
		}
		add_route_dialog->ArdourDialog::on_response (r);
		ARDOUR_UI_UTILS::engine_is_running ();
		return;
	}

	int count;

	switch (r) {
	case AddRouteDialog::Add:
		add_route_dialog->reset_name_edited ();
		break;
	case AddRouteDialog::AddAndClose:
		add_route_dialog->ArdourDialog::on_response (r);
		break;
	default:
		add_route_dialog->ArdourDialog::on_response (r);
		return;
	}

	std::string template_path = add_route_dialog->get_template_path();
	if (!template_path.empty() && template_path.substr (0, 11) == "urn:ardour:") {
		meta_route_setup (template_path.substr (11));
		return;
	}

	if ((count = add_route_dialog->count()) <= 0) {
		return;
	}

	PresentationInfo::order_t order = translate_order (add_route_dialog->insert_at());
	const string name_template = add_route_dialog->name_template ();
	DisplaySuspender ds;

	if (!template_path.empty ()) {
		if (add_route_dialog->name_template_is_default ()) {
			_session->new_route_from_template (count, order, template_path, string ());
		} else {
			_session->new_route_from_template (count, order, template_path, name_template);
		}
		return;
	}

	ChanCount input_chan= add_route_dialog->channels ();
	ChanCount output_chan;
	PluginInfoPtr instrument = add_route_dialog->requested_instrument ();
	RouteGroup* route_group = add_route_dialog->route_group ();
	AutoConnectOption oac = Config->get_output_auto_connect();
	bool strict_io = add_route_dialog->use_strict_io ();
	bool trigger_visibility = true;

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
		session_add_audio_route (true, input_chan.n_audio(), output_chan.n_audio(), add_route_dialog->mode(), route_group, count, name_template, strict_io, order, trigger_visibility);
		break;
	case AddRouteDialog::AudioBus:
		session_add_audio_route (false, input_chan.n_audio(), output_chan.n_audio(), ARDOUR::Normal, route_group, count, name_template, strict_io, order, false);
		break;
	case AddRouteDialog::MidiTrack:
		session_add_midi_route (true, route_group, count, name_template, strict_io, instrument, 0, order, trigger_visibility);
		break;
	case AddRouteDialog::MidiBus:
		session_add_midi_route (false, route_group, count, name_template, strict_io, instrument, 0, order, false);
		break;
	case AddRouteDialog::VCAMaster:
		_session->vca_manager().create_vca (count, name_template);
		break;
	case AddRouteDialog::FoldbackBus:
		session_add_foldback_bus (input_chan.n_audio(), count, name_template);
		ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleFoldbackStrip"))->set_active (true);
		break;
	}
}

void
ARDOUR_UI::disk_overrun_handler ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::disk_overrun_handler)

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		ArdourMessageDialog* msg = new ArdourMessageDialog (_main_window, string_compose (_("\
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
		ArdourMessageDialog* msg = new ArdourMessageDialog (
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
	ArdourMessageDialog d (msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	d.run ();
}

int
ARDOUR_UI::pending_state_dialog ()
{
	HBox* hbox = manage (new HBox());
	Image* image = manage (new Image (Stock::DIALOG_QUESTION, ICON_SIZE_DIALOG));
	ArdourDialog dialog (_("Crash Recovery"), true);
	Label  message (string_compose (_("\
This session appears to have been modified\n\
without save, or in middle of recording when\n\
%1 or the computer was shutdown.\n\
\n\
%1 can recover any changes for\n\
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

	if (Splash::exists ()) {
		Splash::instance()->hide ();
	}

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		return 1;
	default:
		return 0;
	}
}

void
ARDOUR_UI::store_clock_modes ()
{
	if (session_load_in_progress) {
		/* Do not overwrite clock modes while loading them (with a session) */
		return;
	}

	XMLNode* node = new XMLNode(X_("ClockModes"));

	for (vector<AudioClock*>::iterator x = AudioClock::clocks.begin(); x != AudioClock::clocks.end(); ++x) {
		XMLNode* child = new XMLNode (X_("Clock"));

		child->set_property (X_("name"), (*x)->name());
		child->set_property (X_("mode"), (*x)->mode());
		child->set_property (X_("on"), (*x)->on());

		node->add_child_nocopy (*child);
	}

	_session->add_extra_xml (*node);
	_session->set_dirty ();
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
ARDOUR_UI::reset_peak_display ()
{
	if (!_session || !_session->master_out() || !editor_meter) return;
	_clear_editor_meter = true;
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

