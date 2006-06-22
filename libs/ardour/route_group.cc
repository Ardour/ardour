/*
    Copyright (C) 2000-2002 Paul Davis 

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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <algorithm>

#include <sigc++/bind.h>

#include <pbd/error.h>

#include <ardour/route_group.h>
#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/configuration.h>

using namespace ARDOUR;
using namespace sigc;
using namespace std;

RouteGroup::RouteGroup (Session& s, const string &n, Flag f)
	: _session (s), _name (n), _flags (f) 
{
}

void
RouteGroup::set_name (string str)
{
	_name = str;
	_session.set_dirty ();
	FlagsChanged (0); /* EMIT SIGNAL */
}

int
RouteGroup::add (Route *r)
{
	routes.push_back (r);
	r->GoingAway.connect (sigc::bind (mem_fun (*this, &RouteGroup::remove_when_going_away), r));
	_session.set_dirty ();
	changed (); /* EMIT SIGNAL */
	return 0;
}

void
RouteGroup::remove_when_going_away (Route *r)
{
	remove (r);
}
    
int
RouteGroup::remove (Route *r) 
{
	list<Route *>::iterator i;

	if ((i = find (routes.begin(), routes.end(), r)) != routes.end()) {
		routes.erase (i);
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
	
	for (list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		g = (*i)->gain();

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
	
	for (list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		g = (*i)->gain();
		
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
	char buf[32];
	XMLNode *node = new XMLNode ("RouteGroup");
	node->add_property ("name", _name);
	snprintf (buf, sizeof (buf), "%" PRIu32, (uint32_t) _flags);
	node->add_property ("flags", buf);
	return *node;
}

int 
RouteGroup::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	}

	if ((prop = node.property ("flags")) != 0) {
		_flags = atoi (prop->value().c_str());
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
		_flags |= Active;
	} else {
		_flags &= ~Active;
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
		_flags |= Relative;
	} else {
		_flags &= ~Relative;
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
		_flags |= Hidden;
		if (Config->get_hiding_groups_deactivates_groups()) {
			_flags &= ~Active;
		}
	} else {
         	_flags &= ~Hidden;
		if (Config->get_hiding_groups_deactivates_groups()) {
			_flags |= Active;
		}
	}
	_session.set_dirty ();
	FlagsChanged (src); /* EMIT SIGNAL */
}

void
RouteGroup::audio_track_group (set<AudioTrack*>& ats) 
{	
	for (list<Route*>::iterator i = routes.begin(); i != routes.end(); ++i) {
		AudioTrack* at = dynamic_cast<AudioTrack*>(*i);
		if (at) {
			ats.insert (at);
		}
	}
}

