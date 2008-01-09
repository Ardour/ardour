/*
    Copyright (C) 2005 Paul Davis 

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

// -*- c++ -*-

#ifndef NEW_SESSION_DIALOG_H
#define NEW_SESSION_DIALOG_H

#include <string>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treepath.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/notebook.h>
#include <gtkmm/table.h>
#include <gtkmm/alignment.h>
#include <gtkmm/frame.h>
#include <gtkmm/expander.h>

#include <ardour/utils.h>

#include <glibmm/refptr.h>

#include "ardour_dialog.h"
#include "engine_dialog.h"

namespace Gtk {
	class Entry;
	class FileChooserButton;
	class SpinButton;
	class CheckButton;
	class RadioButton;
	class TreeView;
	class Notebook;
}

class NewSessionDialog : public ArdourDialog
{
public:
		
	enum Pages {
		NewPage = 0x1,
		OpenPage = 0x2,
		EnginePage = 0x4
	};

	NewSessionDialog();
	~NewSessionDialog ();

	int run ();

	void set_session_name(const Glib::ustring& name);
	void set_session_folder(const Glib::ustring& folder);

	std::string session_name() const;
	std::string session_folder() const;
	
	bool use_session_template() const;
	std::string session_template_name() const;

	// advanced.

	bool create_master_bus() const;
	int master_channel_count() const;

	bool create_control_bus() const;
	int control_channel_count() const;

	bool connect_inputs() const;
	bool limit_inputs_used_for_connection() const;
	int input_limit_count() const;

	bool connect_outputs() const;
	bool limit_outputs_used_for_connection() const;
	int output_limit_count() const;

	bool connect_outs_to_master() const;
	bool connect_outs_to_physical() const ;
	Pages which_page ();

	int get_current_page();
	void set_current_page (int);
	void reset_recent();

	// reset everything to default values.
	void reset();

	EngineControl engine_control;
	void set_have_engine (bool yn);
	void set_existing_session (bool yn);

protected:

	void reset_name();
	void reset_template();
	
	Gtk::Label * session_name_label;
	Gtk::Label * session_location_label;
	Gtk::Label * session_template_label;
	Gtk::Label * chan_count_label_1;
	Gtk::Label * chan_count_label_2;
	Gtk::Label * chan_count_label_3;
	Gtk::Label * chan_count_label_4;
	Gtk::Table * advanced_table;
	Gtk::HBox * input_port_limit_hbox;
	Gtk::VBox * input_port_vbox;
	Gtk::Table * input_table;
	Gtk::HBox * input_hbox;

	Gtk::Label * bus_label;
	Gtk::Frame * bus_frame;
	Gtk::Table * bus_table;
	Gtk::HBox * bus_hbox;

	Gtk::Label * input_label;
	Gtk::Frame * input_frame;
	Gtk::HBox * output_port_limit_hbox;
	Gtk::VBox * output_port_vbox;
	Gtk::VBox * output_conn_vbox;
	Gtk::VBox * output_vbox;
	Gtk::HBox * output_hbox;

	Gtk::Label * output_label;
	Gtk::Frame * output_frame;
	Gtk::VBox * advanced_vbox;
	Gtk::Label * advanced_label;
	Gtk::Expander * advanced_expander;
	Gtk::Table * new_session_table;
	Gtk::HBox * open_session_hbox;
	Gtk::ScrolledWindow * recent_scrolledwindow;

	Gtk::Label * recent_sesion_label;
	Gtk::Frame * recent_frame;
	Gtk::VBox * open_session_vbox;
	Gtk::Entry*  m_name;
	Gtk::FileChooserButton* m_folder;
	Gtk::FileChooserButton* m_template;
	Gtk::Label * open_session_file_label;

	Gtk::CheckButton* m_create_master_bus;
	Gtk::SpinButton* m_master_bus_channel_count;
       	
	Gtk::CheckButton* m_create_control_bus;
	Gtk::SpinButton* m_control_bus_channel_count;

	Gtk::CheckButton* m_connect_inputs;
	Gtk::CheckButton* m_limit_input_ports;
	Gtk::SpinButton* m_input_limit_count;

	Gtk::CheckButton* m_connect_outputs;	
	Gtk::CheckButton* m_limit_output_ports;
	Gtk::SpinButton* m_output_limit_count;

	Gtk::RadioButton* m_connect_outputs_to_master;
	Gtk::RadioButton* m_connect_outputs_to_physical;
	Gtk::Button* m_okbutton;

	Gtk::FileChooserButton* m_open_filechooser;
	Gtk::TreeView* m_treeview;
	Gtk::Notebook* m_notebook;

 private:

	Pages page_set;

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RecentSessionModelColumns() { 
		    add (visible_name);
		    add (fullpath);
	    }
	  Gtk::TreeModelColumn<std::string> visible_name;
	  Gtk::TreeModelColumn<std::string> fullpath;
	};

	RecentSessionModelColumns    recent_columns;
	Glib::RefPtr<Gtk::TreeStore> recent_model;
	bool in_destructor;

	void recent_session_selection_changed ();
	void nsd_redisplay_recent_sessions();
	void nsd_recent_session_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col);
	struct RecentSessionsSorter {
	  bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
		    return cmp_nocase(a.first, b.first) == -1;
	    }
	};
	void on_new_session_name_entry_changed();
	void notebook_page_changed (GtkNotebookPage*, uint);
	void treeview_selection_changed ();
	void file_chosen ();
	void template_chosen ();
	void recent_row_activated (const Gtk::TreePath&, Gtk::TreeViewColumn*);
	void connect_inputs_clicked ();
	void connect_outputs_clicked ();
	void limit_inputs_clicked ();
	void limit_outputs_clicked ();
	void master_bus_button_clicked ();
	void monitor_bus_button_clicked ();

	bool on_new_session_page;
	bool have_engine;
};

#endif // NEW_SESSION_DIALOG_H
