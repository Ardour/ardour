/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sstream>
#include <vector>
#include <climits>

#include <stdint.h>

#include <sys/time.h>

#include <glibmm/convert.h>

#include "midi++/port.h"

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "temporal/timeline.h"

#include "ardour/amp.h"
#include "ardour/bundle.h"
#include "ardour/debug.h"
#include "ardour/midi_ui.h"
#include "ardour/meter.h"
#include "ardour/monitor_control.h"
#include "ardour/plugin_insert.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/phase_control.h"
#include "ardour/rc_configuration.h"
#include "ardour/record_enable_control.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/send.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/track.h"
#include "ardour/midi_track.h"
#include "ardour/user_bundle.h"
#include "ardour/profile.h"
#include "ardour/value_as_string.h"

#include "us2400_control_protocol.h"
#include "surface_port.h"
#include "surface.h"
#include "strip.h"
#include "button.h"
#include "led.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;
using namespace US2400;

#ifndef timeradd /// only avail with __USE_BSD
#define timeradd(a,b,result)                         \
  do {                                               \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;    \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
    if ((result)->tv_usec >= 1000000)                \
    {                                                \
      ++(result)->tv_sec;                            \
      (result)->tv_usec -= 1000000;                  \
    }                                                \
  } while (0)
#endif

#define ui_context() US2400Protocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

Strip::Strip (Surface& s, const std::string& name, int index, const map<Button::ID,StripButtonInfo>& strip_buttons)
	: Group (name)
	, _solo (0)
	, _mute (0)
	, _select (0)
	, _fader_touch (0)
	, _vpot (0)
	, _fader (0)
	, _meter (0)
	, _index (index)
	, _surface (&s)
	, _controls_locked (false)
	, _transport_is_rolling (false)
	, _metering_active (true)
	, _pan_mode (PanAzimuthAutomation)
{
	_fader = dynamic_cast<Fader*> (Fader::factory (*_surface, index, "fader", *this));
	_vpot = dynamic_cast<Pot*> (Pot::factory (*_surface, Pot::ID + index, "vpot", *this));

	if (s.mcp().device_info().has_meters()) {
		_meter = dynamic_cast<Meter*> (Meter::factory (*_surface, index, "meter", *this));
	}

	for (map<Button::ID,StripButtonInfo>::const_iterator b = strip_buttons.begin(); b != strip_buttons.end(); ++b) {
		Button* bb = dynamic_cast<Button*> (Button::factory (*_surface, b->first, b->second.base_id + index, b->second.name, *this));
		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface %1 strip %2 new button BID %3 id %4 from base %5\n",
								   _surface->number(), index, Button::id_to_name (bb->bid()),
								   bb->id(), b->second.base_id));
	}

	_trickle_counter = 0;
}

Strip::~Strip ()
{
	/* surface is responsible for deleting all controls */
}

void
Strip::add (Control & control)
{
	Button* button;

	Group::add (control);

	/* fader, vpot, meter were all set explicitly */

	if ((button = dynamic_cast<Button*>(&control)) != 0) {
		switch (button->bid()) {
		case Button::Mute:
			_mute = button;
			break;
		case Button::Solo:
			_solo = button;
			break;
		case Button::Select:
			_select = button;
			break;
		case Button::FaderTouch:
			_fader_touch = button;
			break;
		default:
			break;
		}
	}
}

void
Strip::set_stripable (boost::shared_ptr<Stripable> r, bool /*with_messages*/)
{
	if (_controls_locked) {
		return;
	}

	stripable_connections.drop_connections ();

	_solo->set_control (boost::shared_ptr<AutomationControl>());
	_mute->set_control (boost::shared_ptr<AutomationControl>());
	_select->set_control (boost::shared_ptr<AutomationControl>());

	_fader->set_control (boost::shared_ptr<AutomationControl>());
	_vpot->set_control (boost::shared_ptr<AutomationControl>());

	_stripable = r;

	mark_dirty ();

	if (!r) {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface %1 Strip %2 mapped to null route\n", _surface->number(), _index));
		zero ();
		return;
	}

	DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface %1 strip %2 now mapping stripable %3\n",
							   _surface->number(), _index, _stripable->name()));

	_solo->set_control (_stripable->solo_control());
	_mute->set_control (_stripable->mute_control());

	_stripable->solo_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_solo_changed, this), ui_context());
	_stripable->mute_control()->Changed.connect(stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_mute_changed, this), ui_context());

	boost::shared_ptr<AutomationControl> pan_control = _stripable->pan_azimuth_control();
	if (pan_control) {
		pan_control->Changed.connect(stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_panner_azi_changed, this, false), ui_context());
	}

	pan_control = _stripable->pan_width_control();
	if (pan_control) {
		pan_control->Changed.connect(stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_panner_width_changed, this, false), ui_context());
	}

	_stripable->gain_control()->Changed.connect(stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_gain_changed, this, false), ui_context());
	_stripable->PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_property_changed, this, _1), ui_context());
	_stripable->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_property_changed, this, _1), ui_context());

	// TODO this works when a currently-banked stripable is made inactive, but not
	// when a stripable is activated which should be currently banked.

	_stripable->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_stripable_deleted, this), ui_context());

	/* setup legal VPot modes for this stripable */

	possible_pot_parameters.clear();

	if (_stripable->pan_azimuth_control()) {
		possible_pot_parameters.push_back (PanAzimuthAutomation);
	}
	if (_stripable->pan_width_control()) {
		possible_pot_parameters.push_back (PanWidthAutomation);
	}
	if (_stripable->pan_elevation_control()) {
		possible_pot_parameters.push_back (PanElevationAutomation);
	}
	if (_stripable->pan_frontback_control()) {
		possible_pot_parameters.push_back (PanFrontBackAutomation);
	}
	if (_stripable->pan_lfe_control()) {
		possible_pot_parameters.push_back (PanLFEAutomation);
	}

	_pan_mode = PanAzimuthAutomation;

	if (_surface->mcp().subview_mode() == US2400Protocol::None) {
		set_vpot_parameter (_pan_mode);
	}

	_fader->set_control (_stripable->gain_control());

	notify_all ();
}

void
Strip::reset_stripable ()
{
	stripable_connections.drop_connections ();

	_solo->set_control (boost::shared_ptr<AutomationControl>());
	_mute->set_control (boost::shared_ptr<AutomationControl>());
	_select->set_control (boost::shared_ptr<AutomationControl>());

	_fader->reset_control ();
	_vpot->reset_control ();

	_stripable.reset();

	mark_dirty ();

	notify_all ();
}


void
Strip::notify_all()
{
#if 0
	if (!_stripable) {
		zero ();
		return;
	}
#endif
	// The active V-pot control may not be active for this strip
	// But if we zero it in the controls function it may erase
	// the one we do want
#if 0
	_surface->write (_vpot->zero());
#endif

	notify_solo_changed ();
	notify_mute_changed ();
	notify_gain_changed ();
	notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::name));
	notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::selected));
	notify_panner_azi_changed ();
	notify_vpot_change ();
	notify_panner_width_changed ();
	notify_record_enable_changed ();
#if 0
	notify_processor_changed ();
#endif
}

void
Strip::notify_solo_changed ()
{
#if 0
	if (_stripable && _solo) {
		_surface->write (_solo->set_state (_stripable->solo_control()->soloed() ? on : off));
	}
#endif

	_solo->mark_dirty ();
	_trickle_counter = 0;
}

void
Strip::notify_mute_changed ()
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("Strip %1 mute changed\n", _index));
#if 0
	if (_stripable && _mute) {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("\tstripable muted ? %1\n", _stripable->mute_control()->muted()));
		DEBUG_TRACE (DEBUG::US2400, string_compose ("mute message: %1\n", _mute->set_state (_stripable->mute_control()->muted() ? on : off)));

		_surface->write (_mute->set_state (_stripable->mute_control()->muted() ? on : off));
	} else {
		_surface->write (_mute->zero());
	}
#endif

	_mute->mark_dirty ();
	_trickle_counter = 0;
}

void
Strip::notify_record_enable_changed ()
{
}

void
Strip::notify_stripable_deleted ()
{
	_surface->mcp().notify_stripable_removed ();
	_surface->mcp().refresh_current_bank();
}

void
Strip::notify_gain_changed (bool force_update)
{
	_fader->mark_dirty();
	_trickle_counter = 0;
}

void
Strip::notify_processor_changed (bool force_update)
{
}

void
Strip::notify_property_changed (const PropertyChange& what_changed)
{
}

void
Strip::update_selection_state ()
{
	_select->mark_dirty ();
	_trickle_counter = 0;
#if 0
	if(_stripable) {
		_surface->write (_select->set_state (_stripable->is_selected()));
	}
#endif
}

void
Strip::show_stripable_name ()
{
}

void
Strip::notify_vpot_change ()
{
	_vpot->mark_dirty();
	_trickle_counter = 0;
}

void
Strip::notify_panner_azi_changed (bool force_update)
{
	_vpot->mark_dirty();
	_trickle_counter = 0;
}

void
Strip::notify_panner_width_changed (bool force_update)
{
	_trickle_counter = 0;
}

void
Strip::select_event (Button&, ButtonState bs)
{
	DEBUG_TRACE (DEBUG::US2400, "select button\n");

	if (bs == press) {

		int ms = _surface->mcp().main_modifier_state();

		if (ms & US2400Protocol::MODIFIER_CMDALT) {
			_controls_locked = !_controls_locked;
			return;
		}

		DEBUG_TRACE (DEBUG::US2400, "add select button on press\n");
		_surface->mcp().add_down_select_button (_surface->number(), _index);
		_surface->mcp().select_range (_surface->mcp().global_index (*this));

	} else {
		DEBUG_TRACE (DEBUG::US2400, "remove select button on release\n");
		_surface->mcp().remove_down_select_button (_surface->number(), _index);
	}

	_trickle_counter = 0;
}

void
Strip::vselect_event (Button&, ButtonState bs)
{
}

void
Strip::fader_touch_event (Button&, ButtonState bs)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("fader touch, press ? %1\n", (bs == press)));

	if (bs == press) {

		boost::shared_ptr<AutomationControl> ac = _fader->control ();

		_fader->set_in_use (true);
		_fader->start_touch (timepos_t (_surface->mcp().transport_sample()));

	} else {

		_fader->set_in_use (false);
		_fader->stop_touch (timepos_t (_surface->mcp().transport_sample()));

	}
}


void
Strip::handle_button (Button& button, ButtonState bs)
{
	boost::shared_ptr<AutomationControl> control;

	if (bs == press) {
		button.set_in_use (true);
	} else {
		button.set_in_use (false);
	}

	DEBUG_TRACE (DEBUG::US2400, string_compose ("strip %1 handling button %2 press ? %3\n", _index, button.bid(), (bs == press)));

	switch (button.bid()) {
	case Button::Select:
		select_event (button, bs);
		break;

	case Button::FaderTouch:
		fader_touch_event (button, bs);
		break;

	default:
		if ((control = button.control ())) {
			if (bs == press) {
				DEBUG_TRACE (DEBUG::US2400, "add button on press\n");
				_surface->mcp().add_down_button ((AutomationType) control->parameter().type(), _surface->number(), _index);

				float new_value = control->get_value() ? 0.0 : 1.0;

				/* get all controls that either have their
				 * button down or are within a range of
				 * several down buttons
				 */

				US2400Protocol::ControlList controls = _surface->mcp().down_controls ((AutomationType) control->parameter().type(),
				                                                                             _surface->mcp().global_index(*this));


				DEBUG_TRACE (DEBUG::US2400, string_compose ("there are %1 buttons down for control type %2, new value = %3\n",
									    controls.size(), control->parameter().type(), new_value));

				/* apply change, with potential modifier semantics */

				Controllable::GroupControlDisposition gcd;

				if (_surface->mcp().main_modifier_state() & US2400Protocol::MODIFIER_SHIFT) {
					gcd = Controllable::InverseGroup;
				} else {
					gcd = Controllable::UseGroup;
				}

				for (US2400Protocol::ControlList::iterator c = controls.begin(); c != controls.end(); ++c) {
					(*c)->set_value (new_value, gcd);
				}

			} else {
				DEBUG_TRACE (DEBUG::US2400, "remove button on release\n");
				_surface->mcp().remove_down_button ((AutomationType) control->parameter().type(), _surface->number(), _index);
			}
		}
		break;
	}
}


void
Strip::handle_fader_touch (Fader& fader, bool touch_on)
{
	if (touch_on) {
		fader.start_touch (timepos_t (_surface->mcp().transport_sample()));
	} else {
		fader.stop_touch (timepos_t (_surface->mcp().transport_sample()));
	}
}

void
Strip::handle_fader (Fader& fader, float position)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("fader to %1\n", position));
	boost::shared_ptr<AutomationControl> ac = fader.control();
	if (!ac) {
		return;
	}

	Controllable::GroupControlDisposition gcd = Controllable::UseGroup;

	if (_surface->mcp().main_modifier_state() & US2400Protocol::MODIFIER_SHIFT) {
		gcd = Controllable::InverseGroup;
	}

	fader.set_value (position, gcd);

	/* From the Mackie Control MIDI implementation docs:

	   In order to ensure absolute synchronization with the host software,
	   Mackie Control uses a closed-loop servo system for the faders,
	   meaning the faders will always move to their last received position.
	   When a host receives a Fader Position Message, it must then
	   re-transmit that message to the Mackie Control or else the faders
	   will return to their last position.
	*/

	_surface->write (fader.set_position (position));
}

void
Strip::handle_pot (Pot& pot, float delta)
{
	/* Pots only emit events when they move, not when they
	   stop moving. So to get a stop event, we need to use a timeout.
	*/

	boost::shared_ptr<AutomationControl> ac = pot.control();
	if (!ac) {
		return;
	}

	Controllable::GroupControlDisposition gcd;

	if (_surface->mcp().main_modifier_state() & US2400Protocol::MODIFIER_SHIFT) {
		gcd = Controllable::InverseGroup;
	} else {
		gcd = Controllable::UseGroup;
	}

	if (ac->toggled()) {

		/* make it like a single-step, directional switch */

		if (delta > 0) {
			ac->set_value (1.0, gcd);
		} else {
			ac->set_value (0.0, gcd);
		}

	} else if (ac->desc().enumeration || ac->desc().integer_step) {

		/* use Controllable::get_value() to avoid the
		 * "scaling-to-interface" that takes place in
		 * Control::get_value() via the pot member.
		 *
		 * an enumeration with 4 values will have interface values of
		 * 0.0, 0.25, 0.5 and 0.75 or some similar oddness. Lets not
		 * deal with that.
		 */

		if (delta > 0) {
			ac->set_value (min (ac->upper(), ac->get_value() + 1.0), gcd);
		} else {
			ac->set_value (max (ac->lower(), ac->get_value() - 1.0), gcd);
		}

	} else {
		ac->set_interface ((ac->internal_to_interface (ac->get_value(), true) + delta), true, gcd);
	}
}

void
Strip::periodic (PBD::microseconds_t now)
{

	update_meter ();

	if ( _trickle_counter %24 == 0 ) {

		if ( _fader->control() ) {
			_surface->write (_fader->set_position (_fader->control()->internal_to_interface (_fader->control()->get_value ())));
		} else {
			_surface->write (_fader->set_position(0.0));
		}

		bool showing_pan = false;
		if (_pan_mode >= PanAzimuthAutomation && _pan_mode <= PanLFEAutomation) {
			showing_pan = true;
		}
		if (_pan_mode == SendAzimuthAutomation) {
			showing_pan = true;
		}

		if ( _vpot->control() ) {
			_surface->write (_vpot->set (_vpot->control()->internal_to_interface (_vpot->control()->get_value (), showing_pan ? true: false), true));
		} else {
			_surface->write (_vpot->set(0.0, false));
		}

		if (_stripable) {
			_surface->write (_solo->set_state (_stripable->solo_control()->soloed() ? on : off));
			_surface->write (_mute->set_state (_stripable->mute_control()->muted() ? on : off));
			_surface->write (_select->set_state (_stripable->is_selected()));
		} else {
			_surface->write (_solo->set_state (off));
			_surface->write (_mute->set_state (off));
			_surface->write (_select->set_state (off));
		}

	}

	//after a hard write, queue us for trickling data later
	if (_trickle_counter == 0)
		_trickle_counter = global_index()+1;

	_trickle_counter++;

}

void
Strip::redisplay (PBD::microseconds_t now, bool force)
{
}

void
Strip::update_automation ()
{
}

void
Strip::update_meter ()
{
	if (!_stripable) {
		return;
	}

	if (_meter && _transport_is_rolling && _metering_active && _stripable->peak_meter()) {
		float dB = _stripable->peak_meter()->meter_level (0, MeterMCP);
		_meter->send_update (*_surface, dB);
		return;
	}
}

void
Strip::zero ()
{
	_trickle_counter = 0;
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

string
Strip::vpot_mode_string ()
{
	return "???";
}

void
Strip::next_pot_mode ()
{
	vector<AutomationType>::iterator i;

	boost::shared_ptr<AutomationControl> ac = _vpot->control();

	if (!ac) {
		return;
	}


	if (_surface->mcp().subview_mode() != US2400Protocol::None) {
		return;
	}

	if (possible_pot_parameters.empty() || (possible_pot_parameters.size() == 1 && possible_pot_parameters.front() == ac->parameter().type())) {
		return;
	}

	for (i = possible_pot_parameters.begin(); i != possible_pot_parameters.end(); ++i) {
		if ((*i) == ac->parameter().type()) {
			break;
		}
	}

	/* move to the next mode in the list, or back to the start (which will
	   also happen if the current mode is not in the current pot mode list)
	*/

	if (i != possible_pot_parameters.end()) {
		++i;
	}

	if (i == possible_pot_parameters.end()) {
		i = possible_pot_parameters.begin();
	}

	set_vpot_parameter (*i);
}

void
/*
 *
 * name: Strip::subview_mode_changed
 * @param
 * @return
 *
 */
Strip::subview_mode_changed ()
{
	switch (_surface->mcp().subview_mode()) {

	case US2400Protocol::None:
		set_vpot_parameter (_pan_mode);
		notify_metering_state_changed ();
		break;

	case US2400Protocol::TrackView:
		boost::shared_ptr<Stripable> r = _surface->mcp().subview_stripable();
		if (r) {
			DEBUG_TRACE (DEBUG::US2400, string_compose("subview_mode_changed strip %1:%2- assigning trackview pot\n",  _surface->number(), _index));
			setup_trackview_vpot (r);
		} else {
			DEBUG_TRACE (DEBUG::US2400, string_compose("subview_mode_changed strip %1:%2 - no stripable\n",  _surface->number(), _index));
		}
		break;

	}

	_trickle_counter = 0;
}

void
Strip::setup_dyn_vpot (boost::shared_ptr<Stripable> r)
{
}

void
Strip::setup_eq_vpot (boost::shared_ptr<Stripable> r)
{
}

void
Strip::setup_sends_vpot (boost::shared_ptr<Stripable> r)
{

}

void
Strip::setup_trackview_vpot (boost::shared_ptr<Stripable> r)
{
	subview_connections.drop_connections ();

	if (!r) {
		return;
	}


	boost::shared_ptr<AutomationControl> pc;
	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (r);
	string label;

	_vpot->set_mode(Pot::wrap);

#ifdef MIXBUS
	const uint32_t global_pos = _surface->mcp().global_index (*this);

	//Trim & dynamics
	switch (global_pos) {
	case 0:
		pc = r->trim_control ();
		_vpot->set_mode(Pot::boost_cut);
		break;

	case 1:
		pc = r->pan_azimuth_control ();
		_vpot->set_mode(Pot::dot);
		break;

	case 2:
		pc = r->comp_threshold_controllable();
		break;

	case 3:
		pc = r->comp_speed_controllable();
		break;

	case 4:
		pc = r->comp_mode_controllable();
		_vpot->set_mode(Pot::wrap);
		break;

	case 5:
		pc = r->comp_makeup_controllable();
		break;


	}  //trim & dynamics


	//EQ
	int eq_band = -1;
	if (r->mixbus () || r->is_master()) {

		switch (global_pos) {

			case 6:
				pc = r->pan_width_control();
				break;

			case 7:
				pc = r->tape_drive_controllable();
				break;

			case 8:
			case 9:
			case 10:
				eq_band = (global_pos-8);
				pc = r->eq_gain_controllable (eq_band);
				_vpot->set_mode(Pot::boost_cut);
				break;
		}

	} else if (r->is_input_strip ()) {

#ifdef MIXBUS32C
		switch (global_pos) {
			case 6:
				pc = r->filter_freq_controllable(true);
				break;
			case 7:
				pc = r->filter_freq_controllable(false);
				break;
			case 8:
			case 10:
			case 12:
			case 14: {
				eq_band = (global_pos-8) / 2;
				pc = r->eq_freq_controllable (eq_band);
				} break;
			case 9:
			case 11:
			case 13:
			case 15: {
				eq_band = (global_pos-8) / 2;
				pc = r->eq_gain_controllable (eq_band);
				_vpot->set_mode(Pot::boost_cut);
				} break;
		}

#else  //regular Mixbus channel EQ

		switch (global_pos) {
			case 7:
				pc = r->filter_freq_controllable(true);
				break;
			case 8:
			case 10:
			case 12:
				eq_band = (global_pos-8) / 2;
				pc = r->eq_gain_controllable (eq_band);
				_vpot->set_mode(Pot::boost_cut);
				break;
			case 9:
			case 11:
			case 13:
				eq_band = (global_pos-8) / 2;
				pc = r->eq_freq_controllable (eq_band);
				break;
		}


#endif

		//mixbus sends
		switch (global_pos) {
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
			pc = r->send_level_controllable ( global_pos - 16 );
			break;
		}  //global_pos switch

	} //if input_strip
#endif //ifdef MIXBUS

	if (pc) {  //control found; set our knob to watch for changes in it
		_vpot->set_control (pc);
		pc->Changed.connect (subview_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_vpot_change, this), ui_context());
	} else {  //no control, just set the knob to "empty"
		_vpot->reset_control ();
	}

	notify_vpot_change ();
}

void
Strip::set_vpot_parameter (AutomationType p)
{
	if (!_stripable || (p == NullAutomation)) {
		_vpot->set_control (boost::shared_ptr<AutomationControl>());
		return;
	}

	boost::shared_ptr<AutomationControl> pan_control;

	DEBUG_TRACE (DEBUG::US2400, string_compose ("switch to vpot mode %1\n", p));

	mark_dirty ();

	switch (p) {
	case PanAzimuthAutomation:
		pan_control = _stripable->pan_azimuth_control ();
		break;
	case PanWidthAutomation:
		pan_control = _stripable->pan_width_control ();
		break;
	case PanElevationAutomation:
		break;
	case PanFrontBackAutomation:
		break;
	case PanLFEAutomation:
		break;
	default:
		return;
	}

	if (pan_control) {
		_pan_mode = p;
		_vpot->set_mode (Pot::dot);
		_vpot->set_control (pan_control);
	}

	notify_panner_azi_changed (true);
}

bool
Strip::is_midi_track () const
{
	return boost::dynamic_pointer_cast<MidiTrack>(_stripable) != 0;
}

void
Strip::mark_dirty ()
{
	_fader->mark_dirty();
	_vpot->mark_dirty();
	_solo->mark_dirty();
	_mute->mark_dirty();
	_trickle_counter=0;
}

void
Strip::notify_metering_state_changed()
{
	if (_surface->mcp().subview_mode() != US2400Protocol::None) {
		return;
	}

	if (!_stripable || !_meter) {
		return;
	}

	bool transport_is_rolling = (_surface->mcp().get_transport_speed () != 0.0f);
	bool metering_active = _surface->mcp().metering_active ();

	if ((_transport_is_rolling == transport_is_rolling) && (_metering_active == metering_active)) {
		return;
	}

	_meter->notify_metering_state_changed (*_surface, transport_is_rolling, metering_active);

	if (!transport_is_rolling || !metering_active) {
		notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::name));
		notify_panner_azi_changed (true);
	}

	_transport_is_rolling = transport_is_rolling;
	_metering_active = metering_active;
}
