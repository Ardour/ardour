/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#include <sstream>
#include <stdint.h>
#include "strip.h"

#include "midi++/port.h"

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/debug.h"
#include "ardour/midi_ui.h"
#include "ardour/route.h"
#include "ardour/track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/rc_configuration.h"
#include "ardour/meter.h"

#include "mackie_control_protocol.h"
#include "surface_port.h"
#include "surface.h"
#include "button.h"
#include "led.h"
#include "ledring.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"

using namespace Mackie;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

#define ui_context() MackieControlProtocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */
#define ui_bind(f, ...) boost::protect (boost::bind (f, __VA_ARGS__))

extern PBD::EventLoop::InvalidationRecord* __invalidator (sigc::trackable& trackable, const char*, int);
#define invalidator() __invalidator (*(MackieControlProtocol::instance()), __FILE__, __LINE__)

Strip::Strip (Surface& s, const std::string& name, int index, StripControlDefinition* ctls)
	: Group (name)
	, _solo (0)
	, _recenable (0)
	, _mute (0)
	, _select (0)
	, _vselect (0)
	, _fader_touch (0)
	, _vpot (0)
	, _gain (0)
	, _index (index)
	, _surface (&s)
{
	/* build the controls for this track, which will automatically add them
	   to the Group 
	*/

	for (uint32_t i = 0; ctls[i].name[0]; ++i) {
		ctls[i].factory (*_surface, ctls[i].base_id + index, ctls[i].name, *this);
	}
}	

Strip::~Strip ()
{
	
}

/**
	TODO could optimise this to use enum, but it's only
	called during the protocol class instantiation.
*/
void Strip::add (Control & control)
{
	Group::add (control);

	if  (control.name() == "gain") {
		_gain = reinterpret_cast<Fader*>(&control);
	} else if  (control.name() == "vpot") {
		_vpot = reinterpret_cast<Pot*>(&control);
	} else if  (control.name() == "recenable") {
		_recenable = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "solo") {
		_solo = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "mute") {
		_mute = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "select") {
		_select = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "vselect") {
		_vselect = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "fader_touch") {
		_fader_touch = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "meter") {
		_meter = reinterpret_cast<Meter*>(&control);
	} else if  (control.type() == Control::type_led || control.type() == Control::type_led_ring) {
		// relax
	} else {
		ostringstream os;
		os << "Strip::add: unknown control type " << control;
		throw MackieControlException (os.str());
	}
}

Fader& 
Strip::gain()
{
	if  (_gain == 0) {
		throw MackieControlException ("gain is null");
	}
	return *_gain;
}

Pot& 
Strip::vpot()
{
	if  (_vpot == 0) {
		throw MackieControlException ("vpot is null");
	}
	return *_vpot;
}

Button& 
Strip::recenable()
{
	if  (_recenable == 0) {
		throw MackieControlException ("recenable is null");
	}
	return *_recenable;
}

Button& 
Strip::solo()
{
	if  (_solo == 0) {
		throw MackieControlException ("solo is null");
	}
	return *_solo;
}
Button& 
Strip::mute()
{
	if  (_mute == 0) {
		throw MackieControlException ("mute is null");
	}
	return *_mute;
}

Button& 
Strip::select()
{
	if  (_select == 0) {
		throw MackieControlException ("select is null");
	}
	return *_select;
}

Button& 
Strip::vselect()
{
	if  (_vselect == 0) {
		throw MackieControlException ("vselect is null");
	}
	return *_vselect;
}

Button& 
Strip::fader_touch()
{
	if  (_fader_touch == 0) {
		throw MackieControlException ("fader_touch is null");
	}
	return *_fader_touch;
}

Meter& 
Strip::meter()
{
	if  (_meter == 0) {
		throw MackieControlException ("meter is null");
	}
	return *_meter;
}

std::ostream & Mackie::operator <<  (std::ostream & os, const Strip & strip)
{
	os << typeid (strip).name();
	os << " { ";
	os << "has_solo: " << boolalpha << strip.has_solo();
	os << ", ";
	os << "has_recenable: " << boolalpha << strip.has_recenable();
	os << ", ";
	os << "has_mute: " << boolalpha << strip.has_mute();
	os << ", ";
	os << "has_select: " << boolalpha << strip.has_select();
	os << ", ";
	os << "has_vselect: " << boolalpha << strip.has_vselect();
	os << ", ";
	os << "has_fader_touch: " << boolalpha << strip.has_fader_touch();
	os << ", ";
	os << "has_vpot: " << boolalpha << strip.has_vpot();
	os << ", ";
	os << "has_gain: " << boolalpha << strip.has_gain();
	os << " }";
	
	return os;
}

void
Strip::set_route (boost::shared_ptr<Route> r)
{
	route_connections.drop_connections ();

	_route = r;

	if (r) {
		
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 strip %2 now mapping route %3\n",
								   _surface->number(), _index, _route->name()));
		

		if (has_solo()) {
			_route->solo_control()->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_solo_changed, this), ui_context());
		}
		if (has_mute()) {
			_route->mute_control()->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_mute_changed, this), ui_context());
		}
		
		if (has_gain()) {
			_route->gain_control()->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_gain_changed, this, false), ui_context());
		}
		
		_route->PropertyChanged.connect (route_connections, invalidator(), ui_bind (&Strip::notify_property_changed, this, _1), ui_context());
		
		if (_route->pannable()) {
			_route->pannable()->pan_azimuth_control->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_panner_changed, this, false), ui_context());
			_route->pannable()->pan_width_control->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_panner_changed, this, false), ui_context());
		}
		
		boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<ARDOUR::Track>(_route);
	
		if (trk) {
			trk->rec_enable_control()->Changed .connect(route_connections, invalidator(), ui_bind (&Strip::notify_record_enable_changed, this), ui_context());
		}
		
		// TODO this works when a currently-banked route is made inactive, but not
		// when a route is activated which should be currently banked.
		
		_route->active_changed.connect (route_connections, invalidator(), ui_bind (&Strip::notify_active_changed, this), ui_context());
		_route->DropReferences.connect (route_connections, invalidator(), ui_bind (&Strip::notify_route_deleted, this), ui_context());
	
		// TODO
		// SelectedChanged
		// RemoteControlIDChanged. Better handled at Session level.

		/* Update */

		notify_all ();
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 strip %2 now unmapped\n",
								   _surface->number(), _index));
	}
}

void 
Strip::notify_all()
{
	if  (has_solo()) {
		notify_solo_changed ();
	}
	
	if  (has_mute()) {
		notify_mute_changed ();
	}
	
	if  (has_gain()) {
		notify_gain_changed ();
	}
	
	notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::name));
	
	if  (has_vpot()) {
		notify_panner_changed ();
	}
	
	if  (has_recenable()) {
		notify_record_enable_changed ();
	}
}

void 
Strip::notify_solo_changed ()
{
	if (_route) {
		Button& button = solo();
		_surface->write (builder.build_led (button, _route->soloed()));
	}
}

void 
Strip::notify_mute_changed ()
{
	if (_route) {
		Button & button = mute();
		_surface->write (builder.build_led (button, _route->muted()));
	}
}

void 
Strip::notify_record_enable_changed ()
{
	if (_route) {
		Button & button = recenable();
		_surface->write (builder.build_led (button, _route->record_enabled()));
	}
}

void 
Strip::notify_active_changed ()
{
	_surface->mcp().refresh_current_bank();
}

void 
Strip::notify_route_deleted ()
{
	_surface->mcp().refresh_current_bank();
}

void 
Strip::notify_gain_changed (bool force_update)
{
	if (_route) {
		Fader & fader = gain();

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("route %1 gain change, update fader %2 on port %3 in-use ? %4\n", 
								   _route->name(), 
								   fader.raw_id(),
								   _surface->port().output_port().name(),
								   fader.in_use()));
		if (!fader.in_use()) {
			float gain_value = gain_to_slider_position (_route->gain_control()->get_value());
			// check that something has actually changed
			if (force_update || gain_value != _last_gain_written) {
				_surface->write (builder.build_fader (fader, gain_value));
				_last_gain_written = gain_value;
			} else {
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader not updated because gain still equals %1\n", gain_value));
			}
		}
	}
}

void 
Strip::notify_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	if (_route) {
		string line1;
		string fullname = _route->name();
		
		if (fullname.length() <= 6) {
			line1 = fullname;
		} else {
			line1 = PBD::short_version (fullname, 6);
		}
		
		_surface->write (builder.strip_display (*_surface, *this, 0, line1));
		_surface->write (builder.strip_display_blank (*_surface, *this, 1));
	}
}

void 
Strip::notify_panner_changed (bool force_update)
{
	if (_route) {
		Pot & pot = vpot();
		boost::shared_ptr<Panner> panner = _route->panner();
		if (panner) {
			double pos = panner->position ();

			// cache the MidiByteArray here, because the mackie led control is much lower
			// resolution than the panner control. So we save lots of byte
			// sends in spite of more work on the comparison
			MidiByteArray bytes = builder.build_led_ring (pot, ControlState (on, pos), MackieMidiBuilder::midi_pot_mode_dot);
			// check that something has actually changed
			if (force_update || bytes != _last_pan_written)
			{
				_surface->write (bytes);
				_last_pan_written = bytes;
			}
		} else {
			_surface->write (builder.zero_control (pot));
		}
	}
}

bool 
Strip::handle_button (SurfacePort & port, Control & control, ButtonState bs)
{
	if (!_route) {
		// no route so always switch the light off
		// because no signals will be emitted by a non-route
		_surface->write (builder.build_led (control.led(), off));
		return false;
	}

	bool state = false;

	if (bs == press) {
		if (control.name() == "recenable") {
			state = !_route->record_enabled();
			_route->set_record_enabled (state, this);
		} else if (control.name() == "mute") {
			state = !_route->muted();
			_route->set_mute (state, this);
		} else if (control.name() == "solo") {
			state = !_route->soloed();
			_route->set_solo (state, this);
		} else if (control.name() == "select") {
			_surface->mcp().select_track (_route);
		} else if (control.name() == "vselect") {
			// TODO could be used to select different things to apply the pot to?
			//state = default_button_press (dynamic_cast<Button&> (control));
		}
	}

	if (control.name() == "fader_touch") {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader touch, press ? %1\n", (bs == press)));

		state = (bs == press);

		gain().set_in_use (state);
		
		if (ARDOUR::Config->get_mackie_emulation() == "bcf" && state) {

			/* BCF faders don't support touch, so add a timeout to reset
			   their `in_use' state.
			*/

			_surface->mcp().add_in_use_timeout (*_surface, gain(), &fader_touch());
		}
	}

	return state;
}

void
Strip::periodic ()
{
	if (!_route) {
		return;
	}

	update_automation ();
	update_meter ();
}

void 
Strip::update_automation ()
{
	ARDOUR::AutoState gain_state = _route->gain_control()->automation_state();

	if (gain_state == Touch || gain_state == Play) {
		notify_gain_changed (false);
	}

	if (_route->panner()) {
		ARDOUR::AutoState panner_state = _route->panner()->automation_state();
		if (panner_state == Touch || panner_state == Play) {
			notify_panner_changed (false);
		}
	}
}

void
Strip::update_meter ()
{
	float dB = const_cast<PeakMeter&> (_route->peak_meter()).peak_power (0);
	_surface->write (meter().update_message (dB));
}
