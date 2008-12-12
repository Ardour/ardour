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

#include <ardour/route.h>
#include <ardour/track.h>
#include <ardour/panner.h>

#include "mackie_control_protocol.h"

#include <stdexcept>

using namespace Mackie;
using namespace std;

void RouteSignal::connect()
{
	back_insert_iterator<Connections> cins = back_inserter( _connections );
	
	if ( _strip.has_solo() )
		cins = _route.solo_control()->Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_solo_changed ), this ) );
	
	if ( _strip.has_mute() )
		cins = _route.mute_control()->Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_mute_changed ), this ) );
	
	if ( _strip.has_gain() )
		cins = _route.gain_control()->Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_gain_changed ), this, true ) );
		
	cins = _route.NameChanged.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_name_changed ), this ) );
	
	cins = _route.panner().Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_panner_changed ), this, true ) );
	for ( unsigned int i = 0; i < _route.panner().npanners(); ++i )
	{
		cins = _route.panner().streampanner (i).Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_panner_changed ), this, true ) );
	}
	
	try
	{
		cins = dynamic_cast<ARDOUR::Track&>( _route )
			.rec_enable_control()
			->Changed
			.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_record_enable_changed ), this ) )
		;
	}
	catch ( std::bad_cast & )
	{
		// this should catch the dynamic_cast to Track, if what we're working
		// with can't be record-enabled
	}

	// TODO this works when a currently-banked route is made inactive, but not
	// when a route is activated which should be currently banked.
	cins = _route.active_changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_active_changed ), this ) );
	
	// TODO
	// SelectedChanged
	// RemoteControlIDChanged. Better handled at Session level.
}

void RouteSignal::disconnect()
{
	for ( Connections::iterator it = _connections.begin(); it != _connections.end(); ++it )
	{
		it->disconnect();
	}
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
