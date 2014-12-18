/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include <fstream>
#include <algorithm>

#include "waves_button.h"

#include <gtkmm/filechooser.h>

#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"
#include "pbd/whitespace.h"
#include "pbd/stacktrace.h"
#include "pbd/openuri.h"

#include "ardour/audioengine.h"
#include "ardour/engine_state_controller.h"
#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/template_utils.h"
#include "ardour/filename_extensions.h"
#include "ardour/rc_configuration.h"

#include "ardour_ui.h"
#include "session_dialog.h"
#include "opts.h"
#include "i18n.h"
#include "utils.h"
#include "gui_thread.h" 

#include "open_file_dialog_proxy.h"
#include "dbg_msg.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

static string poor_mans_glob (string path)
{
	string copy = path;
	replace_all (copy, "~", Glib::get_home_dir());
	return copy;
}

void SessionDialog::init()
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);

	_open_selected_button.set_sensitive (false);

	if (!_provided_session_name.empty() && !_new_only) {
		response (RESPONSE_OK);
		return;
	}

	_open_selected_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_open_selected));
	_open_saved_session_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_open_saved_session));
	_quit_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_quit));
	_new_session_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_new_session));
	_system_configuration_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_system_configuration));
	
    EngineStateController::instance ()->InputConfigChanged.connect  (_system_config_update, invalidator (*this), boost::bind (&SessionDialog::on_system_configuration_change, this), gui_context());
    EngineStateController::instance ()->OutputConfigChanged.connect (_system_config_update, invalidator (*this), boost::bind (&SessionDialog::on_system_configuration_change, this), gui_context());
    EngineStateController::instance ()->EngineRunning.connect       (_system_config_update, invalidator (*this), boost::bind (&SessionDialog::on_system_configuration_change, this), gui_context());
    EngineStateController::instance ()->PortRegistrationChanged.connect(_system_config_update, invalidator (*this), boost::bind (&SessionDialog::on_system_configuration_change, this), gui_context());
    
    for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
		_recent_session_button[i]->signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_session ));
		_recent_session_button[i]->signal_double_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_session_double_click ));
	}
	redisplay_system_configuration ();
	redisplay_recent_sessions();
}

void
SessionDialog::clear_given ()
{
	_provided_session_path = "";
	_provided_session_name = "";
}

std::string
SessionDialog::session_name (bool& should_be_new)
{
    should_be_new = false;
    
	if (!_provided_session_name.empty() && !_new_only) {
		return _provided_session_name;
	}

	/* Try recent session selection */

	if (!_selected_session_full_name.empty()) {
        should_be_new = (_selection_type == NewSession);
		return should_be_new ? Glib::path_get_basename(_selected_session_full_name) :
                               _selected_session_full_name;
	}
	
	return "";
}

std::string
SessionDialog::session_folder ()
{
	if (!_selected_session_full_name.empty() ) {
		if (Glib::file_test (_selected_session_full_name, Glib::FILE_TEST_IS_REGULAR)) {
			return Glib::path_get_dirname (_selected_session_full_name);
		}
		return _selected_session_full_name;
	}    
    return "";
}

void
SessionDialog::session_selected ()
{
}

void
SessionDialog::on_new_session (WavesButton*)
{
    string temp_session_full_file_name;
    
    set_keep_above(false);
    temp_session_full_file_name = ARDOUR::save_file_dialog(Config->get_default_session_parent_dir(),_("Create New Session"));
    set_keep_above(true);
    
    if (!temp_session_full_file_name.empty()) {
        _selected_session_full_name = temp_session_full_file_name;
        
            for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
                    _recent_session_button[i]->set_active (false);
            }
            
            hide();
            _selection_type = NewSession;
            response (Gtk::RESPONSE_ACCEPT);
    }
}

void
SessionDialog::on_system_configuration_change ()
{
    redisplay_system_configuration ();
}

void
SessionDialog::redisplay_system_configuration ()
{
	ARDOUR::EngineStateController* eng_controller (ARDOUR::EngineStateController::instance() );
    
    std::string operation_mode = _("UNKNOWN");
    if (Config->get_output_auto_connect() & AutoConnectPhysical) {
        operation_mode = _("MULTI OUT");
    } else if (Config->get_output_auto_connect() & AutoConnectMaster) {
        operation_mode = _("STEREO OUT");
    }
    
    string channel_config_info;
    int n_ch_in = eng_controller->get_available_inputs_count();
    int n_ch_out = eng_controller->get_available_outputs_count();
    std::stringstream ss;
    ss << n_ch_in << " In, " << n_ch_out << " Out";
    channel_config_info = ss.str();
    
    _session_details_label.set_text(string_compose (_("%1\n%2\n%3\n%4"),
                                                    eng_controller->get_current_device_name(),
                                                    channel_config_info,
                                                    operation_mode,
													eng_controller->get_current_sample_rate()));
}

int
SessionDialog::redisplay_recent_sessions ()
{
	for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
		_recent_session_button[i]->set_sensitive(false);
	}

	std::vector<std::string> session_directories;
	RecentSessionsSorter cmp;

	ARDOUR::RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		return 0;
	}

	// sort them alphabetically
	// sort (rs.begin(), rs.end(), cmp);

	for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		session_directories.push_back ((*i).second);
	}

	int session_snapshot_count = 0;

	for (vector<std::string>::const_iterator i = session_directories.begin();
		 (session_snapshot_count < MAX_RECENT_SESSION_COUNTS) && (i != session_directories.end());
		 ++i)
	{
		std::vector<std::string> state_file_paths;

		// now get available states for this session

		get_state_files_in_directory (*i, state_file_paths);

		vector<string> states;
		vector<const gchar*> item;
		string dirname = *i;

		/* remove any trailing / */
		if (dirname[dirname.length()-1] == '/') {
			dirname = dirname.substr (0, dirname.length()-1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(dirname.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			continue;
		}

		/* now get available states for this session */

		states = Session::possible_states (dirname);

                if (states.empty()) {
                        /* no state file? */
                        continue;
                }

		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		if (state_file_names.empty()) {
			continue;
		}

		_recent_session_full_name[session_snapshot_count] = Glib::build_filename (dirname, state_file_names.front() + statefile_suffix);
		_recent_session_button[session_snapshot_count]->set_text(Glib::path_get_basename (dirname));
		_recent_session_button[session_snapshot_count]->set_sensitive(true);
		ARDOUR_UI::instance()->set_tip(*_recent_session_button[session_snapshot_count], _recent_session_full_name[session_snapshot_count]);
		++session_snapshot_count;
	}

	return session_snapshot_count;
}

bool
SessionDialog::on_delete_event (GdkEventAny* ev)
{
	response (RESPONSE_CANCEL);
	return WavesDialog::on_delete_event (ev);
}

//app logic
void
SessionDialog::on_quit (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_REJECT);
}

void
SessionDialog::on_open_selected (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_ACCEPT);
}

void
SessionDialog::on_open_saved_session (WavesButton*)
{
	set_keep_above(false);
        string temp_session_full_file_name = ARDOUR::open_file_dialog(Config->get_default_session_parent_dir(), _("Select Saved Session"));
        set_keep_above(true);
        
        if(!temp_session_full_file_name.empty()) {
                _selected_session_full_name = temp_session_full_file_name;
                for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
                        _recent_session_button[i]->set_active (false);
                }
                _selection_type = SavedSession;
                hide ();
                response (Gtk::RESPONSE_ACCEPT);
        } 
        
	return;
}

void
SessionDialog::on_recent_session (WavesButton* clicked_button)
{
	if (clicked_button->get_active()) {
        return;
    }
	else {
        _selected_session_full_name = "";
        _selection_type = Nothing;
		for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
			if (_recent_session_button[i] == clicked_button) {
				_selected_session_full_name = _recent_session_full_name[i];
                _recent_session_button[i]->set_active(true);
			} else {
                _recent_session_button[i]->set_active(false);
                _selection_type = RecentSession;
            }
		}
    }

	_open_selected_button.set_sensitive (_selection_type == RecentSession);
}

void
SessionDialog::on_recent_session_double_click (WavesButton*)
{
	// we suppose the first click, occurred prior to the second in the 
	// double click sequence has been processed correctly and now
	// the job is just to respond with ok

	hide();
	response (Gtk::RESPONSE_ACCEPT);
}


void
SessionDialog::on_system_configuration (WavesButton* clicked_button)
{
	set_keep_above(false);
	_system_configuration_dialog->set_keep_above(true);
	_system_configuration_dialog->run();
	redisplay_system_configuration ();
	set_keep_above(true);
}

bool
SessionDialog::on_key_press_event (GdkEventKey* ev)
{
    switch (ev->keyval)
    {
        case GDK_Return: // button Enter was pressed
        case GDK_KP_Enter:
            if ( _open_selected_button.get_sensitive () ) // if recent session was choosen
            {
                response (Gtk::RESPONSE_ACCEPT); // load choosen recent session
                return true;
            } else
            {
                return true;
            }
        case GDK_Escape:
            return true;
    }
    
	return WavesDialog::on_key_press_event (ev);
}

