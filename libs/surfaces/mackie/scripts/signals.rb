#~ /usr/bin/ruby
# Copyright (C) 2006,2007 John Anderson

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

require 'erb'

signals = %w{
solo_changed
mute_changed
record_enable_changed
gain_changed
name_changed
panner_changed
}

@signal_calls = { 'panner_changed' => 'panner()[0]->Changed' }

def connection_call( x )
  if @signal_calls.include? x
    @signal_calls[x]
  else
    x
  end
end

signals.each do |x|
	puts <<EOF
void MackieControlProtocol::notify_#{x}( void *, ARDOUR::Route * route )
{
	try
	{
		strip_from_route( route ).#{x.gsub( /_changed/, '' )}();
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

EOF
end

class_def = <<EOF
#ifndef route_signal_h
#define route_signal_h

#include <sigc++/sigc++.h>

class MackieControlProtocol;

namespace ARDOUR {
	class Route;
}
	
namespace Mackie
{

class Strip;

/**
  This class is intended to easily create and destroy the set of
  connections from a route to a control surface strip. Instanting
  it will connect the signals, and destructing it will disconnect
  the signals.
*/
class RouteSignal
{
public:
	RouteSignal( ARDOUR::Route & route, MackieControlProtocol & mcp, Strip & strip )
	: _route( route ), _mcp( mcp ), _strip( strip )
	{
		connect();
	}
	
	~RouteSignal()
	{
		disconnect();
	}
	
private:
	ARDOUR::Route & _route;
	MackieControlProtocol & _mcp;
	Strip & _strip;
	
<% signals.each do |x| -%>
	sigc::connection _<%= x %>_connection;
<% end -%>
};

}

#endif
EOF

erb = ERB.new( class_def, 0, ">-" )
erb.run

impl_def = <<EOF
#include "route_signal.h"

#include <ardour/route.h>
#include <ardour/panner.h>

#include "mackie_control_protocol.h"

using namespace Mackie;

void RouteSignal::connect()
{
<% signals.each do |x| -%>
	_<%=x%>_connection = _route.<%=connection_call(x)%>.connect( sigc::bind ( mem_fun ( _mcp, &MackieControlProtocol::notify_<%=x%> ), &_route ) );
<% end -%>
}

void RouteSignal::disconnect()
{
<% signals.each do |x| -%>
  _<%= x %>_connection.disconnect();
<% end -%>
}
EOF

erb = ERB.new( impl_def, 0, ">-" )
erb.run
