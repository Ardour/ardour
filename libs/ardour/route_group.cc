/*
    Copyright (C) 2000-2009 Paul Davis

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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <algorithm>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/strsplit.h"

#include "ardour/amp.h"
#include "ardour/route_group.h"
#include "ardour/audio_track.h"
#include "ardour/audio_diskstream.h"
#include "ardour/configuration.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

RouteGroup::RouteGroup (Session& s, const string &n, Flag f, Property p)
	: _session (s)
	, routes (new RouteList)
	, _name (n)
	, _flags (f)
	, _properties (Property (p))
{
}

RouteGroup::~RouteGroup ()
{
	for (RouteList::iterator i = routes->begin(); i != routes->end();) {
		RouteList::iterator tmp = i;
		++tmp;

		(*i)->leave_route_group ();
		
		i = tmp;
	}
}

void
RouteGroup::set_name (string str)
{
	_name = str;
	_session.set_dirty ();
	FlagsChanged (0); /* EMIT SIGNAL */
}

int
RouteGroup::add (boost::shared_ptr<Route> r)
{
	r->leave_route_group ();

	routes->push_back (r);

	r->join_route_group (this);
	r->GoingAway.connect (sigc::bind (sigc::mem_fun (*this, &RouteGroup::remove_when_going_away), boost::weak_ptr<Route> (r)));
	
	_session.set_dirty ();
	changed (); /* EMIT SIGNAL */
	return 0;
}

void
RouteGroup::remove_when_going_away (boost::weak_ptr<Route> wr)
{
	boost::shared_ptr<Route> r (wr.lock());

	if (r) {
		remove (r);
	}
}

int
RouteGroup::remove (boost::shared_ptr<Route> r)
{
	RouteList::iterator i;

	if ((i = find (routes->begin(), routes->end(), r)) != routes->end()) {
		r->leave_route_group ();
		routes->erase (i);
		_session.set_dirty ();
		changed (); /* EMIT SIGNAL */
		return 0;
	}

	return -1;
}


gain_t
RouteGroup::get_min_factor(gain_t factor)
{
	gain_t g;

	for (RouteList::iterator i = routes->begin(); i != routes->end(); i++) {
		g = (*i)->amp()->gain();

		if ( (g+g*factor) >= 0.0f)
			continue;

		if ( g <= 0.0000003f )
			return 0.0f;

		factor = 0.0000003f/g - 1.0f;
	}
	return factor;
}

gain_t
RouteGroup::get_max_factor(gain_t factor)
{
	gain_t g;

	for (RouteList::iterator i = routes->begin(); i != routes->end(); i++) {
		g = (*i)->amp()->gain();

		// if the current factor woulnd't raise this route above maximum
		if ( (g+g*factor) <= 1.99526231f)
			continue;

		// if route gain is already at peak, return 0.0f factor
	    if (g>=1.99526231f)
			return 0.0f;

		// factor is calculated so that it would raise current route to max
		factor = 1.99526231f/g - 1.0f;
	}

	return factor;
}

XMLNode&
RouteGroup::get_state (void)
{
	XMLNode *node = new XMLNode ("RouteGroup");
	node->add_property ("name", _name);
	node->add_property ("flags", enum_2_string (_flags));
	node->add_property ("properties", enum_2_string (_properties));

	if (!routes->empty()) {
		stringstream str;
		
		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			str << (*i)->id () << ' ';
		}

		node->add_property ("routes", str.str());
	}

	return *node;
}

int
RouteGroup::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	const XMLProperty *prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	}

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));
	}

	if ((prop = node.property ("properties")) != 0) {
		_properties = Property (string_2_enum (prop->value(), _properties));
	}

	if ((prop = node.property ("routes")) != 0) {
		stringstream str (prop->value());
		vector<string> ids;
		split (str.str(), ids, ' ');
		
		for (vector<string>::iterator i = ids.begin(); i != ids.end(); ++i) {
			PBD::ID id (*i);
			boost::shared_ptr<Route> r = _session.route_by_id (id);
			
			if (r) {
				add (r);
			} 
		}
	}

	return 0;
}

int
RouteGroup::set_state_2X (const XMLNode& node, int /*version*/)
{
	XMLProperty const * prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	}

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));
	}

	if (node.name() == "MixGroup") {
		_properties = Property (Gain | Mute | Solo | RecEnable);
	} else if (node.name() == "EditGroup") {
		_properties = Property (Select | Edit);
	}

	return 0;
}

void
RouteGroup::set_active (bool yn, void *src)
{
	if (is_active() == yn) {
		return;
	}
	if (yn) {
		_flags = Flag (_flags | Active);
	} else {
		_flags = Flag (_flags & ~Active);
	}
	_session.set_dirty ();
	FlagsChanged (src); /* EMIT SIGNAL */
}

void
RouteGroup::set_relative (bool yn, void *src)

{
	if (is_relative() == yn) {
		return;
	}
	if (yn) {
		_flags = Flag (_flags | Relative);
	} else {
		_flags = Flag (_flags & ~Relative);
	}
	_session.set_dirty ();
	FlagsChanged (src); /* EMIT SIGNAL */
}

void
RouteGroup::set_hidden (bool yn, void *src)

{
	if (is_hidden() == yn) {
		return;
	}
	if (yn) {
		_flags = Flag (_flags | Hidden);
		if (Config->get_hiding_groups_deactivates_groups()) {
			_flags = Flag (_flags & ~Active);
		}
	} else {
		_flags = Flag (_flags & ~Hidden);
		if (Config->get_hiding_groups_deactivates_groups()) {
			_flags = Flag (_flags | Active);
		}
	}
	_session.set_dirty ();
	FlagsChanged (src); /* EMIT SIGNAL */
}

void
RouteGroup::audio_track_group (set<boost::shared_ptr<AudioTrack> >& ats)
{
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack>(*i);
		if (at) {
			ats.insert (at);
		}
	}
}

void
RouteGroup::make_subgroup ()
{
	RouteList rl;
	uint32_t nin = 0;

	/* since we don't do MIDI Busses yet, check quickly ... */

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		if ((*i)->output()->n_ports().n_midi() != 0) {
			PBD::info << _("You cannot subgroup MIDI tracks at this time") << endmsg;
			return;
		}
	}

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		nin = max (nin, (*i)->output()->n_ports().n_audio());
	}

	try {
		/* use master bus etc. to determine default nouts */
		rl = _session.new_audio_route (false, nin, 2, 0, 1);
	} catch (...) {
		return;
	}

	subgroup_bus = rl.front();
	subgroup_bus->set_name (_name);

	boost::shared_ptr<Bundle> bundle = subgroup_bus->input()->bundle ();

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->output()->disconnect (this);
		(*i)->output()->connect_ports_to_bundle (bundle, this);
	}
}

void
RouteGroup::destroy_subgroup ()
{
	if (!subgroup_bus) {
		return;
	}
	
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->output()->disconnect (this);
		/* XXX find a new bundle to connect to */
	}

	_session.remove_route (subgroup_bus);
	subgroup_bus.reset ();
}
