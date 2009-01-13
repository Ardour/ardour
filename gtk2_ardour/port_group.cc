/*
    Copyright (C) 2002-2009 Paul Davis 

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

#include "port_group.h"
#include "i18n.h"
#include "ardour/session.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/audioengine.h"
#include <boost/shared_ptr.hpp>
#include <cstring>

using namespace std;
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

/** PortGroupList constructor.
 *  @param session Session to get ports from.
 *  @param type Type of ports to offer (audio or MIDI)
 *  @param offer_inputs true to offer output ports, otherwise false.
 *  @param mask Mask of groups to make visible by default.
 */

PortGroupList::PortGroupList (ARDOUR::Session & session, ARDOUR::DataType type, bool offer_inputs, Mask mask)
	: _session (session), _type (type), _offer_inputs (offer_inputs),
	  _buss (_("Bus"), "ardour:", mask & BUSS),
	  _track (_("Track"), "ardour:", mask & TRACK),
	  _system (_("System"), "system:", mask & SYSTEM),
	  _other (_("Other"), "", mask & OTHER)
{
	refresh ();
}

/** Find or re-find all our ports and set up our lists */
void
PortGroupList::refresh ()
{
	clear ();
	
	_buss.ports.clear ();
	_track.ports.clear ();
	_system.ports.clear ();
	_other.ports.clear ();

	/* Find the ports provided by ardour; we can't derive their type just from their
	   names, so we'll have to be more devious. 
	*/

	boost::shared_ptr<ARDOUR::Session::RouteList> routes = _session.get_routes ();

	for (ARDOUR::Session::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {

		PortGroup* g = 0;

		if (_type == ARDOUR::DataType::AUDIO) {

			if (boost::dynamic_pointer_cast<ARDOUR::AudioTrack> (*i)) {
				g = &_track;
			} else if (!boost::dynamic_pointer_cast<ARDOUR::MidiTrack>(*i)) {
				g = &_buss;
			} 


		} else if (_type == ARDOUR::DataType::MIDI) {

			if (boost::dynamic_pointer_cast<ARDOUR::MidiTrack> (*i)) {
				g = &_track;
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
				_system.add (p);
			} else {
				if (p.substr(0, client_matching_string.length()) != client_matching_string) {
					/* other (non-ardour) prefix */
					_other.add (p);
				}
			}

			++n;
		}

		free (ports);
	}

	push_back (&_system);
	push_back (&_buss);
	push_back (&_track);
	push_back (&_other);
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

