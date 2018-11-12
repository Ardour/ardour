/*
    Copyright (C) 2012 Paul Davis
	Copyright (C) 2017 Ben Loftis

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
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <cmath>

#include <glibmm/convert.h>

#include "pbd/stacktrace.h"

#include "midi++/port.h"

#include "ardour/audioengine.h"
#include "ardour/automation_control.h"
#include "ardour/debug.h"
#include "ardour/route.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/utils.h"

#include <gtkmm2ext/gui_thread.h>

#include "control_group.h"
#include "surface_port.h"
#include "surface.h"
#include "strip.h"
#include "us2400_control_protocol.h"
#include "jog_wheel.h"

#include "strip.h"
#include "button.h"
#include "led.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif

using namespace std;
using namespace PBD;
using ARDOUR::Stripable;
using ARDOUR::Panner;
using ARDOUR::Profile;
using ARDOUR::AutomationControl;
using namespace ArdourSurface;
using namespace US2400;

#define ui_context() US2400Protocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

// The MCU sysex header.4th byte Will be overwritten
// when we get an incoming sysex that identifies
// the device type
static MidiByteArray mackie_sysex_hdr  (5, MIDI::sysex, 0x0, 0x0, 0x66, 0x14);

// The MCU extender sysex header.4th byte Will be overwritten
// when we get an incoming sysex that identifies
// the device type
static MidiByteArray mackie_sysex_hdr_xt  (5, MIDI::sysex, 0x0, 0x0, 0x66, 0x15);

static MidiByteArray empty_midi_byte_array;

Surface::Surface (US2400Protocol& mcp, const std::string& device_name, uint32_t number, surface_type_t stype)
	: _mcp (mcp)
	, _stype (stype)
	, _number (number)
	, _name (device_name)
	, _active (false)
	, _connected (false)
	, _jog_wheel (0)
	, _master_fader (0)
	, _last_master_gain_written (-0.0f)
	, _joystick_active (false)
	, connection_state (0)
	, input_source (0)
{
	DEBUG_TRACE (DEBUG::US2400, "Surface::Surface init\n");

	try {
		_port = new SurfacePort (*this);
	} catch (...) {
		throw failed_constructor ();
	}

	/* only the first Surface object has global controls */
	/* lets use master_position instead */
	uint32_t mp = _mcp.device_info().master_position();
	if (_number == mp) {
		DEBUG_TRACE (DEBUG::US2400, "Surface matches MasterPosition. Might have global controls.\n");
		if (_mcp.device_info().has_global_controls()) {
			init_controls ();
			DEBUG_TRACE (DEBUG::US2400, "init_controls done\n");
		}

		if (_mcp.device_info().has_master_fader()) {
			setup_master ();
			DEBUG_TRACE (DEBUG::US2400, "setup_master done\n");
		}
	}

	uint32_t n = _mcp.device_info().strip_cnt();

	if (n) {
		init_strips (n);
		DEBUG_TRACE (DEBUG::US2400, "init_strips done\n");
	}

	connect_to_signals ();

	DEBUG_TRACE (DEBUG::US2400, "Surface::Surface done\n");
}

Surface::~Surface ()
{
	DEBUG_TRACE (DEBUG::US2400, "Surface::~Surface init\n");

	if (input_source) {
		g_source_destroy (input_source);
		input_source = 0;
	}

	// delete groups (strips)
	for (Groups::iterator it = groups.begin(); it != groups.end(); ++it) {
		delete it->second;
	}

	// delete controls (global buttons, master fader etc)
	for (Controls::iterator it = controls.begin(); it != controls.end(); ++it) {
		delete *it;
	}

	delete _jog_wheel;
	delete _port;
	// the ports take time to release and we may be rebuilding right away
	// in the case of changing devices.
	g_usleep (10000);
	DEBUG_TRACE (DEBUG::US2400, "Surface::~Surface done\n");
}

bool
Surface::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	if (!_port) {
		return false;
	}

	string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (_port->input_name());
	string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (_port->output_name());

	if (ni == name1 || ni == name2) {
		if (yn) {
			connection_state |= InputConnected;
		} else {
			connection_state &= ~InputConnected;
		}
	} else if (no == name1 || no == name2) {
		if (yn) {
			connection_state |= OutputConnected;
		} else {
			connection_state &= ~OutputConnected;
		}
	} else {
		/* not our ports */
		return false;
	}

	if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* this will send a device query message, which should
		   result in a response that will kick off device type
		   discovery and activation of the surface(s).

		   The intended order of events is:

		   - each surface sends a device query message
		   - devices respond with either MCP or LCP response (sysex in both
		   cases)
		   - sysex message causes Surface::turn_it_on() which tells the
		   MCP object that the surface is ready, and sets up strip
		   displays and binds faders and buttons for that surface

		   In the case of LCP, where this is a handshake process that could
		   fail, the response process to the initial sysex after a device query
		   will mark the surface inactive, which won't shut anything down
		   but will stop any writes to the device.

		   Note: there are no known cases of the handshake process failing.

		   We actually can't initiate this in this callback, so we have
		   to queue it with the MCP event loop.
		*/

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
		connected ();

	} else {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface %1 disconnected (input or output or both)\n", _name));
		_active = false;
	}

	return true; /* connection status changed */
}

XMLNode&
Surface::get_state()
{
	XMLNode* node = new XMLNode (X_("Surface"));
	node->set_property (X_("name"), _name);
	node->add_child_nocopy (_port->get_state());
	return *node;
}

int
Surface::set_state (const XMLNode& node, int version)
{
	/* Look for a node named after the device we're part of */

	XMLNodeList const& children = node.children();
	XMLNode* mynode = 0;

	for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
		std::string name;
		if ((*c)->get_property (X_("name"), name) && name == _name) {
			mynode = *c;
			break;
		}
	}

	if (!mynode) {
		return 0;
	}

	XMLNode* portnode = mynode->child (X_("Port"));
	if (portnode) {
		if (_port->set_state (*portnode, version)) {
			return -1;
		}
	}

	return 0;
}

const MidiByteArray&
Surface::sysex_hdr() const
{
	switch  (_stype) {
	case st_mcu: return mackie_sysex_hdr;
	case st_ext: return mackie_sysex_hdr_xt;
	default: return mackie_sysex_hdr_xt;
	}
	cout << "SurfacePort::sysex_hdr _port_type not known" << endl;
	return mackie_sysex_hdr;
}

static GlobalControlDefinition mackie_global_controls[] = {
	{ "external", Pot::External, Pot::factory, "none" },
	{ "fader_touch", Led::FaderTouch, Led::factory, "master" },
	{ "timecode", Led::Timecode, Led::factory, "none" },
	{ "beats", Led::Beats, Led::factory, "none" },
	{ "solo", Led::RudeSolo, Led::factory, "none" },
	{ "relay_click", Led::RelayClick, Led::factory, "none" },
	{ "", 0, Led::factory, "" }
};

void
Surface::init_controls()
{
	Group* group;

	DEBUG_TRACE (DEBUG::US2400, "Surface::init_controls: creating groups\n");
	groups["assignment"] = new Group  ("assignment");
	groups["automation"] = new Group  ("automation");
	groups["bank"] = new Group  ("bank");
	groups["cursor"] = new Group  ("cursor");
	groups["display"] = new Group  ("display");
	groups["function select"] = new Group  ("function select");
	groups["global view"] = new Group ("global view");
	groups["master"] = new Group ("master");
	groups["modifiers"] = new Group  ("modifiers");
	groups["none"] = new Group  ("none");
	groups["transport"] = new Group  ("transport");
	groups["user"] = new Group  ("user");
	groups["utilities"] = new Group  ("utilities");

	DEBUG_TRACE (DEBUG::US2400, "Surface::init_controls: creating jog wheel\n");
	if (_mcp.device_info().has_jog_wheel()) {
		_jog_wheel = new US2400::JogWheel (_mcp);
	}

	DEBUG_TRACE (DEBUG::US2400, "Surface::init_controls: creating global controls\n");
	for (uint32_t n = 0; mackie_global_controls[n].name[0]; ++n) {
		group = groups[mackie_global_controls[n].group_name];
		Control* control = mackie_global_controls[n].factory (*this, mackie_global_controls[n].id, mackie_global_controls[n].name, *group);
		controls_by_device_independent_id[mackie_global_controls[n].id] = control;
	}

	/* add global buttons */
	DEBUG_TRACE (DEBUG::US2400, "Surface::init_controls: adding global buttons\n");
	const map<Button::ID,GlobalButtonInfo>& global_buttons (_mcp.device_info().global_buttons());

	for (map<Button::ID,GlobalButtonInfo>::const_iterator b = global_buttons.begin(); b != global_buttons.end(); ++b){
		group = groups[b->second.group];
		controls_by_device_independent_id[b->first] = Button::factory (*this, b->first, b->second.id, b->second.label, *group);
	}
}

void
Surface::init_strips (uint32_t n)
{
	const map<Button::ID,StripButtonInfo>& strip_buttons (_mcp.device_info().strip_buttons());

	//surface 4  has no strips
	if ((_stype != st_mcu) && (_stype != st_ext))
		return;
	
	for (uint32_t i = 0; i < n; ++i) {

		char name[32];

		snprintf (name, sizeof (name), "strip_%d", (8* _number) + i);

		Strip* strip = new Strip (*this, name, i, strip_buttons);

		strip->set_global_index (_number*n + i);

		groups[name] = strip;
		strips.push_back (strip);
	}
}

void
Surface::master_monitor_may_have_changed ()
{
	if (_number == _mcp.device_info().master_position()) {
		setup_master ();
	}
}

void
Surface::setup_master ()
{
	boost::shared_ptr<Stripable> m;

	if ((m = _mcp.get_session().monitor_out()) == 0) {
		m = _mcp.get_session().master_out();
	}

	if (!m) {
		if (_master_fader) {
			_master_fader->reset_control ();
		}
		master_connection.disconnect ();
		return;
	}

	if (!_master_fader) {
		Groups::iterator group_it;
		Group* master_group;
		group_it = groups.find("master");

		if (group_it == groups.end()) {
			groups["master"] = master_group = new Group ("master");
		} else {
			master_group = group_it->second;
		}

		_master_fader = dynamic_cast<Fader*> (Fader::factory (*this, _mcp.device_info().strip_cnt(), "master", *master_group));

		DeviceInfo device_info = _mcp.device_info();
		GlobalButtonInfo master_button = device_info.get_global_button(Button::MasterFaderTouch);
		Button* bb = dynamic_cast<Button*> (Button::factory (
		                                    *this,
		                                    Button::MasterFaderTouch,
		                                    master_button.id,
		                                    master_button.label,
		                                    *(group_it->second)
		                                    ));

		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface %1 Master Fader new button BID %2 id %3\n",
		                                                   number(), Button::MasterFaderTouch, bb->id()));
	} else {
		master_connection.disconnect ();
	}

	_master_fader->set_control (m->gain_control());
	m->gain_control()->Changed.connect (master_connection, MISSING_INVALIDATOR, boost::bind (&Surface::master_gain_changed, this), ui_context());
	_last_master_gain_written = FLT_MAX; /* some essentially impossible value */
	_port->write (_master_fader->set_position (0.0));
	master_gain_changed ();
}

void
Surface::master_gain_changed ()
{
	if (!_master_fader) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = _master_fader->control();
	if (!ac) {
		return;
	}

	float normalized_position = ac->internal_to_interface (ac->get_value());
	if (normalized_position == _last_master_gain_written) {
		return;
	}

	DEBUG_TRACE (DEBUG::US2400, "Surface::master_gain_changed: updating surface master fader\n");

	_port->write (_master_fader->set_position (normalized_position));
	_last_master_gain_written = normalized_position;
}

float
Surface::scaled_delta (float delta, float current_speed)
{
	/* XXX needs work before use */
	const float sign = delta < 0.0 ? -1.0 : 1.0;

	return ((sign * std::pow (delta + 1.0, 2.0)) + current_speed) / 100.0;
}

void
Surface::blank_jog_ring ()
{
}

float
Surface::scrub_scaling_factor () const
{
	return 100.0;
}

void
Surface::connect_to_signals ()
{
	if (!_connected) {


		DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface %1 connecting to signals on port %2\n",
								   number(), _port->input_port().name()));

		MIDI::Parser* p = _port->input_port().parser();

		/* Incoming sysex */
		p->sysex.connect_same_thread (*this, boost::bind (&Surface::handle_midi_sysex, this, _1, _2, _3));
		/* V-Pot messages are Controller */
		p->controller.connect_same_thread (*this, boost::bind (&Surface::handle_midi_controller_message, this, _1, _2));
		/* Button messages are NoteOn */
		p->note_on.connect_same_thread (*this, boost::bind (&Surface::handle_midi_note_on_message, this, _1, _2));
		/* Button messages are NoteOn but libmidi++ sends note-on w/velocity = 0 as note-off so catch them too */
		p->note_off.connect_same_thread (*this, boost::bind (&Surface::handle_midi_note_on_message, this, _1, _2));
		/* Fader messages are Pitchbend */
		uint32_t i;
		for (i = 0; i < _mcp.device_info().strip_cnt(); i++) {
			p->channel_pitchbend[i].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, i));
		}
		// Master fader
		p->channel_pitchbend[_mcp.device_info().strip_cnt()].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, _mcp.device_info().strip_cnt()));

		_connected = true;
	}
}

void
Surface::handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t pb, uint32_t fader_id)
{
	/* Pitchbend messages are fader position messages. Nothing in the data we get
	 * from the MIDI::Parser conveys the fader ID, which was given by the
	 * channel ID in the status byte.
	 *
	 * Instead, we have used bind() to supply the fader-within-strip ID
	 * when we connected to the per-channel pitchbend events.
	 */

	DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface::handle_midi_pitchbend_message on port %3, fader = %1 value = %2 (%4)\n",
							   fader_id, pb, _number, pb/16384.0));

	turn_it_on ();

	Fader* fader = faders[fader_id];

	if (fader) {
		Strip* strip = dynamic_cast<Strip*> (&fader->group());
		float pos = pb / 16384.0;
		if (strip) {
			strip->handle_fader (*fader, pos);
		} else {
			DEBUG_TRACE (DEBUG::US2400, "Handling master fader\n");
			/* master fader */
			fader->set_value (pos); // alter master gain
			_port->write (fader->set_position (pos)); // write back value (required for servo)
		}
	} else {
		DEBUG_TRACE (DEBUG::US2400, "fader not found\n");
	}
}

void
Surface::handle_midi_note_on_message (MIDI::Parser &, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface::handle_midi_note_on_message %1 = %2\n", (int) ev->note_number, (int) ev->velocity));

	turn_it_on ();

	/* fader touch sense is given by "buttons" 0xe..0xe7 and 0xe8 for the
	 * master.
	 */

	if (ev->note_number >= 0xE0 && ev->note_number <= 0xE8) {
		Fader* fader = faders[ev->note_number];

		DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface: fader touch message, fader = %1\n", fader));

		if (fader) {

			Strip* strip = dynamic_cast<Strip*> (&fader->group());

			if (ev->velocity > 64) {
				strip->handle_fader_touch (*fader, true);
			} else {
				strip->handle_fader_touch (*fader, false);
			}
		}
		return;
	}

	Button* button = buttons[ev->note_number];

	if (button) {

		if (ev->velocity > 64) {
			button->pressed ();
		}

		Strip* strip = dynamic_cast<Strip*> (&button->group());

		if (mcp().main_modifier_state() == US2400Protocol::MODIFIER_OPTION) {

			/* special case: CLR Solo looks like a strip's solo button, but with MODIFIER_OPTION it becomes global CLR SOLO */
			DEBUG_TRACE (DEBUG::US2400, string_compose ("HERE option global button %1\n", button->id()));
			_mcp.handle_button_event (*this, *button, ev->velocity > 64 ? press : release);

		} else if (strip) {
			DEBUG_TRACE (DEBUG::US2400, string_compose ("strip %1 button %2 pressed ? %3\n",
									   strip->index(), button->name(), (ev->velocity > 64)));
			strip->handle_button (*button, ev->velocity > 64 ? press : release);
		} else {
			/* global button */
			DEBUG_TRACE (DEBUG::US2400, string_compose ("global button %1\n", button->id()));
			_mcp.handle_button_event (*this, *button, ev->velocity > 64 ? press : release);
		}

		if (ev->velocity <= 64) {
			button->released ();
		}

	} else {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("no button found for %1\n", (int) ev->note_number));
	}

	/* button release should reset timer AFTER handler(s) have run */
}

void
Surface::handle_midi_controller_message (MIDI::Parser &, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("SurfacePort::handle_midi_controller %1 = %2\n", (int) ev->controller_number, (int) ev->value));

	turn_it_on ();

	/* The joystick is not touch sensitive.
	 * ignore the joystick until the user clicks the "null" button.
	 * The joystick sends spurious controller messages,
	 * and since they are absolute values (joy position) this can send undesired changes. 
	 */
	if (_stype == st_joy && ev->controller_number == 0x01) {
		_joystick_active = true;

	/* Unfortunately the device does not appear to respond to the NULL button's LED,
	 * to indicate that the joystick is active.
	 */
#if 0 // this approach doesn't seem to work
		MidiByteArray joy_active (3, 0xB0, 0x01, 0x01);
		_port->write (joy_active);
#endif
	}

#ifdef MIXBUS32C  //in 32C, we can use the joystick for the last 2 mixbus send level & pans

	if (_stype == st_joy && _joystick_active) {
		if (ev->controller_number == 0x03) {
			float value = (float)ev->value / 127.0;
			float db_value = 20.0 * value;
			float inv_db = 20.0 - db_value; 
			boost::shared_ptr<Stripable> r = mcp().subview_stripable();
			if (r && r->is_input_strip()) {
				boost::shared_ptr<AutomationControl> pc = r->send_level_controllable (10);
				if (pc) {
					pc->set_value (-db_value , PBD::Controllable::NoGroup);
				}
				pc = r->send_level_controllable (11);
				if (pc) {
					pc->set_value (-inv_db, PBD::Controllable::NoGroup);
				}
			}
		}
		if (ev->controller_number == 0x02) {
			float value = (float)ev->value / 127.0;
			boost::shared_ptr<Stripable> r = mcp().subview_stripable();
			if (r && r->is_input_strip()) {
				boost::shared_ptr<AutomationControl> pc = r->send_pan_azi_controllable (10);
				if (pc) {
					float v = pc->interface_to_internal(value);
					pc->set_value (v, PBD::Controllable::NoGroup);
				}
				pc = r->send_pan_azi_controllable (11);
				if (pc) {
					float v = pc->interface_to_internal(value);
					pc->set_value (v, PBD::Controllable::NoGroup);
				}
			}
		}
		return;
	}
#endif

	Pot* pot = pots[ev->controller_number];

	// bit 6 gives the sign
	float sign = (ev->value & 0x40) == 0 ? 1.0 : -1.0;
	// bits 0..5 give the velocity. we interpret this as "ticks
	// moved before this message was sent"
	float ticks = (ev->value & 0x3f);
	if (ticks == 0) {
		/* euphonix and perhaps other devices send zero
		   when they mean 1, we think.
		*/
		ticks = 1;
	}

	float delta = 0;
	if (mcp().main_modifier_state() == US2400Protocol::MODIFIER_SHIFT) {
		delta = sign * (ticks / (float) 0xff);
	} else {
		delta = sign * (ticks / (float) 0x3f);
	}

	if (!pot) {
		if (ev->controller_number == Jog::ID && _jog_wheel) {

			DEBUG_TRACE (DEBUG::US2400, string_compose ("Jog wheel moved %1\n", ticks));
			_jog_wheel->jog_event (delta);
			return;
		}
		// add external (pedal?) control here

		return;
	}

	Strip* strip = dynamic_cast<Strip*> (&pot->group());
	if (strip) {
		strip->handle_pot (*pot, delta);
	}
}

void
Surface::handle_midi_sysex (MIDI::Parser &, MIDI::byte * raw_bytes, size_t count)
{
	MidiByteArray bytes (count, raw_bytes);

	/* always save the device type ID so that our outgoing sysex messages
	 * are correct
	 */

	if (_stype == st_mcu) {
		mackie_sysex_hdr[4] = bytes[4];
	} else {
		mackie_sysex_hdr_xt[4] = bytes[4];
	}

	switch (bytes[5]) {
	case 0x01:
		if (!_active) {
			DEBUG_TRACE (DEBUG::US2400, string_compose ("surface #%1,  handle_midi_sysex: %2\n", _number, bytes));
			DEBUG_TRACE (DEBUG::US2400, string_compose ("Mackie Control Device ready, current status = %1\n", _active));
			turn_it_on ();
		}
		break;

	case 0x06:
		if (!_active) {
			DEBUG_TRACE (DEBUG::US2400, string_compose ("surface #%1,  handle_midi_sysex: %2\n", _number, bytes));
		}
		/* Behringer X-Touch Compact: Device Ready
		*/
		DEBUG_TRACE (DEBUG::US2400, string_compose ("Behringer X-Touch Compact ready, current status = %1\n", _active));
		turn_it_on ();
		break;

	case 0x03: /* LCP Connection Confirmation */
		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface #%1,  handle_midi_sysex: %2\n", _number, bytes));
		DEBUG_TRACE (DEBUG::US2400, "Logic Control Device confirms connection, ardour replies\n");
//		if (bytes[4] == 0x10 || bytes[4] == 0x11) {
//			write_sysex (host_connection_confirmation (bytes));			turn_it_on ();
			turn_it_on ();
//		}
		break;

//	case 0x04: /* LCP: Confirmation Denied */
//		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface #%1,  handle_midi_sysex: %2\n", _number, bytes));
//		DEBUG_TRACE (DEBUG::US2400, "Logic Control Device denies connection\n");
//		_active = false;
//		break;

	default:
		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface #%1,  handle_midi_sysex: %2\n", _number, bytes));
//		DEBUG_TRACE (DEBUG::US2400, string_compose ("unknown device ID byte %1", (int) bytes[5]));
		error << "MCP: unknown sysex: " << bytes << endmsg;
	}
}

static MidiByteArray
calculate_challenge_response (MidiByteArray::iterator begin, MidiByteArray::iterator end)
{
	MidiByteArray l;
	back_insert_iterator<MidiByteArray> back  (l);
	copy (begin, end, back);

	MidiByteArray retval;

	// this is how to calculate the response to the challenge.
	// from the Logic docs.
	retval <<  (0x7f &  (l[0] +  (l[1] ^ 0xa) - l[3]));
	retval <<  (0x7f &  ((l[2] >> l[3]) ^  (l[0] + l[3])));
	retval <<  (0x7f &  ((l[3] -  (l[2] << 2)) ^  (l[0] | l[1])));
	retval <<  (0x7f &  (l[1] - l[2] +  (0xf0 ^  (l[3] << 4))));

	return retval;
}

MidiByteArray
Surface::host_connection_query (MidiByteArray & bytes)
{
	MidiByteArray response;

	if (bytes[4] != 0x10 && bytes[4] != 0x11) {
		/* not a Logic Control device - no response required */
		return response;
	}

	// handle host connection query
	DEBUG_TRACE (DEBUG::US2400, string_compose ("host connection query: %1\n", bytes));

	if  (bytes.size() != 18) {
		cerr << "expecting 18 bytes, read " << bytes << " from " << _port->input_port().name() << endl;
		return response;
	}

	// build and send host connection reply
	response << 0x02;
	copy (bytes.begin() + 6, bytes.begin() + 6 + 7, back_inserter (response));
	response << calculate_challenge_response (bytes.begin() + 6 + 7, bytes.begin() + 6 + 7 + 4);
	return response;
}

MidiByteArray
Surface::host_connection_confirmation (const MidiByteArray & bytes)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("host_connection_confirmation: %1\n", bytes));

	// decode host connection confirmation
	if  (bytes.size() != 14) {
		ostringstream os;
		os << "expecting 14 bytes, read " << bytes << " from " << _port->input_port().name();
		throw MackieControlException (os.str());
	}

	// send version request
	return MidiByteArray (2, 0x13, 0x00);
}

void
Surface::turn_it_on ()
{
	if (_active) {
		return;
	}

	_active = true;

	_mcp.device_ready ();  //this gets redundantly called with each new surface connection; but this is desirable to get the banks set up correctly

	for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->notify_all ();
	}

}

void
Surface::write_sysex (const MidiByteArray & mba)
{
	if (mba.empty()) {
		return;
	}

	MidiByteArray buf;
	buf << sysex_hdr() << mba << MIDI::eox;
	_port->write (buf);
}

void
Surface::write_sysex (MIDI::byte msg)
{
	MidiByteArray buf;
	buf << sysex_hdr() << msg << MIDI::eox;
	_port->write (buf);
}

uint32_t
Surface::n_strips (bool with_locked_strips) const
{
	if (with_locked_strips) {
		return strips.size();
	}

	uint32_t n = 0;

	for (Strips::const_iterator it = strips.begin(); it != strips.end(); ++it) {
		if (!(*it)->locked()) {
			++n;
		}
	}
	return n;
}

Strip*
Surface::nth_strip (uint32_t n) const
{
	if (n > n_strips()) {
		return 0;
	}
	return strips[n];
}

void
Surface::zero_all ()
{
	if (_mcp.device_info().has_master_fader () && _master_fader) {
		_port->write (_master_fader->zero ());
	}

	// zero all strips
	for (Strips::iterator it = strips.begin(); it != strips.end(); ++it) {
		(*it)->zero();
	}

	zero_controls ();
}

void
Surface::zero_controls ()
{
	if (!_mcp.device_info().has_global_controls()) {
		return;
	}

	// turn off global buttons and leds

	for (Controls::iterator it = controls.begin(); it != controls.end(); ++it) {
		Control & control = **it;
		if (!control.group().is_strip()) {
			_port->write (control.zero());
		}
	}

	// and the led ring for the master strip
	blank_jog_ring ();

	_last_master_gain_written = 0.0f;
}

void
Surface::periodic (uint64_t now_usecs)
{
	if (_active) {
		master_gain_changed();
		for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
			(*s)->periodic (now_usecs);
		}
	}
}

void
Surface::redisplay (ARDOUR::microseconds_t now, bool force)
{
	for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->redisplay (now, force);
	}
}

void
Surface::write (const MidiByteArray& data)
{
	if (_active) {
		_port->write (data);
	} else {
		DEBUG_TRACE (DEBUG::US2400, "surface not active, write ignored\n");
	}
}

void
Surface::update_strip_selection ()
{
	Strips::iterator s = strips.begin();
	for ( ; s != strips.end(); ++s) {
		(*s)->update_selection_state();
	}
}

void
Surface::map_stripables (const vector<boost::shared_ptr<Stripable> >& stripables)
{
	vector<boost::shared_ptr<Stripable> >::const_iterator r;
	Strips::iterator s = strips.begin();

	DEBUG_TRACE (DEBUG::US2400, string_compose ("Mapping %1 stripables to %2 strips\n", stripables.size(), strips.size()));

	for (r = stripables.begin(); r != stripables.end() && s != strips.end(); ++s) {

		/* don't try to assign stripables to a locked strip. it won't
		   use it anyway, but if we do, then we get out of sync
		   with the proposed mapping.
		*/

		if (!(*s)->locked()) {
			DEBUG_TRACE (DEBUG::US2400, string_compose ("Mapping stripable \"%1\" to strip %2\n", (*r)->name(), (*s)->global_index()));
			(*s)->set_stripable (*r);
			++r;
		}
	}

	for (; s != strips.end(); ++s) {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("strip %1 being set to null stripable\n", (*s)->global_index()));
		(*s)->reset_stripable ();
	}
}

void
Surface::subview_mode_changed ()
{
	for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->subview_mode_changed ();
	}
	
	//channel selection likely changed.  disable the joystick so it doesn't send spurious messages
	if (_stype == st_joy) {
		_joystick_active = false;
	}
}

void
Surface::say_hello ()
{
	/* wakeup for Mackie Control */
	MidiByteArray wakeup (7, MIDI::sysex, 0x00, 0x00, 0x66, 0x14, 0x00, MIDI::eox);
	_port->write (wakeup);
	wakeup[4] = 0x15; /* wakup Mackie XT */
	_port->write (wakeup);
	wakeup[4] = 0x10; /* wakeup Logic Control */
	_port->write (wakeup);
	wakeup[4] = 0x11; /* wakeup Logic Control XT */
	_port->write (wakeup);
}

void
Surface::next_jog_mode ()
{
}

void
Surface::set_jog_mode (JogWheel::Mode)
{
}

bool
Surface::stripable_is_locked_to_strip (boost::shared_ptr<Stripable> stripable) const
{
	for (Strips::const_iterator s = strips.begin(); s != strips.end(); ++s) {
		if ((*s)->stripable() == stripable && (*s)->locked()) {
			return true;
		}
	}
	return false;
}

bool
Surface::stripable_is_mapped (boost::shared_ptr<Stripable> stripable) const
{
	for (Strips::const_iterator s = strips.begin(); s != strips.end(); ++s) {
		if ((*s)->stripable() == stripable) {
			return true;
		}
	}

	return false;
}

void
Surface::notify_metering_state_changed()
{
	for (Strips::const_iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->notify_metering_state_changed ();
	}
}

void
Surface::reset ()
{
	if (_port) {
		/* reset msg for Mackie Control */
		MidiByteArray msg;
		msg << sysex_hdr();
		msg << 0x08;
		msg << 0x00;
		msg << MIDI::eox;
		_port->write (msg);
	}
}

void
Surface::toggle_backlight ()
{
return;  //avoid sending anything that might be misconstrued
}

void
Surface::recalibrate_faders ()
{
return;  //avoid sending anything that might be misconstrued
}

void
Surface::set_touch_sensitivity (int sensitivity)
{
	/* NOTE: assumed called from GUI code, hence sleep() */

	/* sensitivity already clamped by caller */

	if (_port) {
		MidiByteArray msg;

		msg << sysex_hdr ();
		msg << 0x0e;
		msg << 0xff; /* overwritten for each fader below */
		msg << (sensitivity & 0x7f);
		msg << MIDI::eox;

		for (int fader = 0; fader < 9; ++fader) {
			msg[6] = fader;
			_port->write (msg);
		}
	}
}

void
Surface::hui_heartbeat ()
{
	if (!_port) {
		return;
	}

	MidiByteArray msg (3, MIDI::on, 0x0, 0x0);
	_port->write (msg);
}

void
Surface::connected ()
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("Surface %1 now connected, trying to ping device...\n", _name));

	say_hello ();
}


