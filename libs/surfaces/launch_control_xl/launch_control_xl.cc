/*
 * Copyright (C) 2018-2019 Jan Lentfer <jan.lentfer@web.de>
 * Copyright (C) 2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Térence Clastres <t.clastres@gmail.com>
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

#include <stdlib.h>
#include <pthread.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"

#include "ardour/amp.h"
#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/audio_track.h"
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/tempo.h"
#include "ardour/types_convert.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"



#include "gtkmm2ext/gui_thread.h"

#include "gui.h"
#include "launch_control_xl.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
#include "pbd/abstract_ui.cc" // instantiate template

/* init global object */
LaunchControlXL* lcxl = 0;

LaunchControlXL::LaunchControlXL (ARDOUR::Session& s)
	: ControlProtocol (s, string (X_("Novation Launch Control XL")))
	, AbstractUI<LaunchControlRequest> (name())
	, in_use (false)
	, _track_mode(TrackMute)
	, _template_number(8) // default template (factory 1)
	, _fader8master (false)
	, _device_mode (false)
#ifdef MIXBUS32C
	, _ctrllowersends (false)
	, _fss_is_mixbus (false)
#endif
	, _refresh_leds_flag (false)
	, _send_bank_base (0)
	, bank_start (0)
	, connection_state (ConnectionState (0))
	, gui (0)
	, in_range_select (false)
{
	lcxl = this;
	/* we're going to need this */

	/* master cannot be removed, so no need to connect to going-away signal */
	master = session->master_out ();

	run_event_loop ();

	/* Ports exist for the life of this instance */

	ports_acquire ();

	/* Catch port connections and disconnections */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::connection_handler, this, _1, _2, _3, _4, _5), this);

	session->RouteAdded.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::stripables_added, this), lcxl);
	session->vca_manager().VCAAdded.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::stripables_added, this), lcxl);
}

LaunchControlXL::~LaunchControlXL ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "Launch Control XL  control surface object being destroyed\n");

	/* do this before stopping the event loop, so that we don't get any notifications */
	port_connection.disconnect ();
	session_connections.drop_connections ();
	stripable_connections.drop_connections ();

	stop_using_device ();
	ports_release ();

	stop_event_loop ();
	tear_down_gui ();
}


void
LaunchControlXL::run_event_loop ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "start event loop\n");
	BaseUI::run ();
}

void
LaunchControlXL::stop_event_loop ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "stop event loop\n");
	BaseUI::quit ();
}

int
LaunchControlXL::begin_using_device ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "begin using device\n");

	switch_template(template_number()); // first factory template

	connect_session_signals ();

	build_maps();

	reset(template_number());

	init_buttons (true);
	init_knobs ();
	button_track_mode(track_mode());
	set_send_bank(0);

	in_use = true;

	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("fader8master inital value  '%1'\n", fader8master()));
	if (fader8master()) {
		set_fader8master (fader8master());
	}
#ifdef MIXBUS32C
	if (ctrllowersends()) {
		set_ctrllowersends (ctrllowersends());
	}
#endif

	return 0;
}

int
LaunchControlXL::stop_using_device ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "stop using device\n");

	if (!in_use) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "nothing to do, device not in use\n");
		return 0;
	}

	init_buttons (false);

	session_connections.drop_connections ();

	in_use = false;
	return 0;
}

int
LaunchControlXL::ports_acquire ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "acquiring ports\n");

	/* setup ports */

	_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("Launch Control XL in"), true);
	_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("Launch Control XL out"), true);

	if (_async_in == 0 || _async_out == 0) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "cannot register ports\n");
		return -1;
	}

	/* We do not add our ports to the input/output bundles because we don't
	 * want users wiring them by hand. They could use JACK tools if they
	 * really insist on that (and use JACK)
	 */

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();

	session->BundleAddedOrRemoved ();

	connect_to_parser ();

	/* Connect input port to event loop */

	AsyncMIDIPort* asp;

	asp = static_cast<AsyncMIDIPort*> (_input_port);
	asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &LaunchControlXL::midi_input_handler), _input_port));
	asp->xthread().attach (main_loop()->get_context());

	return 0;
}

void
LaunchControlXL::ports_release ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "releasing ports\n");

	/* wait for button data to be flushed */
	AsyncMIDIPort* asp;
	asp = static_cast<AsyncMIDIPort*> (_output_port);
	asp->drain (10000, 500000);

	{
		Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
		AudioEngine::instance()->unregister_port (_async_in);
		AudioEngine::instance()->unregister_port (_async_out);
	}

	_async_in.reset ((ARDOUR::Port*) 0);
	_async_out.reset ((ARDOUR::Port*) 0);
	_input_port = 0;
	_output_port = 0;
}

list<boost::shared_ptr<ARDOUR::Bundle> >
LaunchControlXL::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_output_bundle) {
		b.push_back (_output_bundle);
	}

	return b;
}

void
LaunchControlXL::init_knobs_and_buttons()
{
	init_knobs();
	init_buttons();
}

void
LaunchControlXL::init_buttons()
{
	init_buttons(false);
}

void
LaunchControlXL::init_buttons (ButtonID buttons[], uint8_t i)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "init_buttons buttons[]\n");
	for (uint8_t n = 0; n < i; ++n) {
		boost::shared_ptr<TrackButton> button = boost::dynamic_pointer_cast<TrackButton> (id_note_button_map[buttons[n]]);
		if (button) {
			switch ((button->check_method)()) {
				case (dev_nonexistant):
					button->set_color(Off);
					break	;
				case (dev_inactive):
					button->set_color(button->color_disabled());
					break;
				case (dev_active):
					button->set_color(button->color_enabled());
					break;
			}
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Button %1 check_method returned: %2\n", n, (int)button->check_method()));
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Write state_msg for Button:%1\n", n));
			write (button->state_msg());
		}
	}
	/* set "Track Select" LEDs always on - we cycle through stripables */
	boost::shared_ptr<SelectButton> sl = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectLeft]);
	boost::shared_ptr<SelectButton> sr = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectRight]);
	if (sl && sr) {
		write(sl->state_msg(true));
		write(sr->state_msg(true));
	}

	boost::shared_ptr<TrackStateButton> db =  boost::dynamic_pointer_cast<TrackStateButton>(id_note_button_map[Device]);
	if (db) {
		write(db->state_msg(device_mode()));
	}
}

void
LaunchControlXL::init_buttons (bool startup)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "init_buttons (bool startup)\n");
	if (startup && !device_mode()) {
		switch_bank(bank_start);
		return;
	}

	if (device_mode()) {
		ButtonID buttons[] = { Focus1, Focus2, Focus3, Focus4, Focus5, Focus6, Focus7, Focus8,
			Control1, Control2, Control3, Control4, Control5, Control6, Control7, Control8 };

		for (size_t n = 0; n < sizeof (buttons) / sizeof (buttons[0]); ++n) {
			boost::shared_ptr<TrackButton> button = boost::dynamic_pointer_cast<TrackButton> (id_note_button_map[buttons[n]]);
			if (button) {
				switch ((button->check_method)()) {
					case (dev_nonexistant):
						button->set_color(Off);
						break;
					case (dev_inactive):
						button->set_color(button->color_disabled());
						break;
					case (dev_active):
						button->set_color(button->color_enabled());
						break;
				}
				DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Button %1 check_method returned: %2\n", n, (int)button->check_method()));
				DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Write state_msg for Button:%1\n", n));
				write (button->state_msg());
			}
		}
	}

	/* set "Track Select" LEDs always on - we cycle through stripables */
	boost::shared_ptr<SelectButton> sl = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectLeft]);
	boost::shared_ptr<SelectButton> sr = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectRight]);
	if (sl && sr) {
		write(sl->state_msg(true));
		write(sr->state_msg(true));
	}
#ifdef MIXBUS // for now we only offer a device mode for Mixbus
	boost::shared_ptr<TrackStateButton> db =  boost::dynamic_pointer_cast<TrackStateButton>(id_note_button_map[Device]);
	if (db) {
		write(db->state_msg(device_mode()));
	}
#endif
}

void
LaunchControlXL::init_knobs (KnobID knobs[], uint8_t i)
{
	for (uint8_t n = 0; n < i ; ++n) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("init_knobs from array - n:%1\n", n));
		boost::shared_ptr<Knob> knob = id_knob_map[knobs[n]];
		if (knob) {
			switch ((knob->check_method)()) {
				case (dev_nonexistant):
					knob->set_color(Off);
					break;
				case (dev_inactive):
					knob->set_color(knob->color_disabled());
					break;
				case (dev_active):
					knob->set_color(knob->color_enabled());
					break;
			}
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Write state_msg for Knob:%1\n", n));
			write (knob->state_msg());
		}
	}
}


void
LaunchControlXL::init_knobs ()
{
	if (!device_mode()) {
		for (int n = 0; n < 8; ++n) {
			update_knob_led_by_strip(n);
		}
	} else {
		KnobID knobs[] = {	SendA1, SendA2, SendA3, SendA4, SendA5, SendA6, SendA7, SendA8,
							SendB1, SendB2, SendB3, SendB4, SendB5, SendB6, SendB7, SendB8,
							Pan1, Pan2, Pan3, Pan4, Pan5, Pan6, Pan7, Pan8 };

		for (size_t n = 0; n < sizeof (knobs) / sizeof (knobs[0]); ++n) {
			boost::shared_ptr<Knob> knob = id_knob_map[knobs[n]];
			if (knob) {
				switch ((knob->check_method)()) {
					case (dev_nonexistant):
						knob->set_color(Off);
						break;
					case (dev_inactive):
						knob->set_color(knob->color_disabled());
						break;
					case (dev_active):
						knob->set_color(knob->color_enabled());
						break;
				}

				DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Write state_msg for Knob:%1\n", n));
				write (knob->state_msg());
			}
		}
	}
}

bool
LaunchControlXL::probe ()
{
	return true;
}

void*
LaunchControlXL::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
LaunchControlXL::do_request (LaunchControlRequest * req)
{
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop_using_device ();
	}
}

void
LaunchControlXL::reset(uint8_t chan)
{
	MidiByteArray msg (3, 176 + chan, 0, 0); // turn off all leds, reset buffer settings and duty cycle

	write(msg);
}
int
LaunchControlXL::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("LaunchControlProtocol::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {
		if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {
			begin_using_device ();
		} else {
			/* begin_using_device () will get called once we're connected */
		}

	} else {
		/* Control Protocol Manager never calls us with false, but
		 * insteads destroys us.
		 */
	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("LaunchControlProtocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

void
LaunchControlXL::write (const MidiByteArray& data)
{
	/* immediate delivery */
	_output_port->write (&data[0], data.size(), 0);
}

/* Device to Ardour message handling */

bool
LaunchControlXL::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "MIDI port closed\n");
		return false;
	}

	if (ioc & IO_IN) {

		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("something happened on  %1\n", port->name()));

		AsyncMIDIPort* asp = static_cast<AsyncMIDIPort*>(port);
		if (asp) {
			asp->clear ();
		}

		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("data available on %1\n", port->name()));
		if (in_use) {
			samplepos_t now = AudioEngine::instance()->sample_time();
			port->parse (now);
		}
	}

	return true;
}


void
LaunchControlXL::connect_to_parser ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Connecting to signals on port %1\n", _input_port->name()));

	MIDI::Parser* p = _input_port->parser();

	/* Incoming sysex */
	p->sysex.connect_same_thread (*this, boost::bind (&LaunchControlXL::handle_midi_sysex, this, _1, _2, _3));

 for (MIDI::channel_t n = 0; n < 16; ++n) {
	/* Controller */
		p->channel_controller[(int)n].connect_same_thread (*this, boost::bind (&LaunchControlXL::handle_midi_controller_message, this, _1, _2, n));
		/* Button messages are NoteOn */
		p->channel_note_on[(int)n].connect_same_thread (*this, boost::bind (&LaunchControlXL::handle_midi_note_on_message, this, _1, _2, n));
		/* Button messages are NoteOn but libmidi++ sends note-on w/velocity = 0 as note-off so catch them too */
		p->channel_note_off[(int)n].connect_same_thread (*this, boost::bind (&LaunchControlXL::handle_midi_note_off_message, this, _1, _2, n));
	}
}

void
LaunchControlXL::handle_midi_sysex (MIDI::Parser&, MIDI::byte* raw_bytes, size_t sz)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Sysex, %1 bytes\n", sz));

	if (sz < 8) {
		return;
	}

	MidiByteArray msg (sz, raw_bytes);
	MidiByteArray lcxl_sysex_header (6, 0xF0, 0x00, 0x20, 0x29, 0x02, 0x11);

	if (!lcxl_sysex_header.compare_n (msg, 6)) {
		return;
	}


	switch (msg[6]) {
	case 0x77: /* template change */
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Template change: %1\n", (int)msg[7]));
		_template_number = msg[7];
		bank_start = 0;
		if (!device_mode ()) {
			switch_bank(bank_start);
		} else {
			init_device_mode();
		}
		break;
	}
}


void
LaunchControlXL::handle_button_message(boost::shared_ptr<Button> button, MIDI::EventTwoBytes* ev)
{
	if (ev->value) {
		/* any press cancels any pending long press timeouts */
		for (set<ButtonID>::iterator x = buttons_down.begin(); x != buttons_down.end(); ++x) {
			boost::shared_ptr<ControllerButton> cb = id_controller_button_map[*x];
			boost::shared_ptr<NoteButton>	nb = id_note_button_map[*x];
			if (cb != 0) {
				cb->timeout_connection.disconnect();
			} else if (nb != 0) {
				nb->timeout_connection.disconnect();
			}
		}

		buttons_down.insert(button->id());
		DEBUG_TRACE(DEBUG::LaunchControlXL, string_compose("button pressed: %1\n", LaunchControlXL::button_name_by_id(button->id())));
		start_press_timeout(button, button->id());
	} else {
		DEBUG_TRACE(DEBUG::LaunchControlXL, string_compose("button depressed: %1\n", LaunchControlXL::button_name_by_id(button->id())));
		buttons_down.erase(button->id());
		button->timeout_connection.disconnect();
		if (button ==  id_note_button_map[Device] && refresh_leds_flag()) {
			switch_bank (bank_start);
		}
	}

	set<ButtonID>::iterator c = consumed.find(button->id());

	if (c == consumed.end()) {
		if (ev->value == 0) {
			(button->release_method)();
		} else {
			(button->press_method)();
		}
	} else {
		DEBUG_TRACE(DEBUG::LaunchControlXL, "button was consumed, ignored\n");
		consumed.erase(c);
	}
}


bool
LaunchControlXL::check_pick_up(boost::shared_ptr<Controller> controller, boost::shared_ptr<AutomationControl> ac, bool rotary)
{
	/* returns false until the controller value matches with the current setting of the stripable's ac */
	return (abs (controller->value() / 127.0 - ac->internal_to_interface(ac->get_value(), rotary)) < 0.007875);
}

void
LaunchControlXL::handle_midi_controller_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev, MIDI::channel_t chan)
{
	_template_number = (int)chan;

	if (template_number() < 8) {
		return; // only treat factory templates
	}
	// DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));

	CCControllerButtonMap::iterator b = cc_controller_button_map.find (ev->controller_number);
	CCFaderMap::iterator f = cc_fader_map.find (ev->controller_number);
	CCKnobMap::iterator k = cc_knob_map.find (ev->controller_number);

	if (b != cc_controller_button_map.end()) {
		boost::shared_ptr<Button> button = b->second;
		handle_button_message(button, ev);
	} else if (f != cc_fader_map.end()) {
		boost::shared_ptr<Fader> fader = f->second;
		fader->set_value(ev->value);
		(fader->action_method)();
	} else if (k != cc_knob_map.end()) {
		boost::shared_ptr<Knob> knob = k->second;
		knob->set_value(ev->value);
		(knob->action_method)();
	}
}

void
LaunchControlXL::handle_midi_note_on_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev, MIDI::channel_t chan)
{
	_template_number = (int)chan;

	if (template_number() < 8) {
		return; // only treat factory templates
	}

	 //DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Note On %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));

	 NNNoteButtonMap::iterator b = nn_note_button_map.find (ev->controller_number);

	 if (b != nn_note_button_map.end()) {
		 boost::shared_ptr<Button> button = b->second;
		handle_button_message(button, ev);
	}
}

void LaunchControlXL::handle_midi_note_off_message(MIDI::Parser & parser, MIDI::EventTwoBytes *ev, MIDI::channel_t chan)
{
  //DEBUG_TRACE(DEBUG::LaunchControlXL, string_compose("Note Off %1 (velocity %2)\n",(int)ev->note_number, (int)ev->velocity));
	handle_midi_note_on_message(parser, ev, chan); /* we handle both case in handle_midi_note_on_message */
}

/* Ardour session signals connection */

void
LaunchControlXL::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

void
LaunchControlXL::connect_session_signals()
{
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::notify_parameter_changed, this, _1), this);

	// receive rude solo changed
	//session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::notify_solo_active_changed, this, _1), this);
	// receive record state toggled
	//session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::notify_record_state_changed, this), this);

}


void
LaunchControlXL::notify_transport_state_changed ()
{ /*
	Button* b = id_button_map[Play];

	if (session->transport_rolling()) {
		b->set_state (LED::OneShot24th);
		b->set_color (LED::GreenFull);
	} else {

		 disable any blink on FixedLength from pending edit range op
		Button* fl = id_button_map[FixedLength];

		fl->set_color (LED::Black);
		fl->set_state (LED::NoTransition);
		write (fl->state_msg());

		b->set_color (LED::White);
		b->set_state (LED::NoTransition);
	}

	write (b->state_msg()); */
}

void
LaunchControlXL::notify_loop_state_changed ()
{
}

void
LaunchControlXL::notify_parameter_changed (std::string param)
{ /*
	IDButtonMap::iterator b;

	if (param == "clicking") {
		if ((b = id_button_map.find (Metronome)) == id_button_map.end()) {
			return;
		}
		if (Config->get_clicking()) {
			b->second->set_state (LED::Blinking4th);
			b->second->set_color (LED::White);
		} else {
			b->second->set_color (LED::White);
			b->second->set_state (LED::NoTransition);
		}
		write (b->second->state_msg ()) ;
	} */
}

/* connection handling */

XMLNode&
LaunchControlXL::get_state()
{
	XMLNode& node (ControlProtocol::get_state());
	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (_async_in->get_state());
	node.add_child_nocopy (*child);
	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (_async_out->get_state());
	node.add_child_nocopy (*child);

	child = new XMLNode (X_("Configuration"));
	child->set_property ("fader8master", fader8master());
#ifdef MIXBUS32C
	child->set_property ("ctrllowersends", ctrllowersends());
#endif
	node.add_child_nocopy (*child);

	return node;
}

int
LaunchControlXL::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("LaunchControlXL::set_state: active %1\n", active()));

	int retval = 0;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	XMLNode* child;

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			_async_in->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			_async_out->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Configuration"))) !=0) {
		/* this should propably become a for-loop at some point */
		child->get_property ("fader8master", _fader8master);
#ifdef MIXBUS32C
		child->get_property ("ctrllowersends", _ctrllowersends);
#endif
	}

	return retval;
}

bool
LaunchControlXL::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "LaunchControlXL::connection_handler start\n");
	if (!_async_in || !_async_out) {
		return false;
	}

	string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_async_in)->name());
	string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_async_out)->name());

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
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Connections between %1 and %2 changed, but I ignored it\n", name1, name2));
		// not our ports
		return false;
	}

	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("our ports changed connection state: %1 -> %2 connected ? %3\n",
	                                           name1, name2, yn));

	if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
                DEBUG_TRACE (DEBUG::LaunchControlXL, "device now connected for both input and output\n");

                begin_using_device ();

	} else {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "Device disconnected (input or output or both) or not yet fully connected\n");
		stop_using_device ();
	}

	ConnectionChange (); /* emit signal for our GUI */

	DEBUG_TRACE (DEBUG::LaunchControlXL, "LaunchControlXL::connection_handler  end\n");

	return true; /* connection status changed */
}


boost::shared_ptr<Port>
LaunchControlXL::output_port()
{
	return _async_out;
}

boost::shared_ptr<Port>
LaunchControlXL::input_port()
{
	return _async_in;
}

/* Stripables handling */

void
LaunchControlXL::stripable_selection_changed ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "Stripable Selection changed\n");
	if (!device_mode()) {
		switch_bank (bank_start);
	} else {
#ifdef MIXBUS32C
		if (first_selected_stripable()) {
			DEBUG_TRACE (DEBUG::LaunchControlXL, "32C special handling. Checking if stripable type changed\n");
			bool fss_unchanged;
			fss_unchanged = (fss_is_mixbus() == (first_selected_stripable()->mixbus() || first_selected_stripable()->is_master()));
			if (!fss_unchanged) {
				DEBUG_TRACE (DEBUG::LaunchControlXL, "32C special handling: Stripable type DID CHANGE\n");
				reset(template_number());
				build_maps();
			} else {
				DEBUG_TRACE (DEBUG::LaunchControlXL, "32C special handling: Stripable type DID NOT CHANGE\n");
			}
		} else {
			reset(template_number());
		}
		store_fss_type();
#endif
		init_knobs_and_buttons();
		init_dm_callbacks();
		set_send_bank(0);
	}
}


void
LaunchControlXL::stripable_property_change (PropertyChange const& what_changed, uint32_t which)
{
	if (!device_mode()) {
		if (what_changed.contains (Properties::hidden)) {
			switch_bank (bank_start);
		}

		if (what_changed.contains (Properties::selected)) {

			if (!stripable[which]) {
				return;
			}
			if (which < 8) {
				update_track_focus_led ((uint8_t) which);
				update_knob_led_by_strip((uint8_t) which);
			}
		}
	} else {
		init_knobs_and_buttons();
	}
}
/* strip filter definitions */

static bool flt_default (boost::shared_ptr<Stripable> s) {
	if (s->is_master() || s->is_monitor()) {
		return false;
	}
	return (boost::dynamic_pointer_cast<Route>(s) != 0 ||
			boost::dynamic_pointer_cast<VCA>(s) != 0);
}

static bool flt_track (boost::shared_ptr<Stripable> s) {
	return boost::dynamic_pointer_cast<Track>(s) != 0;
}

static bool flt_auxbus (boost::shared_ptr<Stripable> s) {
	if (s->is_master() || s->is_monitor()) {
		return false;
	}
	if (boost::dynamic_pointer_cast<Route>(s) == 0) {
		return false;
	}
#ifdef MIXBUS
	if (s->mixbus () > 0) {
		return false;
	}
#endif
	return boost::dynamic_pointer_cast<Track>(s) == 0;
}

#ifdef MIXBUS
static bool flt_mixbus (boost::shared_ptr<Stripable> s) {
	if (s->mixbus () == 0) {
		return false;
	}
	return boost::dynamic_pointer_cast<Track>(s) == 0;
}
#endif

static bool flt_vca (boost::shared_ptr<Stripable> s) {
	return boost::dynamic_pointer_cast<VCA>(s) != 0;
}

static bool flt_selected (boost::shared_ptr<Stripable> s) {
	return s->is_selected ();
}

#ifdef MIXBUS
#else
static bool flt_rec_armed (boost::shared_ptr<Stripable> s) {
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(s);
	if (!t) {
		return false;
	}
	return t->rec_enable_control ()->get_value () > 0;
}
#endif

static bool flt_mains (boost::shared_ptr<Stripable> s) {
	return (s->is_master() || s->is_monitor());
}

void
LaunchControlXL::filter_stripables(StripableList& strips) const
{
	typedef bool (*FilterFunction)(boost::shared_ptr<Stripable>);
	FilterFunction flt;

	switch ((int)template_number()) {
		default:
			/* FALLTHROUGH */
		case 8:
			flt = &flt_default;
			break;
		case 9:
			flt = &flt_track;
			break;
		case 10:
			flt = &flt_auxbus;
			break;
#ifdef MIXBUS
		case 11:
			flt = &flt_mixbus;
			break;
		case 12:
			flt = &flt_vca;
			break;
#else
		case 11:
			flt = &flt_vca;
			break;
		case 12:
			flt = &flt_rec_armed;
			break;
#endif
		case 13:
			flt = &flt_selected;
			break;
		case 14:	// Factory Template 7 behaves strange, don't map it to anyhting
			flt = &flt_default;
			break;
		case 15:
			flt = &flt_mains;
			break;
	}

	StripableList all;
	session->get_stripables (all);

	for (StripableList::const_iterator s = all.begin(); s != all.end(); ++s) {
		if ((*s)->is_auditioner ()) { continue; }
		if ((*s)->is_hidden ()) { continue; }

		if ((*flt)(*s)) {
			strips.push_back (*s);
		}
	}
	strips.sort (Stripable::Sorter(true));
}

void
LaunchControlXL::switch_template (uint8_t t)
{
	MidiByteArray msg (9, 0xf0, 0x00, 0x20, 0x29, 0x02, 0x11, 0x77, t, 0xf7);
	write (msg);
}

void
LaunchControlXL::switch_bank (uint32_t base)
{
	if (device_mode()) { return; }

	reset(template_number());
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("switch_bank bank_start:%1\n", bank_start));
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("switch_bank base:%1\n", base));

	StripableList strips;
	filter_stripables (strips);
	
	set_send_bank(0);

	boost::shared_ptr<SelectButton> sl = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectLeft]);
	boost::shared_ptr<SelectButton> sr = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectRight]);

	boost::shared_ptr<Stripable> s[8];
	boost::shared_ptr<Stripable> next_base;
	uint32_t stripable_counter = get_amount_of_tracks();
	uint32_t skip = base;
	uint32_t n = 0;

	for (StripableList::const_iterator strip = strips.begin(); strip != strips.end(); ++strip) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("StripableList iterator - skip: %1, n: %2\n", skip, n));
		if (skip > 0) {
			--skip;
			continue;
		}

		if (n < stripable_counter) {
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("StripableList iterator - assigning stripable for n: %1\n", n));
			s[n] = *strip;
		}

		if (n == stripable_counter) { /* last strip +1 -> another bank exists */
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("StripableList iterator - n: %1. Filling next_base\n", n));
			next_base = *strip;
			break;
		}

		++n;
	}

	if (!s[0]) {
		/* not even the first stripable exists, do nothing */
		DEBUG_TRACE (DEBUG::LaunchControlXL, "not even first stripable exists.. returning\n");
		return;
	} else {
		bank_start = base;
	}

	if (sl && sr) {
		write(sl->state_msg(base));
		write(sr->state_msg(next_base != 0));
	}

	stripable_connections.drop_connections ();

	for (uint32_t n = 0; n < stripable_counter; ++n) {
		stripable[n] = s[n];
	}

	for (int n = 0; n < 8; ++n) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Binding Callbacks for n: %1\n", n));
		if (stripable[n]) {
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Binding Callbacks stripable[%1] exists\n", n));

			stripable[n]->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR,
					boost::bind (&LaunchControlXL::switch_bank, this, bank_start), lcxl);
			stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR,
					boost::bind (&LaunchControlXL::stripable_property_change, this, _1, n), lcxl);
			stripable[n]->solo_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR,
					boost::bind (&LaunchControlXL::solo_changed, this, n), lcxl);
			stripable[n]->mute_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR,
					boost::bind (&LaunchControlXL::mute_changed, this, n), lcxl);
			if (stripable[n]->solo_isolate_control()) {	/*VCAs are stripables without isolate solo */
				stripable[n]->solo_isolate_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR,
						boost::bind (&LaunchControlXL::solo_iso_changed, this,n ), lcxl);
			}
#ifdef MIXBUS
			if (stripable[n]->master_send_enable_controllable()) {
				stripable[n]->master_send_enable_controllable()->Changed.connect (stripable_connections, MISSING_INVALIDATOR,
						boost::bind (&LaunchControlXL::master_send_changed, this,n ), lcxl);
			}
#endif
			if (stripable[n]->rec_enable_control()) {
				stripable[n]->rec_enable_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR,
						boost::bind (&LaunchControlXL::rec_changed, this, n), lcxl);

			}

		}
		update_track_focus_led(n);
		update_track_control_led(n);
		update_knob_led_by_strip(n);
	}
	button_track_mode(track_mode());
}

void
LaunchControlXL::init_dm_callbacks()
{
	stripable_connections.drop_connections ();

	if (!first_selected_stripable()) {
		return;
	}
	if (first_selected_stripable()->mute_control()) {
		first_selected_stripable()->mute_control()->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_buttons,this), lcxl);
	}
	if (first_selected_stripable()->solo_control()) {
		first_selected_stripable()->solo_control()->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_buttons,this), lcxl);
	}
	if (first_selected_stripable()->rec_enable_control()) {
		first_selected_stripable()->rec_enable_control()->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_buttons,this), lcxl);
	}
#ifdef MIXBUS
	if (first_selected_stripable()->eq_enable_controllable()) {
		first_selected_stripable()->eq_enable_controllable()->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_knobs_and_buttons,this), lcxl);
	}
	if (first_selected_stripable()->eq_shape_controllable(0)) {
		first_selected_stripable()->eq_shape_controllable(0)->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_buttons,this), lcxl);
	}
	if (first_selected_stripable()->eq_shape_controllable(3)) {
		first_selected_stripable()->eq_shape_controllable(3)->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_buttons,this), lcxl);
	}

	if (first_selected_stripable()->comp_enable_controllable()) {
		first_selected_stripable()->comp_enable_controllable()->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_knobs_and_buttons,this), lcxl);
	}
	if (first_selected_stripable()->filter_enable_controllable(true)) { // only handle one case, as Mixbus only has one
		first_selected_stripable()->filter_enable_controllable(true)->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_knobs_and_buttons, this), lcxl);
	}
	if (first_selected_stripable()->master_send_enable_controllable()) {
		first_selected_stripable()->master_send_enable_controllable()->Changed.connect (stripable_connections,
		MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_knobs_and_buttons, this), lcxl);
	}

	for (uint8_t se = 0; se < 12 ; ++se) {
		if (first_selected_stripable()->send_enable_controllable(se)) {
			first_selected_stripable()->send_enable_controllable(se)->Changed.connect (stripable_connections,
			MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::init_knobs_and_buttons, this), lcxl);
		}
	}
#endif
}


#ifdef MIXBUS32C
void
LaunchControlXL::store_fss_type()
{
	if (first_selected_stripable()) {
		if (first_selected_stripable()->mixbus() || first_selected_stripable()->is_master()) {
			DEBUG_TRACE (DEBUG::LaunchControlXL, "Storing fss is mixbus: true\n");
			_fss_is_mixbus = true;
		} else {
			DEBUG_TRACE (DEBUG::LaunchControlXL, "Storing fss is mixbus: false\n");
			_fss_is_mixbus = false;
		}
	} else {
		_fss_is_mixbus = false;
	}
}
#endif

void
LaunchControlXL::init_device_mode()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "Initializing device mode\n");
	init_knobs();
	init_buttons(false);
#ifdef MIXBUS32C
	set_ctrllowersends(false);
	store_fss_type();
#endif
	init_dm_callbacks();
}

void
LaunchControlXL::stripables_added ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "LaunchControlXL::new stripable added!\n");
	if (!device_mode()) {
		/* reload current bank */
		switch_bank (bank_start);
	} else {
		return;
	}
}


void LaunchControlXL::set_track_mode (TrackMode mode) {
	_track_mode = mode;

	// now do led stuffs to signify the change

	ButtonID trk_cntrl_btns[] = {	Control1, Control2, Control3, Control4,
					Control5, Control6, Control7, Control8 };

	LEDColor color_on, color_off;
	switch(mode) {
		case TrackMute:
			color_on = YellowFull;
			color_off = YellowLow;
			break;
		case TrackSolo:
			color_on = GreenFull;
			color_off = GreenLow;
			break;
		case TrackRecord:
			color_on = RedFull;
			color_off = RedLow;
			break;
	default:
		break;
	}

	for ( size_t n = 0 ; n < sizeof (trk_cntrl_btns) / sizeof (trk_cntrl_btns[0]); ++n) {
		boost::shared_ptr<TrackButton> b = boost::dynamic_pointer_cast<TrackButton> (id_note_button_map[trk_cntrl_btns[n]]);
		if (b) {
			b->set_color_enabled(color_on);
			b->set_color_disabled(color_off);
		}
	}
}

void
LaunchControlXL::set_device_mode (bool yn)
{
	_device_mode = yn;
	reset(template_number());
	boost::shared_ptr<TrackStateButton> db =  boost::dynamic_pointer_cast<TrackStateButton>(id_note_button_map[Device]);
	write(db->state_msg(_device_mode));
	set_send_bank(0);
	build_maps();
	if (device_mode()) {
		init_device_mode();
	} else {
#ifdef MIXBUS32C
		set_ctrllowersends(ctrllowersends());
#endif
		switch_bank (bank_start);
	}
}


void
LaunchControlXL::set_fader8master (bool yn)
{
	_fader8master = yn;
	if (_fader8master) {
		stripable[7] = master;
		if (bank_start > 0) {
			bank_start -= 1;
		}
	} else {
		if (bank_start > 0) {
			bank_start += 1;
		}
	}

	switch_bank (bank_start);
}

#ifdef MIXBUS32C
void
LaunchControlXL::set_ctrllowersends (bool yn)
{

	_ctrllowersends = yn;

	if (device_mode()) { return; }

	/* reinit the send bank */
	if (_ctrllowersends) {
		_send_bank_base = 6;
	} else {
		_send_bank_base = 0;
	}
	set_send_bank(0);
}
#endif

void
LaunchControlXL::set_send_bank (int offset)
{

	int lowersendsoffset = 0;

#ifdef MIXBUS32C
	if (ctrllowersends() && !device_mode()) {
		lowersendsoffset = 6;
	}
#endif
	if ((_send_bank_base == (0 + lowersendsoffset)  && offset < 0) || (_send_bank_base == (4 + lowersendsoffset) && offset > 0)) {
		return;
	}

	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("set_send_bank - _send_bank_base: %1 \n", send_bank_base()));
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("set_send_bank - applying offset %1 \n", offset));

	boost::shared_ptr<SelectButton> sbu = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectUp]);
	boost::shared_ptr<SelectButton> sbd = boost::dynamic_pointer_cast<SelectButton>(id_controller_button_map[SelectDown]);

	if (!sbu || !sbd ) {
		return;
	}

	_send_bank_base = _send_bank_base + offset;
	_send_bank_base = max (0 + lowersendsoffset, min (4 + lowersendsoffset, _send_bank_base));

	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("set_send_bank - _send_bank_base: %1 \n", send_bank_base()));


#ifdef MIXBUS
	if (device_mode()) {	/* in device mode rebuild send led bindings */
		build_maps();
		//init_knobs_and_buttons();
		KnobID knobs[] = { Pan1, Pan2, Pan3, Pan4, Pan5, Pan6, Pan7, Pan8 };
		ButtonID buttons[] = { Focus1, Focus2, Focus3, Focus4, Focus5, Focus6, Focus7, Focus8 };
		init_knobs (knobs, 8);
		init_buttons (buttons, 8);
	}
#endif
	switch (_send_bank_base) {
		case 0:
		case 1:
		case 6:
		case 7:
			write (sbu->state_msg(false));
			write (sbd->state_msg(true));
			break;
		case 2:
		case 3:
		case 8:
		case 9:
			write (sbu->state_msg(true));
			write (sbd->state_msg(true));
			break;
		case 4:
		case 5:
		case 10:
		case 11:
			write (sbu->state_msg(true));
			write (sbd->state_msg(false));
			break;
	}
}

int
LaunchControlXL::get_amount_of_tracks ()
{
	int no_of_tracks;
	if (fader8master ()) {
		no_of_tracks = 7;
        } else {
                no_of_tracks = 8;
	}

	return no_of_tracks;
}

void
LaunchControlXL::set_refresh_leds_flag (bool yn)
{
	_refresh_leds_flag = yn;
}
