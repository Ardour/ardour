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
#include <gtkmm/image.h>
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menu_elems.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/menushell.h>
#include <glibmm/objectbase.h>
#include <gtkmm2ext/doi.h>
#include <ardour/port_insert.h>
#include "ardour/session.h"
#include "ardour/io.h"
#include "ardour/audioengine.h"
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/data_type.h"
#include "io_selector.h"
#include "keyboard.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace Gtk;

/** Add a port to a group.
 *  @param p Port name, with or without prefix.
 */

void
PortGroup::add (std::string const & p)
{
	if (prefix.empty() == false && p.substr (0, prefix.length()) == prefix) {
		ports.push_back (p.substr (prefix.length()));
	} else {
		ports.push_back (p);
	}
}

/** PortGroupUI constructor.
 *  @param m PortMatrix to work for.
 *  @Param g PortGroup to represent.
 */

PortGroupUI::PortGroupUI (PortMatrix& m, PortGroup& g)
	: _port_matrix (m)
	, _port_group (g)
	, _ignore_check_button_toggle (false)
	, _visibility_checkbutton (g.name)
{
	_port_group.visible = true;
 	_ignore_check_button_toggle = false;
	_visibility_checkbutton.signal_toggled().connect (sigc::mem_fun (*this, &PortGroupUI::visibility_checkbutton_toggled));
}

/** The visibility of a PortGroupUI has been toggled */
void
PortGroupUI::visibility_checkbutton_toggled ()
{
	_port_group.visible = _visibility_checkbutton.get_active ();
}

/** @return Checkbutton used to toggle visibility */
Widget&
PortGroupUI::get_visibility_checkbutton ()
{
	return _visibility_checkbutton;
}


/** Handle a toggle of a port check button */
void
PortGroupUI::port_checkbutton_toggled (CheckButton* b, int r, int c)
{
 	if (_ignore_check_button_toggle == false) {
		// _port_matrix.hide_group (_port_group);
	}
}

/** Set up visibility of the port group according to PortGroup::visible */
void
PortGroupUI::setup_visibility ()
{
	if (_visibility_checkbutton.get_active () != _port_group.visible) {
		_visibility_checkbutton.set_active (_port_group.visible);
	}
}

PortMatrix::PortMatrix (ARDOUR::Session& session, ARDOUR::DataType type, bool offer_inputs, PortGroupList::Mask mask)
	: _offer_inputs (offer_inputs), _port_group_list (session, type, offer_inputs, mask), _type (type)
{
	_row_labels_vbox = 0;
	_side_vbox_pad = 0;

	_visibility_checkbutton_box.pack_start (*(manage (new Label (_("Connections displayed: ")))), false, false, 10);
 	pack_start (_visibility_checkbutton_box, false, false);

	_scrolled_window.set_policy (POLICY_ALWAYS, POLICY_AUTOMATIC);
	_scrolled_window.set_shadow_type (SHADOW_NONE);

	VBox* b = manage (new VBox);

	b->pack_start (_port_group_hbox, false, false);
	b->pack_start (_port_group_hbox, false, false);

	_scrolled_window.add (matrix);

	if (offer_inputs) {
		_overall_hbox.pack_start (_side_vbox, false, false, 6);
		_overall_hbox.pack_start (_scrolled_window, true, true);
	} else {
		_overall_hbox.pack_start (_scrolled_window, true, true, 6);
		_overall_hbox.pack_start (_side_vbox, false, false);
	}

	pack_start (_overall_hbox);

	_port_group_hbox.signal_size_allocate().connect (sigc::hide (sigc::mem_fun (*this, &IOSelector::setup_dimensions)));
}

PortMatrix::~PortMatrix ()
{
	clear ();
}

/** Clear out the things that change when the number of source or destination ports changes */
void
PortMatrix::clear ()
{
	for (std::vector<EventBox*>::iterator j = _row_labels.begin(); j != _row_labels.end(); ++j) {
		delete *j;
	}
	_row_labels.clear ();
		
	if (_row_labels_vbox) {
		_side_vbox.remove (*_row_labels_vbox);
		delete _row_labels_vbox;
		_row_labels_vbox = 0;
	}
	
	/* remove lurking, invisible label and padding */
	
	_side_vbox.children().clear ();

	if (_side_vbox_pad) {
		delete _side_vbox_pad;
		_side_vbox_pad = 0;
	}

	for (std::vector<PortGroupUI*>::iterator i = _port_group_ui.begin(); i != _port_group_ui.end(); ++i) {
		_visibility_checkbutton_box.remove ((*i)->get_visibility_checkbutton());
		delete *i;
	}

	_port_group_ui.clear ();
}


/** Set up dimensions of some of our widgets which depend on other dimensions
 *  within the dialogue.
 */
void
PortMatrix::setup_dimensions ()
{
	/* Row labels */
	for (std::vector<EventBox*>::iterator j = _row_labels.begin(); j != _row_labels.end(); ++j) {
		(*j)->get_child()->set_size_request (-1, matrix.row_spacing());
	}
	
	if (_side_vbox_pad) {
		if (_offer_inputs) {
			_side_vbox_pad->set_size_request (-1, matrix.row_spacing() / 4);
		} else {
			_side_vbox_pad->set_size_request (-1, matrix.row_spacing() / 4);
		}
	} 
}


/** Set up the dialogue */
void
PortMatrix::setup ()
{
	clear ();

 	int const rows = n_rows ();
	
 	/* Row labels */

	_row_labels_vbox = new VBox;
	int const run_rows = std::max (1, rows);

	for (int j = 0; j < run_rows; ++j) {
		
		/* embolden the port/channel name */
		
		string s = "<b>";
		s += row_name (j);
		s += "</b>";
		
		Label* label = manage (new Label (s));
		EventBox* b = manage (new EventBox);
		
		label->set_use_markup (true);
		
		b->set_events (Gdk::BUTTON_PRESS_MASK);
		b->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &IOSelector::row_label_button_pressed), j));
		b->add (*label);
		
		_row_labels.push_back (b);
		_row_labels_vbox->pack_start (*b, false, false);
	}

	_side_vbox_pad = new Label (""); /* unmanaged, explicitly deleted */

	if (_offer_inputs) {
		_side_vbox.pack_start (*_side_vbox_pad, false, false);
		_side_vbox.pack_start (*_row_labels_vbox, false, false);
		_side_vbox.pack_start (*manage (new Label ("")));
	} else {
		_side_vbox.pack_start (*manage (new Label ("")));
		_side_vbox.pack_start (*_row_labels_vbox, false, false);
		_side_vbox.pack_start (*_side_vbox_pad, false, false);
	}

 	/* Checkbutton tables and visibility checkbuttons */
 	for (PortGroupList::iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {

		PortGroupUI* t = new PortGroupUI (*this, **i);
		
		_port_group_ui.push_back (t);
		
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

void
PortMatrix::redisplay ()
{
	_port_group_list.refresh ();
	setup ();
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
	redisplay ();
}

void
PortMatrix::set_offer_inputs (bool i)
{
	_offer_inputs = i;
	_port_group_list.set_offer_inputs (i);
	redisplay ();
}

/** PortGroupList constructor.
 *  @param session Session to get ports from.
 *  @param type Type of ports to offer (audio or MIDI)
 *  @param offer_inputs true to offer output ports, otherwise false.
 *  @param mask Mask of groups to make visible by default.
 */

PortGroupList::PortGroupList (ARDOUR::Session & session, ARDOUR::DataType type, bool offer_inputs, Mask mask)
	: _session (session), _type (type), _offer_inputs (offer_inputs),
	  buss (_("Bus"), "ardour:", mask & BUSS),
	  track (_("Track"), "ardour:", mask & TRACK),
	  system (_("System"), "system:", mask & SYSTEM),
	  other (_("Other"), "", mask & OTHER)
{
	refresh ();
}

void
PortGroupList::refresh ()
{
	clear ();
	
	buss.ports.clear ();
	track.ports.clear ();
	system.ports.clear ();
	other.ports.clear ();

	/* Find the ports provided by ardour; we can't derive their type just from their
	   names, so we'll have to be more devious. 
	*/

	boost::shared_ptr<ARDOUR::Session::RouteList> routes = _session.get_routes ();

	for (ARDOUR::Session::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {

		PortGroup* g = 0;

		if (_type == ARDOUR::DataType::AUDIO) {

			if (boost::dynamic_pointer_cast<ARDOUR::AudioTrack> (*i)) {
				g = &track;
			} else if (!boost::dynamic_pointer_cast<ARDOUR::MidiTrack>(*i)) {
				g = &buss;
			} 


		} else if (_type == ARDOUR::DataType::MIDI) {

			if (boost::dynamic_pointer_cast<ARDOUR::MidiTrack> (*i)) {
				g = &track;
			}

			/* No MIDI busses yet */
		} 
			
		if (g) {
			ARDOUR::PortSet const & p = _offer_inputs ? ((*i)->inputs()) : ((*i)->outputs());
			for (uint32_t j = 0; j < p.num_ports(); ++j) {
				g->add (p.port(j)->name ());
			}

			std::sort (g->ports.begin(), g->ports.end());
		}
	}
	
	/* XXX: inserts, sends, plugin inserts? */
	
	/* Now we need to find the non-ardour ports; we do this by first
	   finding all the ports that we can connect to. 
	*/

	const char **ports = _session.engine().get_ports ("", _type.to_jack_type(), _offer_inputs ? 
							  JackPortIsInput : JackPortIsOutput);
	if (ports) {

		int n = 0;
		string client_matching_string;

		client_matching_string = _session.engine().client_name();
		client_matching_string += ':';

		while (ports[n]) {
			std::string const p = ports[n];

			if (p.substr(0, strlen ("system:")) == "system:") {
				/* system: prefix */
				system.add (p);
			} else {
				if (p.substr(0, client_matching_string.length()) != client_matching_string) {
					/* other (non-ardour) prefix */
					other.add (p);
				}
			}

			++n;
		}

		free (ports);
	}

	push_back (&system);
	push_back (&buss);
	push_back (&track);
	push_back (&other);
}

int
PortGroupList::n_visible_ports () const
{
	int n = 0;
	
	for (const_iterator i = begin(); i != end(); ++i) {
		if ((*i)->visible) {
			n += (*i)->ports.size();
		}
	}

	return n;
}

std::string
PortGroupList::get_port_by_index (int n, bool with_prefix) const
{
	/* XXX: slightly inefficient algorithm */

	for (const_iterator i = begin(); i != end(); ++i) {
		for (std::vector<std::string>::const_iterator j = (*i)->ports.begin(); j != (*i)->ports.end(); ++j) {
			if (n == 0) {
				if (with_prefix) {
					return (*i)->prefix + *j;
				} else {
					return *j;
				}
			}
			--n;
		}
	}

	return "";
}

void
PortGroupList::set_type (ARDOUR::DataType t)
{
	_type = t;
}

void
PortGroupList::set_offer_inputs (bool i)
{
	_offer_inputs = i;
}

