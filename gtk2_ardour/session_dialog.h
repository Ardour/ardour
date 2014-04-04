/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk2_ardour_session_dialog_h__
#define __gtk2_ardour_session_dialog_h__

#include <string>
#include "waves_dialog.h"

#include <gdkmm/pixbuf.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/combobox.h>

#include "ardour/utils.h"

#include "ardour_dialog.h"

class EngineControl;
#define MAX_RECENT_SESSION_COUNTS 10
class SessionDialog : public WavesDialog {
  public:
        SessionDialog (bool require_new, const std::string& session_name, const std::string& session_path, 
		       const std::string& template_name, bool cancel_not_quit);
	~SessionDialog ();

        void clear_given ();

	std::string session_name (bool& should_be_new);
	std::string session_folder ();
    
	bool use_session_template() { return false; }
	std::string session_template_name() { return ""; }

	// advanced session options

	bool create_master_bus() const { return true; }
	int master_channel_count() const { return 2; }

	bool connect_inputs() const { return true; }
	bool limit_inputs_used_for_connection() const { return false; }
	int input_limit_count() const { return 0; }

	bool connect_outputs() const { return true; }
	bool limit_outputs_used_for_connection() const { return false; }
	int output_limit_count() const { return 0; }

	bool connect_outs_to_master() const { return true; }
	bool connect_outs_to_physical() const { return false; }

  private:
	WavesButton& quit_button;
	WavesButton& new_session_button;
	WavesButton& open_selected_button;
	WavesButton& open_saved_session_button;
	WavesButton* recent_session_button[MAX_RECENT_SESSION_COUNTS];

  private: //app logic
	void on_quit (WavesButton*);
	void on_open_selected (WavesButton*);
	void on_open_saved_session (WavesButton*);
    void on_new_session (WavesButton*);
    void on_recent_session (WavesButton*);

	int selected_recent_session;
	std::string recent_session_full_name[MAX_RECENT_SESSION_COUNTS];
	std::string selected_session_full_name;

  private:
	bool new_only;
    std::string _provided_session_name;
    std::string _provided_session_path;

	bool on_delete_event (GdkEventAny*);

	struct RecentSessionsSorter {
	    bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
		    return cmp_nocase(a.first, b.first) == -1;
	    }
	};

	int redisplay_recent_sessions ();
    void session_selected ();

	bool _existing_session_chooser_used; ///< set to true when the existing session chooser has been used

	Gtk::Label info_scroller_label;
    std::string::size_type info_scroller_count;
    bool info_scroller_update();
	sigc::connection info_scroller_connection;
};

#endif /* __gtk2_ardour_session_dialog_h__ */
