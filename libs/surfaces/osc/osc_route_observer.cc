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

#include "boost/lambda/lambda.hpp"

#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "osc.h"
#include "osc_route_observer.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace PBD;
using namespace ARDOUR;
using namespace boost;

OSCRouteObserver::OSCRouteObserver (boost::shared_ptr<Route> r, lo_address a)
	: _route (r)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));
	
	_route->PropertyChanged.connect (name_changed_connection, MISSING_INVALIDATOR, boost::bind (&OSCRouteObserver::name_changed, this, boost::lambda::_1), OSC::instance());

	if (dynamic_pointer_cast<AudioTrack>(_route) || dynamic_pointer_cast<MidiTrack>(_route)) {

		boost::shared_ptr<Track> track = dynamic_pointer_cast<Track>(r);
		boost::shared_ptr<Controllable> rec_controllable = dynamic_pointer_cast<Controllable>(track->rec_enable_control());

		rec_controllable->Changed.connect (rec_changed_connection, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/route/rec"), track->rec_enable_control()), OSC::instance());
	}
	
	boost::shared_ptr<Controllable> mute_controllable = dynamic_pointer_cast<Controllable>(_route->mute_control());
	mute_controllable->Changed.connect (mute_changed_connection, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/route/mute"), _route->mute_control()), OSC::instance());

	boost::shared_ptr<Controllable> solo_controllable = dynamic_pointer_cast<Controllable>(_route->solo_control());
	solo_controllable->Changed.connect (solo_changed_connection, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/route/solo"), _route->solo_control()), OSC::instance());

	boost::shared_ptr<Controllable> gain_controllable = dynamic_pointer_cast<Controllable>(_route->gain_control());
	gain_controllable->Changed.connect (gain_changed_connection, MISSING_INVALIDATOR, bind (&OSCRouteObserver::send_change_message, this, X_("/route/gain"), _route->gain_control()), OSC::instance());
}

OSCRouteObserver::~OSCRouteObserver ()
{
	name_changed_connection.disconnect();
	rec_changed_connection.disconnect();
	mute_changed_connection.disconnect();
	solo_changed_connection.disconnect();
	gain_changed_connection.disconnect();

	lo_address_free (addr);
}

void
OSCRouteObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}
	
	if (!_route) {
		return;
	}
	
	lo_message msg = lo_message_new ();

	lo_message_add_int32 (msg, _route->remote_control_id());
	lo_message_add_string (msg, _route->name().c_str());

	lo_send_message (addr, "/route/name", msg);
	lo_message_free (msg);
}

void
OSCRouteObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	lo_message_add_int32 (msg, _route->remote_control_id());
	lo_message_add_float (msg, (float) controllable->get_value());

	/* XXX thread issues */

	//std::cerr << "ORC: send " << path << " = " << controllable->get_value() << std::endl;

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}
