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
#ifndef route_signal_h
#define route_signal_h

#include <sigc++/sigc++.h>

#include <vector>

class MackieControlProtocol;

namespace ARDOUR {
	class Route;
}
	
namespace Mackie
{

class Strip;
class MackiePort;

/**
  This class is intended to easily create and destroy the set of
  connections from a route to a control surface strip. Instantiating
  it will connect the signals, and destructing it will disconnect
  the signals.
*/
class RouteSignal
{
public:
	RouteSignal( ARDOUR::Route & route, MackieControlProtocol & mcp, Strip & strip, MackiePort & port )
	: _route( route ), _mcp( mcp ), _strip( strip ), _port( port ), _last_gain_written(0.0), _last_pan_written(0.0)
	{
		connect();
	}
	
	~RouteSignal()
	{
		disconnect();
	}
	
	void connect();
	void disconnect();
	
	// call all signal handlers manually
	void notify_all();
	
	const ARDOUR::Route & route() const { return _route; }
	Strip & strip() { return _strip; }
	MackiePort & port() { return _port; }
	
	float last_gain_written() const { return _last_gain_written; }
	void last_gain_written( float other ) { _last_gain_written = other; }
	
	float last_pan_written() const { return _last_pan_written; }
	void last_pan_written( float other ) { _last_pan_written = other; }
	
private:
	ARDOUR::Route & _route;
	MackieControlProtocol & _mcp;
	Strip & _strip;
	MackiePort & _port;	

	typedef std::vector<sigc::connection> Connections;
	Connections _connections;

	// Last written values for the gain and pan, to avoid overloading
	// the midi connection to the surface
	float _last_gain_written;
	float _last_pan_written;
};

}

#endif
