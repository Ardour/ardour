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
#include "ardour/panner_shell.h"
#include "ardour/rc_configuration.h"
#include "ardour/meter.h"

#include "mackie_control_protocol.h"
#include "surface_port.h"
#include "surface.h"
#include "button.h"
#include "led.h"
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

	Fader* fader;
	Pot* pot;
	Button* button;
	Meter* meter;

	if ((fader = dynamic_cast<Fader*>(&control)) != 0) {

		_gain = fader;

	} else if ((pot = dynamic_cast<Pot*>(&control)) != 0) {

		_vpot = pot;

	} else if ((button = dynamic_cast<Button*>(&control)) != 0) {

		if (control.raw_id() >= Button::recenable_base_id &&
		    control.raw_id() < Button::recenable_base_id + 8) {
			
			_recenable = button;

		} else if (control.raw_id() >= Button::mute_base_id &&
			   control.raw_id() < Button::mute_base_id + 8) {

			_mute = button;

		} else if (control.raw_id() >= Button::solo_base_id &&
			   control.raw_id() < Button::solo_base_id + 8) {

			_solo = button;

		} else if (control.raw_id() >= Button::select_base_id &&
			   control.raw_id() < Button::select_base_id + 8) {

			_select = button;

		} else if (control.raw_id() >= Button::vselect_base_id &&
			   control.raw_id() < Button::vselect_base_id + 8) {

			_vselect = button;

		} else if (control.raw_id() >= Button::fader_touch_base_id &&
			   control.raw_id() < Button::fader_touch_base_id + 8) {

			_fader_touch = button;
		}

	} else if ((meter = dynamic_cast<Meter*>(&control)) != 0) {
		_meter = meter;
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
	if (_route && _solo) {
		_surface->write (_solo->led().set_state (_route->soloed() ? on : off));
	}
}

void 
Strip::notify_mute_changed ()
{
	if (_route && _mute) {
		_surface->write (_mute->led().set_state (_route->muted() ? on : off));
	}
}

void 
Strip::notify_record_enable_changed ()
{
	if (_route && _recenable)  {
		_surface->write (_recenable->led().set_state (_route->record_enabled() ? on : off));
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

		if (!fader.in_use()) {
			float position = gain_to_slider_position (_route->gain_control()->get_value());
			// check that something has actually changed
			if (force_update || position != _last_gain_written) {
				_surface->write (fader.set_position (position));
				_last_gain_written = position;
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
		
		_surface->write (display (0, line1));
		_surface->write (blank_display (1));
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

			MidiByteArray bytes = pot.set_all (pos, true, Pot::dot);

			// check that something has actually changed
			if (force_update || bytes != _last_pan_written)
			{
				_surface->write (bytes);
				_last_pan_written = bytes;
			}
		} else {
			_surface->write (pot.zero());
		}
	}
}

void
Strip::handle_button (Button& button, ButtonState bs)
{
	button.set_in_use (bs == press);

	if (!_route) {
		// no route so always switch the light off
		// because no signals will be emitted by a non-route
		_surface->write (button.led().set_state  (off));
		return;
	}

	if (bs == press) {
		if (button.raw_id() >= Button::recenable_base_id &&
		    button.raw_id() < Button::recenable_base_id + 8) {

			_route->set_record_enabled (!_route->record_enabled(), this);

		} else if (button.raw_id() >= Button::mute_base_id &&
			   button.raw_id() < Button::mute_base_id + 8) {

			_route->set_mute (!_route->muted(), this);

		} else if (button.raw_id() >= Button::solo_base_id &&
			   button.raw_id() < Button::solo_base_id + 8) {

			_route->set_solo (!_route->soloed(), this);

		} else if (button.raw_id() >= Button::select_base_id &&
			   button.raw_id() < Button::select_base_id + 8) {

			_surface->mcp().select_track (_route);

		} else if (button.raw_id() >= Button::vselect_base_id &&
			   button.raw_id() < Button::vselect_base_id + 8) {

		}
	}

	if (button.raw_id() >= Button::fader_touch_base_id &&
	    button.raw_id() < Button::fader_touch_base_id + 8) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader touch, press ? %1\n", (bs == press)));

		bool state = (bs == press);

		_gain->set_in_use (state);
		
		if (ARDOUR::Config->get_mackie_emulation() == "bcf" && state) {

			/* BCF faders don't support touch, so add a timeout to reset
			   their `in_use' state.
			*/

			_surface->mcp().add_in_use_timeout (*_surface, gain(), &fader_touch());
		}
	}
}

void
Strip::handle_fader (Fader& fader, float position)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader to %1\n", position));

	if (_route) {
		_route->gain_control()->set_value (slider_position_to_gain (position));
	}
	
	if (ARDOUR::Config->get_mackie_emulation() == "bcf") {
		/* reset the timeout while we're still moving the fader */
		_surface->mcp().add_in_use_timeout (*_surface, fader, fader.in_use_touch_control);
	}
	
	// must echo bytes back to slider now, because
	// the notifier only works if the fader is not being
	// touched. Which it is if we're getting input.

	_surface->write (fader.set_position (position));
}

void
Strip::handle_pot (Pot& pot, ControlState& state)
{
	if (!_route) {
		_surface->write (pot.set_onoff (false));
		return;
	}

	boost::shared_ptr<Pannable> pannable = _route->pannable();

	if (pannable) {
		boost::shared_ptr<AutomationControl> ac;
		
		if (_surface->mcp().modifier_state() & MackieControlProtocol::MODIFIER_CONTROL) {
			ac = pannable->pan_width_control;
		} else {
			ac = pannable->pan_azimuth_control;
		}
		
		double p = ac->get_value();
                
		// calculate new value, and adjust
		p += state.delta * state.sign;
		p = min (1.0, p);
		p = max (0.0, p);

		ac->set_value (p);
	}
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

MidiByteArray
Strip::zero ()
{
	MidiByteArray retval;

	for (Group::Controls::const_iterator it = _controls.begin(); it != _controls.end(); ++it) {
		retval << (*it)->zero ();
	}

	retval << blank_display (0);
	retval << blank_display (1);
	
	return retval;
}

MidiByteArray
Strip::blank_display (uint32_t line_number)
{
	return display (line_number, string());
}

MidiByteArray
Strip::display (uint32_t line_number, const std::string& line)
{
	assert (line_number <= 1);

	MidiByteArray retval;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip_display index: %1, line %2 = %3\n", _index, line_number, line));

	// sysex header
	retval << _surface->sysex_hdr();
	
	// code for display
	retval << 0x12;
	// offset (0 to 0x37 first line, 0x38 to 0x6f for second line)
	retval << (_index * 7 + (line_number * 0x38));
	
	// ascii data to display
	retval << line;
	// pad with " " out to 6 chars
	for (int i = line.length(); i < 6; ++i) {
		retval << ' ';
	}
	
	// column spacer, unless it's the right-hand column
	if (_index < 7) {
		retval << ' ';
	}

	// sysex trailer
	retval << MIDI::eox;
	
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieMidiBuilder::strip_display midi: %1\n", retval));

	return retval;
}
