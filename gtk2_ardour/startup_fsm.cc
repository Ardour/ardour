/*
 * Copyright (C) 2019 Paul Davis <paul@linuxaudiosystems.com>
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

#include <vector>

#include <gtkmm/dialog.h>
#include <gtkmm/liststore.h>
#include <gtkmm/messagedialog.h>

#include "pbd/basename.h"
#include "pbd/file_archive.h"
#include "pbd/file_utils.h"
#include "pbd/i18n.h"

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"
#include "ardour/recent_sessions.h"
#include "ardour/rc_configuration.h"
#include "ardour/search_paths.h"
#include "ardour/session.h"
#include "ardour/session_utils.h"
#include "ardour/template_utils.h"

#include "gtkmm2ext/application.h"
#include <gtkmm2ext/doi.h>

#include "engine_dialog.h"
#include "new_user_wizard.h"
#include "opts.h"
#include "session_dialog.h"
#include "startup_fsm.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

using std::string;
using std::vector;

StartupFSM::StartupFSM (EngineControl& amd)
	: session_existing_sample_rate (0)
	, session_is_new (false)
	, new_user (NewUserWizard::required())
	, new_session (true)
	, _state (new_user ? NeedWizard : NeedSessionPath)
	, new_user_wizard (0)
	, audiomidi_dialog (amd)
	, session_dialog (0)
{
	Application* app = Application::instance ();

	app->ShouldQuit.connect (sigc::mem_fun (*this, &StartupFSM::queue_finish));
	app->ShouldLoad.connect (sigc::mem_fun (*this, &StartupFSM::load_from_application_api));

	/* this may cause the delivery of ShouldLoad etc if we were invoked in
	 * particular ways. It will happen when the event loop runs again.
	 */

	app->ready ();
}

StartupFSM::~StartupFSM ()
{
	delete session_dialog;
}

void
StartupFSM::queue_finish ()
{
	_signal_response (ExitProgram);
}

void
StartupFSM::start ()
{
	if (new_user) {
		/* show new user wizard */
		_state = NeedSessionPath;
		show_new_user_wizard ();
	} else {
		/* pretend we just showed the new user wizard and we're done
		   with it
		*/
		dialog_response_handler (RESPONSE_OK, NewUserDialog);
	}
}


void
StartupFSM::end()
{

}

void
StartupFSM::dialog_response_handler (int response, StartupFSM::DialogID dialog_id)
{
	const bool new_session_required = (ARDOUR_COMMAND_LINE::new_session || (!ARDOUR::Profile->get_mixbus() && new_user));

	switch (_state) {
	case NeedSessionPath:
		switch (dialog_id) {
		case NewUserDialog:

			current_dialog_connection.disconnect ();
			delete_when_idle (new_user_wizard);

			switch (response) {
			case RESPONSE_OK:
				break;
			default:
				exit (1);
			}

			/* new user wizard done, now lets get session params
			 * either from the command line (if given) or a dialog
			 * (if nothing given on the command line or if the
			 * command line arguments don't work for some reason
			 */

			if (ARDOUR_COMMAND_LINE::session_name.empty()) {

				/* nothing given on the command line ... show new session dialog */

				session_path = string();
				session_name = string();
				session_template = string();

				_state = NeedSessionPath;
				session_dialog = new SessionDialog (new_session_required, string(), string(), string(), false);
				show_session_dialog ();

			} else {

				if (get_session_parameters_from_command_line (new_session_required)) {

					/* command line arguments all OK. Get engine parameters */

					_state = NeedEngineParams;

					if (!new_session_required && session_existing_sample_rate > 0) {
						audiomidi_dialog.set_desired_sample_rate (session_existing_sample_rate);
					}

					show_audiomidi_dialog ();

				} else {

					/* command line arguments not good. Use
					 * dialog, but prime the dialog with
					 * the information we set up in
					 * get_session_parameters_from_command_line()
					 */

					_state = NeedSessionPath;
					session_dialog = new SessionDialog (new_session_required, session_name, session_path, session_template, false);
					show_session_dialog ();
				}
			}
			break;

		case NewSessionDialog:
			switch (response) {
			case RESPONSE_OK:
			case RESPONSE_ACCEPT:
				switch (check_session_parameters (new_session_required)) {
				case -1:
					/* Unrecoverable error */
					_signal_response (ExitProgram);
					break;
				case 1:
					/* do nothing - keep dialog up for a
					 * retry. Errors were addressed by
					 * ::check_session_parameters()
					 */
					break;
				case 0:
					_state = NeedEngineParams;
					session_dialog->hide ();
					delete session_dialog;
					session_dialog = 0;
					current_dialog_connection.disconnect();
					if (!session_is_new && session_existing_sample_rate > 0) {
						audiomidi_dialog.set_desired_sample_rate (session_existing_sample_rate);
					}
					show_audiomidi_dialog ();
					break;
				}
				break;

			default:
				_signal_response (ExitProgram);
				break;
			}
			break;

		default:
			/* ERROR */
			break;
		}
		break;

	case NeedEngineParams:
		switch (dialog_id) {
		case AudioMIDISetup:
			switch (response) {
			case RESPONSE_OK:
			case RESPONSE_ACCEPT:
				if (AudioEngine::instance()->running()) {
					audiomidi_dialog.hide ();
					current_dialog_connection.disconnect();
					_signal_response (LoadSession);
				} else {
					/* just keep going */
				}
				break;
			default:
				_signal_response (ExitProgram);
			}
			break;
		default:
			/* ERROR */
			break;
		}

	case NeedWizard:
		/* ERROR */
		break;
	}
}

void
StartupFSM::show_new_user_wizard ()
{
	new_user_wizard = new NewUserWizard;
	current_dialog_connection = new_user_wizard->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), NewUserDialog));
	new_user_wizard->set_position (WIN_POS_CENTER);
	new_user_wizard->present ();
}

void
StartupFSM::show_session_dialog ()
{
	current_dialog_connection = session_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), NewSessionDialog));
	session_dialog->set_position (WIN_POS_CENTER);
	session_dialog->present ();
}

void
StartupFSM::show_audiomidi_dialog ()
{
	current_dialog_connection = audiomidi_dialog.signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), AudioMIDISetup));
	audiomidi_dialog.set_position (WIN_POS_CENTER);
	audiomidi_dialog.present ();
}

bool
StartupFSM::get_session_parameters_from_command_line (bool new_session_required)
{
	return get_session_parameters_from_path (ARDOUR_COMMAND_LINE::session_name, ARDOUR_COMMAND_LINE::load_template, new_session_required);
}

bool
StartupFSM::get_session_parameters_from_path (string const & path, string const & template_name, bool new_session_required)
{
	if (path.empty()) {
		/* use GUI to ask the user */
		return false;
	}

	if (Glib::file_test (path.c_str(), Glib::FILE_TEST_EXISTS)) {

		session_is_new = false;

		if (new_session_required) {
			/* wait! it already exists */

			if (!ask_about_loading_existing_session (path)) {
				return false;
			} else {
				/* load it anyway */
			}
		}

		session_name = basename_nosuffix (path);

		if (Glib::file_test (path.c_str(), Glib::FILE_TEST_IS_REGULAR)) {
			/* session/snapshot file, change path to be dir */
			session_path = Glib::path_get_dirname (path);
		} else {
			session_path = path;
		}

		float sr;
		SampleFormat fmt;
		string program_version;

		const string statefile_path = Glib::build_filename (session_path, session_name + ARDOUR::statefile_suffix);
		if (Session::get_info_from_path (statefile_path, sr, fmt, program_version)) {
			/* exists but we can't read it correctly */
			error << string_compose (_("Cannot get existing session information from %1"), statefile_path) << endmsg;
			return false;
		}

		session_existing_sample_rate = sr;
		return true;

	}

	/*  Everything after this involves a new session
	 *
	 *  ... did the  user give us a path or just a name?
	 */

	if (path.find (G_DIR_SEPARATOR) == string::npos) {
		/* user gave session name with no path info, use
		   default session folder.
		*/
		session_name = ARDOUR_COMMAND_LINE::session_name;
		session_path = Glib::build_filename (Config->get_default_session_parent_dir (), session_name);
	} else {
		session_name = basename_nosuffix (path);
		session_path = path;
	}


	if (!template_name.empty()) {

		/* Allow the user to specify a template via path or name on the
		 * command line
		 */

		bool have_resolved_template_name = false;

		/* compare by name (path may or may not be UTF-8) */

		vector<TemplateInfo> templates;
		find_session_templates (templates, false);
		for (vector<TemplateInfo>::iterator x = templates.begin(); x != templates.end(); ++x) {
			if ((*x).name == template_name) {
				session_template = (*x).path;
				have_resolved_template_name = true;
				break;
			}
		}

		/* look up script by name */
		LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::SessionInit));
		LuaScriptList& as (LuaScripting::instance ().scripts (LuaScriptInfo::EditorAction));
		for (LuaScriptList::const_iterator s = as.begin(); s != as.end(); ++s) {
			if ((*s)->subtype & LuaScriptInfo::SessionSetup) {
				scripts.push_back (*s);
			}
		}
		std::sort (scripts.begin(), scripts.end(), LuaScripting::Sorter());
		for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
			if ((*s)->name == template_name) {
				session_template = "urn:ardour:" + (*s)->path;
				have_resolved_template_name = true;
				break;
			}
		}

		if (!have_resolved_template_name) {
			/* this will produce a more or less meaninful error later:
			 * "ERROR: Could not open session template [abs-path to user-config dir]"
			 */
			session_template = Glib::build_filename (ARDOUR::user_template_directory (), template_name);
		}
	}

	/* We don't know what this is, because the session is new and the
	 * command line doesn't let us specify it. The user will get to decide
	 * in the audio/MIDI dialog.
	 */

	session_existing_sample_rate = 0;
	session_is_new = true;

	/* this is an arbitrary default value but since the user insists on
	 * starting a new session from the command line, it will do as well as
	 * any other possible value. I mean, seriously, what else could it be
	 * by default?
	 */

	bus_profile.master_out_channels = 2;

	return true;
}

/** return values:
 * -1: failure
 *  1: failure but user can retry
 *  0: success, seesion parameters ready for use
 */
int
StartupFSM::check_session_parameters (bool must_be_new)
{
	bool requested_new = false;

	session_name = session_dialog->session_name (requested_new);
	session_path = session_dialog->session_folder ();

	if (must_be_new) {
		assert (requested_new);
	}

	if (!must_be_new) {

		/* See if the specified session is a session archive */

		int rv = ARDOUR::inflate_session (session_name, Config->get_default_session_parent_dir(), session_path, session_name);
		if (rv < 0) {
			MessageDialog msg (*session_dialog, string_compose (_("Extracting session-archive failed: %1"), inflate_error (rv)));
			msg.run ();

			return 1;
		} else if (rv == 0) {
			/* names are good (and session is unarchived/inflated */
			return 0;
		}
	}

	/* check for ".ardour" in statefile name, because we don't want
	 * it
	 *
	 * XXX Note this wierd conflation of a
	 * file-name-without-a-suffix and the session name. It's not
	 * really a session name at all, but rather the suffix-free
	 * name of a statefile (snapshot).
	 */

	const string::size_type suffix_at = session_name.find (statefile_suffix);

	if (suffix_at != string::npos) {
		session_name = session_name.substr (0, suffix_at);
	}

	/* this shouldn't happen, but we catch it just in case it does */

	if (session_name.empty()) {
		return 1; /* keep running dialog */
	}

	if (session_dialog->use_session_template()) {
		session_template = session_dialog->session_template_name();
	}

	if (session_name[0] == G_DIR_SEPARATOR ||
#ifdef PLATFORM_WINDOWS
	    // Windows file system .. detect absolute path
	    // C:/*
	    (session_name.length() > 3 && session_name[1] == ':' && session_name[2] == G_DIR_SEPARATOR)
#else
	    // Sensible file systems
	    // /* or ./* or ../*
	    (session_name.length() > 2 && session_name[0] == '.' && session_name[1] == G_DIR_SEPARATOR) ||
	    (session_name.length() > 3 && session_name[0] == '.' && session_name[1] == '.' && session_name[2] == G_DIR_SEPARATOR)
#endif
		)
	{

		/* user typed absolute path or cwd-relative path
		   specified into session name field. So ... infer
		   session path and name from what was given.
		*/

		session_path = Glib::path_get_dirname (session_name);
		session_name = Glib::path_get_basename (session_name);

	} else {

		/* session name is just a name */
	}

	/* check if name is legal */

	const char illegal = Session::session_name_is_legal (session_name);

	if (illegal) {
		MessageDialog msg (*session_dialog,
		                   string_compose (_("To ensure compatibility with various systems\n"
		                                     "session names may not contain a '%1' character"),
		                                   illegal));
		msg.run ();
		ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
		return 1; /* keep running dialog */
	}


	/* check if the currently-exists status matches whether or not
	 * it should be new
	 */

	if (Glib::file_test (session_path, Glib::FileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {

		if (requested_new /*&& !nsm*/) {

			std::string existing = Glib::build_filename (session_path, session_name);

			if (!ask_about_loading_existing_session (existing)) {
				session_dialog->clear_name ();
				return 1; /* try again */
			}
		}

		session_is_new = false;

	} else {

		/* does not exist at present */

		if (!requested_new) {
			// pop_back_splash (session_dialog);
			MessageDialog msg (string_compose (_("There is no existing session at \"%1\""), session_path));
			msg.run ();
			session_dialog->clear_name();
			return 1;
		}

		session_is_new = true;
	}

	float sr;
	SampleFormat fmt;
	string program_version;
	const string statefile_path = Glib::build_filename (session_path, session_name + ARDOUR::statefile_suffix);

	if (!session_is_new) {

		if (Session::get_info_from_path (statefile_path, sr, fmt, program_version)) {
			/* exists but we can't read it */
			return -1;
		}

		session_existing_sample_rate = sr;

	} else {

		bus_profile.master_out_channels = session_dialog->master_channel_count ();
	}

	return 0;
}

void
StartupFSM::copy_demo_sessions ()
{
	// TODO: maybe IFF brand_new_user
	if (ARDOUR::Profile->get_mixbus () && Config->get_copy_demo_sessions ()) {
		std::string dspd (Config->get_default_session_parent_dir());
		Searchpath ds (ARDOUR::ardour_data_search_path());
		ds.add_subdirectory_to_paths ("sessions");
		vector<string> demos;
		find_files_matching_pattern (demos, ds, ARDOUR::session_archive_suffix);

		ARDOUR::RecentSessions rs;
		ARDOUR::read_recent_sessions (rs);

		for (vector<string>::iterator i = demos.begin(); i != demos.end (); ++i) {
			/* "demo-session" must be inside "demo-session.<session_archive_suffix>" */
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
			} catch (...) {
				/* relax ? */
			}
		}
	}
}

void
StartupFSM::load_from_application_api (const std::string& path)
{
	/* macOS El Capitan (and probably later) now somehow passes the command
	   line arguments to an app via the openFile delegate protocol. Ardour
	   already does its own command line processing, and having both
	   pathways active causes crashes. So, if the command line was already
	   set, do nothing here.
	*/

	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {
		return;
	}

	/* Cancel SessionDialog if it's visible to make macOS delegates work.
	 *
	 * there's a race condition here: we connect to app->ShouldLoad
	 * and then at some point (might) show a session dialog. The race is
	 * caused by the non-deterministic interaction between the macOS event
	 * loop(s) and the GDK one(s).
	 *
	 *  - ShouldLoad does not arrive before we show the session dialog
	 *          -> here we should hide the session dialog, then use the
	 *             supplied path as if it was provided on the command line
	 *  - ShouldLoad signal arrives before we show a session dialog
	 *          -> don't bother showing the session dialog, just use the
	 *             supplied path as if it was provided on the command line
	 *
	 */

	if (session_dialog) {
		session_dialog->hide ();
		delete_when_idle (session_dialog);
		session_dialog = 0;
	}

	/* no command line argument given ... must just be via
	 * desktop/finder/window manager API (e.g. double click on "foo.ardour"
	 * icon)
	 */

	if (get_session_parameters_from_path (path, string(), false)) {
		_signal_response (LoadSession);
		return;
	}

	/* given parameters failed for some reason. This is probably true
	 * anyway, but force it to be true and then carry on with whatever the
	 * main event loop is doing.
	 */

	_state = NeedSessionPath;
}

bool
StartupFSM::ask_about_loading_existing_session (const std::string& session_path)
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
	// pop_back_splash (msg);

	switch (msg.run()) {
	case RESPONSE_YES:
		return true;
		break;
	}
	return false;
}
