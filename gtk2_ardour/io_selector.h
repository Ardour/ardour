/*
    Copyright (C) 2002-2007 Paul Davis 

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

#ifndef __ardour_ui_io_selector_h__
#define __ardour_ui_io_selector_h__

#include "ardour_dialog.h"
#include "port_matrix.h"

class IOSelector : public PortMatrix {
  public:
	IOSelector (ARDOUR::Session&, boost::shared_ptr<ARDOUR::IO>, bool);

	void set_state (int, std::string const &, bool);
	bool get_state (int, std::string const &) const;
	uint32_t n_rows () const;
	uint32_t maximum_rows () const;
	uint32_t minimum_rows () const;
	std::string row_name (int) const;
	void add_row ();
	void remove_row (int);
	std::string row_descriptor () const;

  private:
	
	void ports_changed (ARDOUR::IOChange, void*);
	
	boost::shared_ptr<ARDOUR::IO> _io;
};

class IOSelectorWindow : public ArdourDialog
{
  public:
	IOSelectorWindow (ARDOUR::Session&, boost::shared_ptr<ARDOUR::IO>, bool for_input, bool can_cancel = false);
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

	void rescan ();
	void cancel ();
	void accept ();
};


class PortInsertUI : public Gtk::VBox
{
  public: 
	PortInsertUI (ARDOUR::Session&, boost::shared_ptr<ARDOUR::PortInsert>);
	
	void redisplay ();
	void finished (IOSelector::Result);

  private:
	Gtk::HBox hbox;
	IOSelector input_selector;
	IOSelector output_selector;
};

class PortInsertWindow : public ArdourDialog
{
  public: 
	PortInsertWindow (ARDOUR::Session&, boost::shared_ptr<ARDOUR::PortInsert>, bool can_cancel = false);
	
  protected:
	void on_map ();
	
  private:
	PortInsertUI _portinsertui;
	Gtk::VBox vbox;
	
	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Button rescan_button;
	Gtk::Frame button_frame;
	
	void rescan ();
	void cancel ();
	void accept ();

	void plugin_going_away ();
	sigc::connection going_away_connection;
};


#endif
