/*
 * Copyright (C) 2016 W.P. van Paass
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 *
 * Thanks to Rolf Meyerhoff for reverse engineering the CC121 protocol.
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

#include <cstdlib>
#include <sstream>
#include <algorithm>

#include <stdint.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/compose.h"
#include "pbd/xml++.h"

#include "midi++/port.h"

#include "control_protocol/basic_ui.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/amp.h"
#include "ardour/bundle.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"
#include "ardour/monitor_control.h"
#include "ardour/monitor_processor.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/record_enable_control.h"
#include "ardour/stripable.h"
#include "ardour/session.h"
#include "ardour/session_configuration.h"
#include "ardour/track.h"

#include "cc121.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

CC121::CC121 (Session& s)
	: ControlProtocol (s, _("Steinberg CC121"))
	, AbstractUI<CC121Request> (name())
	, gui (0)
	, connection_state (ConnectionState (0))
	, _device_active (false)
	, fader_msb (0)
	, fader_lsb (0)
	, fader_is_touched (false)
        , _jogmode(scroll)
	, button_state (ButtonState (0))
	, blink_state (false)
	, rec_enable_state (false)
{
	last_encoder_time = 0;

	boost::shared_ptr<ARDOUR::Port> inp;
	boost::shared_ptr<ARDOUR::Port> outp;

	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "CC121 Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "CC121 Send", true);

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(inp);
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(outp);

	if (_input_port == 0 || _output_port == 0) {
		throw failed_constructor();
	}

	_input_bundle.reset (new ARDOUR::Bundle (_("CC121 Support (Receive)"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("CC121 Support (Send) "), false));

	_input_bundle->add_channel (
		"",
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (inp->name())
		);

	_output_bundle->add_channel (
		"",
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (outp->name())
		);


	/* Catch port connections and disconnections */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&CC121::connection_handler, this, _1, _2, _3, _4, _5), this);
	buttons.insert (std::make_pair (EButton, Button (*this, _("EButton"), EButton)));
	buttons.insert (std::make_pair (OpenVST, Button (*this, _("OpenVST"), OpenVST)));
	buttons.insert (std::make_pair (InputMonitor, Button (*this, _("InputMonitor"), InputMonitor)));
	buttons.insert (std::make_pair (EQ1Enable, Button (*this, _("EQ1Enable"), EQ1Enable)));
	buttons.insert (std::make_pair (EQ2Enable, Button (*this, _("EQ2Enable"), EQ2Enable)));
	buttons.insert (std::make_pair (EQ3Enable, Button (*this, _("EQ3Enable"), EQ3Enable)));
	buttons.insert (std::make_pair (EQ4Enable, Button (*this, _("EQ4Enable"), EQ4Enable)));
	buttons.insert (std::make_pair (EQType, Button (*this, _("EQType"), EQType)));
	buttons.insert (std::make_pair (AllBypass, Button (*this, _("AllBypass"), AllBypass)));
	buttons.insert (std::make_pair (Function1, Button (*this, _("Function1"), Function1)));
	buttons.insert (std::make_pair (Function2, Button (*this, _("Function2"), Function2)));
	buttons.insert (std::make_pair (Function3, Button (*this, _("Function3"), Function3)));
	buttons.insert (std::make_pair (Function4, Button (*this, _("Function4"), Function4)));
	buttons.insert (std::make_pair (Value, Button (*this, _("Value"), Value)));
	buttons.insert (std::make_pair (Jog, Button (*this, _("Jog"), Jog)));
	buttons.insert (std::make_pair (Lock, Button (*this, _("Lock"), Lock)));
	buttons.insert (std::make_pair (ToStart, Button (*this, _("ToStart"), ToStart)));
	buttons.insert (std::make_pair (ToEnd, Button (*this, _("ToEnd"), ToEnd)));
	buttons.insert (std::make_pair (Mute, Button (*this, _("Mute"), Mute)));
	buttons.insert (std::make_pair (Solo, Button (*this, _("Solo"), Solo)));
	buttons.insert (std::make_pair (Rec, Button (*this, _("Rec"), Rec)));
	buttons.insert (std::make_pair (Left, Button (*this, _("Left"), Left)));
	buttons.insert (std::make_pair (Right, Button (*this, _("Right"), Right)));
	buttons.insert (std::make_pair (Output, Button (*this, _("Output"), Output)));
	buttons.insert (std::make_pair (FP_Read, Button (*this, _("Read"), FP_Read)));
	buttons.insert (std::make_pair (FP_Write, Button (*this, _("Write"), FP_Write)));
	buttons.insert (std::make_pair (Loop, Button (*this, _("Loop"), Loop)));
	buttons.insert (std::make_pair (Rewind, Button (*this, _("Rewind"), Rewind)));
	buttons.insert (std::make_pair (Ffwd, Button (*this, _("Ffwd"), Ffwd)));
	buttons.insert (std::make_pair (Stop, Button (*this, _("Stop"), Stop)));
	buttons.insert (std::make_pair (Play, Button (*this, _("Play"), Play)));
	buttons.insert (std::make_pair (RecEnable, Button (*this, _("RecEnable"), RecEnable)));
	buttons.insert (std::make_pair (Footswitch, Button (*this, _("Footswitch"), Footswitch)));
	buttons.insert (std::make_pair (FaderTouch, Button (*this, _("Fader (touch)"), FaderTouch)));

	get_button (Left).set_action ( boost::bind (&CC121::left, this), true);
	get_button (Right).set_action ( boost::bind (&CC121::right, this), true);

	get_button (FP_Read).set_action (boost::bind (&CC121::read, this), true);
	get_button (FP_Write).set_action (boost::bind (&CC121::write, this), true);
	get_button (EButton).set_action (boost::bind (&CC121::touch, this), true);
	get_button (OpenVST).set_action (boost::bind (&CC121::off, this), true);

	get_button (Play).set_action (boost::bind (&BasicUI::transport_play, this, true), true);
	get_button (ToStart).set_action (boost::bind (&BasicUI::prev_marker, this), true);
	get_button (ToEnd).set_action (boost::bind (&BasicUI::next_marker, this), true);
	get_button (RecEnable).set_action (boost::bind (&BasicUI::rec_enable_toggle, this), true);
	get_button (Stop).set_action (boost::bind (&BasicUI::transport_stop, this), true);
	get_button (Ffwd).set_action (boost::bind (&BasicUI::ffwd, this), true);

	get_button (Rewind).set_action (boost::bind (&BasicUI::rewind, this), true);
	get_button (Loop).set_action (boost::bind (&BasicUI::loop_toggle, this), true);

	get_button (Jog).set_action (boost::bind (&CC121::jog, this), true);
	get_button (Mute).set_action (boost::bind (&CC121::mute, this), true);
	get_button (Solo).set_action (boost::bind (&CC121::solo, this), true);
	get_button (Rec).set_action (boost::bind (&CC121::rec_enable, this), true);

	get_button (InputMonitor).set_action (boost::bind (&CC121::input_monitor, this), true);
}

CC121::~CC121 ()
{
	all_lights_out ();

	if (_input_port) {
		DEBUG_TRACE (DEBUG::CC121, string_compose ("unregistering input port %1\n", boost::shared_ptr<ARDOUR::Port>(_input_port)->name()));
		AudioEngine::instance()->unregister_port (_input_port);
		_input_port.reset ();
	}

	if (_output_port) {
		_output_port->drain (10000,  250000); /* check every 10 msecs, wait up to 1/4 second for the port to drain */
		DEBUG_TRACE (DEBUG::CC121, string_compose ("unregistering output port %1\n", boost::shared_ptr<ARDOUR::Port>(_output_port)->name()));
		AudioEngine::instance()->unregister_port (_output_port);
		_output_port.reset ();
	}

	tear_down_gui ();

	/* stop event loop */
	DEBUG_TRACE (DEBUG::CC121, "BaseUI::quit ()\n");
	BaseUI::quit ();
}

void*
CC121::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
CC121::start_midi_handling ()
{
	/* handle buttons press */
        _input_port->parser()->channel_note_on[0].connect_same_thread (midi_connections, boost::bind (&CC121::button_press_handler, this, _1, _2));
	/* handle buttons release*/
        _input_port->parser()->channel_note_off[0].connect_same_thread (midi_connections, boost::bind (&CC121::button_release_handler, this, _1, _2));
	/* handle fader */
        _input_port->parser()->pitchbend.connect_same_thread (midi_connections, boost::bind (&CC121::fader_handler, this, _1, _2));
	/* handle encoder */
	_input_port->parser()->controller.connect_same_thread (midi_connections, boost::bind (&CC121::encoder_handler, this, _1, _2));

	/* This connection means that whenever data is ready from the input
	 * port, the relevant thread will invoke our ::midi_input_handler()
	 * method, which will read the data, and invoke the parser.
	 */

	_input_port->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &CC121::midi_input_handler), _input_port));
	_input_port->xthread().attach (main_loop()->get_context());
}

void
CC121::stop_midi_handling ()
{
	midi_connections.drop_connections ();

	/* Note: the input handler is still active at this point, but we're no
	 * longer connected to any of the parser signals
	 */
}

void
CC121::do_request (CC121Request* req)
{
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
CC121::stop ()
{
	BaseUI::quit ();

	return 0;
}

void
CC121::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

void
CC121::all_lights_out ()
{
	for (ButtonMap::iterator b = buttons.begin(); b != buttons.end(); ++b) {
		b->second.set_led_state (_output_port, false);
	}
}

CC121::Button&
CC121::get_button (ButtonID id) const
{
	ButtonMap::const_iterator b = buttons.find (id);
	assert (b != buttons.end());
	return const_cast<Button&>(b->second);
}

void
CC121::button_press_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	DEBUG_TRACE (DEBUG::CC121, string_compose ("button press event for ID %1 press ? %2\n", (int) tb->controller_number, (tb->value ? "yes" : "no")));

	ButtonID id (ButtonID (tb->controller_number));
	Button& button (get_button (id));

	buttons_down.insert (id);
	ButtonState bs (ButtonState (0));

	switch (id) {
	case FaderTouch:
	  fader_is_touched = true;
		if (_current_stripable) {
			boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
			if (gain) {
				timepos_t now (session->engine().sample_time());
			  gain->start_touch (now);
			}
		}
		break;
	default:
	  break;
	}

	if (bs) {
		button_state = ButtonState (button_state|bs);
		DEBUG_TRACE (DEBUG::CC121, string_compose ("reset button state to %1 using %2\n", button_state, (int) bs));
	}

	if (button.uses_flash()) {
		button.set_led_state (_output_port, (int)tb->value);
	}

	set<ButtonID>::iterator c = consumed.find (id);

	if (c == consumed.end()) {
		button.invoke (button_state, true);
	} else {
		DEBUG_TRACE (DEBUG::CC121, "button was consumed, ignored\n");
		consumed.erase (c);
	}
}

void
CC121::button_release_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	DEBUG_TRACE (DEBUG::CC121, string_compose ("button release event for ID %1 release ? %2\n", (int) tb->controller_number, (tb->value ? "yes" : "no")));

	ButtonID id (ButtonID (tb->controller_number));
	Button& button (get_button (id));

	buttons_down.erase (id);
	button.timeout_connection.disconnect ();

	ButtonState bs (ButtonState (0));

	switch (id) {
	case FaderTouch:
		fader_is_touched = false;
		if (_current_stripable) {
			boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
			if (gain) {
				timepos_t now (session->engine().sample_time());
				gain->stop_touch (now);
			}
		}
		break;
	default:
		break;
	}

	if (bs) {
		button_state = ButtonState (button_state&~bs);
		DEBUG_TRACE (DEBUG::CC121, string_compose ("reset button state to %1 using %2\n", button_state, (int) bs));
	}

	if (button.uses_flash()) {
		button.set_led_state (_output_port, (int)tb->value);
	}

	set<ButtonID>::iterator c = consumed.find (id);

	if (c == consumed.end()) {
		button.invoke (button_state, false);
	} else {
		DEBUG_TRACE (DEBUG::CC121, "button was consumed, ignored\n");
		consumed.erase (c);
	}
}

void
CC121::encoder_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
        DEBUG_TRACE (DEBUG::CC121, "encoder handler");

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (_current_stripable);
	/* Extract absolute value*/
	float adj = static_cast<float>(tb->value & ~0x40);
	/* Get direction (negative values start at 0x40)*/
	float sign = (tb->value & 0x40) ? -1.0 : 1.0;

	/* Get amount of change (encoder clicks) * (change per click)
	 * Create an exponential curve
	 */
	float curve = sign * powf (adj, (1.f + 10.f) / 10.f);
	adj = curve * (31.f / 1000.f);

	switch(tb->controller_number) {
	case 0x10:
	  /* pan */
	  if (r) { set_controllable (r->pan_azimuth_control(), adj); }
	  break;
	case 0x20:
	  /* EQ 1 Q */
	  if (r) { set_controllable (r->eq_q_controllable(0), adj); }
	  break;
	case 0x21:
	  /* EQ 2 Q */
	  if (r) { set_controllable (r->eq_q_controllable(1), adj); }
	  break;
	case 0x22:
	  /* EQ 3 Q */
	  if (r) { set_controllable (r->eq_q_controllable(2), adj); }
	  break;
	case 0x23:
	  /* EQ 4 Q */
	  if (r) { set_controllable (r->eq_q_controllable(3), adj); }
	  break;
	case 0x30:
	  /* EQ 1 Frequency */
	  if (r) { set_controllable (r->eq_freq_controllable(0), adj); }
	  break;
	case 0x31:
	  /* EQ 2 Frequency */
	  if (r) { set_controllable (r->eq_freq_controllable(1), adj); }
	  break;
	case 0x32:
	  /* EQ 3 Frequency */
	  if (r) { set_controllable (r->eq_freq_controllable(2), adj); }
	  break;
	case 0x33:
	  /* EQ 4 Frequency */
	  if (r) { set_controllable (r->eq_freq_controllable(3), adj); }
	  break;
	case 0x3C:
	  /* AI */
	  if (sign < 0.0f) {
	    if (_jogmode == scroll) {
	      ScrollTimeline(-0.05);
	    }
	    else {
	      ZoomIn();
	    }
	  }
	  else {
	    if (_jogmode == scroll) {
	      ScrollTimeline(0.05);
	    }
	    else {
	      ZoomOut();
	    }
	  }
	  break;
	case 0x40:
	  /* EQ 1 Gain */
	  if (r) { set_controllable (r->eq_gain_controllable(0), adj); }
	  break;
	case 0x41:
	  /* EQ 2 Gain */
	  if (r) { set_controllable (r->eq_gain_controllable(1), adj); }
	  break;
	case 0x42:
	  /* EQ 3 Gain */
	  if (r) { set_controllable (r->eq_gain_controllable(2), adj); }
	  break;
	case 0x43:
	  /* EQ 4 Gain */
	  if (r) { set_controllable (r->eq_gain_controllable(3), adj); }
	  break;
	case 0x50:
	  /* Value */
	  break;
	default:
	  break;
	}
}

void
CC121::fader_handler (MIDI::Parser &, MIDI::pitchbend_t pb)
{
        DEBUG_TRACE (DEBUG::CC121, "fader handler");

	if (_current_stripable) {
	  boost::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
	  if (gain) {
	    float val = gain->interface_to_internal (pb/16384.0);
	    /* even though the cc121 only controls a
	       single stripable at a time, allow the fader to
	       modify the group, if appropriate.
	    */
	    _current_stripable->gain_control()->set_value (val, Controllable::UseGroup);
	  }
	}
}

int
CC121::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::CC121, string_compose("CC121::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		/* start event loop */

		BaseUI::run ();

		connect_session_signals ();

		Glib::RefPtr<Glib::TimeoutSource> blink_timeout = Glib::TimeoutSource::create (200); // milliseconds
		blink_connection = blink_timeout->connect (sigc::mem_fun (*this, &CC121::blink));
		blink_timeout->attach (main_loop()->get_context());

		Glib::RefPtr<Glib::TimeoutSource> heartbeat_timeout = Glib::TimeoutSource::create (800); // milliseconds
		heartbeat_connection = heartbeat_timeout->connect (sigc::mem_fun (*this, &CC121::beat));
		heartbeat_timeout->attach (main_loop()->get_context());

		Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (100); // milliseconds
		periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &CC121::periodic));
		periodic_timeout->attach (main_loop()->get_context());

	} else {

		BaseUI::quit ();
		close ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::CC121, string_compose("CC121::set_active done with yn: '%1'\n", yn));

	return 0;
}

bool
CC121::periodic ()
{
	if (!_current_stripable) {
		return true;
	}

	ARDOUR::AutoState gain_state = _current_stripable->gain_control()->automation_state();

	if (gain_state == ARDOUR::Touch || gain_state == ARDOUR::Play) {
		map_gain ();
	}

	return true;
}

void
CC121::stop_blinking (ButtonID id)
{
	blinkers.remove (id);
	get_button (id).set_led_state (_output_port, false);
}

void
CC121::start_blinking (ButtonID id)
{
	blinkers.push_back (id);
	get_button (id).set_led_state (_output_port, true);
}

bool
CC121::beat ()
{
	MIDI::byte buf[8];

	buf[0] = 0xf0;
	buf[1] = 0x43;
	buf[2] = 0x10;
	buf[3] = 0x3e;
	buf[4] = 0x15;
	buf[5] = 0x00;
	buf[6] = 0x01;
	buf[7] = 0xF7;

	_output_port->write (buf, 8, 0);

        return true;
}

bool
CC121::blink ()
{
	blink_state = !blink_state;

	for (Blinkers::iterator b = blinkers.begin(); b != blinkers.end(); b++) {
		get_button(*b).set_led_state (_output_port, blink_state);
	}

	map_recenable_state ();

	return true;
}

void
CC121::close ()
{
	all_lights_out ();

	stop_midi_handling ();
	session_connections.drop_connections ();
	port_connection.disconnect ();
	blink_connection.disconnect ();
	heartbeat_connection.disconnect ();
	selection_connection.disconnect ();
	stripable_connections.drop_connections ();

#if 0
	stripable_connections.drop_connections ();
#endif
}

void
CC121::map_recenable_state ()
{
	/* special case for RecEnable because its status can change as a
	 * confluence of unrelated parameters: (a) session rec-enable state (b)
	 * rec-enabled tracks. So we don't add the button to the blinkers list,
	 * we just call this:
	 *
	 *  * from the blink callback
	 *  * when the session tells us about a status change
	 *
	 * We do the last one so that the button changes state promptly rather
	 * than waiting for the next blink callback. The change in "blinking"
	 * based on having record-enabled tracks isn't urgent, and that happens
	 * during the blink callback.
	 */

	bool onoff;

	switch (session->record_status()) {
	case Session::Disabled:
		onoff = false;
		break;
	case Session::Enabled:
		onoff = blink_state;
		break;
	case Session::Recording:
		if (session->have_rec_enabled_track ()) {
			onoff = true;
		} else {
			onoff = blink_state;
		}
		break;
	}

	if (onoff != rec_enable_state) {
		get_button(RecEnable).set_led_state (_output_port, onoff);
		rec_enable_state = onoff;
	}
}

void
CC121::map_transport_state ()
{
	get_button (Loop).set_led_state (_output_port, session->get_play_loop());

	float ts = get_transport_speed();

	if (ts == 0) {
		stop_blinking (Play);
	} else if (fabs (ts) == 1.0) {
		stop_blinking (Play);
		get_button (Play).set_led_state (_output_port, true);
	} else {
		start_blinking (Play);
	}

	get_button (Stop).set_led_state (_output_port, stop_button_onoff());
	get_button (Rewind).set_led_state (_output_port, rewind_button_onoff());
	get_button (Ffwd).set_led_state (_output_port, ffwd_button_onoff());
	get_button (Jog).set_led_state (_output_port, _jogmode == scroll);
}

void
CC121::connect_session_signals()
{
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_recenable_state, this), this);
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_transport_state, this), this);
}

bool
CC121::midi_input_handler (Glib::IOCondition ioc, boost::shared_ptr<ARDOUR::AsyncMIDIPort> port)
{
	DEBUG_TRACE (DEBUG::CC121, string_compose ("something happend on  %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		port->clear ();
		DEBUG_TRACE (DEBUG::CC121, string_compose ("data available on %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));
		samplepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}


XMLNode&
CC121::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());

	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_input_port)->get_state());
	node.add_child_nocopy (*child);


	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_output_port)->get_state());
	node.add_child_nocopy (*child);

	/* Save action state for Function1..4, Lock, Value, EQnEnable, EQType,
	 * AllBypass and Footswitch buttons, since these
	 * are user controlled. We can only save named-action operations, since
	 * internal functions are just pointers to functions and hard to
	 * serialize without enumerating them all somewhere.
	 */

	node.add_child_nocopy (get_button (Function1).get_state());
	node.add_child_nocopy (get_button (Function2).get_state());
	node.add_child_nocopy (get_button (Function3).get_state());
	node.add_child_nocopy (get_button (Function4).get_state());
	node.add_child_nocopy (get_button (Value).get_state());
	node.add_child_nocopy (get_button (Lock).get_state());
	node.add_child_nocopy (get_button (EQ1Enable).get_state());
	node.add_child_nocopy (get_button (EQ2Enable).get_state());
	node.add_child_nocopy (get_button (EQ3Enable).get_state());
	node.add_child_nocopy (get_button (EQ4Enable).get_state());
	node.add_child_nocopy (get_button (EQType).get_state());
	node.add_child_nocopy (get_button (AllBypass).get_state());
	node.add_child_nocopy (get_button (Footswitch).get_state());

	return node;
}

int
CC121::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;
	XMLNode const* child;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			boost::shared_ptr<ARDOUR::Port>(_input_port)->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			boost::shared_ptr<ARDOUR::Port>(_output_port)->set_state (*portnode, version);
		}
	}

	for (XMLNodeList::const_iterator n = node.children().begin(); n != node.children().end(); ++n) {
		if ((*n)->name() == X_("Button")) {
			int32_t xid;
			if (!(*n)->get_property ("id", xid)) {
				continue;
			}
			ButtonMap::iterator b = buttons.find (ButtonID (xid));
			if (b == buttons.end ()) {
				continue;
			}
			b->second.set_state (**n);
		}
	}

	return 0;
}

bool
CC121::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	DEBUG_TRACE (DEBUG::CC121, "CC121::connection_handler  start\n");
	if (!_input_port || !_output_port) {
		return false;
	}

	string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_input_port)->name());
	string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_output_port)->name());

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
		DEBUG_TRACE (DEBUG::CC121, string_compose ("Connections between %1 and %2 changed, but I ignored it\n", name1, name2));
		/* not our ports */
		return false;
	}

	if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
                DEBUG_TRACE (DEBUG::CC121, "device now connected for both input and output\n");
		connected ();

	} else {
		DEBUG_TRACE (DEBUG::CC121, "Device disconnected (input or output or both) or not yet fully connected\n");
		_device_active = false;
	}

	ConnectionChange (); /* emit signal for our GUI */

	DEBUG_TRACE (DEBUG::CC121, "CC121::connection_handler  end\n");

	return true; /* connection status changed */
}

void
CC121::connected ()
{
	DEBUG_TRACE (DEBUG::CC121, "connected");

	_device_active = true;

	start_midi_handling ();

	all_lights_out ();

	/* catch up on state */

	/* make sure that rec_enable_state is consistent with current device state */
	get_button (RecEnable).set_led_state (_output_port, rec_enable_state);

	map_transport_state ();
	map_recenable_state ();
}

void
CC121::Button::invoke (CC121::ButtonState bs, bool press)
{
	DEBUG_TRACE (DEBUG::CC121, string_compose ("invoke button %1 for %2 state %3%4%5\n", id, (press ? "press":"release"), hex, bs, dec));

	ToDoMap::iterator x;

	if (press) {
		if ((x = on_press.find (bs)) == on_press.end()) {
			DEBUG_TRACE (DEBUG::CC121, string_compose ("no press action for button %1 state %2 @ %3 in %4\n", id, bs, this, &on_press));
			return;
		}
	} else {
		if ((x = on_release.find (bs)) == on_release.end()) {
			DEBUG_TRACE (DEBUG::CC121, string_compose ("no release action for button %1 state %2 @%3 in %4\n", id, bs, this, &on_release));
			return;
		}
	}

	switch (x->second.type) {
	case NamedAction:
		if (!x->second.action_name.empty()) {
			fp.access_action (x->second.action_name);
		}
		break;
	case InternalFunction:
		if (x->second.function) {
			x->second.function ();
		}
	}
}

void
CC121::Button::set_action (string const& name, bool when_pressed, CC121::ButtonState bs)
{
	ToDo todo;

	todo.type = NamedAction;

	if (when_pressed) {
		if (name.empty()) {
			on_press.erase (bs);
		} else {
			DEBUG_TRACE (DEBUG::CC121, string_compose ("set button %1 to action %2 on press + %3%4%5\n", id, name, bs));
			todo.action_name = name;
			on_press[bs] = todo;
		}
	} else {
		if (name.empty()) {
			on_release.erase (bs);
		} else {
			DEBUG_TRACE (DEBUG::CC121, string_compose ("set button %1 to action %2 on release + %3%4%5\n", id, name, bs));
			todo.action_name = name;
			on_release[bs] = todo;
		}
	}
}

string
CC121::Button::get_action (bool press, CC121::ButtonState bs)
{
	ToDoMap::iterator x;

	if (press) {
		if ((x = on_press.find (bs)) == on_press.end()) {
			return string();
		}
		if (x->second.type != NamedAction) {
			return string ();
		}
		return x->second.action_name;
	} else {
		if ((x = on_release.find (bs)) == on_release.end()) {
			return string();
		}
		if (x->second.type != NamedAction) {
			return string ();
		}
		return x->second.action_name;
	}
}

void
CC121::Button::set_action (boost::function<void()> f, bool when_pressed, CC121::ButtonState bs)
{
	ToDo todo;
	todo.type = InternalFunction;

	if (when_pressed) {
		DEBUG_TRACE (DEBUG::CC121, string_compose ("set button %1 (%2) @ %5 to some functor on press + %3 in %4\n", id, name, bs, &on_press, this));
		todo.function = f;
		on_press[bs] = todo;
	} else {
		DEBUG_TRACE (DEBUG::CC121, string_compose ("set button %1 (%2) @ %5 to some functor on release + %3\n", id, name, bs, this));
		todo.function = f;
		on_release[bs] = todo;
	}
}

void
CC121::Button::set_led_state (boost::shared_ptr<MIDI::Port> port, bool onoff)
{
	MIDI::byte buf[3];
	DEBUG_TRACE(DEBUG::CC121, "Set Led State\n");
	buf[0] = 0x90;
	buf[1] = id;
	buf[2] = (onoff ? 0x7F:0x00);
	port->write (buf, 3, 0);
}

int
CC121::Button::set_state (XMLNode const& node)
{
	int32_t xid;
	if (node.get_property ("id", xid) && xid != id) {
		return -1;
	}

	typedef pair<string,CC121::ButtonState> state_pair_t;
	vector<state_pair_t> state_pairs;

	state_pairs.push_back (make_pair (string ("plain"), ButtonState (0)));

	for (vector<state_pair_t>::const_iterator sp = state_pairs.begin(); sp != state_pairs.end(); ++sp) {
		string prop_name;
		string prop_value;

		prop_name = sp->first + X_("-press");
		if (node.get_property (prop_name.c_str(), prop_value)) {
			set_action (prop_value, true, sp->second);
		}

		prop_name = sp->first + X_("-release");
		if (node.get_property (prop_name.c_str(), prop_value)) {
			set_action (prop_value, false, sp->second);
		}
	}

	return 0;
}

XMLNode&
CC121::Button::get_state () const
{
	XMLNode* node = new XMLNode (X_("Button"));

	node->set_property (X_("id"), (int32_t)id);

	ToDoMap::const_iterator x;
	ToDo null;
	null.type = NamedAction;

	typedef pair<string,CC121::ButtonState> state_pair_t;
	vector<state_pair_t> state_pairs;

	state_pairs.push_back (make_pair (string ("plain"), ButtonState (0)));

	for (vector<state_pair_t>::const_iterator sp = state_pairs.begin(); sp != state_pairs.end(); ++sp) {
		if ((x = on_press.find (sp->second)) != on_press.end()) {
			if (x->second.type == NamedAction) {
				node->set_property (string (sp->first + X_("-press")).c_str(), x->second.action_name);
			}
		}

		if ((x = on_release.find (sp->second)) != on_release.end()) {
			if (x->second.type == NamedAction) {
				node->set_property (string (sp->first + X_("-release")).c_str(), x->second.action_name);
			}
		}
	}

	return *node;
}

void
CC121::stripable_selection_changed ()
{
	set_current_stripable (first_selected_stripable());
}

void
CC121::drop_current_stripable ()
{
	if (_current_stripable) {
		if (_current_stripable == session->monitor_out()) {
			set_current_stripable (session->master_out());
		} else {
			set_current_stripable (boost::shared_ptr<Stripable>());
		}
	}
}

void
CC121::set_current_stripable (boost::shared_ptr<Stripable> r)
{
	stripable_connections.drop_connections ();

	_current_stripable = r;

	if (_current_stripable) {
		_current_stripable->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::drop_current_stripable, this), this);

		_current_stripable->mute_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_mute, this), this);
		_current_stripable->solo_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_solo, this), this);

		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (_current_stripable);
		if (t) {
			t->rec_enable_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_recenable, this), this);
			t->monitoring_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_monitoring, this), this);
		}

		boost::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
		if (control) {
			control->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_gain, this), this);
			control->alist()->automation_state_changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_auto, this), this);
		}

		boost::shared_ptr<MonitorProcessor> mp = _current_stripable->monitor_control();
		if (mp) {
			mp->cut_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&CC121::map_cut, this), this);
		}
	}

	//ToDo: subscribe to the fader automation modes so we can light the LEDs

	map_stripable_state ();
}

void
CC121::map_auto ()
{
	boost::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
	const AutoState as = control->automation_state ();

	switch (as) {
		case ARDOUR::Play:
			get_button (FP_Read).set_led_state (_output_port, true);
			get_button (FP_Write).set_led_state (_output_port, false);
			get_button (EButton).set_led_state (_output_port, false);
			get_button (OpenVST).set_led_state (_output_port, false);
		break;
		case ARDOUR::Write:
			get_button (FP_Read).set_led_state (_output_port, false);
			get_button (FP_Write).set_led_state (_output_port, true);
			get_button (EButton).set_led_state (_output_port, false);
			get_button (OpenVST).set_led_state (_output_port, false);
		break;
		case ARDOUR::Latch:
		case ARDOUR::Touch:
			get_button (EButton).set_led_state (_output_port, true);
			get_button (FP_Read).set_led_state (_output_port, false);
			get_button (FP_Write).set_led_state(_output_port, false);
			get_button (OpenVST).set_led_state (_output_port, false);
		break;
		case ARDOUR::Off:
			get_button (OpenVST).set_led_state (_output_port, true);
			get_button (FP_Read).set_led_state (_output_port, false);
			get_button (FP_Write).set_led_state (_output_port, false);
			get_button (EButton).set_led_state (_output_port, false);
		break;
	}
}

void
CC121::map_cut ()
{
	boost::shared_ptr<MonitorProcessor> mp = _current_stripable->monitor_control();

	if (mp) {
		bool yn = mp->cut_all ();
		if (yn) {
			start_blinking (Mute);
		} else {
			stop_blinking (Mute);
		}
	} else {
		stop_blinking (Mute);
	}
}

void
CC121::map_mute ()
{
	if (_current_stripable) {
		if (_current_stripable->mute_control()->muted()) {
			stop_blinking (Mute);
			get_button (Mute).set_led_state (_output_port, true);
		} else if (_current_stripable->mute_control()->muted_by_others_soloing () || _current_stripable->mute_control()->muted_by_masters()) {
			start_blinking (Mute);
		} else {
			stop_blinking (Mute);
		}
	} else {
		stop_blinking (Mute);
	}
}

void
CC121::map_solo ()
{
	if (_current_stripable) {
		get_button (Solo).set_led_state (_output_port, _current_stripable->solo_control()->soloed());
	} else {
		get_button (Solo).set_led_state (_output_port, false);
	}
}

void
CC121::map_recenable ()
{
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (_current_stripable);
	if (t) {
		get_button (Rec).set_led_state (_output_port, t->rec_enable_control()->get_value());
	} else {
		get_button (Rec).set_led_state (_output_port, false);
	}
	map_monitoring ();
}

void
CC121::map_monitoring ()
{
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (_current_stripable);
	if (t) {
	  MonitorState state = t->monitoring_control()->monitoring_state ();
		if (state == MonitoringInput || state == MonitoringCue) {
	    get_button(InputMonitor).set_led_state (_output_port, true);
		} else {
	    get_button(InputMonitor).set_led_state (_output_port, false);
		}
	} else {
		get_button(InputMonitor).set_led_state (_output_port, false);
	}
}

void
CC121::map_gain ()
{
	if (fader_is_touched) {
		/* Do not send fader moves while the user is touching the fader */
		return;
	}

	if (!_current_stripable) {
		return;
	}

	boost::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
	double val;

	if (!control) {
		val = 0.0;
	} else {
		val = control->internal_to_interface (control->get_value ());
	}

	int ival = (int)((val * 16384.0f) + 0.5f);
	if (ival < 0) {
	  ival = 0;
	}
	else if (ival > 16383) {
	  ival = 16383;
	}

	MIDI::byte buf[3];

	buf[0] = 0xE0;
	buf[1] = ival & 0x7F;
	buf[2] = (ival >> 7) & 0x7F;

	_output_port->write (buf, 3, 0);
}

void
CC121::map_stripable_state ()
{
	if (!_current_stripable) {
		stop_blinking (Mute);
		stop_blinking (Solo);
		get_button (Rec).set_led_state (_output_port, false);
	} else {
		map_solo ();
		map_recenable ();
		map_gain ();
		map_auto ();
		map_monitoring ();

		if (_current_stripable == session->monitor_out()) {
			map_cut ();
		} else {
			map_mute ();
		}
	}
}

list<boost::shared_ptr<ARDOUR::Bundle> >
CC121::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_input_bundle) {
		b.push_back (_input_bundle);
		b.push_back (_output_bundle);
	}

	return b;
}

boost::shared_ptr<Port>
CC121::output_port()
{
	return _output_port;
}

boost::shared_ptr<Port>
CC121::input_port()
{
	return _input_port;
}

void
CC121::set_action (ButtonID id, std::string const& action_name, bool on_press, ButtonState bs)
{
	get_button(id).set_action (action_name, on_press, bs);
}

string
CC121::get_action (ButtonID id, bool press, ButtonState bs)
{
	return get_button(id).get_action (press, bs);
}
