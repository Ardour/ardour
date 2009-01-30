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

#include <cstring>
#include <boost/shared_ptr.hpp>

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/bundle.h"
#include "ardour/io_processor.h"
#include "ardour/midi_track.h"
#include "ardour/port.h"
#include "ardour/session.h"

#include "port_group.h"
#include "port_matrix.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;

/** Add a bundle to a group.
 *  @param b Bundle.
 */
void
PortGroup::add_bundle (boost::shared_ptr<ARDOUR::Bundle> b)
{
	assert (b.get());
	_bundles.push_back (b);

	Modified ();
}

/** Add a port to a group.
 *  @param p Port.
 */
void
PortGroup::add_port (std::string const &p)
{
	ports.push_back (p);

	Modified ();
}

void
PortGroup::clear ()
{
	_bundles.clear ();
	ports.clear ();

	Modified ();
}

bool
PortGroup::has_port (std::string const& p) const
{
	for (ARDOUR::BundleList::const_iterator i = _bundles.begin(); i != _bundles.end(); ++i) {
		if ((*i)->offers_port_alone (p)) {
			return true;
		}
	}

	for (vector<std::string>::const_iterator i = ports.begin(); i != ports.end(); ++i) {
		if (*i == p) {
			return true;
		}
	}

	return false;
}

boost::shared_ptr<ARDOUR::Bundle>
PortGroup::only_bundle ()
{
	assert (_bundles.size() == 1);
	return _bundles.front();
}


uint32_t
PortGroup::total_ports () const
{
	uint32_t n = 0;
	for (ARDOUR::BundleList::const_iterator i = _bundles.begin(); i != _bundles.end(); ++i) {
		n += (*i)->nchannels ();
	}

	n += ports.size();

	return n;
}

	
/** PortGroupList constructor.
 */

PortGroupList::PortGroupList ()
	: _type (ARDOUR::DataType::AUDIO), _bundles_dirty (true)
{
	
}

void
PortGroupList::set_type (ARDOUR::DataType t)
{
	_type = t;
	clear ();
}

/** Gather bundles from around the system and put them in this PortGroupList */
void
PortGroupList::gather (ARDOUR::Session& session, bool inputs)
{
	clear ();

	boost::shared_ptr<PortGroup> buss (new PortGroup (_("Buss")));
	boost::shared_ptr<PortGroup> track (new PortGroup (_("Track")));
	boost::shared_ptr<PortGroup> system (new PortGroup (_("System")));
	boost::shared_ptr<PortGroup> other (new PortGroup (_("Other")));

	/* Find the bundles for routes.  We take their bundles, copy them,
	   and add ports from the route's processors */

	boost::shared_ptr<ARDOUR::RouteList> routes = session.get_routes ();

	for (ARDOUR::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {
		/* Copy the appropriate bundle from the route */
		boost::shared_ptr<ARDOUR::Bundle> bundle (
			new ARDOUR::Bundle (
				inputs ? (*i)->bundle_for_inputs() : (*i)->bundle_for_outputs ()
				)
			);

		/* Add ports from the route's processors */
		uint32_t n = 0;
		while (1) {
			boost::shared_ptr<ARDOUR::Processor> p = (*i)->nth_processor (n);
			if (p == 0) {
				break;
			}

			boost::shared_ptr<ARDOUR::IOProcessor> iop = boost::dynamic_pointer_cast<ARDOUR::IOProcessor> (p);

			if (iop) {
				boost::shared_ptr<ARDOUR::Bundle> pb = inputs ?
					iop->io()->bundle_for_inputs() : iop->io()->bundle_for_outputs();
				bundle->add_channels_from_bundle (pb);
			}

			++n;
		}
			
		/* Work out which group to put this bundle in */
		boost::shared_ptr<PortGroup> g;
		if (_type == ARDOUR::DataType::AUDIO) {

			if (boost::dynamic_pointer_cast<ARDOUR::AudioTrack> (*i)) {
				g = track;
			} else if (!boost::dynamic_pointer_cast<ARDOUR::MidiTrack>(*i)) {
				g = buss;
			} 


		} else if (_type == ARDOUR::DataType::MIDI) {

			if (boost::dynamic_pointer_cast<ARDOUR::MidiTrack> (*i)) {
				g = track;
			}

			/* No MIDI busses yet */
		} 
			
		if (g) {
			g->add_bundle (bundle);
		}
	}

	/* Bundles created by the session */
	
	boost::shared_ptr<ARDOUR::BundleList> b = session.bundles ();
	for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
		if ((*i)->ports_are_inputs() == inputs && (*i)->type() == _type) {
			system->add_bundle (*i);
		}
	}

	/* Now find all other ports that we haven't thought of yet */

 	const char **ports = session.engine().get_ports ("", _type.to_jack_type(), inputs ? 
 							  JackPortIsInput : JackPortIsOutput);
 	if (ports) {

		int n = 0;
		string client_matching_string;

		client_matching_string = session.engine().client_name();
		client_matching_string += ':';

		while (ports[n]) {
			
			std::string const p = ports[n];

			if (!system->has_port(p) && !buss->has_port(p) && !track->has_port(p) && !other->has_port(p)) {
				
				if (port_has_prefix (p, "system:") ||
				    port_has_prefix (p, "alsa_pcm") ||
				    port_has_prefix (p, "ardour:")) {
					system->add_port (p);
				} else {
					other->add_port (p);
				}
			}
			
			++n;
		}

		free (ports);
	}

	add_group (system);
	add_group (buss);
	add_group (track);
	add_group (other);

	_bundles_dirty = true;
}

bool
PortGroupList::port_has_prefix (const std::string& n, const std::string& p) const
{
	return n.substr (0, p.length()) == p;
}
	

void
PortGroupList::update_bundles () const
{
	_bundles.clear ();
		
	for (PortGroupList::List::const_iterator i = begin (); i != end (); ++i) {
		if ((*i)->visible()) {
			
			std::copy ((*i)->bundles().begin(), (*i)->bundles().end(), std::back_inserter (_bundles));

			/* make a bundle for the ports, if there are any */
			if (!(*i)->ports.empty()) {

				boost::shared_ptr<ARDOUR::Bundle> b (new ARDOUR::Bundle ("", _type, !_offer_inputs));
				
				std::string const pre = common_prefix ((*i)->ports);
				if (!pre.empty()) {
					b->set_name (pre.substr (0, pre.length() - 1));
				}

				for (uint32_t j = 0; j < (*i)->ports.size(); ++j) {
					std::string const p = (*i)->ports[j];
					b->add_channel (p.substr (pre.length()));
					b->set_port (j, p);
				}
					
				_bundles.push_back (b);
			}
		}
	}

	_bundles_dirty = false;
}

std::string
PortGroupList::common_prefix (std::vector<std::string> const & p) const
{
	/* common prefix before '/' ? */
	if (p[0].find_first_of ("/") != std::string::npos) {
		std::string const fp = p[0].substr (0, (p[0].find_first_of ("/") + 1));
		uint32_t j = 1;
		while (j < p.size()) {
			if (p[j].substr (0, fp.length()) != fp) {
				break;
			}
			++j;
		}
		
		if (j == p.size()) {
			return fp;
		}
	}
	
	/* or before ':' ? */
	if (p[0].find_first_of (":") != std::string::npos) {
		std::string const fp = p[0].substr (0, (p[0].find_first_of (":") + 1));
		uint32_t j = 1;
		while (j < p.size()) {
			if (p[j].substr (0, fp.length()) != fp) {
				break;
			}
			++j;
		}
		
		if (j == p.size()) {
			return fp;
		}
	}

	return "";
}

void
PortGroupList::clear ()
{
	_groups.clear ();
	_bundles_dirty = true;
}

ARDOUR::BundleList const &
PortGroupList::bundles () const
{
	if (_bundles_dirty) {
		update_bundles ();
	}

	return _bundles;
}

uint32_t
PortGroupList::total_visible_ports () const
{
	uint32_t n = 0;
	
	for (PortGroupList::List::const_iterator i = begin(); i != end(); ++i) {
		if ((*i)->visible()) {
			n += (*i)->total_ports ();
		}
	}

	return n;
}

void
PortGroupList::group_modified ()
{
	_bundles_dirty = true;
}

void
PortGroupList::add_group (boost::shared_ptr<PortGroup> g)
{
	_groups.push_back (g);
	g->Modified.connect (sigc::mem_fun (*this, &PortGroupList::group_modified));
	_bundles_dirty = true;
}
