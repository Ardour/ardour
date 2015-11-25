/*
    Copyright (C) 2015 Paul Davis

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

#include <cstdlib>
#include <sstream>
#include <algorithm>

#include <stdint.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/controllable_descriptor.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/compose.h"
#include "pbd/xml++.h"

#include "midi++/port.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/amp.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"
#include "ardour/monitor_processor.h"
#include "ardour/rc_configuration.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track.h"

#include "faderport.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

FaderPort::FaderPort (Session& s)
	: ControlProtocol (s, _("Faderport"))
	, AbstractUI<FaderPortRequest> ("faderport")
	, gui (0)
	, connection_state (ConnectionState (0))
	, _device_active (false)
	, fader_msb (0)
	, fader_lsb (0)
	, fader_is_touched (false)
	, button_state (ButtonState (0))
	, blink_state (false)
{
	last_encoder_time = 0;

	boost::shared_ptr<ARDOUR::Port> inp;
	boost::shared_ptr<ARDOUR::Port> outp;

	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "Faderport Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "Faderport Send", true);

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(inp);
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(outp);

	if (_input_port == 0 || _output_port == 0) {
		throw failed_constructor();
	}

	TrackSelectionChanged.connect (selection_connection, MISSING_INVALIDATOR, boost::bind (&FaderPort::gui_track_selection_changed, this, _1), this);

	/* Catch port connections and disconnections */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&FaderPort::connection_handler, this, _1, _2, _3, _4, _5), this);

	buttons.insert (std::make_pair (Mute, ButtonInfo (*this, _("Mute"), Mute, 21)));
	buttons.insert (std::make_pair (Solo, ButtonInfo (*this, _("Solo"), Solo, 22)));
	buttons.insert (std::make_pair (Rec, ButtonInfo (*this, _("Rec"), Rec, 23)));
	buttons.insert (std::make_pair (Left, ButtonInfo (*this, _("Left"), Left, 20)));
	buttons.insert (std::make_pair (Bank, ButtonInfo (*this, _("Bank"), Bank, 19)));
	buttons.insert (std::make_pair (Right, ButtonInfo (*this, _("Right"), Right, 18)));
	buttons.insert (std::make_pair (Output, ButtonInfo (*this, _("Output"), Output, 17)));
	buttons.insert (std::make_pair (FP_Read, ButtonInfo (*this, _("Read"), FP_Read, 13)));
	buttons.insert (std::make_pair (FP_Write, ButtonInfo (*this, _("Write"), FP_Write, 14)));
	buttons.insert (std::make_pair (FP_Touch, ButtonInfo (*this, _("Touch"), FP_Touch, 15)));
	buttons.insert (std::make_pair (FP_Off, ButtonInfo (*this, _("Off"), FP_Off, 16)));
	buttons.insert (std::make_pair (Mix, ButtonInfo (*this, _("Mix"), Mix, 12)));
	buttons.insert (std::make_pair (Proj, ButtonInfo (*this, _("Proj"), Proj, 11)));
	buttons.insert (std::make_pair (Trns, ButtonInfo (*this, _("Trns"), Trns, 10)));
	buttons.insert (std::make_pair (Undo, ButtonInfo (*this, _("Undo"), Undo, 9)));
	buttons.insert (std::make_pair (Shift, ButtonInfo (*this, _("Shift"), Shift, 5)));
	buttons.insert (std::make_pair (Punch, ButtonInfo (*this, _("Punch"), Punch, 6)));
	buttons.insert (std::make_pair (User, ButtonInfo (*this, _("User"), User, 7)));
	buttons.insert (std::make_pair (Loop, ButtonInfo (*this, _("Loop"), Loop, 8)));
	buttons.insert (std::make_pair (Rewind, ButtonInfo (*this, _("Rewind"), Rewind, 4)));
	buttons.insert (std::make_pair (Ffwd, ButtonInfo (*this, _("Ffwd"), Ffwd, 3)));
	buttons.insert (std::make_pair (Stop, ButtonInfo (*this, _("Stop"), Stop, 2)));
	buttons.insert (std::make_pair (Play, ButtonInfo (*this, _("Play"), Play, 1)));
	buttons.insert (std::make_pair (RecEnable, ButtonInfo (*this, _("RecEnable"), RecEnable, 0)));
	buttons.insert (std::make_pair (FaderTouch, ButtonInfo (*this, _("Fader (touch)"), FaderTouch, -1)));

	button_info (Mix).set_action ( string("Common/toggle-editor-mixer"), true);
	button_info (Proj).set_action ( string("Common/toggle-meterbridge"), true);
	button_info (Trns).set_action ( string("Window/toggle-locations"), true);

	button_info (Left).set_action ( boost::bind (&FaderPort::left, this), true);
	button_info (Right).set_action ( boost::bind (&FaderPort::right, this), true);

	button_info (Undo).set_action (boost::bind (&FaderPort::undo, this), true);
	button_info (Undo).set_action (boost::bind (&FaderPort::redo, this), true, ShiftDown);
	button_info (Undo).set_flash (true);

	button_info (FP_Read).set_action (boost::bind (&FaderPort::read, this), true);
	button_info (FP_Write).set_action (boost::bind (&FaderPort::write, this), true);
	button_info (FP_Touch).set_action (boost::bind (&FaderPort::touch, this), true);
	button_info (FP_Off).set_action (boost::bind (&FaderPort::off, this), true);

	button_info (Play).set_action (boost::bind (&BasicUI::transport_play, this, true), true);
	button_info (RecEnable).set_action (boost::bind (&BasicUI::rec_enable_toggle, this), true);
	/* Stop is a modifier, so we have to use its own button state to get
	   the default action (since StopDown will be set when looking for the
	   action to invoke.
	*/
	button_info (Stop).set_action (boost::bind (&BasicUI::transport_stop, this), true, StopDown);
	button_info (Ffwd).set_action (boost::bind (&BasicUI::ffwd, this), true);

	/* See comments about Stop above .. */
	button_info (Rewind).set_action (boost::bind (&BasicUI::rewind, this), true, RewindDown);
	button_info (Rewind).set_action (boost::bind (&BasicUI::goto_zero, this), true, ButtonState (RewindDown|StopDown));
	button_info (Rewind).set_action (boost::bind (&BasicUI::goto_start, this), true, ButtonState (RewindDown|ShiftDown));

	button_info (Ffwd).set_action (boost::bind (&BasicUI::ffwd, this), true);
	button_info (Ffwd).set_action (boost::bind (&BasicUI::goto_end, this), true, ShiftDown);

	button_info (Loop).set_action (boost::bind (&BasicUI::loop_toggle, this), true);
	button_info (Loop).set_action (boost::bind (&BasicUI::add_marker, this, string()), true, ShiftDown);

	button_info (Punch).set_action (boost::bind (&BasicUI::prev_marker, this), true, ShiftDown);
	button_info (User).set_action (boost::bind (&BasicUI::next_marker, this), true, ShiftDown);

	button_info (Mute).set_action (boost::bind (&FaderPort::mute, this), true);
	button_info (Solo).set_action (boost::bind (&FaderPort::solo, this), true);
	button_info (Rec).set_action (boost::bind (&FaderPort::rec_enable, this), true);

	button_info (Output).set_action (boost::bind (&FaderPort::use_master, this), true);
	button_info (Output).set_action (boost::bind (&FaderPort::use_monitor, this), true, ShiftDown);
}

FaderPort::~FaderPort ()
{
	if (_input_port) {
		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("unregistering input port %1\n", boost::shared_ptr<ARDOUR::Port>(_input_port)->name()));
		AudioEngine::instance()->unregister_port (_input_port);
		_input_port.reset ();
	}

	if (_output_port) {
//		_output_port->drain (10000);  //ToDo:  is this necessary?  It hangs the shutdown, for me
		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("unregistering output port %1\n", boost::shared_ptr<ARDOUR::Port>(_output_port)->name()));
		AudioEngine::instance()->unregister_port (_output_port);
		_output_port.reset ();
	}

	tear_down_gui ();
}

void
FaderPort::start_midi_handling ()
{
	/* handle device inquiry response */
	_input_port->parser()->sysex.connect_same_thread (midi_connections, boost::bind (&FaderPort::sysex_handler, this, _1, _2, _3));
	/* handle switches */
	_input_port->parser()->poly_pressure.connect_same_thread (midi_connections, boost::bind (&FaderPort::switch_handler, this, _1, _2));
	/* handle encoder */
	_input_port->parser()->pitchbend.connect_same_thread (midi_connections, boost::bind (&FaderPort::encoder_handler, this, _1, _2));
	/* handle fader */
	_input_port->parser()->controller.connect_same_thread (midi_connections, boost::bind (&FaderPort::fader_handler, this, _1, _2));

	/* This connection means that whenever data is ready from the input
	 * port, the relevant thread will invoke our ::midi_input_handler()
	 * method, which will read the data, and invoke the parser.
	 */

	_input_port->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &FaderPort::midi_input_handler), _input_port));
	_input_port->xthread().attach (main_loop()->get_context());
}

void
FaderPort::stop_midi_handling ()
{
	midi_connections.drop_connections ();

	/* Note: the input handler is still active at this point, but we're no
	 * longer connected to any of the parser signals
	 */
}

void
FaderPort::do_request (FaderPortRequest* req)
{
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
FaderPort::stop ()
{
	BaseUI::quit ();

	return 0;
}

void
FaderPort::thread_init ()
{
	struct sched_param rtparam;

	pthread_set_name (X_("FaderPort"));

	PBD::notify_gui_about_thread_creation (X_("gui"), pthread_self(), X_("FaderPort"), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (X_("FaderPort"), 128);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */

	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
	}
}

void
FaderPort::all_lights_out ()
{
	for (ButtonMap::iterator b = buttons.begin(); b != buttons.end(); ++b) {
		b->second.set_led_state (_output_port, false, true);
	}
}

FaderPort::ButtonInfo&
FaderPort::button_info (ButtonID id) const
{
	ButtonMap::const_iterator b = buttons.find (id);
	assert (b != buttons.end());
	return const_cast<ButtonInfo&>(b->second);
}

void
FaderPort::switch_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	ButtonID id (ButtonID (tb->controller_number));

	switch (id) {
	case Shift:
		button_state = (tb->value ? ButtonState (button_state|ShiftDown) : ButtonState (button_state&~ShiftDown));
		break;
	case Stop:
		button_state = (tb->value ? ButtonState (button_state|StopDown) : ButtonState (button_state&~StopDown));
		break;
	case Rewind:
		button_state = (tb->value ? ButtonState (button_state|RewindDown) : ButtonState (button_state&~RewindDown));
		break;
	case FaderTouch:
		fader_is_touched = tb->value;
		break;
	default:
		break;
	}

	ButtonInfo& bi (button_info (id));

	if (bi.uses_flash()) {
		bi.set_led_state (_output_port, (int)tb->value);
	}

	bi.invoke (button_state, tb->value ? true : false);
}

void
FaderPort::encoder_handler (MIDI::Parser &, MIDI::pitchbend_t pb)
{
	int delta = 1;
	if (pb < 8192) {
		cerr << "Encoder right\n";
	} else {
		delta = -1;
		cerr << "Encoder left\n";
	}

	//knob debouncing and hysteresis.  The presonus encoder often sends bursts of events, or goes the wrong direction
	{
		last_last_encoder_delta = last_encoder_delta;
		last_encoder_delta = delta;
		microseconds_t now = get_microseconds ();
		if ((now - last_encoder_time) < 10*1000) { //require at least 10ms interval between changes, because the device sometimes sends multiple deltas
			return;
		}
		if ((now - last_encoder_time) < 100*1000) { //avoid directional changes while "spinning", 100ms window
			if ( (delta == last_encoder_delta) && (delta == last_last_encoder_delta) ) {
				last_good_encoder_delta = delta;  //3 in a row, grudgingly accept this as the new direction
			}
			if (delta != last_good_encoder_delta) {  //otherwise ensure we keep going the same way
				delta = last_good_encoder_delta;
			}
		} else {  //we aren't yet in a spin window, just assume this move is really what we want
			//NOTE:  if you are worried about where these get initialized, here it is.
			last_last_encoder_delta = delta;
			last_encoder_delta = delta;
		}
		last_encoder_time = now;
		last_good_encoder_delta = delta;
	}

	if (_current_route) {

		if ( (button_state & ShiftDown) == ShiftDown ) {    //shift+encoder = input trim
			boost::shared_ptr<AutomationControl> gain = _current_route->trim()->gain_control ();
			if (gain) {
				float val = gain->get_user();  //for gain elements, the "user" value is in dB
				val += delta;
				gain->set_user(val);
			}
		} else {  //pan / balance
			//ToDo
		}

	}

}

void
FaderPort::fader_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	bool was_fader = false;

	if (tb->controller_number == 0x0) {
		fader_msb = tb->value;
		was_fader = true;
	} else if (tb->controller_number == 0x20) {
		fader_lsb = tb->value;
		was_fader = true;
	}

	if (was_fader) {
		if (_current_route) {
			boost::shared_ptr<AutomationControl> gain = _current_route->gain_control ();
			if (gain) {
				int ival = (fader_msb << 7) | fader_lsb;
				float val = gain->interface_to_internal (ival/16384.0);
				_current_route->set_gain (val, this);
			}
		}
	}
}

void
FaderPort::sysex_handler (MIDI::Parser &p, MIDI::byte *buf, size_t sz)
{
	if (sz < 17) {
		return;
	}

	if (buf[2] != 0x7f ||
	    buf[3] != 0x06 ||
	    buf[4] != 0x02 ||
	    buf[5] != 0x0 ||
	    buf[6] != 0x1 ||
	    buf[7] != 0x06 ||
	    buf[8] != 0x02 ||
	    buf[9] != 0x0 ||
	    buf[10] != 0x01 ||
	    buf[11] != 0x0) {
		return;
	}

	_device_active = true;

	cerr << "FaderPort identified\n";

	/* put it into native mode */

	MIDI::byte native[3];
	native[0] = 0x91;
	native[1] = 0x00;
	native[2] = 0x64;

	_output_port->write (native, 3, 0);

	all_lights_out ();

	/* catch up on state */

	notify_transport_state_changed ();
	notify_record_state_changed ();
}

int
FaderPort::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::FaderPort, string_compose("MackieControlProtocol::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		/* start event loop */

		BaseUI::run ();

		connect_session_signals ();

		Glib::RefPtr<Glib::TimeoutSource> blink_timeout = Glib::TimeoutSource::create (200); // milliseconds
		blink_connection = blink_timeout->connect (sigc::mem_fun (*this, &FaderPort::blink));
		blink_timeout->attach (main_loop()->get_context());

	} else {

		BaseUI::quit ();
		close ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::FaderPort, string_compose("MackieControlProtocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

bool
FaderPort::blink ()
{
	blink_state = !blink_state;

	for (Blinkers::iterator b = blinkers.begin(); b != blinkers.end(); b++) {
		button_info(*b).set_led_state (_output_port, blink_state);
	}

	return true;
}

void
FaderPort::close ()
{
	all_lights_out ();

	stop_midi_handling ();
	session_connections.drop_connections ();
	port_connection.disconnect ();
	blink_connection.disconnect ();
	selection_connection.disconnect ();
	route_connections.drop_connections ();

#if 0
	route_connections.drop_connections ();
#endif
}

void
FaderPort::notify_record_state_changed ()
{
	switch (session->record_status()) {
	case Session::Disabled:
		button_info (RecEnable).set_led_state (_output_port, false);
		blinkers.remove (RecEnable);
		break;
	case Session::Enabled:
		button_info (RecEnable).set_led_state (_output_port, true);
		blinkers.push_back (RecEnable);
		break;
	case Session::Recording:
		button_info (RecEnable).set_led_state (_output_port, true);
		blinkers.remove (RecEnable);
		break;
	}
}

void
FaderPort::notify_transport_state_changed ()
{
	button_info (Loop).set_led_state (_output_port, session->get_play_loop());
	button_info (Play).set_led_state (_output_port, session->transport_speed() == 1.0);
	button_info (Stop).set_led_state (_output_port, session->transport_stopped ());
	button_info (Rewind).set_led_state (_output_port, session->transport_speed() < 0.0);
	button_info (Ffwd).set_led_state (_output_port, session->transport_speed() > 1.0);
}

void
FaderPort::connect_session_signals()
{
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::notify_record_state_changed, this), this);
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::notify_transport_state_changed, this), this);
}

bool
FaderPort::midi_input_handler (Glib::IOCondition ioc, boost::shared_ptr<ARDOUR::AsyncMIDIPort> port)
{
	DEBUG_TRACE (DEBUG::FaderPort, string_compose ("something happend on  %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		if (port) {
			port->clear ();
		}

		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("data available on %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));
		framepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}


XMLNode&
FaderPort::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());

	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_input_port)->get_state());
	node.add_child_nocopy (*child);


	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_output_port)->get_state());
	node.add_child_nocopy (*child);

	return node;
}

int
FaderPort::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode const* child;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			boost::shared_ptr<ARDOUR::Port>(_input_port)->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			boost::shared_ptr<ARDOUR::Port>(_output_port)->set_state (*portnode, version);
		}
	}

	return 0;
}

bool
FaderPort::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
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
		/* not our ports */
		return false;
	}

	if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
		connected ();

	} else {
		DEBUG_TRACE (DEBUG::FaderPort, "Device disconnected (input or output or both) or not yet fully connected\n");
		_device_active = false;
	}

	return true; /* connection status changed */
}

void
FaderPort::connected ()
{
	std::cerr << "faderport connected\n";

	start_midi_handling ();

	/* send device inquiry */

	MIDI::byte buf[6];

	buf[0] = 0xf0;
	buf[1] = 0x7e;
	buf[2] = 0x7f;
	buf[3] = 0x06;
	buf[4] = 0x01;
	buf[5] = 0xf7;

	_output_port->write (buf, 6, 0);
}

void
FaderPort::ButtonInfo::invoke (FaderPort::ButtonState bs, bool press)
{
	switch (type) {
	case NamedAction:
		if (press) {
			ToDoMap::iterator x = on_press.find (bs);
			if (x != on_press.end()) {
				if (!x->second.action_name.empty()) {
					fp.access_action (x->second.action_name);
				}
			}
		} else {
			ToDoMap::iterator x = on_release.find (bs);
			if (x != on_release.end()) {
				if (!x->second.action_name.empty()) {
					fp.access_action (x->second.action_name);
				}
			}
		}
		break;
	case InternalFunction:
		if (press) {
			ToDoMap::iterator x = on_press.find (bs);
			if (x != on_press.end()) {
				if (x->second.function) {
					x->second.function ();
				}
			}
		} else {
			ToDoMap::iterator x = on_release.find (bs);
			if (x != on_release.end()) {
				if (x->second.function) {
					x->second.function ();
				}
			}
		}
		break;
	}
}

void
FaderPort::ButtonInfo::set_action (string const& name, bool when_pressed, FaderPort::ButtonState bs)
{
	ToDo todo;

	type = NamedAction;

	if (when_pressed) {
		todo.action_name = name;
		on_press[bs] = todo;
	} else {
		todo.action_name = name;
		on_release[bs] = todo;
	}

}

void
FaderPort::ButtonInfo::set_action (boost::function<void()> f, bool when_pressed, FaderPort::ButtonState bs)
{
	ToDo todo;
	type = InternalFunction;

	if (when_pressed) {
		todo.function = f;
		on_press[bs] = todo;
	} else {
		todo.function = f;
		on_release[bs] = todo;
	}
}

void
FaderPort::ButtonInfo::set_led_state (boost::shared_ptr<MIDI::Port> port, int onoff, bool force)
{
	if (!force && (led_on == (bool) onoff)) {
		/* nothing to do */
		return;
	}

	if (out < 0) {
		/* fader button ID - no LED */
		return;
	}

	MIDI::byte buf[3];
	buf[0] = 0xa0;
	buf[1] = out;
	buf[2] = onoff ? 1 : 0;
	port->write (buf, 3, 0);
	led_on = (onoff ? true : false);
}

void
FaderPort::gui_track_selection_changed (RouteNotificationListPtr routes)
{
	boost::shared_ptr<Route> r;

	if (!routes->empty()) {
		r = routes->front().lock();
	}

	set_current_route (r);
}

void
FaderPort::drop_current_route ()
{
	if (_current_route) {
		if (_current_route == session->monitor_out()) {
			set_current_route (session->master_out());
		} else {
			set_current_route (boost::shared_ptr<Route>());
		}
	}
}

void
FaderPort::set_current_route (boost::shared_ptr<Route> r)
{
	route_connections.drop_connections ();

	_current_route = r;

	/* turn this off. It will be turned on back on in use_master() or
	   use_monitor() as appropriate.
	*/
	button_info(Output).set_led_state (_output_port, false);

	if (_current_route) {
		_current_route->DropReferences.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::drop_current_route, this), this);

		_current_route->mute_changed.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::map_mute, this, _1), this);
		_current_route->solo_changed.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::map_solo, this, _1, _2, _3), this);
		_current_route->listen_changed.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::map_listen, this, _1, _2), this);

		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (_current_route);
		if (t) {
			t->RecordEnableChanged.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::map_recenable, this), this);
		}

		boost::shared_ptr<AutomationControl> control = _current_route->gain_control ();
		if (control) {
			control->Changed.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::map_gain, this), this);
		}

		boost::shared_ptr<MonitorProcessor> mp = _current_route->monitor_control();
		if (mp) {
			mp->cut_control()->Changed.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort::map_cut, this), this);
		}
	}
	
	//ToDo: subscribe to the fader automation modes so we can light the LEDs

	map_route_state ();
}

void
FaderPort::map_cut ()
{
	boost::shared_ptr<MonitorProcessor> mp = _current_route->monitor_control();

	if (mp) {
		bool yn = mp->cut_all ();
		button_info (Mute).set_led_state (_output_port, yn);
		if (yn) {
			blinkers.push_back (Mute);
		} else {
			blinkers.remove (Mute);
		}
	} else {
		blinkers.remove (Mute);
	}
}

void
FaderPort::map_mute (void*)
{
	button_info (Mute).set_led_state (_output_port, _current_route->muted());
}

void
FaderPort::map_solo (bool, void*, bool)
{
	button_info (Solo).set_led_state (_output_port, _current_route->soloed() || _current_route->listening_via_monitor());
}

void
FaderPort::map_listen (void*, bool)
{
	button_info (Solo).set_led_state (_output_port, _current_route->listening_via_monitor());
}

void
FaderPort::map_recenable ()
{
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (_current_route);
	if (t) {
		button_info (Rec).set_led_state (_output_port, t->record_enabled());
	} else {
		button_info (Rec).set_led_state (_output_port, false);
	}
}

void
FaderPort::map_gain ()
{
	if (fader_is_touched) {
		/* Do not send fader moves while the user is touching the fader */
		return;
	}

	if (!_current_route) {
		return;
	}

	boost::shared_ptr<AutomationControl> control = _current_route->gain_control ();
	double val;

	if (!control) {
		val = 0.0;
	} else {
		val = control->internal_to_interface (control->get_value ());
	}

	/* Faderport sends fader position with range 0..16384 (though some of
	 * the least-significant bits at the top end are missing - it may only
	 * get to 1636X or so).
	 *
	 * But ... position must be sent in the range 0..1023.
	 *
	 * Thanks, Obama.
	 */

	int ival = (int) lrintf (val * 1023.0);

	/* MIDI normalization requires that we send two separate messages here,
	 * not one single 6 byte one.
	 */

	MIDI::byte buf[3];

	buf[0] = 0xb0;
	buf[1] = 0x0;
	buf[2] = ival >> 7;

	_output_port->write (buf, 3, 0);

	buf[1] = 0x20;
	buf[2] = ival & 0x7f;

	_output_port->write (buf, 3, 0);
}

void
FaderPort::map_route_state ()
{
	if (!_current_route) {
		button_info (Mute).set_led_state (_output_port, false);
		button_info (Solo).set_led_state (_output_port, false);
		button_info (Rec).set_led_state (_output_port, false);
		blinkers.remove (Mute);
		blinkers.remove (Solo);
	} else {
		/* arguments to these map_*() methods are all ignored */
		map_mute (0);
		map_solo (false, 0, false);
		map_recenable ();
		map_gain ();
		map_cut ();
	}
}
