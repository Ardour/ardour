/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __ardour_gtk_connection_editor_h__
#define __ardour_gtk_connection_editor_h__

#if __GNUC__ >= 3
#include <ext/slist>
using __gnu_cxx::slist;
#else
#include <slist.h>
#endif

#include <gtkmm/box.h>
#include <gtkmm/window.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/button.h>
#include <gtkmm/frame.h>
#include <gtkmm/notebook.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

#include "ardour_dialog.h"

#include <glibmm/thread.h>

namespace ARDOUR {
	class Session;
	class Connection;
}

class ConnectionEditor : public ArdourDialog {
  public:
	ConnectionEditor ();
	~ConnectionEditor ();

	void set_session (ARDOUR::Session *);

  protected:
	void on_map ();

  private:
	ARDOUR::Connection *current_connection;
	int                 selected_port;
	bool                push_at_front;


	struct ConnectionDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    ConnectionDisplayModelColumns() { 
		    add (name);
		    add (connection);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> name;
	    Gtk::TreeModelColumn<ARDOUR::Connection*> connection;
	};

	ConnectionDisplayModelColumns connection_columns;

	Glib::RefPtr<Gtk::ListStore> input_connection_model;
	Glib::RefPtr<Gtk::ListStore> output_connection_model;

	Gtk::TreeView input_connection_display;
	Gtk::TreeView output_connection_display;
	Gtk::ScrolledWindow input_scroller;
	Gtk::ScrolledWindow output_scroller;

	Gtk::Frame input_frame;
	Gtk::Frame output_frame;
	Gtk::VBox input_box;
	Gtk::VBox output_box;
	Gtk::VBox connection_box;

	Gtk::HBox main_hbox;
	Gtk::VBox main_vbox;

	Gtk::VBox left_vbox;
	Gtk::VBox right_vbox;
	Gtk::VBox port_and_selector_box;
	

	Gtk::Button new_input_connection_button;
	Gtk::Button new_output_connection_button;
	Gtk::Button delete_connection_button;

	/* client/port selection */

	Gtk::Notebook notebook;
	Gtk::Button clear_client_button;
	Gtk::Frame selector_frame;
	Gtk::VBox selector_box;
	Gtk::HBox selector_button_box;

	/* connection displays */

	Gtk::HBox port_box;
	Gtk::HBox port_button_box;
	Gtk::VBox port_and_button_box;
	Gtk::Frame port_frame;
	Gtk::Button clear_button;
	Gtk::Button add_port_button;

	Glib::Mutex port_display_lock;
	slist<Gtk::ScrolledWindow *> port_displays;

	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Button rescan_button;

	Gtk::Frame button_frame;
	Gtk::HBox  button_box;

	void new_connection (bool for_input);
	void delete_connection ();

	void display_ports ();
	void display_connection_state (bool for_input);

	void add_connection (ARDOUR::Connection *);
	void add_connection_and_select (ARDOUR::Connection *);
	void proxy_add_connection_and_select (ARDOUR::Connection *);
	void proxy_remove_connection (ARDOUR::Connection *);
	void remove_connection (ARDOUR::Connection *);
	void refill_connection_display ();

	void rescan ();
	void clear ();
	void cancel ();
	void accept ();

	void selection_changed (Gtk::TreeView* display);

	void add_port ();
	void remove_port (int which_port);

	void port_column_click (gint col, Gtk::TreeView* );
	gint port_button_event (GdkEventButton *, Gtk::TreeView*);
	gint connection_click (GdkEventButton *ev, Gtk::TreeView*);
	void connection_selection_changed (Gtk::TreeView&, Glib::RefPtr<Gtk::ListStore>&);

	sigc::connection config_connection;
	sigc::connection connect_connection;
	void configuration_changed (bool);
	void connections_changed (int, bool);
};

#endif /* __ardour_gtk_connection_editor_h__ */
