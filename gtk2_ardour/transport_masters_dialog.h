/*
    Copyright (C) 2018 Paul Davis

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

#ifndef __ardour_gtk_transport_masters_dialog_h__
#define __ardour_gtk_transport_masters_dialog_h__

#include <vector>
#include <string>

#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/treestore.h>

#include "ardour_window.h"

namespace Gtk {
	class Menu;
}

namespace ARDOUR {
	class TransportMaster;
}

class FloatingTextEntry;

class TransportMastersWidget : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
  public:
	TransportMastersWidget ();
	~TransportMastersWidget ();

	void update (ARDOUR::samplepos_t);
	void set_transport_master (boost::shared_ptr<ARDOUR::TransportMaster>);

  protected:
	void on_map ();
	void on_unmap ();

  private:

	struct AddTransportMasterDialog : public ArdourDialog {
	  public:
		AddTransportMasterDialog ();
		std::string get_name () const;
		ARDOUR::SyncSource get_type () const;

	  private:
		Gtk::Label name_label;
		Gtk::Label type_label;
		Gtk::HBox name_hbox;
		Gtk::HBox type_hbox;
		Gtk::Entry name_entry;
		Gtk::ComboBoxText type_combo;
	};

	struct Row : sigc::trackable, PBD::ScopedConnectionList {
		TransportMastersWidget& parent;
		Gtk::EventBox label_box;
		Gtk::Label label;
		Gtk::Label type;
		Gtk::Label format;
		Gtk::Label current;
		Gtk::Label last;
		Gtk::Label timestamp;
		Gtk::Label delta;
		Gtk::CheckButton collect_button;
		Gtk::RadioButton use_button;
		Gtk::ComboBoxText port_combo;
		Gtk::CheckButton sclock_synced_button;
		Gtk::CheckButton fr2997_button;
		Gtk::Button request_options;
		Gtk::Menu* request_option_menu;
		Gtk::Button remove_button;
		FloatingTextEntry* name_editor;
		samplepos_t save_when;

		void build_request_options();

		boost::shared_ptr<ARDOUR::TransportMaster> tm;

		void update (ARDOUR::Session*, ARDOUR::samplepos_t);

		Row (TransportMastersWidget& parent);

		struct PortColumns : public Gtk::TreeModel::ColumnRecord {
			PortColumns() {
				add (short_name);
				add (full_name);
			}
			Gtk::TreeModelColumn<std::string> short_name;
			Gtk::TreeModelColumn<std::string> full_name;
		};

		PortColumns port_columns;

		void populate_port_combo ();
		Glib::RefPtr<Gtk::ListStore> build_port_list (std::vector<std::string> const & ports);

		void use_button_toggled ();
		void collect_button_toggled ();
		void sync_button_toggled ();
		void fr2997_button_toggled ();
		void port_choice_changed ();
		void connection_handler ();
		bool request_option_press (GdkEventButton*);
		void prop_change (PBD::PropertyChange);
		void remove_clicked ();

		bool name_press (GdkEventButton*);
		void name_edited (std::string, int);

		PBD::ScopedConnection property_change_connection;
		bool ignore_active_change;
	};

	std::vector<Row*> rows;

	Gtk::Table table;
	Gtk::Label col_title[14];
	Gtk::Button add_button;

	sigc::connection update_connection;
	PBD::ScopedConnection current_connection;
	PBD::ScopedConnection add_connection;
	PBD::ScopedConnection remove_connection;

	void rebuild ();
	void current_changed (boost::shared_ptr<ARDOUR::TransportMaster> old_master, boost::shared_ptr<ARDOUR::TransportMaster> new_master);
	void add_master ();
};

class TransportMastersWindow : public ArdourWindow
{
  public:
	TransportMastersWindow ();

	void set_session (ARDOUR::Session*);

  protected:
	void on_realize ();

  private:
	TransportMastersWidget w;
};


#endif /* __ardour_gtk_transport_masters_dialog_h__ */
