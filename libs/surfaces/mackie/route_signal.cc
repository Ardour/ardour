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

void RouteSignal::connect()
{
	if ( _strip.has_solo() )
		_solo_changed_connection = _route.solo_control().Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_solo_changed ), &_route, &_port ) );
	
	if ( _strip.has_mute() )
		_mute_changed_connection = _route.mute_control().Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_mute_changed ), &_route, &_port ) );
	
	if ( _strip.has_gain() )
		_gain_changed_connection = _route.gain_control().Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_gain_changed ), &_route, &_port ) );
		
	_name_changed_connection = _route.name_changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_name_changed ), &_route, &_port ) );
	
	if ( _route.panner().size() == 1 )
	{
		_panner_changed_connection = _route.panner()[0]->Changed.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_panner_changed ), &_route, &_port ) );
	}
	
	try
	{
		_record_enable_changed_connection =
			dynamic_cast<ARDOUR::Track&>( _route ).rec_enable_control().Changed
				.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_record_enable_changed ), &_route, &_port ) )
		;
	}
	catch ( std::bad_cast & )
	{
		// this should catch the dynamic_cast to Track, if what we're working
		// with can't be record-enabled
	}

	// TODO
	// active_changed
	// SelectedChanged
	// RemoteControlIDChanged
}

void RouteSignal::disconnect()
{
	_solo_changed_connection.disconnect();
	_mute_changed_connection.disconnect();
	_gain_changed_connection.disconnect();
	_name_changed_connection.disconnect();
	_panner_changed_connection.disconnect();
	_record_enable_changed_connection.disconnect();
}

void RouteSignal::notify_all()
{
	void * src = &_route;
	
	if ( _strip.has_solo() )
		_mcp.notify_solo_changed( &_route, &_port );
	
	if ( _strip.has_mute() )
		_mcp.notify_mute_changed( &_route, &_port );
	
	if ( _strip.has_gain() )
		_mcp.notify_gain_changed( &_route, &_port );
	
	_mcp.notify_name_changed( src, &_route, &_port );
	
	if ( _strip.has_vpot() )
		_mcp.notify_panner_changed( &_route, &_port );
	
	if ( _strip.has_recenable() )
		_mcp.notify_record_enable_changed( &_route, &_port );
}
