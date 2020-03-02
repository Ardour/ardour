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

#ifndef __gtk2_ardour_startup_fsm_h__
#define __gtk2_ardour_startup_fsm_h__

#include <string>

#include <sigc++/trackable.h>

#include "ardour/types.h"

class ArdourDialog;
class NewUserWizard;
class EngineControl;
class SessionDialog;
class PluginScanDialog;

class StartupFSM : public sigc::trackable
{
  public:
	enum DialogID {
		PreReleaseDialog,
		NewUserDialog,
		NewSessionDialog,
		AudioMIDISetup,
		PluginDialog
	};

	enum Result {
		LoadSession,
		ExitProgram,
	};

	enum MainState {
		WaitingForPreRelease,
		WaitingForNewUser,
		WaitingForSessionPath,
		WaitingForEngineParams,
		WaitingForPlugins
	};

	StartupFSM (EngineControl&);
	~StartupFSM ();

	void start ();
	void reset ();

	std::string session_path;
	std::string session_name;
	std::string session_template;
	int         session_existing_sample_rate;
	bool        session_is_new;
	ARDOUR::BusProfile bus_profile;

	/* It's not a dialog but we provide this to make it behave like a (non-modal)
	 * dialog
	 */

	sigc::signal1<void,Result>& signal_response() { return _signal_response; }

	bool brand_new_user() const { return new_user; }

	void handle_path (std::string const & path);

  private:
	bool new_user;
	bool new_session_required;

	MainState _state;

	void set_state (MainState);
	void dialog_response_handler (int response, DialogID);
	template<typename T> void end_dialog (T**);
	template<typename T> void end_dialog (T&);

	void show_new_user_dialog ();
	void show_session_dialog (bool new_session_required);
	void show_audiomidi_dialog ();
	void show_pre_release_dialog ();
	void show_plugin_scan_dialog ();

	void copy_demo_sessions ();
	void load_from_application_api (std::string const &);
	bool get_session_parameters_from_command_line (bool new_session_required);
	bool get_session_parameters_from_path (std::string const & path, std::string const & template_name, bool new_session_required);
	void queue_finish ();
	bool ask_about_loading_existing_session (const std::string& session_path);
	int  check_session_parameters (bool must_be_new);
	void start_audio_midi_setup ();
	void engine_running ();
	void handle_waiting_for_session_path ();

	/* the Audio/MIDI dialog needs to be persistent and is thus owned by
	 * ARDOUR_UI and we use it by reference. All other dialogs can be
	 * created and destroyed within the scope of startup.
	 */

	EngineControl& audiomidi_dialog;
	NewUserWizard* new_user_dialog;
	SessionDialog* session_dialog;
	ArdourDialog* pre_release_dialog;
	PluginScanDialog* plugin_scan_dialog;

	sigc::connection current_dialog_connection;

	sigc::signal1<void,Result> _signal_response;

	void dialog_hidden (Gtk::Window*);
};

#endif /* __gtk2_ardour_startup_fsm_h__ */
