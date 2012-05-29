#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <cmath>

#include "midi++/port.h"
#include "midi++/manager.h"

#include "ardour/automation_control.h"
#include "ardour/debug.h"
#include "ardour/route.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/utils.h"

#include "control_group.h"
#include "surface_port.h"
#include "surface.h"
#include "strip.h"
#include "mackie_control_protocol.h"
#include "jog_wheel.h"

#include "strip.h"
#include "button.h"
#include "led.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace Mackie;
using ARDOUR::Route;
using ARDOUR::Panner;
using ARDOUR::Pannable;
using ARDOUR::AutomationControl;

#define ui_context() MackieControlProtocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

// The MCU sysex header.4th byte Will be overwritten
// when we get an incoming sysex that identifies
// the device type
static MidiByteArray mackie_sysex_hdr  (5, MIDI::sysex, 0x0, 0x0, 0x66, 0x14);

// The MCU extender sysex header.4th byte Will be overwritten
// when we get an incoming sysex that identifies
// the device type
static MidiByteArray mackie_sysex_hdr_xt  (5, MIDI::sysex, 0x0, 0x0, 0x66, 0x15);

static MidiByteArray empty_midi_byte_array;

Surface::Surface (MackieControlProtocol& mcp, const std::string& device_name, uint32_t number, surface_type_t stype)
	: _mcp (mcp)
	, _stype (stype)
	, _number (number)
	, _name (device_name)
	, _active (false)
	, _connected (false)
	, _jog_wheel (0)
{
	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init\n");
	
	_port = new SurfacePort (*this);

	/* only the first Surface object has global controls */

	if (_number == 0) {
		if (_mcp.device_info().has_global_controls()) {
			init_controls ();
		}

		if (_mcp.device_info().has_master_fader()) {
			setup_master ();
		}
	}

	uint32_t n = _mcp.device_info().strip_cnt();
	
	if (n) {
		init_strips (n);
	}
	
	connect_to_signals ();

	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init finish\n");
}

Surface::~Surface ()
{
	DEBUG_TRACE (DEBUG::MackieControl, "Surface: destructor\n");

	zero_all ();

	// delete groups
	for (Groups::iterator it = groups.begin(); it != groups.end(); ++it) {
		delete it->second;
	}
	
	// delete controls
	for (Controls::iterator it = controls.begin(); it != controls.end(); ++it) {
		delete *it;
	}
	
	delete _jog_wheel;
	delete _port;
}

const MidiByteArray& 
Surface::sysex_hdr() const
{
	switch  (_stype) {
	case mcu: return mackie_sysex_hdr;
	case ext: return mackie_sysex_hdr_xt;
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

	groups["assignment"] = new Group  ("assignment");
	groups["automation"] = new Group  ("automation");
	groups["bank"] = new Group  ("bank");
	groups["cursor"] = new Group  ("cursor");
	groups["display"] = new Group  ("display");
	groups["functions"] = new Group  ("functions");
	groups["modifiers"] = new Group  ("modifiers");
	groups["none"] = new Group  ("none");
	groups["transport"] = new Group  ("transport");
	groups["user"] = new Group  ("user");
	groups["master"] = new Group ("master");
	groups["view"] = new Group ("view");
		
	if (_mcp.device_info().has_jog_wheel()) {
		_jog_wheel = new Mackie::JogWheel (_mcp);
	}

	for (uint32_t n = 0; mackie_global_controls[n].name[0]; ++n) {
		group = groups[mackie_global_controls[n].group_name];
		Control* control = mackie_global_controls[n].factory (*this, mackie_global_controls[n].id, mackie_global_controls[n].name, *group);
		controls_by_device_independent_id[mackie_global_controls[n].id] = control;
	}

	/* add global buttons */

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

	for (uint32_t i = 0; i < n; ++i) {

		char name[32];
		
		snprintf (name, sizeof (name), "strip_%d", (8* _number) + i);

		Strip* strip = new Strip (*this, name, i, strip_buttons);
		
		groups[name] = strip;
		strips.push_back (strip);
	}
}

void
Surface::setup_master ()
{
	_master_fader = dynamic_cast<Fader*> (Fader::factory (*this, 8, "master", *groups["master"]));
	
	boost::shared_ptr<Route> m;
	
	if ((m = _mcp.get_session().monitor_out()) == 0) {
		m = _mcp.get_session().master_out();
	} 
	
	if (!m) {
		return;
	}
	
	_master_fader->set_control (m->gain_control());
	m->gain_control()->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&Surface::master_gain_changed, this), ui_context());
}

void
Surface::master_gain_changed ()
{
	boost::shared_ptr<AutomationControl> ac = _master_fader->control();
	float pos = ac->internal_to_interface (ac->get_value());
	_port->write (_master_fader->set_position (pos));
}

float 
Surface::scaled_delta (float delta, float current_speed)
{
	/* XXX needs work before use */
	const float sign = delta < 0.0 ? -1.0 : 1.0;

	return ((sign * std::pow (delta + 1.0, 2.0)) + current_speed) / 100.0;
}

void 
Surface::display_bank_start (uint32_t current_bank)
{
	if  (current_bank == 0) {
		// send Ar. to 2-char display on the master
		show_two_char_display ("Ar", "..");
	} else {
		// write the current first remote_id to the 2-char display
		show_two_char_display (current_bank);
	}
}

void 
Surface::blank_jog_ring ()
{
	Control* control = controls_by_device_independent_id[Jog::ID];

	if (control) {
		Pot* pot = dynamic_cast<Pot*> (control);
		if (pot) {
			_port->write (pot->set (0.0, false, Pot::spread));
		}
	}
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


		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 connecting to signals on port %2\n", 
								   number(), _port->input_port().name()));

		MIDI::Parser* p = _port->input_port().parser();

		/* Incoming sysex */
		p->sysex.connect_same_thread (*this, boost::bind (&Surface::handle_midi_sysex, this, _1, _2, _3));
		/* V-Pot messages are Controller */
		p->controller.connect_same_thread (*this, boost::bind (&Surface::handle_midi_controller_message, this, _1, _2));
		/* Button messages are NoteOn */
		p->note_on.connect_same_thread (*this, boost::bind (&Surface::handle_midi_note_on_message, this, _1, _2));
		/* Button messages are NoteOn. libmidi++ sends note-on w/velocity = 0 as note-off so catch them too */
		p->note_off.connect_same_thread (*this, boost::bind (&Surface::handle_midi_note_on_message, this, _1, _2));
		/* Fader messages are Pitchbend */
		p->channel_pitchbend[0].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 0U));
		p->channel_pitchbend[1].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 1U));
		p->channel_pitchbend[2].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 2U));
		p->channel_pitchbend[3].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 3U));
		p->channel_pitchbend[4].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 4U));
		p->channel_pitchbend[5].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 5U));
		p->channel_pitchbend[6].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 6U));
		p->channel_pitchbend[7].connect_same_thread (*this, boost::bind (&Surface::handle_midi_pitchbend_message, this, _1, _2, 7U));
		
		_connected = true;
	}
}

void
Surface::handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t pb, uint32_t fader_id)
{
	/* Pitchbend messages are fader messages. Nothing in the data we get
	 * from the MIDI::Parser conveys the fader ID, which was given by the
	 * channel ID in the status byte.
	 *
	 * Instead, we have used bind() to supply the fader-within-strip ID 
	 * when we connected to the per-channel pitchbend events.
	 */


	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handle_midi pitchbend on port %3, fader = %1 value = %2\n", 
							   fader_id, pb, _number));
	
	if (_mcp.device_info().no_handshake()) {
		turn_it_on ();
	}

	Fader* fader = faders[fader_id];

	if (fader) {
		Strip* strip = dynamic_cast<Strip*> (&fader->group());
		float pos = (pb >> 4)/1023.0; // only the top 10 bytes are used
		if (strip) {
			strip->handle_fader (*fader, pos);
		} else {
			/* master fader */
			fader->set_value (pos); // alter master gain
			_port->write (fader->set_position (pos)); // write back value (required for servo)
		}
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "fader not found\n");
	}
}

void 
Surface::handle_midi_note_on_message (MIDI::Parser &, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("SurfacePort::handle_note_on %1 = %2\n", (int) ev->note_number, (int) ev->velocity));
	
	if (_mcp.device_info().no_handshake()) {
		turn_it_on ();
	}

	Button* button = buttons[ev->note_number];

	if (button) {
		Strip* strip = dynamic_cast<Strip*> (&button->group());

		if (strip) {
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip %1 button %2 pressed ? %3\n",
									   strip->index(), button->name(), (ev->velocity > 64)));
			strip->handle_button (*button, ev->velocity > 64 ? press : release);
		} else {
			/* global button */
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("global button %1\n", button->id()));
			_mcp.handle_button_event (*this, *button, ev->velocity > 64 ? press : release);
		}
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("no button found for %1\n", ev->note_number));
	}
}

void 
Surface::handle_midi_controller_message (MIDI::Parser &, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("SurfacePort::handle_midi_controller %1 = %2\n", (int) ev->controller_number, (int) ev->value));

	if (_mcp.device_info().no_handshake()) {
		turn_it_on ();
	}

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
	float delta = sign * (ticks / (float) 0x3f);
	
	if (!pot) {
		if (ev->controller_number == Jog::ID && _jog_wheel) {

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Jog wheel moved %1\n", ticks));
			_jog_wheel->jog_event (delta);
			return;
		}

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

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handle_midi_sysex: %1\n", bytes));

	if (_mcp.device_info().no_handshake()) {
		turn_it_on ();
	}

	/* always save the device type ID so that our outgoing sysex messages
	 * are correct 
	 */

	if (_stype == mcu) {
		mackie_sysex_hdr[3] = bytes[4];
	} else {
		mackie_sysex_hdr_xt[3] = bytes[4];
	}

	switch (bytes[5]) {
	case 0x01:
		/* MCP: Device Ready 
		   LCP: Connection Challenge 
		*/
		if (bytes[4] == 0x10 || bytes[4] == 0x11) {
			write_sysex (host_connection_query (bytes));
		} else {
			if (!_active) {
				turn_it_on ();
			}
		}
		break;

	case 0x03: /* LCP Connection Confirmation */
		if (bytes[4] == 0x10 || bytes[4] == 0x11) {
			write_sysex (host_connection_confirmation (bytes));
			_active = true;
		}
		break;

	case 0x04: /* LCP: Confirmation Denied */
		_active = false;
		break;
	default:
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
	retval <<  (0x7f &  ( (l[2] >> l[3]) ^  (l[0] + l[3])));
	retval <<  (0x7f &  ((l[3] -  (l[2] << 2)) ^  (l[0] | l[1])));
	retval <<  (0x7f &  (l[1] - l[2] +  (0xf0 ^  (l[3] << 4))));
	
	return retval;
}

// not used right now
MidiByteArray 
Surface::host_connection_query (MidiByteArray & bytes)
{
	MidiByteArray response;
	
	if (bytes[4] != 0x10 && bytes[4] != 0x11) {
		/* not a Logic Control device - no response required */
		return response;
	}

	// handle host connection query
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host connection query: %1\n", bytes));
	
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

// not used right now
MidiByteArray 
Surface::host_connection_confirmation (const MidiByteArray & bytes)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("host_connection_confirmation: %1\n", bytes));
	
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
	if (!_active) {
		_active = true;
		zero_controls ();
		for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
			(*s)->notify_all ();
		}
		update_view_mode_display ();
	}
}

void 
Surface::handle_port_inactive (SurfacePort*)
{
	_active = false;
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
	// TODO turn off Timecode displays

	// zero all strips
	for (Strips::iterator it = strips.begin(); it != strips.end(); ++it) {
		(*it)->zero();
	}

	zero_controls ();
}

void
Surface::zero_controls ()
{
	if (_stype != mcu || !_mcp.device_info().has_global_controls()) {
		return;
	}

	// turn off global buttons and leds
        // global buttons are only ever on mcu_port, so we don't have
	// to figure out which port.

	for (Controls::iterator it = controls.begin(); it != controls.end(); ++it) {
		Control & control = **it;
		if (!control.group().is_strip()) {
			_port->write (control.zero());
		}
	}

	if (_number == 0 && _mcp.device_info().has_two_character_display()) {
		// any hardware-specific stuff
		// clear 2-char display
		show_two_char_display (" ");
	}

	// and the led ring for the master strip
	blank_jog_ring ();
}

void
Surface::periodic (uint64_t now_usecs)
{
	for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->periodic (now_usecs);
	}
}

void
Surface::write (const MidiByteArray& data) 
{
	if (_active) {
		_port->write (data);
	}
}

void
Surface::map_routes (const vector<boost::shared_ptr<Route> >& routes)
{
	vector<boost::shared_ptr<Route> >::const_iterator r;
	Strips::iterator s = strips.begin();

	for (r = routes.begin(); r != routes.end() && s != strips.end(); ++s) {

		/* don't try to assign routes to a locked strip. it won't
		   use it anyway, but if we do, then we get out of sync
		   with the proposed mapping.
		*/

		if (!(*s)->locked()) {
			(*s)->set_route (*r);
			++r;
		}
	}

	for (; s != strips.end(); ++s) {
		(*s)->set_route (boost::shared_ptr<Route>());
	}


}

static char 
translate_seven_segment (char achar)
{
	achar = toupper (achar);

	if  (achar >= 0x40 && achar <= 0x60) {
		return achar - 0x40;
	} else if  (achar >= 0x21 && achar <= 0x3f) {
		return achar;
	} else {
		return 0x00;
	}
}

void
Surface::show_two_char_display (const std::string & msg, const std::string & dots)
{
	if (_stype != mcu || !_mcp.device_info().has_two_character_display() || msg.length() != 2 || dots.length() != 2) {
		return;
	}
	
	MidiByteArray right (3, 0xb0, 0x4b, 0x00);
	MidiByteArray left (3, 0xb0, 0x4a, 0x00);
	
	right[2] = translate_seven_segment (msg[0]) +  (dots[0] == '.' ? 0x40 : 0x00);
	left[2] = translate_seven_segment (msg[1]) +  (dots[1] == '.' ? 0x40 : 0x00);
	
	_port->write (right);
	_port->write (left);
}

void
Surface::show_two_char_display (unsigned int value, const std::string & /*dots*/)
{
	ostringstream os;
	os << setfill('0') << setw(2) << value % 100;
	show_two_char_display (os.str());
}

void
Surface::display_timecode (const std::string & timecode, const std::string & last_timecode)
{
	if (!_active || _mcp.device_info().has_timecode_display()) {
		return;
	}
	// if there's no change, send nothing, not even sysex header
	if  (timecode == last_timecode) return;
	
	// length sanity checking
	string local_timecode = timecode;

	// truncate to 10 characters
	if  (local_timecode.length() > 10) {
		local_timecode = local_timecode.substr (0, 10);
	}

	// pad to 10 characters
	while  (local_timecode.length() < 10) { 
		local_timecode += " ";
	}
		
	// find the suffix of local_timecode that differs from last_timecode
	std::pair<string::const_iterator,string::iterator> pp = mismatch (last_timecode.begin(), last_timecode.end(), local_timecode.begin());
	
	int position = 0x40;

	// translate characters. These are sent in reverse order of display
	// hence the reverse iterators
	string::reverse_iterator rend = reverse_iterator<string::iterator> (pp.second);
	for  (string::reverse_iterator it = local_timecode.rbegin(); it != rend; ++it) {
		MidiByteArray retval (2, 0xb0, position++);
		retval << translate_seven_segment (*it);
		_port->write (retval);
	}
}

void
Surface::update_flip_mode_display ()
{
	for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->flip_mode_changed (true);
	}
}

void
Surface::update_view_mode_display ()
{
	string text;
	int id = -1;

	if (!_active) {
		return;
	}

	switch (_mcp.view_mode()) {
	case MackieControlProtocol::Mixer:
		show_two_char_display ("Mx");
		id = Button::Pan;
		break;
	case MackieControlProtocol::Dynamics:
		show_two_char_display ("Dy");
		id = Button::Dyn;
		break;
	case MackieControlProtocol::EQ:
		show_two_char_display ("EQ");
		id = Button::Eq;
		break;
	case MackieControlProtocol::Loop:
		show_two_char_display ("LP");
		id = Button::Loop;
		break;
	case MackieControlProtocol::AudioTracks:
		show_two_char_display ("AT");
		break;
	case MackieControlProtocol::MidiTracks:
		show_two_char_display ("MT");
		break;
	case MackieControlProtocol::Sends:
		show_two_char_display ("Sn");
		id = Button::Sends;
		break;
	case MackieControlProtocol::Plugins:
		show_two_char_display ("Pl");
		id = Button::Plugin;
		break;
	default:
		break;
	}

	if (id >= 0) {
		
		/* we are attempting to turn a global button/LED on */

		map<int,Control*>::iterator x = controls_by_device_independent_id.find (id);

		if (x != controls_by_device_independent_id.end()) {
			Button* button = dynamic_cast<Button*> (x->second);
			if (button) {
				_port->write (button->set_state (on));
			}
		}
	}

	if (!text.empty()) {
		for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
			_port->write ((*s)->display (1, text));
		}
	}
}

void
Surface::gui_selection_changed (const ARDOUR::StrongRouteNotificationList& routes)
{
	for (Strips::iterator s = strips.begin(); s != strips.end(); ++s) {
		(*s)->gui_selection_changed (routes);
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
	wakeup[4] = 0x10; /* wakupe Logic Control */
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
Surface::route_is_locked_to_strip (boost::shared_ptr<Route> r) const
{
	for (Strips::const_iterator s = strips.begin(); s != strips.end(); ++s) {
		if ((*s)->route() == r && (*s)->locked()) {
			return true;
		}
	}
	return false;
}
