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
#include "port_matrix.h"
#include "i18n.h"
#include "ardour/session.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/audioengine.h"
#include "ardour/bundle.h"
#include <boost/shared_ptr.hpp>
#include <cstring>

using namespace std;
using namespace Gtk;

/** Add a bundle to a group.
 *  @param b Bundle.
 */
void
PortGroup::add_bundle (boost::shared_ptr<ARDOUR::Bundle> b)
{
	bundles.push_back (b);
}

/** Add a port to a group.
 *  @param p Port.
 */
void
PortGroup::add_port (std::string const &p)
{
	ports.push_back (p);
}

void
PortGroup::clear ()
{
	bundles.clear ();
	ports.clear ();
}

/** PortGroupUI constructor.
 *  @param m PortMatrix to work for.
 *  @Param g PortGroup to represent.
 */

PortGroupUI::PortGroupUI (PortMatrix* m, PortGroup* g)
	: _port_matrix (m)
	, _port_group (g)
	, _visibility_checkbutton (g->name)
{
	_port_group->visible = true;
	setup_visibility_checkbutton ();
	
	_visibility_checkbutton.signal_toggled().connect (sigc::mem_fun (*this, &PortGroupUI::visibility_checkbutton_toggled));
}

/** The visibility of a PortGroupUI has been toggled */
void
PortGroupUI::visibility_checkbutton_toggled ()
{
	_port_group->visible = _visibility_checkbutton.get_active ();
	setup_visibility_checkbutton ();
	_port_matrix->setup ();
}

/** Set up the visibility checkbutton according to PortGroup::visible */
void
PortGroupUI::setup_visibility_checkbutton ()
{
	if (_visibility_checkbutton.get_active () != _port_group->visible) {
		_visibility_checkbutton.set_active (_port_group->visible);
	}
}

/** PortGroupList constructor.
 *  @param session Session to get bundles from.
 *  @param type Type of bundles to offer (audio or MIDI)
 *  @param offer_inputs true to offer output bundles, otherwise false.
 *  @param mask Mask of groups to make visible by default.
 */

PortGroupList::PortGroupList (ARDOUR::Session & session, ARDOUR::DataType type, bool offer_inputs, Mask mask)
	: _session (session), _type (type), _offer_inputs (offer_inputs),
	  _buss (_("Bus"), mask & BUSS),
	  _track (_("Track"), mask & TRACK),
	  _system (_("System"), mask & SYSTEM),
	  _other (_("Other"), mask & OTHER)
{
	refresh ();
}

/** Find or re-find all our bundles and set up our lists */
void
PortGroupList::refresh ()
{
	clear ();

	_buss.clear ();
	_track.clear ();
	_system.clear ();
	_other.clear ();

	/* Find the bundles for routes */

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
			g->add_bundle (_offer_inputs ? (*i)->bundle_for_inputs() : (*i)->bundle_for_outputs ());
		}
	}

	/* Bundles created by the session */
	_session.foreach_bundle (sigc::mem_fun (*this, &PortGroupList::maybe_add_session_bundle));
	
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
				_system.add_port (p);
			} else {
				if (p.substr(0, client_matching_string.length()) != client_matching_string) {
					/* other (non-ardour) prefix */
					_other.add_port (p);
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

void
PortGroupList::maybe_add_session_bundle (boost::shared_ptr<ARDOUR::Bundle> b)
{
	if (b->ports_are_inputs () == _offer_inputs) {
		_system.bundles.push_back (b);
	}
}
