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
	, _fader (0)
	, _index (index)
	, _surface (&s)
	, _controls_locked (false)
	, _last_gain_position_written (-1.0)
	, _last_pan_position_written (-1.0)
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

		_fader = fader;

	} else if ((pot = dynamic_cast<Pot*>(&control)) != 0) {

		_vpot = pot;

	} else if ((button = dynamic_cast<Button*>(&control)) != 0) {

		if (control.id() >= Button::recenable_base_id &&
		    control.id() < Button::recenable_base_id + 8) {
			
			_recenable = button;

		} else if (control.id() >= Button::mute_base_id &&
			   control.id() < Button::mute_base_id + 8) {

			_mute = button;

		} else if (control.id() >= Button::solo_base_id &&
			   control.id() < Button::solo_base_id + 8) {

			_solo = button;

		} else if (control.id() >= Button::select_base_id &&
			   control.id() < Button::select_base_id + 8) {

			_select = button;

		} else if (control.id() >= Button::vselect_base_id &&
			   control.id() < Button::vselect_base_id + 8) {

			_vselect = button;

		} else if (control.id() >= Button::fader_touch_base_id &&
			   control.id() < Button::fader_touch_base_id + 8) {

			_fader_touch = button;
		}

	} else if ((meter = dynamic_cast<Meter*>(&control)) != 0) {
		_meter = meter;
	}
}

void
Strip::set_route (boost::shared_ptr<Route> r)
{
	if (_controls_locked) {
		return;
	}

	route_connections.drop_connections ();

	_route = r;

	if (r) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 strip %2 now mapping route %3\n",
								   _surface->number(), _index, _route->name()));
		

		if (_solo) {
			_solo->set_normal_control (_route->solo_control());
			_solo->set_modified_control (boost::shared_ptr<AutomationControl>());
			_route->solo_control()->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_solo_changed, this), ui_context());
		}

		if (_mute) {
			_mute->set_normal_control (_route->mute_control());
			_mute->set_modified_control (boost::shared_ptr<AutomationControl>());
			_route->mute_control()->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_mute_changed, this), ui_context());
		}
		
		_route->gain_control()->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_gain_changed, this, false), ui_context());
		
		_route->PropertyChanged.connect (route_connections, invalidator(), ui_bind (&Strip::notify_property_changed, this, _1), ui_context());
		
		if (_route->pannable()) {
			_route->pannable()->pan_azimuth_control->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_panner_changed, this, false), ui_context());
			_route->pannable()->pan_width_control->Changed.connect(route_connections, invalidator(), ui_bind (&Strip::notify_panner_changed, this, false), ui_context());
		}

		/* bind fader & pan pot, as appropriate for current flip mode */

		flip_mode_changed (false);
		
		boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<ARDOUR::Track>(_route);
	
		if (trk) {
			_recenable->set_normal_control (trk->rec_enable_control());
			_recenable->set_modified_control (boost::shared_ptr<AutomationControl>());
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
	notify_solo_changed ();
	notify_mute_changed ();
	notify_gain_changed ();
	notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::name));
	notify_panner_changed ();
	notify_record_enable_changed ();
}

void 
Strip::notify_solo_changed ()
{
	if (_route && _solo) {
		_surface->write (_solo->set_state (_route->soloed() ? on : off));
	}
}

void 
Strip::notify_mute_changed ()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Strip %1 mute changed\n", _index));
	if (_route && _mute) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("\troute muted ? %1\n", _route->muted()));
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("mute message: %1\n", _mute->set_state (_route->muted() ? on : off)));

		_surface->write (_mute->set_state (_route->muted() ? on : off));
	}
}

void 
Strip::notify_record_enable_changed ()
{
	if (_route && _recenable)  {
		_surface->write (_recenable->set_state (_route->record_enabled() ? on : off));
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
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("gain changed for strip %1, flip mode %2\n", _index, _surface->mcp().flip_mode()));

	if (_route) {
		
		Control* control;

		if (_surface->mcp().flip_mode()) {
			control = _vpot;
		} else {
			control = _fader;
		}

		if (!control->in_use()) {

			float pos = _route->gain_control()->internal_to_interface (_route->gain_control()->get_value());

			if (force_update || pos != _last_gain_position_written) {

				if (_surface->mcp().flip_mode()) {
					_surface->write (_vpot->set_all (pos, true, Pot::wrap));
				} else {
					_surface->write (_fader->set_position (pos));
				}
				_last_gain_position_written = pos;

			} else {
				DEBUG_TRACE (DEBUG::MackieControl, "value is stale, no message sent\n");
			}
		} else {
			DEBUG_TRACE (DEBUG::MackieControl, "fader in use, no message sent\n");
		}
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "no route or no fader\n");
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
	}
}

void 
Strip::notify_panner_changed (bool force_update)
{
	if (_route) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("pan change for strip %1\n", _index));

		boost::shared_ptr<Pannable> pannable = _route->pannable();

		if (!pannable) {
			_surface->write (_vpot->zero());
			return;
		}

		Control* control;

		if (_surface->mcp().flip_mode()) {
			control = _fader;
		} else {
			control = _vpot;
		}

		if (!control->in_use()) {
			
			double pos = pannable->pan_azimuth_control->internal_to_interface (pannable->pan_azimuth_control->get_value());
			
			if (force_update || pos != _last_pan_position_written) {
				if (_surface->mcp().flip_mode()) {
					_surface->write (_fader->set_position (pos));
				} else {
					_surface->write (_vpot->set_all (pos, true, Pot::dot));
				}
				_last_pan_position_written = pos;
			}
		}
	}
}

void
Strip::handle_button (Button& button, ButtonState bs)
{
	button.set_in_use (bs == press);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip %1 handling button %2\n", _index, button.id()));

	if (bs != press) {
		return;
	}

	int lock_mod = (MackieControlProtocol::MODIFIER_CONTROL|MackieControlProtocol::MODIFIER_SHIFT);
	int ms = _surface->mcp().modifier_state();
	bool modified = (ms & MackieControlProtocol::MODIFIER_CONTROL);

	if (button.id() >= Button::select_base_id &&
	    button.id() < Button::select_base_id + 8) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("select touch, lock ? %1\n", ((ms & lock_mod) == lock_mod) ? 1 : 0));

		if ((ms & lock_mod) == lock_mod) {
			_controls_locked = !_controls_locked;
			return;
		}
		
		if (_route) {
			_surface->mcp().select_track (_route);
		}

		return;
	}

	if (button.id() >= Button::fader_touch_base_id &&
	    button.id() < Button::fader_touch_base_id + 8) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader touch, press ? %1\n", (bs == press)));

		bool state = (bs == press);
		
		_fader->set_in_use (state);
		_fader->start_touch (_surface->mcp().transport_frame(), modified);

		if (!_surface->mcp().device_info().has_touch_sense_faders()) {
			_surface->mcp().add_in_use_timeout (*_surface, *_fader, _fader->control (modified));
		}

		return;
	}

	boost::shared_ptr<AutomationControl> control = button.control (modified);
		
	if (control) {
		if (ms & MackieControlProtocol::MODIFIER_OPTION) {
			/* reset to default/normal value */
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("reset %1 to default of %2\n", control->name(), control->normal()));
			control->set_value (control->normal());
		} else {
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("toggle %1 to default of %2\n", control->name(), control->get_value() ? 0.0 : 1.0));
			control->set_value (control->get_value() ? 0.0 : 1.0);
		}
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("button has no control at present (modified ? %1)\n", modified));
	}
}

void
Strip::handle_fader (Fader& fader, float position)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader to %1\n", position));

	bool modified = (_surface->mcp().modifier_state() & MackieControlProtocol::MODIFIER_CONTROL);

	fader.set_value (position, modified);
	fader.start_touch (_surface->mcp().transport_frame(), modified);

	if (!_surface->mcp().device_info().has_touch_sense_faders()) {
		_surface->mcp().add_in_use_timeout (*_surface, fader, fader.control (modified));
	}
	
	// must echo bytes back to slider now, because
	// the notifier only works if the fader is not being
	// touched. Which it is if we're getting input.

	_surface->write (fader.set_position (position));
}

void
Strip::handle_pot (Pot& pot, float delta)
{
	/* Pots only emit events when they move, not when they
	   stop moving. So to get a stop event, we need to use a timeout.
	*/
	
	bool modified = (_surface->mcp().modifier_state() & MackieControlProtocol::MODIFIER_CONTROL);
	pot.start_touch (_surface->mcp().transport_frame(), modified);
	_surface->mcp().add_in_use_timeout (*_surface, pot, pot.control (modified));

	double p = pot.get_value (modified);
	p += delta;
	p = min (1.0, p);
	p = max (0.0, p);
	pot.set_value (p, modified);
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
	if (_meter) {
		float dB = const_cast<PeakMeter&> (_route->peak_meter()).peak_power (0);
		_surface->write (_meter->update_message (dB));
	}
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

void
Strip::lock_controls ()
{
	_controls_locked = true;
}

void
Strip::unlock_controls ()
{
	_controls_locked = false;
}

MidiByteArray
Strip::gui_selection_changed (ARDOUR::RouteNotificationListPtr rl)
{
	for (ARDOUR::RouteNotificationList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i) == _route) {
			return _select->set_state (on);
		}
	}

	return _select->set_state (off);
}

void
Strip::flip_mode_changed (bool notify)
{
	if (!_route) {
		return;
	}

	boost::shared_ptr<Pannable> pannable = _route->pannable();

	if (_surface->mcp().flip_mode()) {

		if (pannable) {
			_fader->set_normal_control (pannable->pan_azimuth_control);
			_fader->set_modified_control (pannable->pan_width_control);
		}
		_vpot->set_normal_control (_route->gain_control());
		_vpot->set_modified_control (boost::shared_ptr<AutomationControl>());

		_surface->write (display (1, "Fader"));

	} else {

		if (pannable) {
			_vpot->set_normal_control (pannable->pan_azimuth_control);
			_vpot->set_modified_control (pannable->pan_width_control);
		}
		_fader->set_normal_control (_route->gain_control());
		_fader->set_modified_control (boost::shared_ptr<AutomationControl>());

		_surface->write (display (1, "Pan"));
	}

	if (notify) {
		notify_all ();
	}
}
