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

// class SessionDialog : public WavesDialog {
  public:

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
	void set_selected_session_full_path (std::string path) { _selected_session_full_name = path; }

  private:
// data types:
    enum SessionSelectionType {
        Nothing,
        RecentSession,
        SavedSession,
        NewSession
    } _selection_type;

	struct RecentSessionsSorter {
	    bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
		    return ARDOUR::cmp_nocase(a.first, b.first) == -1;
	    }
	};

// attributes & control data
	bool _new_only;
    std::string _provided_session_name;
    std::string _provided_session_path;
	std::string _recent_session_full_name[MAX_RECENT_SESSION_COUNTS];
	std::string _selected_session_full_name;
	bool _existing_session_chooser_used; ///< set to true when the existing session chooser has been used
	Gtk::Label _info_scroller_label;
    std::string::size_type _info_scroller_count;
	sigc::connection _info_scroller_connection;

// methods
	virtual void init();
	void on_quit (WavesButton*);
	void on_open_selected (WavesButton*);
	void on_open_saved_session (WavesButton*);
    void on_new_session (WavesButton*);
    void on_recent_session (WavesButton*);
    void on_recent_session_double_click (WavesButton*);
	void on_system_configuration (WavesButton*);
	bool on_delete_event (GdkEventAny*);

	bool on_key_press_event (GdkEventKey*);

    void on_system_configuration_change();
	void redisplay_system_configuration();
	int redisplay_recent_sessions ();
    void session_selected ();
    bool info_scroller_update();

// connections
    PBD::ScopedConnectionList _system_config_update;
// };
