/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2015-2017 Ben Loftis <ben@harrisonconsoles.com>
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

#include "mackie_control_protocol.h"
#include "subview.h"
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
using namespace Mackie;

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

#define ui_context() MackieControlProtocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

Strip::Strip (Surface& s, const std::string& name, int index, const map<Button::ID,StripButtonInfo>& strip_buttons)
	: Group (name)
	, _solo (0)
	, _recenable (0)
	, _mute (0)
	, _select (0)
	, _vselect (0)
	, _fader_touch (0)
	, _vpot (0)
	, _fader (0)
	, _meter (0)
	, _index (index)
	, _surface (&s)
	, _controls_locked (false)
	, _transport_is_rolling (false)
	, _metering_active (true)
	, _lcd2_available (true)
	, _lcd2_label_pitch (7)
	, _block_screen_redisplay_until (0)
	, return_to_vpot_mode_display_at (UINT64_MAX)
	, _pan_mode (PanAzimuthAutomation)
	, _last_gain_position_written (-1.0)
	, _last_pan_azi_position_written (-1.0)
	, _last_pan_width_position_written (-1.0)
	, _last_trim_position_written (-1.0)
{
	_fader = dynamic_cast<Fader*> (Fader::factory (*_surface, index, "fader", *this));
	_vpot = dynamic_cast<Pot*> (Pot::factory (*_surface, Pot::ID + index, "vpot", *this));

	if (s.mcp().device_info().has_meters()) {
		_meter = dynamic_cast<Meter*> (Meter::factory (*_surface, index, "meter", *this));
	}

	if (s.mcp().device_info().has_qcon_second_lcd()) {
		_lcd2_available = true;

		// The main unit has 9 faders under the second display.
		// Extenders have 8 faders.
		if (s.number() == s.mcp().device_info().master_position()) {
			_lcd2_label_pitch = 6;
		}
	}

	for (map<Button::ID,StripButtonInfo>::const_iterator b = strip_buttons.begin(); b != strip_buttons.end(); ++b) {
		Button* bb = dynamic_cast<Button*> (Button::factory (*_surface, b->first, b->second.base_id + index, b->second.name, *this));
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface %1 strip %2 new button BID %3 id %4 from base %5\n",
								   _surface->number(), index, Button::id_to_name (bb->bid()),
								   bb->id(), b->second.base_id));
	}
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
		case Button::RecEnable:
			_recenable = button;
			break;
		case Button::Mute:
			_mute = button;
			break;
		case Button::Solo:
			_solo = button;
			break;
		case Button::Select:
			_select = button;
			break;
		case Button::VSelect:
			_vselect = button;
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

	mb_pan_controllable.reset();

	stripable_connections.drop_connections ();

	_solo->set_control (boost::shared_ptr<AutomationControl>());
	_mute->set_control (boost::shared_ptr<AutomationControl>());
	_select->set_control (boost::shared_ptr<AutomationControl>());
	_recenable->set_control (boost::shared_ptr<AutomationControl>());
	_fader->set_control (boost::shared_ptr<AutomationControl>());
	_vpot->set_control (boost::shared_ptr<AutomationControl>());

	_stripable = r;

	reset_saved_values ();

	if (!r) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 Strip %2 mapped to null route\n", _surface->number(), _index));
		zero ();
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 strip %2 now mapping stripable %3\n",
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

	boost::shared_ptr<AutomationControl> rec_enable_control = _stripable->rec_enable_control ();

	if (rec_enable_control) {
		_recenable->set_control (rec_enable_control);
		rec_enable_control->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Strip::notify_record_enable_changed, this), ui_context());
	}

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

	if (_surface->mcp().subview()->subview_mode() == Subview::None) {
		set_vpot_parameter (_pan_mode);
	}

	_fader->set_control (_stripable->gain_control());

	notify_all ();
}

void
Strip::notify_all()
{
	if (!_stripable) {
		zero ();
		return;
	}
	// The active V-pot control may not be active for this strip
	// But if we zero it in the controls function it may erase
	// the one we do want
	_surface->write (_vpot->zero());

	notify_solo_changed ();
	notify_mute_changed ();
	notify_gain_changed ();
	notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::name));
	notify_property_changed (PBD::PropertyChange (ARDOUR::Properties::selected));
	notify_panner_azi_changed ();
	notify_panner_width_changed ();
	notify_record_enable_changed ();
	notify_processor_changed ();
}

void
Strip::notify_solo_changed ()
{
	if (_stripable && _solo) {
		_surface->write (_solo->set_state (_stripable->solo_control()->soloed() ? on : off));
	}
}

void
Strip::notify_mute_changed ()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Strip %1 mute changed\n", _index));
	if (_stripable && _mute) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("\tstripable muted ? %1\n", _stripable->mute_control()->muted()));
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("mute message: %1\n", _mute->set_state (_stripable->mute_control()->muted() ? on : off)));

		_surface->write (_mute->set_state (_stripable->mute_control()->muted() ? on : off));
	}
}

void
Strip::notify_record_enable_changed ()
{
	if (_stripable && _recenable)  {
		boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<Track> (_stripable);
		if (trk) {
			_surface->write (_recenable->set_state (trk->rec_enable_control()->get_value() ? on : off));
		}
	}
}

void
Strip::notify_stripable_deleted ()
{
	_surface->mcp().refresh_current_bank();
}

void
Strip::notify_gain_changed (bool force_update)
{
	if (!_stripable) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = _stripable->gain_control();
	Control* control;

	if (!ac) {
		/* doesn't seem possible but lets be safe */
		return;
	}

	/* track gain control could be on vpot or fader, depending in
	 * flip mode.
	 */

	if (_vpot->control() == ac) {
		control = _vpot;
	} else if (_fader->control() == ac) {
		control = _fader;
	} else {
		return;
	}

	float gain_coefficient = ac->get_value();
	float normalized_position = ac->internal_to_interface (gain_coefficient);

	if (force_update || normalized_position != _last_gain_position_written) {

		if (!control->in_use()) {
			if (control == _vpot) {
				_surface->write (_vpot->set (normalized_position, true, Pot::wrap));
			} else {
				_surface->write (_fader->set_position (normalized_position));
			}
		}

		do_parameter_display (ac->desc(), gain_coefficient); // GainAutomation
		_last_gain_position_written = normalized_position;
	}
}

void
Strip::notify_processor_changed (bool force_update)
{
}

void
Strip::notify_property_changed (const PropertyChange& what_changed)
{
	if (!_stripable) {
		return;
	}

	if (what_changed.contains (ARDOUR::Properties::name)) {
		show_stripable_name ();
	}

	if (what_changed.contains (ARDOUR::Properties::selected)) {
		_surface->write (_select->set_state (_stripable->is_selected()));
	}
}

void
Strip::update_selection_state ()
{
	if(_stripable) {
		_surface->write (_select->set_state (_stripable->is_selected()));
	}
}

void
Strip::show_stripable_name ()
{
	Subview::Mode svm = _surface->mcp().subview()->subview_mode();

	if (svm != Subview::None) {
		/* subview mode is responsible for upper line */
		return;
	}

	string fullname = string();
	if (!_stripable) {
		fullname = string();
	} else {
		fullname = _stripable->name();
	}

	if (fullname.length() <= 6) {
		pending_display[0] = fullname;
	} else {
		pending_display[0] = PBD::short_version (fullname, 6);
	}

	if (_lcd2_available) {
		if (fullname.length() <= (_lcd2_label_pitch - 1)) {
			lcd2_pending_display[0] = fullname;
		} else {
			lcd2_pending_display[0] = PBD::short_version (fullname, (_lcd2_label_pitch - 1));
		}
	}
}

void
Strip::notify_panner_azi_changed (bool force_update)
{
	if (!_stripable) {
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("pan change for strip %1\n", _index));

	boost::shared_ptr<AutomationControl> pan_control = _stripable->pan_azimuth_control ();

	if (!pan_control) {
		/* basically impossible, since we're here because that control
		 *  changed, but sure, whatever.
		 */
		return;
	}

	if (_vpot->control() != pan_control) {
		return;
	}

	double normalized_pos = pan_control->internal_to_interface (pan_control->get_value(), true);
	double internal_pos = pan_control->get_value();

	if (force_update || (normalized_pos != _last_pan_azi_position_written)) {

		_surface->write (_vpot->set (normalized_pos, true, Pot::dot));
		/* show actual internal value to user */
		do_parameter_display (pan_control->desc(), internal_pos); // PanAzimuthAutomation

		_last_pan_azi_position_written = normalized_pos;
	}
}

void
Strip::notify_panner_width_changed (bool force_update)
{
	if (!_stripable) {
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("pan width change for strip %1\n", _index));

	boost::shared_ptr<AutomationControl> pan_control = _stripable->pan_width_control ();

	if (!pan_control) {
		/* basically impossible, since we're here because that control
		 *  changed, but sure, whatever.
		 */
		return;
	}

	if (_vpot->control() != pan_control) {
		return;
	}

	double pos = pan_control->internal_to_interface (pan_control->get_value());

	if (force_update || pos != _last_pan_width_position_written) {

		_surface->write (_vpot->set (pos, true, Pot::spread));
		do_parameter_display (pan_control->desc(), pos); // PanWidthAutomation

		_last_pan_width_position_written = pos;
	}
}

void
Strip::select_event (Button&, ButtonState bs)
{
	DEBUG_TRACE (DEBUG::MackieControl, "select button\n");

	if (bs == press) {

		int ms = _surface->mcp().main_modifier_state();

		if (ms & MackieControlProtocol::MODIFIER_CMDALT) {
			_controls_locked = !_controls_locked;
			_surface->write (display (0, 1,_controls_locked ?  "Locked" : "Unlock"));
			block_vpot_mode_display_for (1000);
			return;
		}

		DEBUG_TRACE (DEBUG::MackieControl, "add select button on press\n");
		_surface->mcp().add_down_select_button (_surface->number(), _index);
		_surface->mcp().select_range (_surface->mcp().global_index (*this));

	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "remove select button on release\n");
		_surface->mcp().remove_down_select_button (_surface->number(), _index);
	}
}

void
Strip::vselect_event (Button&, ButtonState bs)
{
	if (_surface->mcp().subview()->subview_mode() != Subview::None) {
		/* most subview modes: vpot press acts like a button for toggle parameters */
		if (bs != press) {
			return;
		}

		_surface->mcp().subview()->handle_vselect_event(_surface->mcp().global_index (*this));
		return;
	}

	if (bs == press) {

		int ms = _surface->mcp().main_modifier_state();

		if (ms & MackieControlProtocol::MODIFIER_SHIFT) {

			boost::shared_ptr<AutomationControl> ac = _vpot->control ();

			if (ac) {

				/* reset to default/normal value */
				ac->set_value (ac->normal(), Controllable::NoGroup);
			}

		}  else {

#ifdef MIXBUS
			if (_stripable) {
				boost::shared_ptr<AutomationControl> ac = _stripable->master_send_enable_controllable ();
				if (ac) {
					Controllable::GroupControlDisposition gcd;

					if (_surface->mcp().main_modifier_state() & MackieControlProtocol::MODIFIER_SHIFT) {
						gcd = Controllable::InverseGroup;
					} else {
						gcd = Controllable::UseGroup;
					}

					bool enabled = ac->get_value();
					ac->set_value (!enabled, gcd);
				}
			}
#else
			DEBUG_TRACE (DEBUG::MackieControl, "switching to next pot mode\n");
			/* switch vpot to control next available parameter */
			next_pot_mode ();
#endif
		}

	}
}

void
Strip::fader_touch_event (Button&, ButtonState bs)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader touch, press ? %1\n", (bs == press)));

	if (bs == press) {

		boost::shared_ptr<AutomationControl> ac = _fader->control ();

		_fader->set_in_use (true);
		_fader->start_touch (timepos_t (_surface->mcp().transport_sample()));

		if (ac) {
			do_parameter_display (ac->desc(), ac->get_value());
		}

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

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip %1 handling button %2 press ? %3\n", _index, button.bid(), (bs == press)));

	switch (button.bid()) {
	case Button::Select:
		select_event (button, bs);
		break;

	case Button::VSelect:
		vselect_event (button, bs);
		break;

	case Button::FaderTouch:
		fader_touch_event (button, bs);
		break;

	default:
		if ((control = button.control ())) {
			if (bs == press) {
				DEBUG_TRACE (DEBUG::MackieControl, "add button on press\n");
				_surface->mcp().add_down_button ((AutomationType) control->parameter().type(), _surface->number(), _index);

				float new_value = control->get_value() ? 0.0 : 1.0;

				/* get all controls that either have their
				 * button down or are within a range of
				 * several down buttons
				 */

				MackieControlProtocol::ControlList controls = _surface->mcp().down_controls ((AutomationType) control->parameter().type(),
				                                                                             _surface->mcp().global_index(*this));


				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("there are %1 buttons down for control type %2, new value = %3\n",
									    controls.size(), control->parameter().type(), new_value));

				/* apply change, with potential modifier semantics */

				Controllable::GroupControlDisposition gcd;

				if (_surface->mcp().main_modifier_state() & MackieControlProtocol::MODIFIER_SHIFT) {
					gcd = Controllable::InverseGroup;
				} else {
					gcd = Controllable::UseGroup;
				}

				for (MackieControlProtocol::ControlList::iterator c = controls.begin(); c != controls.end(); ++c) {
					(*c)->set_value (new_value, gcd);
				}

			} else {
				DEBUG_TRACE (DEBUG::MackieControl, "remove button on release\n");
				_surface->mcp().remove_down_button ((AutomationType) control->parameter().type(), _surface->number(), _index);
			}
		}
		break;
	}
}

std::string
Strip::format_parameter_for_display(
		ARDOUR::ParameterDescriptor const& desc,
		float val,
		boost::shared_ptr<ARDOUR::Stripable> stripable_for_non_mixbus_azimuth_automation,
		bool& overwrite_screen_hold)
{
	std::string formatted_parameter_display;
	char buf[16];

	switch (desc.type) {
	case GainAutomation:
	case BusSendLevel:
	case TrimAutomation:
		// we can't use value_as_string() that'll suffix "dB" and also use "-inf" w/o space :(
		if (val == 0.0) {
			formatted_parameter_display = " -inf ";
		} else {
			float dB = accurate_coefficient_to_dB (val);
			snprintf (buf, sizeof (buf), "%6.1f", dB);
			formatted_parameter_display = buf;
			overwrite_screen_hold = true;
		}
		break;

	case PanAzimuthAutomation:
		if (Profile->get_mixbus()) {
			// XXX no _stripable check?
			snprintf (buf, sizeof (buf), "%2.1f", val);
			formatted_parameter_display = buf;
			overwrite_screen_hold = true;
		} else {
			if (stripable_for_non_mixbus_azimuth_automation) {
				boost::shared_ptr<AutomationControl> pa = stripable_for_non_mixbus_azimuth_automation->pan_azimuth_control();
				if (pa) {
					formatted_parameter_display = pa->get_user_string ();
					overwrite_screen_hold = true;
				}
			}
		}
		break;
	default:
		formatted_parameter_display = ARDOUR::value_as_string (desc, val);
		if (formatted_parameter_display.size () < 6) { // left-padding, right-align
			formatted_parameter_display.insert (0, 6 - formatted_parameter_display.size (), ' ');
		}
		break;
	}

	return formatted_parameter_display;
}

void
Strip::do_parameter_display (ARDOUR::ParameterDescriptor const& desc, float val, bool screen_hold)
{
	pending_display[1] = format_parameter_for_display(desc, val, _stripable, screen_hold);

	if (screen_hold) {
		/* we just queued up a parameter to be displayed.
		   1 second from now, switch back to vpot mode display.
		*/
		block_vpot_mode_display_for (1000);
	}
}

void
Strip::handle_fader_touch (Fader& fader, bool touch_on)
{
	timepos_t now (_surface->mcp().transport_sample());

	if (touch_on) {
		fader.start_touch (now);
	} else {
		fader.stop_touch (now);
	}
}

void
Strip::handle_fader (Fader& fader, float position)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader to %1\n", position));
	boost::shared_ptr<AutomationControl> ac = fader.control();
	if (!ac) {
		return;
	}

	Controllable::GroupControlDisposition gcd = Controllable::UseGroup;

	if (_surface->mcp().main_modifier_state() & MackieControlProtocol::MODIFIER_SHIFT) {
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

	if (_surface->mcp().main_modifier_state() & MackieControlProtocol::MODIFIER_SHIFT) {
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

		double p = ac->get_interface(true);

		p += delta;

		p = max (0.0, p);
		p = min (1.0, p);

		ac->set_interface ( p, true);
	}
}

void
Strip::periodic (PBD::microseconds_t now)
{
	update_meter ();
	update_automation ();
}

void
Strip::redisplay (PBD::microseconds_t now, bool force)
{
	if (_block_screen_redisplay_until >= now) {
		/* no drawing allowed */
		return;
	}

	if (_block_screen_redisplay_until) {
		/* we were blocked, but the time period has elapsed, so we must
		 * force a redraw.
		 */
		force = true;
		_block_screen_redisplay_until = 0;
	}

	if (force || (current_display[0] != pending_display[0])) {
		_surface->write (display (0, 0, pending_display[0]));
		current_display[0] = pending_display[0];
	}

	if (return_to_vpot_mode_display_at <= now) {
		return_to_vpot_mode_display_at = UINT64_MAX;
		return_to_vpot_mode_display ();
	}

	if (force || (current_display[1] != pending_display[1])) {
		_surface->write (display (0, 1, pending_display[1]));
		current_display[1] = pending_display[1];
	}

	if (_lcd2_available) {
		if (force || (lcd2_current_display[0] != lcd2_pending_display[0])) {
			_surface->write (display (1, 0, lcd2_pending_display[0]));
			lcd2_current_display[0] = lcd2_pending_display[0];
		}
		if (force || (lcd2_current_display[1] != lcd2_pending_display[1])) {
			_surface->write (display (1, 1, lcd2_pending_display[1]));
			lcd2_current_display[1] = lcd2_pending_display[1];
		}
	}
}

void
Strip::update_automation ()
{
	if (!_stripable) {
		return;
	}

	ARDOUR::AutoState state = _stripable->gain_control()->automation_state();

	if (state == Touch || state == Play) {
		notify_gain_changed (false);
	}

	boost::shared_ptr<AutomationControl> pan_control = _stripable->pan_azimuth_control ();
	if (pan_control) {
		state = pan_control->automation_state ();
		if (state == Touch || state == Play) {
			notify_panner_azi_changed (false);
		}
	}

	pan_control = _stripable->pan_width_control ();
	if (pan_control) {
		state = pan_control->automation_state ();
		if (state == Touch || state == Play) {
			notify_panner_width_changed (false);
		}
	}
}

void
Strip::update_meter ()
{
	if (!_stripable) {
		return;
	}

	if (_surface->mcp().subview()->subview_mode() != Subview::None) {
		return;
	}

	if (_meter && _metering_active && _stripable->peak_meter()) {
		float dB = _stripable->peak_meter()->meter_level (0, MeterMCP);
		_meter->send_update (*_surface, dB);
		return;
	}
}

void
Strip::zero ()
{
	for (Group::Controls::const_iterator it = _controls.begin(); it != _controls.end(); ++it) {
		_surface->write ((*it)->zero ());
	}

	_surface->write (blank_display (0, 0));
	_surface->write (blank_display (0, 1));
	pending_display[0] = string();
	pending_display[1] = string();
	current_display[0] = string();
	current_display[1] = string();

	if (_lcd2_available) {
		_surface->write (blank_display (1, 0));
		_surface->write (blank_display (1, 1));
		lcd2_pending_display[0] = string();
		lcd2_pending_display[1] = string();
		lcd2_current_display[0] = string();
		lcd2_current_display[1] = string();

	}
}

MidiByteArray
Strip::blank_display (uint32_t lcd_number, uint32_t line_number)
{
	return display (lcd_number, line_number, string());
}

MidiByteArray
Strip::display (uint32_t lcd_number, uint32_t line_number, const std::string& line)
{
	assert (line_number <= 1);

	bool add_left_pad_char = false;
	unsigned left_pad_offset = 0;
	unsigned lcd_label_pitch = 7;
	unsigned max_char_count = lcd_label_pitch - 1;
	MidiByteArray retval;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip_display lcd: %1, index: %2, line %3 = %4\n"
                 , lcd_number, _index, line_number, line));

	if (lcd_number == 0) {
		// Standard MCP display
		retval << _surface->sysex_hdr();
		// code for display
		retval << 0x12;
	}
	else {
		/* The second lcd on the Qcon Pro X master unit uses a 6 character label instead of 7.
		 *  That allows a 9th label for the master fader.
		 *
		 *  Format: _6Char#1_6Char#2_6Char#3_6Char#4_6Char#5_6Char#6_6Char#7_6Char#8_6Char#9_
		 *
		 *  The _ in the format is a space that is inserted as label display seperators
		 *
		 *  The extender unit has 8 faders and uses the standard MCP pitch.
		 *
		 *  The second LCD is an extention to the MCP with a different sys ex header.
		 */

		lcd_label_pitch = _lcd2_label_pitch;
		max_char_count = lcd_label_pitch - 1;

		retval <<  MidiByteArray (5, MIDI::sysex, 0x0, 0x0, 0x67, 0x15);
		// code for display
		retval << 0x13;

		if (lcd_label_pitch == 6) {
			if (_index == 0) {
				add_left_pad_char = true;
			}
			else {
				left_pad_offset = 1;
			}
		}
	}

	// offset (0 to 0x37 first line, 0x38 to 0x6f for second line)
	retval << (_index * lcd_label_pitch + (line_number * 0x38) + left_pad_offset);

	if (add_left_pad_char) {
		retval << ' ';	// add the left pad space
	}

	// ascii data to display. @param line is UTF-8
	string ascii = Glib::convert_with_fallback (line, "UTF-8", "ISO-8859-1", "_");
	string::size_type len = ascii.length();
	if (len > max_char_count) {
		ascii = ascii.substr (0, max_char_count);
		len = max_char_count;
	}
	retval << ascii;
	// pad with " " out to N chars
	for (unsigned i = len; i < max_char_count; ++i) {
		retval << ' ';
	}

	// column spacer, unless it's the right-hand column
	if (_index < 7 || lcd_number == 1) {
		retval << ' ';
	}

	// sysex trailer
	retval << MIDI::eox;

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

string
Strip::vpot_mode_string ()
{
	if (_surface->mcp().subview()->subview_mode() != Subview::None) {
		return string();
	}

	boost::shared_ptr<AutomationControl> ac = _vpot->control();

	if (!ac) {
		return string();
	}

	switch (ac->desc().type) {
	case PanAzimuthAutomation:
		return "Pan";
	case PanWidthAutomation:
		return "Width";
	case PanElevationAutomation:
		return "Elev";
	case PanFrontBackAutomation:
		return "F/Rear";
	case PanLFEAutomation:
		return "LFE";
	default:
		break;
	}
#ifdef MIXBUS
	//"None" mode, by definition (currently) shows the pan control above the fader.
	//Mixbus controllers are created from a LADSPA so they don't have ac->desc().type
	//For the forseeable future, we will just return "Pan" here.
	return "Pan";
#endif

	return "???";
}

void
Strip::flip_mode_changed ()
{
	if (_surface->mcp().subview()->permit_flipping_faders_and_pots()) {

		boost::shared_ptr<AutomationControl> pot_control = _vpot->control();
		boost::shared_ptr<AutomationControl> fader_control = _fader->control();

		if (pot_control && fader_control) {

			_vpot->set_control (fader_control);
			_fader->set_control (pot_control);

			/* update fader with pot value */

			_surface->write (_fader->set_position (pot_control->internal_to_interface (pot_control->get_value ())));

			/* update pot with fader value */

			_surface->write (_vpot->set (fader_control->internal_to_interface (fader_control->get_value()), true, Pot::wrap));


			if (_surface->mcp().flip_mode() == MackieControlProtocol::Normal) {
				do_parameter_display (fader_control->desc(), fader_control->get_value());
			} else {
				do_parameter_display (pot_control->desc(), pot_control->get_value()); // BusSendLevel
			}

		}

	} else {
		/* do nothing */
	}
}

void
Strip::block_screen_display_for (uint32_t msecs)
{
	_block_screen_redisplay_until = PBD::get_microseconds() + (msecs * 1000);
}

void
Strip::block_vpot_mode_display_for (uint32_t msecs)
{
	return_to_vpot_mode_display_at = PBD::get_microseconds() + (msecs * 1000);
}

void
Strip::return_to_vpot_mode_display ()
{
	/* returns the second line of the two-line per-strip display
	   back the mode where it shows what the VPot controls.
	*/

	if (_surface->mcp().subview()->subview_mode() != Subview::None) {
		/* do nothing - second line shows value of current subview parameter */
		return;
	} else if (_stripable) {
		pending_display[1] = vpot_mode_string();
	} else {
		pending_display[1] = string();
	}
}

void
Strip::next_pot_mode ()
{
	vector<AutomationType>::iterator i;

	if (_surface->mcp().flip_mode() != MackieControlProtocol::Normal) {
		/* do not change vpot mode while in flipped mode */
		DEBUG_TRACE (DEBUG::MackieControl, "not stepping pot mode - in flip mode\n");
		pending_display[1] = "Flip";
		block_vpot_mode_display_for (1000);
		return;
	}


	boost::shared_ptr<AutomationControl> ac = _vpot->control();

	if (!ac) {
		return;
	}


	if (_surface->mcp().subview()->subview_mode() != Subview::None) {
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
Strip::subview_mode_changed ()
{
	switch (_surface->mcp().subview()->subview_mode()) {
	case Subview::None:
		set_vpot_parameter (_pan_mode);
		/* need to show strip name again */
		show_stripable_name ();
		if (!_stripable) {
			_surface->write (_vpot->set (0, true, Pot::wrap));
			_surface->write (_fader->set_position (0.0));
		}
		notify_metering_state_changed ();
		break;

	case Subview::EQ:
	case Subview::Dynamics:
	case Subview::Sends:
	case Subview::TrackView:
	case Subview::Plugin:
		_surface->mcp().subview()->setup_vpot(this, _vpot, pending_display);
		break;
	}
}

void
Strip::set_vpot_parameter (AutomationType p)
{
	if (!_stripable || (p == NullAutomation)) {
		_vpot->set_control (boost::shared_ptr<AutomationControl>());
		pending_display[1] = string();
		return;
	}

	boost::shared_ptr<AutomationControl> pan_control;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch to vpot mode %1\n", p));

	reset_saved_values ();

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
		_vpot->set_control (pan_control);
	}

	pending_display[1] = vpot_mode_string ();
}

bool
Strip::is_midi_track () const
{
	return boost::dynamic_pointer_cast<MidiTrack>(_stripable) != 0;
}

void
Strip::reset_saved_values ()
{
	_last_pan_azi_position_written = -1.0;
	_last_pan_width_position_written = -1.0;
	_last_gain_position_written = -1.0;
	_last_trim_position_written = -1.0;

}

void
Strip::notify_metering_state_changed()
{
	if (_surface->mcp().subview()->subview_mode() != Subview::None) {
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
