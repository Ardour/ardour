/*
	Copyright (C) 2006,2007 John Anderson

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
#include "route_signal.h"

#include "ardour/route.h"
#include "ardour/track.h"
#include "ardour/panner.h"

#include "mackie_control_protocol.h"

#include <stdexcept>

using namespace ARDOUR;
using namespace Mackie;
using namespace std;

void RouteSignal::connect()
{
	if (_strip.has_solo()) {
		_route->solo_control()->Changed.connect(connections, boost::bind (&MackieControlProtocol::notify_solo_changed, &_mcp, this));
	}

	if (_strip.has_mute()) {
		_route->mute_control()->Changed.connect(connections, boost::bind (&MackieControlProtocol::notify_mute_changed, &_mcp, this));
	}

	if (_strip.has_gain()) {
		_route->gain_control()->Changed.connect(connections, boost::bind (&MackieControlProtocol::notify_gain_changed, &_mcp, this, false));
	}

	_route->NameChanged.connect (connections, boost::bind (&MackieControlProtocol::notify_name_changed, &_mcp, this));
	
	if (_route->panner()) {
		_route->panner()->Changed.connect(connections, boost::bind (&MackieControlProtocol::notify_panner_changed, &_mcp, this, false));
		
		for ( unsigned int i = 0; i < _route->panner()->npanners(); ++i ) {
			_route->panner()->streampanner(i).Changed.connect (connections, boost::bind (&MackieControlProtocol::notify_panner_changed, &_mcp, this, false));
		}
	}
	
	boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<ARDOUR::Track>(_route);
	if (trk) {
		trk->rec_enable_control()->Changed .connect(connections, boost::bind (&MackieControlProtocol::notify_record_enable_changed, &_mcp, this));
	}
	
	// TODO this works when a currently-banked route is made inactive, but not
	// when a route is activated which should be currently banked.
	_route->active_changed.connect (connections, boost::bind (&MackieControlProtocol::notify_active_changed, &_mcp, this));
	
	// TODO
	// SelectedChanged
	// RemoteControlIDChanged. Better handled at Session level.
}

void RouteSignal::disconnect()
{
	connections.drop_connections ();
}

void RouteSignal::notify_all()
{
#ifdef DEBUG
	cout << "RouteSignal::notify_all for " << _strip << endl;
#endif
	if ( _strip.has_solo() )
		_mcp.notify_solo_changed( this );
	
	if ( _strip.has_mute() )
		_mcp.notify_mute_changed( this );
	
	if ( _strip.has_gain() )
		_mcp.notify_gain_changed( this );
	
	_mcp.notify_name_changed( this );
	
	if ( _strip.has_vpot() )
		_mcp.notify_panner_changed( this );
	
	if ( _strip.has_recenable() )
		_mcp.notify_record_enable_changed( this );
#ifdef DEBUG
	cout << "RouteSignal::notify_all finish" << endl;
#endif
}
