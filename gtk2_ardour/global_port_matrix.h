/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_gtk_global_port_matrix_h__
#define __ardour_gtk_global_port_matrix_h__

#include <gtkmm/button.h>
#include "port_matrix.h"
#include "port_group.h"
#include "ardour_window.h"

class GlobalPortMatrix : public PortMatrix
{
public:
	GlobalPortMatrix (Gtk::Window*, ARDOUR::Session*, ARDOUR::DataType);

	void setup_ports (int);
	void set_session (ARDOUR::Session* s);

	void set_state (ARDOUR::BundleChannel c[2], bool);
	PortMatrixNode::State get_state (ARDOUR::BundleChannel c[2]) const;

	std::string disassociation_verb () const;
	std::string channel_noun () const;

	bool list_is_global (int) const {
		return true;
	}

private:
	/* see PortMatrix: signal flow from 0 to 1 (out to in) */
	enum {
		FLOW_OUT = 0,
		FLOW_IN = 1,
	};
};

class GlobalPortMatrixWindow : public ArdourWindow
{
public:
	GlobalPortMatrixWindow (ARDOUR::Session *, ARDOUR::DataType);

	void set_session (ARDOUR::Session *);

private:
	void on_show ();

	GlobalPortMatrix _port_matrix;
	Gtk::Button _rescan_button;
	Gtk::CheckButton _show_ports_button;
};


#endif
