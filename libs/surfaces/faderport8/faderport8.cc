/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <cstdlib>
#include <sstream>
#include <algorithm>

#include <stdint.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/compose.h"
#include "pbd/xml++.h"

#include "midi++/port.h"

#include "ardour/audioengine.h"
#include "ardour/audio_track.h"
#include "ardour/bundle.h"
#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/midiport_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/rc_configuration.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_configuration.h"
#include "ardour/vca.h"

#include "faderport8.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;
using namespace ArdourSurface::FP8Types;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

#ifndef NDEBUG
//#define VERBOSE_DEBUG
#endif

static void
debug_2byte_msg (std::string const& msg, int b0, int b1)
{
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::FaderPort8)) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, "RECV: ");
		DEBUG_STR_APPEND(a, msg);
		DEBUG_STR_APPEND(a,' ');
		DEBUG_STR_APPEND(a,hex);
		DEBUG_STR_APPEND(a,"0x");
		DEBUG_STR_APPEND(a, b0);
		DEBUG_STR_APPEND(a,' ');
		DEBUG_STR_APPEND(a,"0x");
		DEBUG_STR_APPEND(a, b1);
		DEBUG_STR_APPEND(a,'\n');
		DEBUG_TRACE (DEBUG::FaderPort8, DEBUG_STR(a).str());
	}
#endif
}

FaderPort8::FaderPort8 (Session& s)
	: ControlProtocol (s, _("PreSonus FaderPort8"))
	, AbstractUI<FaderPort8Request> (name())
	, _connection_state (ConnectionState (0))
	, _device_active (false)
	, _ctrls (*this)
	, _channel_off (0)
	, _plugin_off (0)
	, _parameter_off (0)
	, _blink_onoff (false)
	, _shift_lock (false)
	, _shift_pressed (0)
	, gui (0)
{
	boost::shared_ptr<ARDOUR::Port> inp;
	boost::shared_ptr<ARDOUR::Port> outp;

	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "FaderPort8 Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "FaderPort8 Send", true);
	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(inp);
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(outp);

	if (_input_port == 0 || _output_port == 0) {
		throw failed_constructor();
	}

	_input_bundle.reset (new ARDOUR::Bundle (_("FaderPort8 (Receive)"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("FaderPort8 (Send) "), false));

	_input_bundle->add_channel (
		inp->name(),
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (inp->name())
		);

	_output_bundle->add_channel (
		outp->name(),
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (outp->name())
		);

	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::connection_handler, this, _2, _4), this);
	ARDOUR::AudioEngine::instance()->Stopped.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::engine_reset, this), this);
	ARDOUR::Port::PortDrop.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::engine_reset, this), this);

	/* bind button events to call libardour actions */
	setup_actions ();

	_ctrls.FaderModeChanged.connect_same_thread (modechange_connections, boost::bind (&FaderPort8::notify_fader_mode_changed, this));
	_ctrls.MixModeChanged.connect_same_thread (modechange_connections, boost::bind (&FaderPort8::assign_strips, this, true));
}

FaderPort8::~FaderPort8 ()
{
	cerr << "~FP8\n";
	disconnected ();
	close ();

	if (_input_port) {
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("unregistering input port %1\n", boost::shared_ptr<ARDOUR::Port>(_input_port)->name()));
		AudioEngine::instance()->unregister_port (_input_port);
		_input_port.reset ();
	}

	if (_output_port) {
		_output_port->drain (10000,  250000); /* check every 10 msecs, wait up to 1/4 second for the port to drain */
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("unregistering output port %1\n", boost::shared_ptr<ARDOUR::Port>(_output_port)->name()));
		AudioEngine::instance()->unregister_port (_output_port);
		_output_port.reset ();
	}

	tear_down_gui ();

	/* stop event loop */
	DEBUG_TRACE (DEBUG::FaderPort8, "BaseUI::quit ()\n");
	BaseUI::quit ();
}

/* ****************************************************************************
 * Event Loop
 */

void*
FaderPort8::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	 * instantiated in this source module. To provide something visible for
	 * use in the interface/descriptor, we have this static method that is
	 * template-free.
	 */
	return request_buffer_factory (num_requests);
}

void
FaderPort8::do_request (FaderPort8Request* req)
{
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}
}

int
FaderPort8::stop ()
{
	BaseUI::quit ();
	return 0;
}

void
FaderPort8::thread_init ()
{
	struct sched_param rtparam;

	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */

	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
	}
}

bool
FaderPort8::periodic ()
{
	/* prepare TC display -- handled by stripable Periodic () */
	if (_ctrls.display_timecode ()) {
		// TODO allow BBT, HHMMSS
		// used in FP8Strip::periodic_update_timecode
		Timecode::Time TC;
		session->timecode_time (TC);
		_timecode = Timecode::timecode_format_time(TC);
	} else {
		_timecode.clear ();
	}

	/* update stripables */
	Periodic ();
	return true;
}

bool
FaderPort8::blink_it ()
{
	_blink_onoff = !_blink_onoff;
	BlinkIt (_blink_onoff);
	return true;
}

/* ****************************************************************************
 * Port and Signal Connection Management
 */
int
FaderPort8::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::FaderPort8, string_compose("set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {
		/* start event loop */
		BaseUI::run ();
		connect_session_signals ();
	} else {
		BaseUI::quit ();
		close ();
	}

	ControlProtocol::set_active (yn);
	DEBUG_TRACE (DEBUG::FaderPort8, string_compose("set_active done with yn: '%1'\n", yn));
	return 0;
}

void
FaderPort8::close ()
{
	DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::close\n");
	stop_midi_handling ();
	session_connections.drop_connections ();
	automation_state_connections.drop_connections ();
	assigned_stripable_connections.drop_connections ();
	_assigned_strips.clear ();
	drop_ctrl_connections ();
	port_connections.drop_connections ();
	selection_connection.disconnect ();
}

void
FaderPort8::stop_midi_handling ()
{
	_periodic_connection.disconnect ();
	_blink_connection.disconnect ();
	midi_connections.drop_connections ();
	/* Note: the input handler is still active at this point, but we're no
	 * longer connected to any of the parser signals
	 */
}

void
FaderPort8::connected ()
{
	DEBUG_TRACE (DEBUG::FaderPort8, "initializing\n");
	assert (!_device_active);

	if (_device_active) {
		stop_midi_handling (); // re-init
	}

	// ideally check firmware version >= 1.01 (USB bcdDevice 0x0101) (vendor 0x194f prod 0x0202)
	// but we don't have a handle to the underlying USB device here.

	_channel_off = _plugin_off = _parameter_off = 0;
	_blink_onoff = false;
	_shift_lock = false;
	_shift_pressed = 0;

	start_midi_handling ();
	_ctrls.initialize ();

	/* highlight bound user-actions */
	for (FP8Controls::UserButtonMap::const_iterator i = _ctrls.user_buttons ().begin ();
			i != _ctrls.user_buttons ().end (); ++i) {
		_ctrls.button (i->first).set_active (! _user_action_map[i->first].empty ());
	}
	/* shift button lights */
	tx_midi3 (0x90, 0x06, 0x00);
	tx_midi3 (0x90, 0x46, 0x00);

	send_session_state ();
	assign_strips (true);

	Glib::RefPtr<Glib::TimeoutSource> blink_timer =
		Glib::TimeoutSource::create (200);
	_blink_connection = blink_timer->connect (sigc::mem_fun (*this, &FaderPort8::blink_it));
	blink_timer->attach (main_loop()->get_context());

	Glib::RefPtr<Glib::TimeoutSource> periodic_timer =
		Glib::TimeoutSource::create (100);
	_periodic_connection = periodic_timer->connect (sigc::mem_fun (*this, &FaderPort8::periodic));
	periodic_timer->attach (main_loop()->get_context());
}

void
FaderPort8::disconnected ()
{
	stop_midi_handling ();
	if (_device_active) {
		for (uint8_t id = 0; id < 8; ++id) {
			_ctrls.strip(id).unset_controllables ();
		}
		_ctrls.all_lights_off ();
	}
}

void
FaderPort8::engine_reset ()
{
	/* Port::PortDrop is called when the engine is halted or stopped */
	DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::engine_reset\n");
	_connection_state = 0;
	_device_active = false;
	disconnected ();
}

bool
FaderPort8::connection_handler (std::string name1, std::string name2)
{
#ifdef VERBOSE_DEBUG
	DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::connection_handler: start\n");
#endif
	if (!_input_port || !_output_port) {
		return false;
	}

	string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_input_port)->name());
	string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_output_port)->name());

	if (ni == name1 || ni == name2) {
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("Connection notify %1 and %2\n", name1, name2));
		if (_input_port->connected ()) {
			if (_connection_state & InputConnected) {
				return false;
			}
			_connection_state |= InputConnected;
		} else {
			_connection_state &= ~InputConnected;
		}
	} else if (no == name1 || no == name2) {
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("Connection notify %1 and %2\n", name1, name2));
		if (_output_port->connected ()) {
			if (_connection_state & OutputConnected) {
				return false;
			}
			_connection_state |= OutputConnected;
		} else {
			_connection_state &= ~OutputConnected;
		}
	} else {
#ifdef VERBOSE_DEBUG
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("Connections between %1 and %2 changed, but I ignored it\n", name1, name2));
#endif
		/* not our ports */
		return false;
	}

	if ((_connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		 * something prevents the device wakeup messages from being
		 * sent and/or the responses from being received.
		 */
		g_usleep (100000);
		DEBUG_TRACE (DEBUG::FaderPort8, "device now connected for both input and output\n");
		connected ();
		_device_active = true;

	} else {
		DEBUG_TRACE (DEBUG::FaderPort8, "Device disconnected (input or output or both) or not yet fully connected\n");
		if (_device_active) {
			disconnected ();
		}
		_device_active = false;
	}

	ConnectionChange (); /* emit signal for our GUI */

#ifdef VERBOSE_DEBUG
	DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::connection_handler: end\n");
#endif

	return true; /* connection status changed */
}

list<boost::shared_ptr<ARDOUR::Bundle> >
FaderPort8::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_input_bundle) {
		b.push_back (_input_bundle);
		b.push_back (_output_bundle);
	}

	return b;
}

/* ****************************************************************************
 * MIDI I/O
 */
bool
FaderPort8::midi_input_handler (Glib::IOCondition ioc, boost::weak_ptr<ARDOUR::AsyncMIDIPort> wport)
{
	boost::shared_ptr<AsyncMIDIPort> port (wport.lock());

	if (!port) {
		return false;
	}

#ifdef VERBOSE_DEBUG
	DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("something happend on %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));
#endif

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		port->clear ();
#ifdef VERBOSE_DEBUG
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("data available on %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));
#endif
		framepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}

void
FaderPort8::start_midi_handling ()
{
	_input_port->parser()->sysex.connect_same_thread (midi_connections, boost::bind (&FaderPort8::sysex_handler, this, _1, _2, _3));
	_input_port->parser()->poly_pressure.connect_same_thread (midi_connections, boost::bind (&FaderPort8::polypressure_handler, this, _1, _2));
	for (uint8_t i = 0; i < 16; ++i) {
	_input_port->parser()->channel_pitchbend[i].connect_same_thread (midi_connections, boost::bind (&FaderPort8::pitchbend_handler, this, _1, i, _2));
	}
	_input_port->parser()->controller.connect_same_thread (midi_connections, boost::bind (&FaderPort8::controller_handler, this, _1, _2));
	_input_port->parser()->note_on.connect_same_thread (midi_connections, boost::bind (&FaderPort8::note_on_handler, this, _1, _2));
	_input_port->parser()->note_off.connect_same_thread (midi_connections, boost::bind (&FaderPort8::note_off_handler, this, _1, _2));

	/* This connection means that whenever data is ready from the input
	 * port, the relevant thread will invoke our ::midi_input_handler()
	 * method, which will read the data, and invoke the parser.
	 */
	_input_port->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &FaderPort8::midi_input_handler), boost::weak_ptr<AsyncMIDIPort> (_input_port)));
	_input_port->xthread().attach (main_loop()->get_context());
}

size_t
FaderPort8::tx_midi (std::vector<uint8_t> const& d) const
{
	/* work around midi buffer overflow for batch changes */
	if (d.size() == 3 && (d[0] == 0x91 || d[0] == 0x92)) {
		/* set colors triplet in one go */
	} else if (d.size() == 3 && (d[0] == 0x93)) {
		g_usleep (1500);
	} else {
		g_usleep (400 * d.size());
	}
#ifndef NDEBUG
	size_t tx = _output_port->write (&d[0], d.size(), 0);
	assert (tx == d.size());
	return tx;
#else
	return _output_port->write (&d[0], d.size(), 0);
#endif
}

/* ****************************************************************************
 * MIDI Callbacks
 */
void
FaderPort8::polypressure_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	debug_2byte_msg ("PP", tb->controller_number, tb->value);
	// outgoing only (meter)
}

void
FaderPort8::pitchbend_handler (MIDI::Parser &, uint8_t chan, MIDI::pitchbend_t pb)
{
	debug_2byte_msg ("PB", chan, pb);
	/* fader 0..16368 (0x3ff0 -- 1024 steps) */
	bool handled = _ctrls.midi_fader (chan, pb);
	/* if Shift key is held while moving a fader (group override), don't lock shift. */
	if ((_shift_pressed > 0) && handled) {
		_shift_connection.disconnect ();
		_shift_lock = false;
	}
}

void
FaderPort8::controller_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	debug_2byte_msg ("CC", tb->controller_number, tb->value);
	// encoder
	// val Bit 7 = direction, Bits 0-6 = number of steps
	if (tb->controller_number == 0x3c) {
		encoder_navigate (tb->value & 0x40 ? true : false, tb->value & 0x3f);
	}
	if (tb->controller_number == 0x10) {
		encoder_parameter (tb->value & 0x40 ? true : false, tb->value & 0x3f);
	}
}

void
FaderPort8::note_on_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	debug_2byte_msg ("ON", tb->note_number, tb->velocity);

	/* fader touch */
	if (tb->note_number >= 0x68 && tb->note_number <= 0x6f) {
		_ctrls.midi_touch (tb->note_number - 0x68, tb->velocity);
		return;
	}

	/* special case shift */
	if (tb->note_number == 0x06 || tb->note_number == 0x46) {
		_shift_pressed |= (tb->note_number == 0x06) ? 1 : 2;
		if (_shift_pressed == 3) {
			return;
		}
		_shift_connection.disconnect ();
		if (_shift_lock) {
			_shift_lock = false;
			ShiftButtonChange (false);
			tx_midi3 (0x90, 0x06, 0x00);
			tx_midi3 (0x90, 0x46, 0x00);
			return;
		}

		Glib::RefPtr<Glib::TimeoutSource> shift_timer =
			Glib::TimeoutSource::create (1000);
		shift_timer->attach (main_loop()->get_context());
		_shift_connection = shift_timer->connect (sigc::mem_fun (*this, &FaderPort8::shift_timeout));

		ShiftButtonChange (true);
		tx_midi3 (0x90, 0x06, 0x7f);
		tx_midi3 (0x90, 0x46, 0x7f);
		return;
	}

	_ctrls.midi_event (tb->note_number, tb->velocity);
}

void
FaderPort8::note_off_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	debug_2byte_msg ("OF", tb->note_number, tb->velocity);

	if (tb->note_number >= 0x68 && tb->note_number <= 0x6f) {
		// fader touch
		_ctrls.midi_touch (tb->note_number - 0x68, tb->velocity);
		return;
	}

	/* special case shift */
	if (tb->note_number == 0x06 || tb->note_number == 0x46) {
		_shift_pressed &= (tb->note_number == 0x06) ? 2 : 1;
		if (_shift_pressed > 0) {
			return;
		}
		if (_shift_lock) {
			return;
		}
		ShiftButtonChange (false);
		tx_midi3 (0x90, 0x06, 0x00);
		tx_midi3 (0x90, 0x46, 0x00);
		/* just in case this happens concurrently */
		_shift_connection.disconnect ();
		_shift_lock = false;
		return;
	}

	bool handled = _ctrls.midi_event (tb->note_number, tb->velocity);
	/* if Shift key is held while activating an action, don't lock shift. */
	if ((_shift_pressed > 0) && handled) {
		_shift_connection.disconnect ();
		_shift_lock = false;
	}
}

void
FaderPort8::sysex_handler (MIDI::Parser &p, MIDI::byte *buf, size_t size)
{
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::FaderPort8)) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, string_compose ("RECV sysex siz=%1", size));
		for (size_t i=0; i < size; ++i) {
			DEBUG_STR_APPEND(a,hex);
			DEBUG_STR_APPEND(a,"0x");
			DEBUG_STR_APPEND(a,(int)buf[i]);
			DEBUG_STR_APPEND(a,' ');
		}
		DEBUG_STR_APPEND(a,'\n');
		DEBUG_TRACE (DEBUG::FaderPort8, DEBUG_STR(a).str());
	}
#endif
}

/* ****************************************************************************
 * User actions
 */
void
FaderPort8::set_button_action (FP8Controls::ButtonId id, bool press, std::string const& action_name)
{
	if (_ctrls.user_buttons().find (id) == _ctrls.user_buttons().end ()) {
		return;
	}
	_user_action_map[id].action (press).assign_action (action_name);

	if (!_device_active) {
		return;
	}
	_ctrls.button (id).set_active (!_user_action_map[id].empty ());
}

std::string
FaderPort8::get_button_action (FP8Controls::ButtonId id, bool press)
{
	return _user_action_map[id].action(press)._action_name;
}

/* ****************************************************************************
 * Persistent State
 */
XMLNode&
FaderPort8::get_state ()
{
	DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::get_state\n");
	XMLNode& node (ControlProtocol::get_state());

	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_input_port)->get_state());
	node.add_child_nocopy (*child);

	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_output_port)->get_state());
	node.add_child_nocopy (*child);

	for (UserActionMap::const_iterator i = _user_action_map.begin (); i != _user_action_map.end (); ++i) {
		if (i->second.empty()) {
			continue;
		}
		std::string name;
		if (!_ctrls.button_enum_to_name (i->first, name)) {
			continue;
		}
		XMLNode* btn = new XMLNode (X_("Button"));
		btn->set_property (X_("id"), name);
		if (!i->second.action(true).empty ()) {
			btn->set_property ("press", i->second.action(true)._action_name);
		}
		if (!i->second.action(false).empty ()) {
			btn->set_property ("release", i->second.action(false)._action_name);
		}
		node.add_child_nocopy (*btn);
	}

	return node;
}

int
FaderPort8::set_state (const XMLNode& node, int version)
{
	DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::set_state\n");
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode const* child;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::set_state Input\n");
			boost::shared_ptr<ARDOUR::Port>(_input_port)->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::set_state Output\n");
			boost::shared_ptr<ARDOUR::Port>(_output_port)->set_state (*portnode, version);
		}
	}

	_user_action_map.clear ();
	// TODO: When re-loading state w/o surface re-init becomes possible,
	// unset lights and reset colors of user buttons.

	for (XMLNodeList::const_iterator n = node.children().begin(); n != node.children().end(); ++n) {
		if ((*n)->name() != X_("Button")) {
			continue;
		}

		std::string id_str;
		if (!(*n)->get_property (X_("id"), id_str)) {
			continue;
		}

		FP8Controls::ButtonId id;
		if (!_ctrls.button_name_to_enum (id_str, id)) {
			continue;
		}

		std::string action_str;
		if ((*n)->get_property (X_("press"), action_str)) {
			set_button_action (id, true, action_str);
		}
		if ((*n)->get_property (X_("release"), action_str)) {
			set_button_action (id, false, action_str);
		}
	}

	return 0;
}

/* ****************************************************************************
 * Stripable Assignment
 */

static bool flt_audio_track (boost::shared_ptr<Stripable> s) {
	return boost::dynamic_pointer_cast<AudioTrack>(s) != 0;
}

static bool flt_midi_track (boost::shared_ptr<Stripable> s) {
	return boost::dynamic_pointer_cast<MidiTrack>(s) != 0;
}

static bool flt_bus (boost::shared_ptr<Stripable> s) {
	if (boost::dynamic_pointer_cast<Route>(s) == 0) {
		return false;
	}
#ifdef MIXBUS
	if (s->mixbus () == 0) {
		return false;
	}
#endif
	return boost::dynamic_pointer_cast<Track>(s) == 0;
}

static bool flt_auxbus (boost::shared_ptr<Stripable> s) {
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

static bool flt_vca (boost::shared_ptr<Stripable> s) {
	return boost::dynamic_pointer_cast<VCA>(s) != 0;
}

static bool flt_selected (boost::shared_ptr<Stripable> s) {
	return s->is_selected ();
}

static bool flt_mains (boost::shared_ptr<Stripable> s) {
	return (s->is_master() || s->is_monitor());
}

static bool flt_all (boost::shared_ptr<Stripable> s) {
	return true;
}

static bool flt_rec_armed (boost::shared_ptr<Stripable> s) {
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(s);
	if (!t) {
		return false;
	}
	return t->rec_enable_control ()->get_value () > 0.;
}

static bool flt_instrument (boost::shared_ptr<Stripable> s) {
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
	if (!r) {
		return false;
	}
	return 0 != r->the_instrument ();
}

struct FP8SortByNewDisplayOrder
{
	// return strict (a < b)
	bool operator () (const boost::shared_ptr<Stripable> & a, const boost::shared_ptr<Stripable> & b) const
	{
		if (a->presentation_info().flags () == b->presentation_info().flags ()) {
			return a->presentation_info().order() < b->presentation_info().order();
		}

		int cmp_a = 0;
		int cmp_b = 0;

		// see also gtk2_ardour/route_sorter.h
		if (a->presentation_info().flags () & ARDOUR::PresentationInfo::VCA) {
			cmp_a = 2;
		}
#ifdef MIXBUS
		else if (a->presentation_info().flags () & ARDOUR::PresentationInfo::MasterOut) {
			cmp_a = 3;
		}
		else if (a->presentation_info().flags () & ARDOUR::PresentationInfo::Mixbus || a->mixbus()) {
			cmp_a = 1;
		}
#endif

		if (b->presentation_info().flags () & ARDOUR::PresentationInfo::VCA) {
			cmp_b = 2;
		}
#ifdef MIXBUS
		else if (b->presentation_info().flags () & ARDOUR::PresentationInfo::MasterOut) {
			cmp_b = 3;
		}
		else if (b->presentation_info().flags () & ARDOUR::PresentationInfo::Mixbus || b->mixbus()) {
			cmp_b = 1;
		}
#endif

		if (cmp_a == cmp_b) {
			return a->presentation_info().order() < b->presentation_info().order();
		}
		return cmp_a < cmp_b;
	}
};

void
FaderPort8::filter_stripables (StripableList& strips) const
{
	typedef bool (*FilterFunction)(boost::shared_ptr<Stripable>);
	FilterFunction flt;

	bool allow_master = false;
	bool allow_monitor = false;

	switch (_ctrls.mix_mode ()) {
		case MixAudio:
			flt = &flt_audio_track;
			break;
		case MixInstrument:
			flt = &flt_instrument;
			break;
		case MixBus:
			flt = &flt_bus;
			break;
		case MixVCA:
			flt = &flt_vca;
			break;
		case MixMIDI:
			flt = &flt_midi_track;
			break;
		case MixUser:
			allow_master = true;
			flt = &flt_selected;
			break;
		case MixOutputs:
			allow_master = true;
			allow_monitor = true;
			flt = &flt_mains;
			break;
		case MixInputs:
			flt = &flt_rec_armed;
			break;
		case MixFX:
			flt = &flt_auxbus;
			break;
		case MixAll:
			allow_master = true;
			flt = &flt_all;
			break;
	}

	StripableList all;
	session->get_stripables (all);

	for (StripableList::const_iterator s = all.begin(); s != all.end(); ++s) {
		if ((*s)->is_auditioner ()) { continue; }
		if ((*s)->is_hidden ()) { continue; }

		if (!allow_master  && (*s)->is_master ()) { continue; }
		if (!allow_monitor && (*s)->is_monitor ()) { continue; }

		if ((*flt)(*s)) {
			strips.push_back (*s);
		}
	}
	strips.sort (FP8SortByNewDisplayOrder());
}

/* Track/Pan mode: assign stripable to strips */
void
FaderPort8::assign_stripables (bool select_only)
{
	StripableList strips;
	filter_stripables (strips);

	if (!select_only) {
		set_periodic_display_mode (FP8Strip::Stripables);
	}

	int n_strips = strips.size();
	_channel_off = std::min (_channel_off, n_strips - 8);
	_channel_off = std::max (0, _channel_off);

	uint8_t id = 0;
	int skip = _channel_off;
	for (StripableList::const_iterator s = strips.begin(); s != strips.end(); ++s) {
		if (skip > 0) {
			--skip;
			continue;
		}

		_assigned_strips[*s] = id;
		(*s)->DropReferences.connect (assigned_stripable_connections, MISSING_INVALIDATOR,
				boost::bind (&FaderPort8::notify_stripable_added_or_removed, this), this);

		(*s)->PropertyChanged.connect (assigned_stripable_connections, MISSING_INVALIDATOR,
				boost::bind (&FaderPort8::notify_stripable_property_changed, this, boost::weak_ptr<Stripable> (*s), _1), this);
		(*s)->presentation_info ().PropertyChanged.connect (assigned_stripable_connections, MISSING_INVALIDATOR,
				boost::bind (&FaderPort8::notify_stripable_property_changed, this, boost::weak_ptr<Stripable> (*s), _1), this);

		if (select_only) {
			_ctrls.strip(id).set_text_line (3, (*s)->name (), true);
			_ctrls.strip(id).select_button ().set_color ((*s)->presentation_info ().color());
			/* update selection lights */
			_ctrls.strip(id).select_button ().set_active ((*s)->is_selected ());
			_ctrls.strip(id).select_button ().set_blinking (*s == first_selected_stripable ());
		} else {
			_ctrls.strip(id).set_stripable (*s, _ctrls.fader_mode() == ModePan);
		}

		 boost::function<void ()> cb (boost::bind (&FaderPort8::select_strip, this, boost::weak_ptr<Stripable> (*s)));
		 _ctrls.strip(id).set_select_cb (cb);

		if (++id == 8) {
			break;
		}
	}
	for (; id < 8; ++id) {
		_ctrls.strip(id).unset_controllables (select_only ? (FP8Strip::CTRL_SELECT | FP8Strip::CTRL_TEXT3) : FP8Strip::CTRL_ALL);
	}
}

/* ****************************************************************************
 * Plugin selection and parameters
 */

void
FaderPort8::assign_processor_ctrls ()
{
	if (_proc_params.size() == 0) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}
	set_periodic_display_mode (FP8Strip::PluginParam);

	std::vector <ProcessorCtrl*> toggle_params;
	std::vector <ProcessorCtrl*> slider_params;

	for ( std::list <ProcessorCtrl>::iterator i = _proc_params.begin(); i != _proc_params.end(); ++i) {
		if ((*i).ac->toggled()) {
			toggle_params.push_back (&(*i));
		} else {
			slider_params.push_back (&(*i));
		}
	}

	int n_parameters = std::max (toggle_params.size(), slider_params.size());

	_parameter_off = std::min (_parameter_off, n_parameters - 8);
	_parameter_off = std::max (0, _parameter_off);

	uint8_t id = 0;
	for (size_t i = _parameter_off; i < (size_t)n_parameters; ++i) {
		if (i >= toggle_params.size ()) {
			_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_FADER & ~FP8Strip::CTRL_TEXT0 & ~FP8Strip::CTRL_TEXT1);
		}
		else if (i >= slider_params.size ()) {
			_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_SELECT & ~FP8Strip::CTRL_TEXT3);
		} else {
			_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_FADER & ~FP8Strip::CTRL_TEXT0 & ~FP8Strip::CTRL_TEXT1 & ~FP8Strip::CTRL_SELECT & ~FP8Strip::CTRL_TEXT3);
		}

		if (i < slider_params.size ()) {
			_ctrls.strip(id).set_fader_controllable (slider_params[i]->ac);
			_ctrls.strip(id).set_text_line (0, slider_params[i]->name);
		}
		if (i < toggle_params.size ()) {
			_ctrls.strip(id).set_select_controllable (toggle_params[i]->ac);
			_ctrls.strip(id).set_text_line (3, toggle_params[i]->name, true);
		}
		 if (++id == 8) {
			 break;
		 }
	}

	// clear remaining
	for (; id < 8; ++id) {
		_ctrls.strip(id).unset_controllables ();
	}
}

void
FaderPort8::build_well_known_processor_ctrls (boost::shared_ptr<Stripable> s, bool eq)
{
#define PUSH_BACK_NON_NULL(N, C) do {if (C) { _proc_params.push_back (ProcessorCtrl (N, C)); }} while (0)

	_proc_params.clear ();
	if (eq) {
		int cnt = s->eq_band_cnt();
		PUSH_BACK_NON_NULL ("Enable", s->eq_enable_controllable ());
		PUSH_BACK_NON_NULL ("HP/LP", s->filter_enable_controllable ());
#ifdef MIXBUS32
		PUSH_BACK_NON_NULL ("Lo-Bell", s->eq_lpf_controllable ());
		PUSH_BACK_NON_NULL ("Hi-Bell", s->eq_hpf_controllable ());
#else
		PUSH_BACK_NON_NULL ("Freq HP", s->eq_hpf_controllable ());
#endif
		for (int band = 0; band < cnt; ++band) {
			std::string bn = s->eq_band_name (band);
			PUSH_BACK_NON_NULL (string_compose ("Gain %1", bn), s->eq_gain_controllable (band));
			PUSH_BACK_NON_NULL (string_compose ("Freq %1", bn), s->eq_freq_controllable (band));
			PUSH_BACK_NON_NULL (string_compose ("Band %1", bn), s->eq_q_controllable (band));
			PUSH_BACK_NON_NULL (string_compose ("Shape %1", bn), s->eq_shape_controllable (band));
		}
	} else {
		PUSH_BACK_NON_NULL ("Enable", s->comp_enable_controllable ());
		PUSH_BACK_NON_NULL ("Threshold", s->comp_threshold_controllable ());
		PUSH_BACK_NON_NULL ("Speed", s->comp_speed_controllable ());
		PUSH_BACK_NON_NULL ("Mode", s->comp_mode_controllable ());
	}
}

void
FaderPort8::select_plugin (int num)
{
	// make sure drop_ctrl_connections() was called
	assert (_proc_params.size() == 0 && _showing_well_known == 0);

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (first_selected_stripable());
	if (!r) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}
	if (num < 0) {
		build_well_known_processor_ctrls (r, num == -1);
		assign_processor_ctrls ();
		_showing_well_known = num;
		return;
	}
	_showing_well_known = 0;

	boost::shared_ptr<Processor> proc = r->nth_plugin (num);
	if (!proc) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	// switching to "Mode Track" -> calls FaderPort8::notify_fader_mode_changed()
	// which drops the references, disconnects the signal and re-spills tracks
	proc->DropReferences.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FP8Controls::set_fader_mode, &_ctrls, ModeTrack), this);

	// build params
	_proc_params.clear();
	set<Evoral::Parameter> p = proc->what_can_be_automated ();
	for (set<Evoral::Parameter>::iterator i = p.begin(); i != p.end(); ++i) {
		std::string n = proc->describe_parameter (*i);
		if (n == "hidden") {
			continue;
		}
		_proc_params.push_back (ProcessorCtrl (n, proc->automation_control (*i)));
	}

	// TODO: open plugin GUI  if (_proc_params.size() > 0)

	// display
	assign_processor_ctrls ();
}

/* short 4 chars at most */
static std::string plugintype (ARDOUR::PluginType t) {
	switch (t) {
		case AudioUnit:
			return "AU";
		case LADSPA:
			return "LV1";
		case LV2:
			return "LV2";
		case Windows_VST:
		case LXVST:
		case MacVST:
			return "VST";
		case Lua:
			return "Lua";
		default:
			break;
	}
	return enum_2_string (t);
}

void
FaderPort8::spill_plugins ()
{
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (first_selected_stripable());
	if (!r) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	drop_ctrl_connections ();

	// switching to "Mode Track" -> calls FaderPort8::notify_fader_mode_changed()
	// which drops the references, disconnects the signal and re-spills tracks
	r->DropReferences.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FP8Controls::set_fader_mode, &_ctrls, ModeTrack), this);

	// update when processor change
	r->processors_changed.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::spill_plugins, this), this);

	// count available
	boost::shared_ptr<Processor> proc;

	std::vector<uint32_t> procs;

	for (uint32_t i = 0; 0 != (proc = r->nth_plugin (i)); ++i) {
		if (!proc->display_to_user ()) {
#ifdef MIXBUS
			boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
			if (pi->is_channelstrip ()) // don't skip MB PRE
#endif
			continue;
		}
		int n_controls = 0;
		set<Evoral::Parameter> p = proc->what_can_be_automated ();
		for (set<Evoral::Parameter>::iterator j = p.begin(); j != p.end(); ++j) {
			std::string n = proc->describe_parameter (*j);
			if (n == "hidden") {
				continue;
			}
			++n_controls;
		}
		if (n_controls > 0) {
			procs.push_back (i);
		}
	}

	int n_plugins = procs.size();
	int spillwidth = 8;
	bool have_well_known_eq = false;
	bool have_well_known_comp = false;

	// reserve last slot(s) for "well-known"
	if (r->eq_band_cnt() > 0) {
		--spillwidth;
		have_well_known_eq = true;
	}
	if (r->comp_enable_controllable ()) {
		--spillwidth;
		have_well_known_comp = true;
	}

	if (n_plugins == 0 && !have_well_known_eq && !have_well_known_comp) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	set_periodic_display_mode (FP8Strip::PluginSelect);

	_plugin_off = std::min (_plugin_off, n_plugins - spillwidth);
	_plugin_off = std::max (0, _plugin_off);

	uint8_t id = 0;
	for (uint32_t i = _plugin_off; ; ++i) {
		if (i >= procs.size()) {
			break;
		}
		boost::shared_ptr<Processor> proc = r->nth_plugin (procs[i]);
		if (!proc) {
			break;
		}
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
		boost::function<void ()> cb (boost::bind (&FaderPort8::select_plugin, this, procs[i]));

		_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT & ~FP8Strip::CTRL_SELECT);
		_ctrls.strip(id).set_select_cb (cb);
		_ctrls.strip(id).select_button ().set_color (0x00ff00ff);
		_ctrls.strip(id).select_button ().set_active (true /*proc->enabled()*/);
		_ctrls.strip(id).select_button ().set_blinking (false);
		_ctrls.strip(id).set_text_line (0, proc->name());
		_ctrls.strip(id).set_text_line (1, pi->plugin()->maker());
		_ctrls.strip(id).set_text_line (2, plugintype (pi->type()));
		_ctrls.strip(id).set_text_line (3, "");

		if (++id == spillwidth) {
			break;
		}
	}
	// clear remaining
	for (; id < spillwidth; ++id) {
		_ctrls.strip(id).unset_controllables ();
	}

	if (have_well_known_comp) {
			assert (id < 8);
		 boost::function<void ()> cb (boost::bind (&FaderPort8::select_plugin, this, -2));
		 _ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT & ~FP8Strip::CTRL_SELECT);
		 _ctrls.strip(id).set_select_cb (cb);
		 _ctrls.strip(id).select_button ().set_color (0xffff00ff);
		 _ctrls.strip(id).select_button ().set_active (true);
		 _ctrls.strip(id).select_button ().set_blinking (false);
		 _ctrls.strip(id).set_text_line (0, "Comp");
		 _ctrls.strip(id).set_text_line (1, "Built-In");
		 _ctrls.strip(id).set_text_line (2, "--");
		 _ctrls.strip(id).set_text_line (3, "");
		 ++id;
	}
	if (have_well_known_eq) {
			assert (id < 8);
		 boost::function<void ()> cb (boost::bind (&FaderPort8::select_plugin, this, -1));
		 _ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT & ~FP8Strip::CTRL_SELECT);
		 _ctrls.strip(id).set_select_cb (cb);
		 _ctrls.strip(id).select_button ().set_color (0xffff00ff);
		 _ctrls.strip(id).select_button ().set_active (true);
		 _ctrls.strip(id).select_button ().set_blinking (false);
		 _ctrls.strip(id).set_text_line (0, "EQ");
		 _ctrls.strip(id).set_text_line (1, "Built-In");
		 _ctrls.strip(id).set_text_line (2, "--");
		 _ctrls.strip(id).set_text_line (3, "");
		 ++id;
	}
	assert (id == 8);
}

/* ****************************************************************************
 * Aux Sends and Mixbus assigns
 */

void
FaderPort8::assign_sends ()
{
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (!s) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	int n_sends = 0;
	while (0 != s->send_level_controllable (n_sends)) {
		++n_sends;
	}
	if (n_sends == 0) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	drop_ctrl_connections ();
	s->DropReferences.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FP8Controls::set_fader_mode, &_ctrls, ModeTrack), this);

	set_periodic_display_mode (FP8Strip::SendDisplay);

	_plugin_off = std::min (_plugin_off, n_sends - 8);
	_plugin_off = std::max (0, _plugin_off);

	uint8_t id = 0;
	int skip = _parameter_off;
	for (uint32_t i = _plugin_off; ; ++i) {
		if (skip > 0) {
			--skip;
			continue;
		}
		boost::shared_ptr<AutomationControl> send = s->send_level_controllable (i);
		if (!send) {
			break;
		}

		_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_FADER & ~FP8Strip::CTRL_TEXT0 & ~FP8Strip::CTRL_TEXT1 & ~FP8Strip::CTRL_TEXT3 & ~FP8Strip::CTRL_SELECT);
		_ctrls.strip(id).set_fader_controllable (send);
		_ctrls.strip(id).set_text_line (0, s->send_name (i));
		_ctrls.strip(id).set_mute_controllable (s->send_enable_controllable (i));

		if (++id == 8) {
			break;
		}
	}
	// clear remaining
	for (; id < 8; ++id) {
		_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT3 & ~FP8Strip::CTRL_SELECT);
	}
#ifdef MIXBUS // master-assign on last solo
	_ctrls.strip(7).set_solo_controllable (s->master_send_enable_controllable ());
#endif
	/* set select buttons */
	assigned_stripable_connections.drop_connections ();
	_assigned_strips.clear ();
	assign_stripables (true);
}

/* ****************************************************************************
 * Main stripable assignment (dispatch depending on mode)
 */

void
FaderPort8::assign_strips (bool reset_bank)
{
	if (reset_bank) {
		_channel_off = 0;
	}

	assigned_stripable_connections.drop_connections ();
	_assigned_strips.clear ();

	FaderMode fadermode = _ctrls.fader_mode ();
	switch (fadermode) {
		case ModeTrack:
		case ModePan:
			assign_stripables ();
			stripable_selection_changed (); // update selection, automation-state
			break;
		case ModePlugins:
			if (_proc_params.size() > 0) {
				assign_processor_ctrls ();
			} else {
				spill_plugins ();
			}
			break;
		case ModeSend:
			assign_sends ();
			break;
	}
}

/* ****************************************************************************
 * some helper functions
 */

void
FaderPort8::set_periodic_display_mode (FP8Strip::DisplayMode m)
{
	for (uint8_t id = 0; id < 8; ++id) {
		_ctrls.strip(id).set_periodic_display_mode (m);
	}
}

void
FaderPort8::drop_ctrl_connections ()
{
	_proc_params.clear();
	processor_connections.drop_connections ();
	_showing_well_known = 0;
}

/* functor for FP8Strip's select button */
void
FaderPort8::select_strip (boost::weak_ptr<Stripable> ws)
{
	boost::shared_ptr<Stripable> s = ws.lock();
	if (!s) {
		return;
	}
#if 1 /* single exclusive selection by default, toggle via shift */
	if (shift_mod ()) {
		ToggleStripableSelection (s);
	} else {
		SetStripableSelection (s);
	}
#else
	/* tri-state selection: This allows to set the "first selected"
	 * with a single click without clearing the selection.
	 * Single de/select via shift.
	 */
	if (shift_mod ()) {
		if (s->is_selected ()) {
			RemoveStripableFromSelection (s);
		} else {
			SetStripableSelection (s);
		}
		return;
	}
	if (s->is_selected () && s != first_selected_stripable ()) {
		set_first_selected_stripable (s);
		stripable_selection_changed ();
	} else {
		ToggleStripableSelection (s);
	}
#endif
}

/* ****************************************************************************
 * Assigned Stripable Callbacks
 */

void
FaderPort8::notify_fader_mode_changed ()
{
	FaderMode fadermode = _ctrls.fader_mode ();

	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (!s && (fadermode == ModePlugins || fadermode == ModeSend)) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	drop_ctrl_connections ();

	switch (fadermode) {
		case ModeTrack:
		case ModePan:
			break;
		case ModePlugins:
		case ModeSend:
			_plugin_off = 0;
			_parameter_off = 0;
			// force unset rec-arm button, see also FaderPort8::button_arm
			_ctrls.button (FP8Controls::BtnArm).set_active (false);
			ARMButtonChange (false);
			break;
	}
	assign_strips (false);
	notify_automation_mode_changed ();
}

void
FaderPort8::notify_stripable_added_or_removed ()
{
	/* called by
	 *  - DropReferences
	 *  - session->RouteAdded
	 *  - PresentationInfo::Change
	 *    - Properties::hidden
	 *    - Properties::order
	 */
	assign_strips (false);
}

/* called from static PresentationInfo::Change */
void
FaderPort8::notify_pi_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (Properties::hidden)) {
		notify_stripable_added_or_removed ();
	}
	if (what_changed.contains (Properties::order)) {
		notify_stripable_added_or_removed ();
	}
	// Properties::selected is handled via StripableSelectionChanged
}

void
FaderPort8::notify_stripable_property_changed (boost::weak_ptr<Stripable> ws, const PropertyChange& what_changed)
{
	boost::shared_ptr<Stripable> s = ws.lock();
	if (!s) {
		assert (0); // this should not happen
		return;
	}
	if (_assigned_strips.find (s) == _assigned_strips.end()) {
		/* it can happen that signal emission is delayed.
		 * A signal may already be in the queue but the
		 * _assigned_strips has meanwhile changed.
		 *
		 * before _assigned_strips changes, the connections are dropped
		 * but that does not seem to invalidate pending requests :(
		 *
		 * Seen when creating a new MB session and Mixbusses are added
		 * incrementally.
		 */
		return;
	}
	uint8_t id = _assigned_strips[s];

	if (what_changed.contains (Properties::color)) {
		_ctrls.strip(id).select_button ().set_color (s->presentation_info ().color());
	}

	if (what_changed.contains (Properties::name)) {
		switch (_ctrls.fader_mode ()) {
			case ModeSend:
				_ctrls.strip(id).set_text_line (3, s->name(), true);
				break;
			case ModeTrack:
			case ModePan:
				_ctrls.strip(id).set_text_line (0, s->name());
				break;
			case ModePlugins:
				assert (0);
				break;
		}
	}
}

void
FaderPort8::stripable_selection_changed ()
{
	if (!_device_active) {
		/* this can be called anytime from the static
		 * ControlProtocol::StripableSelectionChanged
		 */
		return;
	}
	automation_state_connections.drop_connections();

	switch (_ctrls.fader_mode ()) {
		case ModePlugins:
			if (_proc_params.size () > 0 && _showing_well_known < 0) {
				/* w/well-known -> re-assign to new strip */
				int wk = _showing_well_known;
				drop_ctrl_connections ();
				select_plugin (wk);
			}
			return;
		case ModeSend:
			_plugin_off = 0;
			assign_sends ();
			return;
		case ModeTrack:
		case ModePan:
			break;
	}

	/* update selection lights */
	for (StripAssignmentMap::const_iterator i = _assigned_strips.begin(); i != _assigned_strips.end(); ++i) {
		boost::shared_ptr<ARDOUR::Stripable> s = i->first;
		uint8_t id = i->second;
		bool sel = s->is_selected ();
		_ctrls.strip(id).select_button ().set_active (sel);
		_ctrls.strip(id).select_button ().set_blinking (sel && s == first_selected_stripable ());
	}

	/* track automation-mode of primary selection */
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (s) {
		boost::shared_ptr<AutomationControl> ac;
		ac = s->gain_control();
		if (ac && ac->alist()) {
			ac->alist()->automation_state_changed.connect (automation_state_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_automation_mode_changed, this), this);
		}
		ac = s->pan_azimuth_control();
		if (ac && ac->alist()) {
			ac->alist()->automation_state_changed.connect (automation_state_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_automation_mode_changed, this), this);
		}
	}
	/* set lights */
	notify_automation_mode_changed ();
}


/* ****************************************************************************
 * Banking
 */

void
FaderPort8::move_selected_into_view ()
{
	boost::shared_ptr<Stripable> selected = first_selected_stripable ();
	if (!selected) {
		return;
	}

	StripableList strips;
	filter_stripables (strips);

	StripableList::iterator it = std::find (strips.begin(), strips.end(), selected);
	if (it == strips.end()) {
		return;
	}
	int off = std::distance (strips.begin(), it);

	if (_channel_off <= off && off < _channel_off + 8) {
		return;
	}

	if (_channel_off > off) {
		_channel_off = off;
	} else {
		_channel_off = off - 7;
	}
	assign_strips (false);
}

void
FaderPort8::select_prev_next (bool next)
{
	StripableList strips;
	filter_stripables (strips);

	boost::shared_ptr<Stripable> selected = first_selected_stripable ();
	if (!selected) {
		if (strips.size() > 0) {
			if (next) {
				SetStripableSelection (strips.front ());
			} else {
				SetStripableSelection (strips.back ());
			}
		}
		return;
	}

	bool found = false;
	boost::shared_ptr<Stripable> toselect;
	for (StripableList::const_iterator s = strips.begin(); s != strips.end(); ++s) {
		if (*s == selected) {
			if (!next) {
				found = true;
				break;
			}
			++s;
			if (s != strips.end()) {
				toselect = *s;
				found = true;
			}
			break;
		}
		if (!next) {
			toselect = *s;
		}
	}

	if (found && toselect) {
		SetStripableSelection (toselect);
	}
}

void
FaderPort8::bank (bool down, bool page)
{
	int dt = page ? 8 : 1;
	if (down) {
		dt *= -1;
	}
	_channel_off += dt;
	assign_strips (false);
}

void
FaderPort8::bank_param (bool down, bool page)
{
	int dt = page ? 8 : 1;
	if (down) {
		dt *= -1;
	}
	switch (_ctrls.fader_mode ()) {
		case ModePlugins:
			if (_proc_params.size() > 0) {
				_parameter_off += dt;
				assign_processor_ctrls ();
			} else {
				_plugin_off += dt;
				spill_plugins ();
			}
			break;
		case ModeSend:
			_plugin_off += dt;
			assign_sends ();
			break;
		default:
			break;
	}
}
