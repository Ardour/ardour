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

    $Id$
*/

#ifndef __ardour_ui_io_selector_h__
#define __ardour_ui_io_selector_h__

#if __GNUC__ >= 3
#include <ext/slist>
using __gnu_cxx::slist;
#else
#include <slist.h>
#endif

#include <string>
#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/notebook.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

#include <ardour_dialog.h>

namespace ARDOUR {
	class IO;
	class Session;
	class PortInsert;
	class Port;
	class Redirect;
}

class IOSelector : public Gtk::VBox {
  public:
	IOSelector (ARDOUR::Session&, ARDOUR::IO&, bool for_input);
	~IOSelector ();

	void redisplay ();

	enum Result {
		Cancelled,
		Accepted
	};

	sigc::signal<void,Result> Finished;

  protected:
	ARDOUR::Session& session;

  private:
	ARDOUR::IO& io;
	bool for_input;
	ARDOUR::Port *selected_port;

	Gtk::VBox main_box;
	Gtk::HBox port_and_selector_box;

	/* column model */

	struct PortDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {

	    PortDisplayModelColumns() { 
		    add (displayed_name);
		    add (full_name);
	    }

	    Gtk::TreeModelColumn<Glib::ustring>       displayed_name;
	    Gtk::TreeModelColumn<Glib::ustring>       full_name;
	};

	PortDisplayModelColumns port_display_columns;

	/* client/port selection */

	Gtk::Notebook notebook;
	Gtk::Frame selector_frame;
	Gtk::VBox selector_box;
	Gtk::HBox selector_button_box;

	/* ports */

	Gtk::VBox port_box;
	Gtk::HBox port_button_box;
	Gtk::VBox port_and_button_box;
	Gtk::Frame port_frame;
	Gtk::Button add_port_button;
	Gtk::Button remove_port_button;
	Gtk::Button clear_connections_button;
	Gtk::ScrolledWindow port_display_scroller;

	PBD::Lock port_display_lock;
	slist<Gtk::TreeView *> port_displays;
	void display_ports ();

	void rescan ();
	void clear_connections ();

	void port_selection_changed(Gtk::TreeView*);

	void ports_changed (ARDOUR::IOChange, void *);
	void name_changed (void*);

	void add_port ();
	void remove_port ();
	gint remove_port_when_idle (ARDOUR::Port *);

	gint port_column_button_release (GdkEventButton*, Gtk::TreeView*);
	gint connection_button_release (GdkEventButton *, Gtk::TreeView*);
	
	void select_treeview(Gtk::TreeView*);
	void select_next_treeview ();
};

class IOSelectorWindow : public ArdourDialog
{
  public:
	IOSelectorWindow (ARDOUR::Session&, ARDOUR::IO&, bool for_input, bool can_cancel=false);
	~IOSelectorWindow ();

	IOSelector& selector() { return _selector; }

  protected:
	void on_map ();
	
  private:
	IOSelector _selector;

	/* overall operation buttons */

	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Button rescan_button;
	Gtk::HBox button_box;

	void rescan ();
	void cancel ();
	void accept ();
};


class PortInsertUI : public Gtk::VBox
{
  public: 
	PortInsertUI (ARDOUR::Session&, ARDOUR::PortInsert&);
	
	void redisplay ();
	void finished (IOSelector::Result);

  private:
	
	Gtk::HBox  hbox;
	IOSelector input_selector;
	IOSelector output_selector;
	
};

class PortInsertWindow : public ArdourDialog
{
  public: 
	PortInsertWindow (ARDOUR::Session&, ARDOUR::PortInsert&, bool can_cancel=false);
	
  protected:
	void on_map ();
	
  private:
	
	PortInsertUI _portinsertui;
	Gtk::VBox vbox;
	
	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Button rescan_button;
	Gtk::Frame button_frame;
	Gtk::HBox button_box;
	
	void rescan ();
	void cancel ();
	void accept ();

	void plugin_going_away (ARDOUR::Redirect*);
};


#endif /* __ardour_ui_io_selector_h__ */
