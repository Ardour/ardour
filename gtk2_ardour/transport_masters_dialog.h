/*
 * Copyright (C) 2018-2019 Paul Davis <paul@linuxaudiosystems.com>
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

#include "widgets/ardour_button.h"

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

	void update (Temporal::timepos_t);
	void set_transport_master (boost::shared_ptr<ARDOUR::TransportMaster>);

	void set_session (ARDOUR::Session*);

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
		Gtk::EventBox current_box;
		Gtk::EventBox last_box;
		Gtk::Label label;
		Gtk::Label type;
		Gtk::Label format;
		Gtk::Label current;
		Gtk::Label last;
		Gtk::RadioButton use_button;
		Gtk::ComboBoxText port_combo;
		Gtk::CheckButton sclock_synced_button;
		Gtk::CheckButton fr2997_button;
		ArdourWidgets::ArdourButton request_options;
		Gtk::Menu* request_option_menu;
		ArdourWidgets::ArdourButton remove_button;
		FloatingTextEntry* name_editor;
		samplepos_t save_when;
		std::string save_last;

		void build_request_options();
		void mod_request_type (ARDOUR::TransportRequestType);

		boost::shared_ptr<ARDOUR::TransportMaster> tm;

		void update (ARDOUR::Session*, ARDOUR::samplepos_t);

		Row (TransportMastersWidget& parent);
		~Row ();

		void populate_port_combo ();
		void build_port_list (ARDOUR::DataType);

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

		bool port_combo_proxy (GdkEventButton*);
	};

	std::vector<Row*> rows;

	Gtk::Table table;
	Gtk::Label col_title[14];
	float align[14];
	ArdourWidgets::ArdourButton add_master_button;
	Gtk::CheckButton lost_sync_button;

	sigc::connection update_connection;
	PBD::ScopedConnection current_connection;
	PBD::ScopedConnection add_connection;
	PBD::ScopedConnection remove_connection;
	PBD::ScopedConnection engine_running_connection;

	struct PortColumns : public Gtk::TreeModel::ColumnRecord {
		PortColumns() {
			add (short_name);
			add (full_name);
		}
		Gtk::TreeModelColumn<std::string> short_name;
		Gtk::TreeModelColumn<std::string> full_name;
	};

	PortColumns port_columns;

	friend class Row;
	Glib::RefPtr<Gtk::ListStore> midi_port_store;
	Glib::RefPtr<Gtk::ListStore> audio_port_store;

	PBD::ScopedConnectionList port_reg_connection;
	void update_ports ();
	bool ignore_active_change;
	void build_port_model (Glib::RefPtr<Gtk::ListStore>, std::vector<std::string> const &);

	void rebuild ();
	void clear ();
	void current_changed (boost::shared_ptr<ARDOUR::TransportMaster> old_master, boost::shared_ptr<ARDOUR::TransportMaster> new_master);
	void add_master ();
	void update_usability ();
	void allow_master_select (bool);

	void lost_sync_changed ();
	void lost_sync_button_toggled ();
	void param_changed (std::string const &);
	PBD::ScopedConnection config_connection;
	PBD::ScopedConnection session_config_connection;

  public:
	bool idle_remove (Row*);
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
