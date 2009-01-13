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

#include <gtkmm/label.h>
#include <gtkmm/enums.h>
#include <gtkmm/menu.h>
#include <gtkmm/menu_elems.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/menushell.h>
#include <glibmm/objectbase.h>
#include <gtkmm2ext/doi.h>
#include "ardour/data_type.h"
#include "i18n.h"
#include "port_matrix.h"

using namespace Gtk;

PortMatrix::PortMatrix (ARDOUR::Session& session, ARDOUR::DataType type, bool offer_inputs, PortGroupList::Mask mask)
	: _offer_inputs (offer_inputs), _port_group_list (session, type, offer_inputs, mask), _type (type), matrix (this)
{
	_side_vbox_pad = 0;

	_visibility_checkbutton_box.pack_start (*(manage (new Label (_("Connections displayed: ")))), false, false, 10);
 	pack_start (_visibility_checkbutton_box, false, false);

	_scrolled_window.set_policy (POLICY_ALWAYS, POLICY_AUTOMATIC);
	_scrolled_window.set_shadow_type (SHADOW_NONE);

	_scrolled_window.add (matrix);

	if (offer_inputs) {
		_overall_hbox.pack_start (_side_vbox, false, false, 6);
		_overall_hbox.pack_start (_scrolled_window, true, true);
	} else {
		_overall_hbox.pack_start (_scrolled_window, true, true, 6);
		_overall_hbox.pack_start (_side_vbox, false, false);
	}

	pack_start (_overall_hbox);
}

PortMatrix::~PortMatrix ()
{
	clear ();
}

void 
PortMatrix::set_ports (const std::list<std::string>& ports)
{
	matrix.set_ports (ports);
}

/** Clear out the things that change when the number of source or destination ports changes */
void
PortMatrix::clear ()
{
	/* remove lurking, invisible label and padding */
	
	_side_vbox.children().clear ();

	delete _side_vbox_pad;
	_side_vbox_pad = 0;

	for (std::vector<PortGroupUI*>::iterator i = _port_group_ui.begin(); i != _port_group_ui.end(); ++i) {
		_visibility_checkbutton_box.remove ((*i)->get_visibility_checkbutton());
		delete *i;
	}

	_port_group_ui.clear ();
}

/** Set up the dialogue */
void
PortMatrix::setup ()
{
	/* sort out the ports that we'll offer to connect to */
	_port_group_list.refresh ();
	
	clear ();

	_side_vbox_pad = new Label (""); /* unmanaged, explicitly deleted */

	_side_vbox.pack_start (*_side_vbox_pad, false, false);
	_side_vbox.pack_start (*manage (new Label ("")));

	matrix.clear ();

 	/* Matrix and visibility checkbuttons */
 	for (PortGroupList::iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {

		PortGroupUI* t = new PortGroupUI (*this, **i);
		
		_port_group_ui.push_back (t);

		matrix.add_group (**i);

		_visibility_checkbutton_box.pack_start (t->get_visibility_checkbutton(), false, false);

		CheckButton* chk = dynamic_cast<CheckButton*>(&t->get_visibility_checkbutton());

		if (chk) { 
			chk->signal_toggled().connect (sigc::mem_fun (*this, &PortMatrix::reset_visibility));
		}
	}

	show_all ();

	reset_visibility ();
}

void
PortMatrix::reset_visibility ()
{
	for (std::vector<PortGroupUI*>::iterator i = _port_group_ui.begin(); i != _port_group_ui.end(); ++i) {

		(*i)->setup_visibility ();
		
		if ((*i)->port_group().visible) {
			matrix.show_group ((*i)->port_group());
		} else {
			matrix.hide_group ((*i)->port_group());
		}
	}
}


/** Handle a button press on a row label */
bool
PortMatrix::row_label_button_pressed (GdkEventButton* e, int r)
{
	if (e->type != GDK_BUTTON_PRESS || e->button != 3) {
		return false;
	}

	Menu* menu = manage (new Menu);
	Menu_Helpers::MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	bool const can_add = maximum_rows () > n_rows ();
	bool const can_remove = minimum_rows () < n_rows ();
	std::string const name = row_name (r);
	
	items.push_back (
		Menu_Helpers::MenuElem (string_compose(_("Add %1"), row_descriptor()), sigc::mem_fun (*this, &PortMatrix::add_row))
		);

	items.back().set_sensitive (can_add);

	items.push_back (
		Menu_Helpers::MenuElem (string_compose(_("Remove %1 \"%2\""), row_descriptor(), name), sigc::bind (sigc::mem_fun (*this, &PortMatrix::remove_row), r))
		);

	items.back().set_sensitive (can_remove);

	menu->popup (e->button, e->time);
	
	return true;
}

void
PortMatrix::set_type (ARDOUR::DataType t)
{
	_type = t;
	_port_group_list.set_type (t);
	setup ();
}

void
PortMatrix::set_offer_inputs (bool i)
{
	_offer_inputs = i;
	_port_group_list.set_offer_inputs (i);
	setup ();
}

