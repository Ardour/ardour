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

#include <gtkmm/progressbar.h>
#include <gtkmm/stock.h>

#include "pbd/basename.h"
#include "pbd/localtime_r.h"
#include "pbd/unwind.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/doi.h"

#include "widgets/prompter.h"

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/session_utils.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_directory.h"

#include "ardour_message.h"
#include "ardour_ui.h"
#include "engine_dialog.h"
#include "missing_filesource_dialog.h"
#include "missing_plugin_dialog.h"
#include "opts.h"
#include "public_editor.h"
#include "save_as_dialog.h"
#include "session_dialog.h"
#include "session_archive_dialog.h"
#include "timers.h"
#include "utils.h"

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace std;
using namespace ArdourWidgets;

bool
ARDOUR_UI::ask_about_loading_existing_session (const std::string& session_path)
{
	std::string str = string_compose (_("This session\n%1\nalready exists. Do you want to open it?"), session_path);

	ArdourMessageDialog msg (str,
	                         false,
	                         Gtk::MESSAGE_WARNING,
	                         Gtk::BUTTONS_YES_NO,
	                         true);

	msg.set_name (X_("OpenExistingDialog"));
	msg.set_title (_("Open Existing Session"));
	msg.set_wmclass (X_("existing_session"), PROGRAM_NAME);
	msg.set_position (Gtk::WIN_POS_CENTER);

	switch (msg.run()) {
	case RESPONSE_YES:
		return true;
		break;
	}
	return false;
}

void
ARDOUR_UI::build_session_from_dialog (SessionDialog& sd, const std::string& session_path, const std::string& session_name, std::string const& session_template)
{
	BusProfile bus_profile;

	if (nsm) {
		bus_profile.master_out_channels = 2;
	} else if ( Profile->get_mixbus()) {
		bus_profile.master_out_channels = 2;
	} else {
		/* get settings from advanced section of NSD */
		bus_profile.master_out_channels = (uint32_t) sd.master_channel_count();
	}

	build_session (session_path, session_name, session_template, bus_profile);
}

/** This is only ever used once Ardour is already running with a session
 * loaded. The startup case is handled by StartupFSM
 */
void
ARDOUR_UI::start_session_load (bool create_new)
{
	/* deal with any existing DIRTY session now, rather than later. don't
	 * treat a non-dirty session this way, so that it stays visible
	 * as we bring up the new session dialog.
	 */

	if (_session && ARDOUR_UI::instance()->video_timeline) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();
	}

	if (_session && _session->dirty()) {
		if (unload_session (false)) {
			/* unload cancelled by user */
			return;
		}
	}

	SessionDialog* session_dialog = new SessionDialog (create_new, string(), Config->get_default_session_parent_dir(), string(), true);
	session_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::session_dialog_response_handler), session_dialog));
	session_dialog->present ();
}

void
ARDOUR_UI::session_dialog_response_handler (int response, SessionDialog* session_dialog)
{
	string session_name;
	string session_path;
	string template_name;
	bool likely_new = false;

	session_path = "";
	session_name = "";

	switch (response) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return; /* back to main event loop */
	}

	session_name = session_dialog->session_name (likely_new);
	session_path = session_dialog->session_folder ();

	if (nsm) {
		likely_new = true;
	}

	/* could be an archived session, so test for that and use the
	 * result if it was
	 */

	if (!likely_new) {
		int rv = ARDOUR::inflate_session (session_name, Config->get_default_session_parent_dir(), session_path, session_name);

		if (rv < 0) {
			ArdourMessageDialog msg (*session_dialog, string_compose (_("Extracting session-archive failed: %1"), inflate_error (rv)));
			msg.run ();
			return; /* back to main event loop */
		} else if (rv == 0) {
			session_dialog->set_provided_session (session_name, session_path);
		}
	}

	string::size_type suffix = session_name.find (statefile_suffix);

	if (suffix != string::npos) {
		session_name = session_name.substr (0, suffix);
	}

	/* this shouldn't happen, but we catch it just in case it does */

	if (session_name.empty()) {
		return; /* back to main event loop */
	}

	if (session_dialog->use_session_template()) {
		template_name = session_dialog->session_template_name();
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

		session_path = session_dialog->session_folder();

		char illegal = Session::session_name_is_legal (session_name);

		if (illegal) {
			ArdourMessageDialog msg (*session_dialog,
			                         string_compose (_("To ensure compatibility with various systems\n"
			                                           "session names may not contain a '%1' character"),
			                                         illegal));
			msg.run ();
			return; /* back to main event loop */
		}
	}

	if (Glib::file_test (session_path, Glib::FileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {


		if (likely_new && !nsm) {

			std::string existing = Glib::build_filename (session_path, session_name);

			if (!ask_about_loading_existing_session (existing)) {
				return; /* back to main event loop */
			}
		}

		_session_is_new = false;

	} else {

		if (!likely_new) {
			ArdourMessageDialog msg (string_compose (_("There is no existing session at \"%1\""), session_path));
			msg.run ();
			return; /* back to main event loop */
		}

		char illegal = Session::session_name_is_legal(session_name);

		if (illegal) {
			ArdourMessageDialog msg (*session_dialog, string_compose(_("To ensure compatibility with various systems\n"
			                                                           "session names may not contain a '%1' character"), illegal));
			msg.run ();
			return; /* back to main event loop */

		}

		_session_is_new = true;
	}


	/* OK, parameters provided ... good to go. */

	session_dialog->hide ();
	delete_when_idle (session_dialog);

	if (!template_name.empty() || likely_new) {

		build_session_from_dialog (*session_dialog, session_path, session_name, template_name);

	} else {

		load_session (session_path, session_name, template_name);
	}
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

	start_session_load (false);
}


/** @param snap_name Snapshot name (without .ardour suffix).
 *  @return -2 if the load failed because we are not connected to the AudioEngine.
 */
int
ARDOUR_UI::load_session (const std::string& path, const std::string& snap_name, std::string mix_template)
{
	/* load_session calls flush_pending() which allows
	 * GUI interaction and potentially loading another session
	 * (that was easy via snapshot sidebar).
	 * Recursing into load_session() from load_session() and recusive
	 * event loops causes all kind of crashes.
	 */
	assert (!session_load_in_progress);
	if (session_load_in_progress) {
		return -1;
	}
	PBD::Unwinder<bool> lsu (session_load_in_progress, true);

	int unload_status;
	bool had_session = false;

	if (_session) {
		had_session = true;

		unload_status = unload_session ();

		if (unload_status != 0) {
			hide_splash ();
			return -1;
		}
	}

	if (had_session) {
		float sr;
		SampleFormat sf;
		string pv;

		Session::get_info_from_path (Glib::build_filename (path, snap_name + statefile_suffix), sr, sf, pv);

		/* this will stop the engine if the SR is different */

		audio_midi_setup->set_desired_sample_rate (sr);

		if (!AudioEngine::instance()->running()) {
			audio_midi_setup->set_position (WIN_POS_CENTER);
			audio_midi_setup->present ();
			_engine_dialog_connection = audio_midi_setup->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::audio_midi_setup_reconfigure_done), path, snap_name, mix_template));
			/* not done yet, but we're avoiding modal dialogs */
			return 0;
		}
	}

	return load_session_stage_two (path, snap_name, mix_template);
}

void
ARDOUR_UI::audio_midi_setup_reconfigure_done (int response, std::string path, std::string snap_name, std::string mix_template)
{
	_engine_dialog_connection.disconnect ();

	switch (response) {
	case Gtk::RESPONSE_DELETE_EVENT:
		break;
	default:
		if (!AudioEngine::instance()->running()) {
			return; // keep dialog visible, maybe try again
		}
	}

	audio_midi_setup->hide();

	(void) load_session_stage_two (path, snap_name, mix_template);
}

int
ARDOUR_UI::load_session_stage_two (const std::string& path, const std::string& snap_name, std::string mix_template)
{
	Session *new_session;
	int retval = -1;

	BootMessage (string_compose (_("Please wait while %1 loads your session"), PROGRAM_NAME));

	try {
		new_session = new Session (*AudioEngine::instance(), path, snap_name, 0, mix_template);
	}

	/* this one is special */

	catch (AudioEngine::PortRegistrationFailure const& err) {

		ArdourMessageDialog msg (err.what(),
		                         true,
		                         Gtk::MESSAGE_INFO,
		                         Gtk::BUTTONS_CLOSE);

		msg.set_title (_("Port Registration Error"));
		msg.set_secondary_text (_("Click the Close button to try again."));
		msg.set_position (Gtk::WIN_POS_CENTER);

		int response = msg.run ();
		msg.hide ();

		switch (response) {
		case RESPONSE_CANCEL:
			exit (EXIT_FAILURE);
		default:
			break;
		}
		goto out;
	}
	catch (SessionException const& e) {
		stringstream ss;
		dump_errors (ss, 6);
		dump_errors (cerr);
		clear_errors ();
		ArdourMessageDialog msg (string_compose(
			                         _("Session \"%1 (snapshot %2)\" did not load successfully:\n%3%4%5"),
			                         path, snap_name, e.what(), ss.str().empty() ? "" : "\n\n---", ss.str()),
		                         false,
		                         Gtk::MESSAGE_INFO,
		                         BUTTONS_OK);

		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);

		(void) msg.run ();
		msg.hide ();

		goto out;
	}
	catch (...) {
		stringstream ss;
		dump_errors (ss, 6);
		dump_errors (cerr);
		clear_errors ();

		ArdourMessageDialog msg (string_compose(
		                           _("Session \"%1 (snapshot %2)\" did not load successfully.%3%4"),
		                           path, snap_name, ss.str().empty() ? "" : "\n\n---", ss.str()),
		                         true,
		                         Gtk::MESSAGE_INFO,
		                         BUTTONS_OK);

		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);

		(void) msg.run ();
		msg.hide ();

		goto out;
	}

	{
		list<string> const u = new_session->missing_filesources (DataType::MIDI);
		if (!u.empty()) {
			MissingFileSourceDialog d (_session, u, DataType::MIDI);
			d.run ();
		}
	}
	{
		list<string> const u = new_session->unknown_processors ();
		if (!u.empty()) {
			MissingPluginDialog d (_session, u);
			d.run ();
		}
	}

	if (!new_session->writable()) {
		ArdourMessageDialog msg (_("This session has been opened in read-only mode.\n\nYou will not be able to record or save."),
		                         true,
		                         Gtk::MESSAGE_INFO,
		                         BUTTONS_OK);

		msg.set_title (_("Read-only Session"));
		msg.set_position (Gtk::WIN_POS_CENTER);
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

	if (!mix_template.empty ()) {
		/* if mix_template is given, assume this is a new session */
		string metascript = Glib::build_filename (mix_template, "template.lua");
		meta_session_setup (metascript);
	}


  out:
	/* For successful session load the splash is hidden by ARDOUR_UI::first_idle,
	 * which is queued by set_session().
	 * If session-loading fails we hide it explicitly.
	 * This covers both cases in a central place.
	 */
	if (retval) {
		hide_splash ();
	}
	return retval;
}

int
ARDOUR_UI::build_session (const std::string& path, const std::string& snap_name, const std::string& session_template, BusProfile const& bus_profile, bool from_startup_fsm)
{
	int x;

	x = unload_session ();

	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 0;
	}

	_session_is_new = true;

	/* when running from startup FSM all is fine,
	 * engine should be running and the FSM will also have
	 * asked for the SR (even if try-autostart-engine is set)
	 */
	if (from_startup_fsm && AudioEngine::instance()->running ()) {
		return build_session_stage_two (path, snap_name, session_template, bus_profile);
	}
	/* Sample-rate cannot be changed when JACK is running */
	if (!ARDOUR::AudioEngine::instance()->setup_required () && AudioEngine::instance()->running ()) {
		return build_session_stage_two (path, snap_name, session_template, bus_profile);
	}

	/* Work-around missing "OK" button:
	 * When the engine is running. The way to proceed w/o engine re-start
	 * is to simply close the dialog. This is not obvious.
	 *
	 * Ideally an engine restart should be avoided since it can invalidate
	 * latency-calibration.
	 */
	ARDOUR::AudioEngine::instance()->stop();

	/* Ask for the Sample-rate to use with the new session */
	audio_midi_setup->set_position (WIN_POS_CENTER);
	audio_midi_setup->set_modal ();
	audio_midi_setup->present ();
	_engine_dialog_connection = audio_midi_setup->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::audio_midi_setup_for_new_session_done), path, snap_name, session_template, bus_profile));

	/* not done yet, but we're avoiding modal dialogs */
	return 0;
}


void
ARDOUR_UI::audio_midi_setup_for_new_session_done (int response, std::string path, std::string snap_name, std::string template_name, BusProfile const& bus_profile)
{
	_engine_dialog_connection.disconnect ();

	switch (response) {
		case Gtk::RESPONSE_DELETE_EVENT:
			audio_midi_setup->set_modal (false);
			break;
		default:
			break;
	}

	if (!AudioEngine::instance()->running()) {
		return; // keep dialog visible, maybe try again
	}
	audio_midi_setup->set_modal (false);
	audio_midi_setup->hide();

	build_session_stage_two (path, snap_name, template_name, bus_profile);
}

int
ARDOUR_UI::build_session_stage_two (std::string const& path, std::string const& snap_name, std::string const& session_template, BusProfile const& bus_profile)
{
	Session* new_session;

	bool meta_session = !session_template.empty() && session_template.substr (0, 11) == "urn:ardour:";

	try {
		new_session = new Session (*AudioEngine::instance(), path, snap_name, bus_profile.master_out_channels > 0 ? &bus_profile : NULL, meta_session ? "" : session_template);
	}
	catch (SessionException const& e) {
		stringstream ss;
		dump_errors (ss, 6);
		cerr << "Here are the errors associated with this failed session:\n";
		dump_errors (cerr);
		cerr << "---------\n";
		clear_errors ();
		ArdourMessageDialog msg (string_compose(_("Could not create session in \"%1\": %2%3%4"), path, e.what(), ss.str().empty() ? "" : "\n\n---", ss.str()));
		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
		msg.run ();
		return -1;
	}
	catch (...) {
		stringstream ss;
		dump_errors (ss, 6);
		cerr << "Here are the errors associated with this failed session:\n";
		dump_errors (cerr);
		cerr << "---------\n";
		clear_errors ();
		ArdourMessageDialog msg (string_compose(_("Could not create session in \"%1\"%2%3"), path, ss.str().empty() ? "" : "\n\n---", ss.str()));
		msg.set_title (_("Loading Error"));
		msg.set_position (Gtk::WIN_POS_CENTER);
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
		n->set_property (X_("playhead"), X_("0"));
		n->set_property (X_("left-frame"), X_("0"));
	}

	set_session (new_session);

	new_session->save_state(new_session->name());

	if (meta_session) {
		meta_session_setup (session_template.substr (11));
	}

	return 0;
}

/** Ask the user for the name of a new snapshot and then take it.
 */

void
ARDOUR_UI::snapshot_session (bool switch_to_it)
{
	if (switch_to_it && _session->dirty()) {
		vector<string> actions;
		actions.push_back (_("Abort saving snapshot"));
		actions.push_back (_("Don't save now, just snapshot"));
		actions.push_back (_("Save it first"));
		switch (ask_about_saving_session(actions)) {
			case -1:
				return;
				break;
			case 1:
				if (save_state_canfail ("")) {
					ArdourMessageDialog msg (_main_window,
							string_compose (_("\
%1 was unable to save your session.\n\n\
If you still wish to proceed, please use the\n\n\
\"Don't save now\" option."), PROGRAM_NAME));
					msg.run ();
					return;
				}
				/* fallthrough */
			case 0:
				_session->remove_pending_capture_state ();
				break;
		}
	}

	Prompter prompter (true);
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

	Prompter prompter (true);
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
				ArdourMessageDialog msg (string_compose (_("To ensure compatibility with various systems\n"
				                                     "session names may not contain a '%1' character"), illegal));
				msg.run ();
				goto again;
			}

			switch (_session->rename (name)) {
			case -1: {
				ArdourMessageDialog msg (_("That name is already in use by another directory/folder. Please try again."));
				msg.run ();
				goto again;
				break;
			}
			case 0:
				break;
			default: {
				ArdourMessageDialog msg (_("Renaming this session failed.\nThings could be seriously messed up at this point"));
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

	if (_session->dirty()) {
		vector<string> actions;
		actions.push_back (_("Abort save-as"));
		actions.push_back (_("Don't save now, just save-as"));
		actions.push_back (_("Save it first"));
		switch (ask_about_saving_session(actions)) {
			case -1:
				return;
				break;
			case 1:
				if (save_state_canfail ("")) {
					ArdourMessageDialog msg (_main_window,
							string_compose (_("\
%1 was unable to save your session.\n\n\
If you still wish to proceed, please use the\n\n\
\"Don't save now\" option."), PROGRAM_NAME));
					msg.run ();
					return;
				}
				/* fallthrough */
			case 0:
				_session->remove_pending_capture_state ();
				break;
		}
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
		ArdourMessageDialog msg (string_compose (_("Save As failed: %1"), sa.failure_message));
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

	if (_session->archive_session (sad.target_folder(), sad.name(), sad.encode_option (), sad.compression_level (), sad.only_used_sources (), &sad)) {
		ArdourMessageDialog msg (_("Session Archiving failed."));
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
		if (switch_to_it && _session->dirty ()) {
			save_state_canfail ("");
		}

		save_state (timebuf, switch_to_it);
}


bool
ARDOUR_UI::process_snapshot_session_prompter (Prompter& prompter, bool switch_to_it)
{
	string snapname;

	prompter.get_result (snapname);

	bool do_save = (snapname.length() != 0);

	if (do_save) {
		char illegal = Session::session_name_is_legal(snapname);
		if (illegal) {
			ArdourMessageDialog msg (string_compose (_("To ensure compatibility with various systems\n"
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
	catch (Glib::Error const& e) {
		std::cerr << "open_session_selector.add_shortcut_folder() threw Glib::Error " << e.what() << std::endl;
	}

	FileFilter session_filter;
	session_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::statefile_suffix));
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	open_session_selector.add_filter (session_filter);

	FileFilter archive_filter;
	archive_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::session_archive_suffix));
	archive_filter.set_name (_("Session Archives"));

	open_session_selector.add_filter (archive_filter);

	open_session_selector.set_filter (session_filter);

	int response = open_session_selector.run();
	open_session_selector.hide ();

	switch (response) {
		case Gtk::RESPONSE_ACCEPT:
			break;
		default:
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
			ArdourMessageDialog msg (_main_window,
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
				exit (EXIT_FAILURE);
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
