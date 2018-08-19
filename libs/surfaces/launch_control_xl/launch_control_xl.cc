/*
  Copyright (C) 2016 Paul Davis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types_convert.h"
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
	, bank_start (0)
	, connection_state (ConnectionState (0))
	, gui (0)
	, in_range_select (false)
{
	lcxl = this;
	/* we're going to need this */

	build_maps ();

	/* master cannot be removed, so no need to connect to going-away signal */
	master = session->master_out ();

	run_event_loop ();

	/* Ports exist for the life of this instance */

	ports_acquire ();

	/* catch arrival and departure of LaunchControlXL itself */
	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (port_reg_connection, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::port_registration_handler, this), this);

	/* Catch port connections and disconnections */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::connection_handler, this, _1, _2, _3, _4, _5), this);

	/* Launch Control XL ports might already be there */
	port_registration_handler ();

	session->RouteAdded.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::stripables_added, this), lcxl);
	session->vca_manager().VCAAdded.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::stripables_added, this), lcxl);

	switch_bank (bank_start);
}

LaunchControlXL::~LaunchControlXL ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "Launch Control XL  control surface object being destroyed\n");

	/* do this before stopping the event loop, so that we don't get any notifications */
	port_reg_connection.disconnect ();
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

	init_buttons (true);

	in_use = true;

	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("use_fader8master inital value  '%1'\n", use_fader8master));
	set_fader8master(use_fader8master);

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
LaunchControlXL::init_buttons (bool startup)
{
	reset(template_number());

	if (startup) {
		switch_bank(bank_start);
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
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Template change: %1 n", msg[7]));
		_template_number = msg[7];
		break;
	}
}


void
LaunchControlXL::handle_button_message(Button* button, MIDI::EventTwoBytes* ev)
{
  if (ev->value) {
    /* any press cancels any pending long press timeouts */
    for (set<ButtonID>::iterator x = buttons_down.begin(); x != buttons_down.end(); ++x) {
      ControllerButton* cb = id_controller_button_map[*x];
			NoteButton*	nb = id_note_button_map[*x];
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
  }

	set<ButtonID>::iterator c = consumed.find(button->id());

  if (c == consumed.end()) {
    if (ev->value == 0) {
      (this->*button->release_method)();
    } else {
      (this->*button->press_method)();
    }
  } else {
    DEBUG_TRACE(DEBUG::LaunchControlXL, "button was consumed, ignored\n");
    consumed.erase(c);
  }
}

bool
LaunchControlXL::check_pick_up(Controller* controller, boost::shared_ptr<AutomationControl> ac)
{
	/* returns false until the controller value matches with the current setting of the stripable's ac */
	return ( abs( controller->value() / 127.0 -  ac->internal_to_interface(ac->get_value()) ) < 0.007875 );
}

void
LaunchControlXL::handle_knob_message (Knob* knob)
{
	uint8_t chan = knob->id() % 8; // get the strip channel number
	if (!stripable[chan]) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;

	if (knob->id() < 8) { // sendA knob
		if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
			ac = stripable[chan]->trim_control();
		} else {
			ac = stripable[chan]->send_level_controllable (0);
		}
	} else if (knob->id() >= 8 && knob->id() < 16) { // sendB knob
		if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
#ifdef MIXBUS
			ac = stripable[chan]->filter_freq_controllable (true);
#else
			/* something */
#endif
		} else {
			ac = stripable[chan]->send_level_controllable (1);
		}
	} else if (knob->id() >= 16 && knob->id() < 24) { // pan knob
		if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
#ifdef MIXBUS
			ac = stripable[chan]->comp_threshold_controllable();
#else
			ac = stripable[chan]->pan_width_control();
#endif
		} else {
			ac = stripable[chan]->pan_azimuth_control();
		}
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::handle_fader_message (Fader* fader)
{

	if (!stripable[fader->id()]) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = stripable[fader->id()]->gain_control();
	if (ac && check_pick_up(fader, ac)) {
		ac->set_value ( ac->interface_to_internal( fader->value() / 127.0), PBD::Controllable::UseGroup );
	}
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
		Button* button = b->second;
		handle_button_message(button, ev);
	} else if (f != cc_fader_map.end()) {
		Fader* fader = f->second;
		fader->set_value(ev->value);
		handle_fader_message(fader);

	} else if (k != cc_knob_map.end()) {
		Knob* knob = k->second;
		knob->set_value(ev->value);
		handle_knob_message(knob);
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
		Button* button = b->second;
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
	child->set_property ("fader8master", LaunchControlXL::use_fader8master);
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
			_async_in->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			_async_out->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Configuration"))) !=0) {
		/* this should propably become a for-loop at some point */
		child->get_property ("fader8master", use_fader8master);
	}

	return retval;
}

void
LaunchControlXL::port_registration_handler ()
{
	if (!_async_in && !_async_out) {
		/* ports not registered yet */
		return;
	}

	if (_async_in->connected() && _async_out->connected()) {
		/* don't waste cycles here */
		return;
	}

#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Ableton
	   and may change in time. This is part of how CoreMIDI works.
	*/
	string input_port_name = X_("system:midi_capture_1319078870");
	string output_port_name = X_("system:midi_playback_3409210341");
#else
	string input_port_name = X_("Novation Launch Control XL MIDI 1 in");
	string output_port_name = X_("Novation Launch Control XL MIDI 1 out");
#endif
	vector<string> in;
	vector<string> out;

	AudioEngine::instance()->get_ports (string_compose (".*%1", input_port_name), DataType::MIDI, PortFlags (IsPhysical|IsOutput), in);
	AudioEngine::instance()->get_ports (string_compose (".*%1", output_port_name), DataType::MIDI, PortFlags (IsPhysical|IsInput), out);

	if (!in.empty() && !out.empty()) {
		cerr << "LaunchControlXL: both ports found\n";
		cerr << "\tconnecting to " << in.front() <<  " + " << out.front() << endl;
		if (!_async_in->connected()) {
			AudioEngine::instance()->connect (_async_in->name(), in.front());
		}
		if (!_async_out->connected()) {
			AudioEngine::instance()->connect (_async_out->name(), out.front());
		}
	}
}

bool
LaunchControlXL::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "LaunchControlXL::connection_handler start\n");
	if (!_input_port || !_output_port) {
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
LaunchControlXL::stripable_selection_changed () // we don't need it but it's needs to be declared...
{
}


void
LaunchControlXL::stripable_property_change (PropertyChange const& what_changed, uint32_t which)
{

	if (what_changed.contains (Properties::hidden)) {
		switch_bank (bank_start);
	}

	if (what_changed.contains (Properties::selected)) {

		if (!stripable[which]) {
			return;
		}
		if (which < 8) {
			update_track_focus_led ((uint8_t) which);
			update_knob_led((uint8_t) which);
		}
	}
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
	SelectButton* sl = static_cast<SelectButton*>(id_controller_button_map[SelectLeft]);
	SelectButton* sr = static_cast<SelectButton*>(id_controller_button_map[SelectRight]);

	/* work backwards so we can tell if we should actually switch banks */

	boost::shared_ptr<Stripable> s[8];
	uint32_t different = 0;
	uint8_t stripable_counter;

	if (LaunchControlXL::use_fader8master) {
		stripable_counter = 7;
	} else {
		stripable_counter = 8;
	}

	for (int n = 0; n < stripable_counter; ++n) {
		s[n] = session->get_remote_nth_stripable (base+n, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
		if (s[n] != stripable[n]) {
			different++;
		}
	}

	if (!s[0]) {
		/* not even the first stripable exists, do nothing */
		return;
	}

	if (sl && sr) {
		boost::shared_ptr<Stripable> next_base = session->get_remote_nth_stripable (base+8, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
		write(sl->state_msg((base)));
		write(sr->state_msg((next_base != 0)));
	}

	stripable_connections.drop_connections ();

	for (int n = 0; n < stripable_counter; ++n) {
		stripable[n] = s[n];
	}

	/* at least one stripable in this bank */

	bank_start = base;

	for (int n = 0; n < 8; ++n) {

		if (stripable[n]) {
			/* stripable goes away? refill the bank, starting at the same point */

			stripable[n]->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::switch_bank, this, bank_start), lcxl);
			stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::stripable_property_change, this, _1, n), lcxl);
			stripable[n]->solo_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::solo_changed, this, n), lcxl);
			stripable[n]->mute_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::mute_changed, this, n), lcxl);
			if (stripable[n]->rec_enable_control()) {
				stripable[n]->rec_enable_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&LaunchControlXL::rec_changed, this, n), lcxl);
			}
		}
		update_track_focus_led(n);
		button_track_mode(track_mode());
		update_knob_led(n);
	}
}

void
LaunchControlXL::stripables_added ()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "LaunchControlXL::new stripable added!\n");
	/* reload current bank */
	switch_bank (bank_start);
}


void LaunchControlXL::set_track_mode (TrackMode mode) {
	_track_mode = mode;

	// now do led stuffs to signify the change
	switch(mode) {
		case TrackMute:

			break;
		case TrackSolo:

			break;
		case TrackRecord:

			break;
	default:
		break;
	}
}

void
LaunchControlXL::set_fader8master (bool yn)
{
	if (yn) {
		stripable[7] = master;
	}
	switch_bank(bank_start);
}
