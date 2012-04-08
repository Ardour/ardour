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
#include "ardour/midi_ui.h"
#include "ardour/pannable.h"
#include "ardour/session_object.h" // for Properties::name 

#include "mackie_control_protocol.h"

#include <stdexcept>

using namespace ARDOUR;
using namespace Mackie;
using namespace std;

#define midi_ui_context() MidiControlUI::instance() /* a UICallback-derived object that specifies the event loop for signal handling */
#define ui_bind(f, ...) boost::protect (boost::bind (f, __VA_ARGS__))

void RouteSignal::connect()
{
	if (_strip.has_solo()) {
		_route->solo_control()->Changed.connect(connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_solo_changed, &_mcp, this), midi_ui_context());
	}

	if (_strip.has_mute()) {
		_route->mute_control()->Changed.connect(connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_mute_changed, &_mcp, this), midi_ui_context());
	}

	if (_strip.has_gain()) {
		_route->gain_control()->Changed.connect(connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_gain_changed, &_mcp, this, false), midi_ui_context());
	}

	_route->PropertyChanged.connect (connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_property_changed, &_mcp, _1, this), midi_ui_context());
	
	if (_route->pannable()) {
		_route->pannable()->pan_azimuth_control->Changed.connect(connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_panner_changed, &_mcp, this, false), midi_ui_context());
		_route->pannable()->pan_width_control->Changed.connect(connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_panner_changed, &_mcp, this, false), midi_ui_context());
	}
	
	boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<ARDOUR::Track>(_route);
	if (trk) {
		trk->rec_enable_control()->Changed .connect(connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_record_enable_changed, &_mcp, this), midi_ui_context());
	}
	
	// TODO this works when a currently-banked route is made inactive, but not
	// when a route is activated which should be currently banked.
	_route->active_changed.connect (connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_active_changed, &_mcp, this), midi_ui_context());

	_route->DropReferences.connect (connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::refresh_current_bank, &_mcp), midi_ui_context());
	
	// TODO
	// SelectedChanged
	// RemoteControlIDChanged. Better handled at Session level.
}

void 
RouteSignal::disconnect()
{
	connections.drop_connections ();
}

void 
RouteSignal::notify_all()
{
	if  (_strip.has_solo()) {
		_mcp.notify_solo_changed (this);
	}
	
	if  (_strip.has_mute()) {
		_mcp.notify_mute_changed (this);
	}
	
	if  (_strip.has_gain()) {
		_mcp.notify_gain_changed (this);
	}
	
	_mcp.notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::name), this);
	
	if  (_strip.has_vpot()) {
		_mcp.notify_panner_changed (this);
	}
	
	if  (_strip.has_recenable()) {
		_mcp.notify_record_enable_changed (this);
	}
}
