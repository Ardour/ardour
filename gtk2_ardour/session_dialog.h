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

class SessionDialog : public ArdourDialog {
  public:
        SessionDialog (bool require_new, const std::string& session_name, const std::string& session_path, const std::string& template_name);
	~SessionDialog ();

	std::string session_name (bool& should_be_new);
	std::string session_folder ();

	bool use_session_template();
	std::string session_template_name();

	// advanced session options

	bool create_master_bus() const;
	int master_channel_count() const;

	bool connect_inputs() const;
	bool limit_inputs_used_for_connection() const;
	int input_limit_count() const;

	bool connect_outputs() const;
	bool limit_outputs_used_for_connection() const;
	int output_limit_count() const;

	bool connect_outs_to_master() const;
	bool connect_outs_to_physical() const;

  private:
	bool new_only;
        std::string _provided_session_name;
        std::string _provided_session_path;

	bool on_delete_event (GdkEventAny*);

        Gtk::Button* cancel_button;
        Gtk::Button* open_button;
        Gtk::Button* back_button;
        Gtk::Button* quit_button;

        bool back_button_pressed (GdkEventButton*);

        Gtk::Frame info_frame;

	/* initial choice page */

	void setup_initial_choice_box ();
	Gtk::VBox ic_vbox;
	Gtk::Button ic_new_session_button;
        void new_session_button_clicked ();

	/* recent sessions */

	void setup_existing_session_page ();

	struct RecentSessionsSorter {
	    bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
		    return cmp_nocase(a.first, b.first) == -1;
	    }
	};

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RecentSessionModelColumns() {
		    add (visible_name);
		    add (tip);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<std::string> visible_name;
	    Gtk::TreeModelColumn<std::string> tip;
	    Gtk::TreeModelColumn<std::string> fullpath;
	};

	RecentSessionModelColumns    recent_session_columns;
	Gtk::TreeView                recent_session_display;
	Glib::RefPtr<Gtk::TreeStore> recent_session_model;
	Gtk::ScrolledWindow          recent_scroller;
        Gtk::Label                   recent_label;
	Gtk::FileChooserButton       existing_session_chooser;
	int redisplay_recent_sessions ();
	void recent_session_row_selected ();
	void recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col);

	void existing_session_selected ();
        void session_selected ();

	/* new sessions */

	void setup_new_session_page ();
	Gtk::Entry new_name_entry;
	Gtk::FileChooserButton new_folder_chooser;

	struct SessionTemplateColumns : public Gtk::TreeModel::ColumnRecord {
		SessionTemplateColumns () {
			add (name);
			add (path);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	SessionTemplateColumns session_template_columns;
	Glib::RefPtr<Gtk::ListStore>  template_model;
	Gtk::ComboBox template_chooser;

	Gtk::VBox session_new_vbox;
	Gtk::VBox session_existing_vbox;
	Gtk::Expander more_new_session_options_button;
	Gtk::CheckButton use_template_button;
        std::string load_template_override;

	void more_new_session_options_button_clicked();
	void new_name_changed ();
	void new_name_activated ();
	void populate_session_templates ();

	/* more options for new sessions */

	Gtk::VBox more_options_vbox;

	Gtk::Label chan_count_label_1;
	Gtk::Label chan_count_label_3;
	Gtk::Label chan_count_label_4;
	Gtk::Table advanced_table;
	Gtk::HBox input_port_limit_hbox;
	Gtk::VBox input_port_vbox;
	Gtk::Table input_table;
	Gtk::HBox input_hbox;

	Gtk::Label bus_label;
	Gtk::Frame bus_frame;
	Gtk::Table bus_table;
	Gtk::HBox bus_hbox;

	Gtk::Label input_label;
	Gtk::Frame input_frame;
	Gtk::HBox output_port_limit_hbox;
	Gtk::VBox output_port_vbox;
	Gtk::VBox output_conn_vbox;
	Gtk::VBox output_vbox;
	Gtk::HBox output_hbox;

	Gtk::Label output_label;
	Gtk::Frame output_frame;
	Gtk::VBox advanced_vbox;
	Gtk::Label advanced_label;

	Gtk::CheckButton _create_master_bus;
	Gtk::SpinButton _master_bus_channel_count;

	Gtk::CheckButton _connect_inputs;
	Gtk::CheckButton _limit_input_ports;
	Gtk::SpinButton _input_limit_count;

	Gtk::CheckButton _connect_outputs;
	Gtk::CheckButton _limit_output_ports;
	Gtk::SpinButton _output_limit_count;

	Gtk::RadioButtonGroup connect_outputs_group;
	Gtk::RadioButton _connect_outputs_to_master;
	Gtk::RadioButton _connect_outputs_to_physical;

	Gtk::Adjustment _output_limit_count_adj;
	Gtk::Adjustment _input_limit_count_adj;
	Gtk::Adjustment _master_bus_channel_count_adj;

	void connect_inputs_clicked ();
	void connect_outputs_clicked ();
	void limit_inputs_clicked ();
	void limit_outputs_clicked ();
	void master_bus_button_clicked ();
	void setup_more_options_box ();

	/* always there */

	Glib::RefPtr<Pango::Layout> layout;

	bool _existing_session_chooser_used; ///< set to true when the existing session chooser has been used

	Gtk::Label info_scroller_label;
        std::string::size_type info_scroller_count;
        bool info_scroller_update();
	sigc::connection info_scroller_connection;
        void updates_button_clicked ();
};

#endif /* __gtk2_ardour_session_dialog_h__ */
