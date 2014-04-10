/*
    Copyright (C) 2014 Valeriy Kamyshniy

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
#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/template_utils.h"
#include "ardour/filename_extensions.h"

#include "ardour_ui.h"
#include "session_dialog.h"
#include "opts.h"
//VKPRefs:#include "engine_dialog.h"
#include "i18n.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();

static string poor_mans_glob (string path)
{
	string copy = path;
	replace_all (copy, "~", Glib::get_home_dir());
	return copy;
}

SessionDialog::SessionDialog (bool require_new, const std::string& session_name, const std::string& session_path, const std::string& template_name, bool cancel_not_quit)
	: WavesDialog (_("session_dialog.xml"), true, false)
	, quit_button (get_waves_button ("quit_button"))
	, new_session_button (get_waves_button ("new_session_button"))
	, open_selected_button (get_waves_button ("open_selected_button"))
	, open_saved_session_button (get_waves_button ("open_saved_session_button"))
	, session_details_label(get_label("session_details_label"))
	, new_only (require_new)
	, _provided_session_name (session_name)
	, _provided_session_path (session_path)
	, _existing_session_chooser_used (false)
	, selected_recent_session(-1)
{
	recent_session_button[0] = &get_waves_button ("recent_session_button_0");
	recent_session_button[1] = &get_waves_button ("recent_session_button_1");
	recent_session_button[2] = &get_waves_button ("recent_session_button_2");
	recent_session_button[3] = &get_waves_button ("recent_session_button_3");
	recent_session_button[4] = &get_waves_button ("recent_session_button_4");
	recent_session_button[5] = &get_waves_button ("recent_session_button_5");
	recent_session_button[6] = &get_waves_button ("recent_session_button_6");
	recent_session_button[7] = &get_waves_button ("recent_session_button_7");
	recent_session_button[8] = &get_waves_button ("recent_session_button_8");
	recent_session_button[9] = &get_waves_button ("recent_session_button_9");

	set_keep_above (true);
	set_position (WIN_POS_CENTER);

	open_selected_button.set_sensitive (false);

	if (!session_name.empty() && !require_new) {
		response (RESPONSE_OK);
		return;
	}

	open_selected_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_open_selected));
	open_saved_session_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_open_saved_session));
	quit_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_quit));
	new_session_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_new_session));
	for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
		recent_session_button[i]->signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_session ));
	}
	redisplay_recent_sessions();
	show_all ();
}

SessionDialog::~SessionDialog()
{
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
	if (!_provided_session_name.empty() && !new_only) {
		should_be_new = false;
        dbg_msg(std::string("return _provided_session_name = \n")+selected_session_full_name);
		return _provided_session_name;
	}

	/* Try recent session selection */

	if (!selected_session_full_name.empty()) {
		should_be_new = false;
        dbg_msg(std::string("return selected_session_full_name = \n")+selected_session_full_name);
		return selected_session_full_name;
	}

	/*
	if (_existing_session_chooser_used) {
		// existing session chosen from file chooser
		should_be_new = false;
		return existing_session_chooser.get_filename ();
	} else {
		should_be_new = true;
		string val = new_name_entry.get_text ();
		strip_whitespace_edges (val);
		return val;
	}
	*/
	return "";
}

std::string
SessionDialog::session_folder ()
{
	if (!_provided_session_path.empty() && !new_only) {
		return _provided_session_path;
	}

	/* Try recent session selection */
	
	if (!selected_session_full_name.empty()) {
		if (Glib::file_test (selected_session_full_name, Glib::FILE_TEST_IS_REGULAR)) {
            dbg_msg(std::string("Glib::path_get_dirname (selected_session_full_name) =\n")+Glib::path_get_dirname (selected_session_full_name));
			return Glib::path_get_dirname (selected_session_full_name);
		}
        dbg_msg(std::string("selected_session_full_name =\t")+selected_session_full_name);
		return selected_session_full_name;
	}

	/*
	if (_existing_session_chooser_used) {
		// existing session chosen from file chooser
		return Glib::path_get_dirname (existing_session_chooser.get_current_folder ());
	} else {
		std::string legal_session_folder_name = legalize_for_path (new_name_entry.get_text());
		return Glib::build_filename (new_folder_chooser.get_current_folder(), legal_session_folder_name);
	}
	*/
}

void
SessionDialog::session_selected ()
{
}

void
SessionDialog::on_new_session (WavesButton*)
{
}

int
SessionDialog::redisplay_recent_sessions ()
{
	for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
		recent_session_button[i]->set_sensitive(false);
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

		vector<string*>* states;
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

		if ((states = Session::possible_states (dirname)) == 0) {
			/* no state file? */
			continue;
		}

		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		if (state_file_names.empty()) {
			continue;
		}

		recent_session_full_name[session_snapshot_count] = Glib::build_filename (dirname, state_file_names.front() + statefile_suffix);
		recent_session_button[session_snapshot_count]->set_text(Glib::path_get_basename (dirname));
		recent_session_button[session_snapshot_count]->set_sensitive(true);
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
	response (Gtk::RESPONSE_CANCEL);
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
	Gtk::FileChooserDialog dialog(*this, _("Select Saved Session"));
	dialog.add_button("CANCEL", Gtk::RESPONSE_CANCEL);
	dialog.add_button("OK", Gtk::RESPONSE_OK);
	if (dialog.run() == Gtk::RESPONSE_OK) {
		selected_session_full_name = dialog.get_filename();
		hide();
		response (Gtk::RESPONSE_ACCEPT);
	}
}

void
SessionDialog::on_recent_session (WavesButton* clicked_button)
{
	if (clicked_button->get_active()) {
		clicked_button->set_active(false);
		selected_recent_session = -1;
		selected_session_full_name = "";
	}
	else {
		if (selected_recent_session >= 0) {
			recent_session_button[selected_recent_session]->set_active(false);
			selected_session_full_name = "";
		}
		clicked_button->set_active(true);
		for (size_t i = 0; i < MAX_RECENT_SESSION_COUNTS; i++) {
			if (recent_session_button[i] == clicked_button) {
				selected_recent_session = i;
				selected_session_full_name = recent_session_full_name[selected_recent_session];
			}
		}
    }

	if (selected_recent_session >= 0) {
		open_selected_button.set_sensitive (true);
		float sr;
		SampleFormat sf;
        dbg_msg(std::string("selected_session_full_name =\n") + selected_session_full_name);
		std::string state_file_path (selected_session_full_name + Glib::path_get_basename(selected_session_full_name) + ARDOUR::statefile_suffix);
        if (Session::get_info_from_path (state_file_path, sr, sf) == 0) {
			std::string sample_format(sf == FormatFloat ? _("32 bit float") : 
														  (sf == FormatInt24 ? _("24 bit") :
																			   (sf == FormatInt16 ? _("16 bit") :
																									 "??")));
			session_details_label.set_text(string_compose (_("<TBI>\n<TBI>\n<TBI>\n%1\n%2"), sr, sample_format));
		}
	} else {
		open_selected_button.set_sensitive (false);
	}
}