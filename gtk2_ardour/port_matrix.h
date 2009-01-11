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

#ifndef __ardour_ui_port_matrix_h__
#define __ardour_ui_port_matrix_h__

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour_dialog.h"
#include "port_group.h"
#include "matrix.h"

namespace ARDOUR {
	class Session;
	class IO;
	class PortInsert;
}

class PortMatrix : public Gtk::VBox {
  public:
	PortMatrix (ARDOUR::Session&, ARDOUR::DataType, bool, PortGroupList::Mask);
	~PortMatrix ();

	void redisplay ();

	enum Result {
		Cancelled,
		Accepted
	};

	sigc::signal<void, Result> Finished;

	void set_type (ARDOUR::DataType);
	void set_offer_inputs (bool);
	bool offering_input() const { return _offer_inputs; }

	virtual void set_state (int, std::string const &, bool, uint32_t) = 0;
	virtual bool get_state (int, std::string const &) const = 0;
	virtual uint32_t n_rows () const = 0;
	virtual uint32_t maximum_rows () const = 0;
	virtual uint32_t minimum_rows () const = 0;
	virtual std::string row_name (int) const = 0;
	virtual void add_row () = 0;
	virtual void remove_row (int) = 0;
	virtual std::string row_descriptor () const = 0;

	Gtk::Widget& scrolled_window() { return _scrolled_window; }

  protected:

	bool _offer_inputs;
	void set_ports (const std::list<std::string>&);

  private:
	PortGroupList _port_group_list;
	ARDOUR::DataType _type;
	Matrix matrix;
	std::vector<PortGroupUI*> _port_group_ui;
	std::vector<Gtk::EventBox*> _row_labels;
	Gtk::VBox* _row_labels_vbox;
	Gtk::HBox _overall_hbox;
	Gtk::VBox _side_vbox;
	Gtk::HBox _port_group_hbox;
	Gtk::ScrolledWindow _scrolled_window;
	Gtk::Label* _side_vbox_pad;
	Gtk::HBox _visibility_checkbutton_box;

	void setup ();
	void clear ();
	bool row_label_button_pressed (GdkEventButton*, int);
	void reset_visibility ();
};

#endif
