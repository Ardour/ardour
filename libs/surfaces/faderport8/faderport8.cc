/*
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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
#include "ardour/panner_shell.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/processor.h"
#include "ardour/rc_configuration.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_configuration.h"
#include "ardour/tempo.h"
#include "ardour/vca.h"

#include "faderport8.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace std;
using namespace ArdourSurface::FP_NAMESPACE;
using namespace ArdourSurface::FP_NAMESPACE::FP8Types;

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

bool
FaderPort8::ProcessorCtrl::operator< (const FaderPort8::ProcessorCtrl& other) const
{
	if (ac->desc().display_priority == other.ac->desc().display_priority) {
		return ac->parameter () < other.ac->parameter ();
	}
	/* sort higher priority first */
	return ac->desc().display_priority > other.ac->desc().display_priority;
}


FaderPort8::FaderPort8 (Session& s)
#ifdef FADERPORT16
	: ControlProtocol (s, _("PreSonus FaderPort16"))
#elif defined FADERPORT2
	: ControlProtocol (s, _("PreSonus FaderPort2"))
#else
	: ControlProtocol (s, _("PreSonus FaderPort8"))
#endif
	, AbstractUI<FaderPort8Request> (name())
	, _connection_state (ConnectionState (0))
	, _device_active (false)
	, _ctrls (*this)
	, _plugin_off (0)
	, _parameter_off (0)
	, _show_presets (false)
	, _showing_well_known (0)
	, _timer_divider (0)
	, _blink_onoff (false)
	, _shift_lock (false)
	, _shift_pressed (0)
	, gui (0)
	, _link_enabled (false)
	, _link_locked (false)
	, _chan_locked (false)
	, _clock_mode (1)
	, _scribble_mode (2)
	, _two_line_text (false)
	, _auto_pluginui (true)
{
	boost::shared_ptr<ARDOUR::Port> inp;
	boost::shared_ptr<ARDOUR::Port> outp;

#ifdef FADERPORT16
	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "FaderPort16 Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "FaderPort16 Send", true);
#elif defined FADERPORT2
	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "FaderPort2 Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "FaderPort2 Send", true);
#else
	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "FaderPort8 Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "FaderPort8 Send", true);
#endif
	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(inp);
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(outp);

	if (_input_port == 0 || _output_port == 0) {
		throw failed_constructor();
	}

#ifdef FADERPORT16
	_input_bundle.reset (new ARDOUR::Bundle (_("FaderPort16 (Receive)"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("FaderPort16 (Send)"), false));
#elif defined FADERPORT2
	_input_bundle.reset (new ARDOUR::Bundle (_("FaderPort2 (Receive)"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("FaderPort2 (Send)"), false));
#else
	_input_bundle.reset (new ARDOUR::Bundle (_("FaderPort8 (Receive)"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("FaderPort8 (Send)"), false));
#endif

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

	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::connection_handler, this, _2, _4), this);
	ARDOUR::AudioEngine::instance()->Stopped.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::engine_reset, this), this);
	ARDOUR::Port::PortDrop.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::engine_reset, this), this);

	/* bind button events to call libardour actions */
	setup_actions ();

	_ctrls.FaderModeChanged.connect_same_thread (modechange_connections, boost::bind (&FaderPort8::notify_fader_mode_changed, this));
	_ctrls.MixModeChanged.connect_same_thread (modechange_connections, boost::bind (&FaderPort8::assign_strips, this));
}

FaderPort8::~FaderPort8 ()
{
	/* this will be called from the main UI thread. during Session::destroy().
	 * There can be concurrent activity from BaseUI::main_thread -> AsyncMIDIPort
	 * -> MIDI::Parser::signal -> ... to any of the midi_connections
	 *
	 * stop event loop early and join thread */
	stop ();

	if (_input_port) {
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("unregistering input port %1\n", boost::shared_ptr<ARDOUR::Port>(_input_port)->name()));
		Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
		AudioEngine::instance()->unregister_port (_input_port);
		_input_port.reset ();
	}

	disconnected (); // zero faders, turn lights off, clear strips

	if (_output_port) {
		_output_port->drain (10000,  250000); /* check every 10 msecs, wait up to 1/4 second for the port to drain */
		DEBUG_TRACE (DEBUG::FaderPort8, string_compose ("unregistering output port %1\n", boost::shared_ptr<ARDOUR::Port>(_output_port)->name()));
		Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
		AudioEngine::instance()->unregister_port (_output_port);
		_output_port.reset ();
	}

	tear_down_gui ();
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
		disconnected ();
	}
}

void
FaderPort8::stop ()
{
	DEBUG_TRACE (DEBUG::FaderPort8, "BaseUI::quit ()\n");
	BaseUI::quit ();
	close (); // drop references, disconnect from session signals
}

void
FaderPort8::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

bool
FaderPort8::periodic ()
{
	/* prepare TC display -- handled by stripable Periodic ()
	 * in FP8Strip::periodic_update_timecode
	 */
	if (_ctrls.display_timecode () && clock_mode ()) {
		Timecode::Time TC;
		session->timecode_time (TC);
		_timecode = Timecode::timecode_format_time(TC);

		char buf[16];
		Temporal::BBT_Time BBT = session->tempo_map ().bbt_at_sample (session->transport_sample ());
		snprintf (buf, sizeof (buf),
				" %02" PRIu32 "|%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32,
				BBT.bars % 100, BBT.beats %100,
				(BBT.ticks/ 100) %100, BBT.ticks %100);
		_musical_time = std::string (buf);
	} else {
		_timecode.clear ();
		_musical_time.clear ();
	}

#ifdef FADERPORT16
	/* every second, send "running" */
	if (++_timer_divider == 10) {
		_timer_divider = 0;
		tx_midi3 (0xa0, 0x00, 0x00);
	}
#endif

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
		stop ();
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
	route_state_connections.drop_connections ();
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

	memset (_channel_off, 0, sizeof (_channel_off));
	_plugin_off = _parameter_off = 0;
	_blink_onoff = false;
	_shift_lock = false;
	_shift_pressed = 0;
	_timer_divider = 0;

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
	assign_strips ();

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
		for (uint8_t id = 0; id < N_STRIPS; ++id) {
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

	if (!port || !_input_port) {
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
		samplepos_t now = session->engine().sample_time();
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
	/* encoder
	 *  val Bit 6 = direction, Bits 0-5 = number of steps
	 */
	static const uint8_t dir_mask = 0x40;
	static const uint8_t step_mask = 0x3f;

	if (tb->controller_number == 0x3c) {
		encoder_navigate (tb->value & dir_mask ? true : false, tb->value & step_mask);
	}
	if (tb->controller_number == 0x10) {
#ifdef FADERPORT2
		if (_ctrls.nav_mode() == NavPan) {
			encoder_parameter (tb->value & dir_mask ? true : false, tb->value & step_mask);
		} else {
			encoder_navigate (tb->value & dir_mask ? true : false, tb->value & step_mask);
		}
#else
		encoder_parameter (tb->value & dir_mask ? true : false, tb->value & step_mask);
#endif
		/* if Shift key is held while turning Pan/Param, don't lock shift. */
		if (_shift_pressed > 0 && !_shift_lock) {
			_shift_connection.disconnect ();
			_shift_lock = false;
		}
	}
}

void
FaderPort8::note_on_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	debug_2byte_msg ("ON", tb->note_number, tb->velocity);

	/* fader touch */
#ifdef FADERPORT16
	static const uint8_t touch_id_uppper = 0x77;
#else
	static const uint8_t touch_id_uppper = 0x6f;
#endif
	if (tb->note_number >= 0x68 && tb->note_number <= touch_id_uppper) {
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

#ifdef FADERPORT16
	static const uint8_t touch_id_uppper = 0x77;
#else
	static const uint8_t touch_id_uppper = 0x6f;
#endif
	if (tb->note_number >= 0x68 && tb->note_number <= touch_id_uppper) {
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

#ifndef FADERPORT2
	node.set_property (X_("clock-mode"), _clock_mode);
	node.set_property (X_("scribble-mode"), _scribble_mode);
	node.set_property (X_("two-line-text"), _two_line_text);
#endif

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
	XMLNode const* child;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::set_state Input\n");
			boost::shared_ptr<ARDOUR::Port>(_input_port)->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			DEBUG_TRACE (DEBUG::FaderPort8, "FaderPort8::set_state Output\n");
			boost::shared_ptr<ARDOUR::Port>(_output_port)->set_state (*portnode, version);
		}
	}

	node.get_property (X_("clock-mode"), _clock_mode);
	node.get_property (X_("scribble-mode"), _scribble_mode);
	node.get_property (X_("two-line-text"), _two_line_text);

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
		default:
			assert (0);
			/* fallthrough */
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
	strips.sort (Stripable::Sorter(true));
}

/* Track/Pan mode: assign stripable to strips, Send-mode: selection */
void
FaderPort8::assign_stripables (bool select_only)
{
	StripableList strips;
	filter_stripables (strips);

	if (!select_only) {
		set_periodic_display_mode (FP8Strip::Stripables);
	}

#ifdef FADERPORT2
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (s) {
		_ctrls.strip(0).set_stripable (s, _ctrls.fader_mode() == ModePan);
	} else {
		_ctrls.strip(0).unset_controllables ( FP8Strip::CTRL_ALL );
	}
	return;
#endif

	int n_strips = strips.size();
	int channel_off = get_channel_off (_ctrls.mix_mode ());
	channel_off = std::min (channel_off, n_strips - N_STRIPS);
	channel_off = std::max (0, channel_off);
	set_channel_off (_ctrls.mix_mode (), channel_off);

	uint8_t id = 0;
	int skip = channel_off;
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

		if (boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(*s)) {
			if (r->panner_shell()) {
				r->panner_shell()->Changed.connect (assigned_stripable_connections, MISSING_INVALIDATOR,
				boost::bind (&FaderPort8::notify_stripable_property_changed, this, boost::weak_ptr<Stripable> (*s), PropertyChange()), this);
			}
		}

		if (select_only) {
			/* used in send mode */
			_ctrls.strip(id).set_text_line (3, (*s)->name (), true);
			_ctrls.strip(id).set_select_button_color ((*s)->presentation_info ().color());
			/* update selection lights */
			_ctrls.strip(id).select_button ().set_active ((*s)->is_selected ());
			_ctrls.strip(id).select_button ().set_blinking (*s == first_selected_stripable ());
		} else {
			_ctrls.strip(id).set_stripable (*s, _ctrls.fader_mode() == ModePan);
		}

		 boost::function<void ()> cb (boost::bind (&FaderPort8::select_strip, this, boost::weak_ptr<Stripable> (*s)));
		 _ctrls.strip(id).set_select_cb (cb);

		if (++id == N_STRIPS) {
			break;
		}
	}
	for (; id < N_STRIPS; ++id) {
		_ctrls.strip(id).unset_controllables (select_only ? (FP8Strip::CTRL_SELECT | FP8Strip::CTRL_TEXT3) : FP8Strip::CTRL_ALL);
		_ctrls.strip(id).set_periodic_display_mode (FP8Strip::Stripables);
	}
}

/* ****************************************************************************
 * Control Link/Lock
 */

void
FaderPort8::unlock_link (bool drop)
{
	link_locked_connection.disconnect ();

	if (drop) {
		stop_link (); // calls back here with drop = false
		return;
	}

	_link_locked = false;

	if (_link_enabled) {
		assert (_ctrls.button (FP8Controls::BtnLink).is_active ());
		_link_control.reset ();
		start_link (); // re-connect & update LED colors
	} else {
		_ctrls.button (FP8Controls::BtnLink).set_active (false);
		_ctrls.button (FP8Controls::BtnLink).set_color (0x888888ff);
		_ctrls.button (FP8Controls::BtnLock).set_active (false);
		_ctrls.button (FP8Controls::BtnLock).set_color (0x888888ff);
	}
}

void
FaderPort8::lock_link ()
{
	boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (_link_control.lock ());
	if (!ac) {
		return;
	}
	ac->DropReferences.connect (link_locked_connection, MISSING_INVALIDATOR, boost::bind (&FaderPort8::unlock_link, this, true), this);

	// stop watching for focus events
	link_connection.disconnect ();

	_link_locked = true;

	_ctrls.button (FP8Controls::BtnLock).set_color (0x00ff00ff);
	_ctrls.button (FP8Controls::BtnLink).set_color (0x00ff00ff);
}

void
FaderPort8::stop_link ()
{
	if (!_link_enabled) {
		return;
	}
	link_connection.disconnect ();
	_link_control.reset ();
	_link_enabled = false;
	unlock_link (); // also updates button colors
}

void
FaderPort8::start_link ()
{
	assert (!_link_locked);

	_link_enabled = true;
	_ctrls.button (FP8Controls::BtnLink).set_active (true);
	_ctrls.button (FP8Controls::BtnLock).set_active (true);
	nofity_focus_control (_link_control); // update BtnLink, BtnLock colors

	PBD::Controllable::GUIFocusChanged.connect (link_connection, MISSING_INVALIDATOR, boost::bind (&FaderPort8::nofity_focus_control, this, _1), this);
}


/* ****************************************************************************
 * Plugin selection and parameters
 */

void
FaderPort8::toggle_preset_param_mode ()
{
	FaderMode fadermode = _ctrls.fader_mode ();
	if (fadermode != ModePlugins || _proc_params.size() == 0) {
		return;
	}
	_show_presets = ! _show_presets;
	assign_processor_ctrls ();
}

void
FaderPort8::preset_changed ()
{
	if (_show_presets) {
		assign_processor_ctrls ();
	}
}

void
FaderPort8::assign_processor_ctrls ()
{
	if (_proc_params.size() == 0) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}
	set_periodic_display_mode (FP8Strip::PluginParam);

	if (_show_presets) {
		if (assign_plugin_presets (_plugin_insert.lock ())) {
			return;
		}
		_show_presets = false;
	}

	std::vector <ProcessorCtrl*> toggle_params;
	std::vector <ProcessorCtrl*> slider_params;

	for (std::list<ProcessorCtrl>::iterator i = _proc_params.begin(); i != _proc_params.end(); ++i) {
		if ((*i).ac->toggled()) {
			toggle_params.push_back (&(*i));
		} else {
			slider_params.push_back (&(*i));
		}
	}

	int n_parameters = std::max (toggle_params.size(), slider_params.size());

	_parameter_off = std::min (_parameter_off, n_parameters - N_STRIPS);
	_parameter_off = std::max (0, _parameter_off);

	uint8_t id = 0;
	for (size_t i = _parameter_off; i < (size_t)n_parameters; ++i) {
		if (i >= toggle_params.size ()) {
			_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_FADER & ~FP8Strip::CTRL_TEXT01 & ~FP8Strip::CTRL_TEXT2);
		}
		else if (i >= slider_params.size ()) {
			_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_SELECT & ~FP8Strip::CTRL_TEXT3);
		} else {
			_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_FADER & ~FP8Strip::CTRL_TEXT & ~FP8Strip::CTRL_SELECT);
		}

		if (i < slider_params.size ()) {
			_ctrls.strip(id).set_fader_controllable (slider_params[i]->ac);
			std::string param_name = slider_params[i]->name;
			_ctrls.strip(id).set_text_line (0, param_name.substr (0, 9));
			_ctrls.strip(id).set_text_line (1, param_name.length () > 9 ? param_name.substr (9) : "");
		}
		if (i < toggle_params.size ()) {
			_ctrls.strip(id).set_select_controllable (toggle_params[i]->ac);
			_ctrls.strip(id).set_text_line (3, toggle_params[i]->name, true);
		}
		if (++id == N_STRIPS) {
			break;
		}
	}

	// clear remaining
	for (; id < N_STRIPS; ++id) {
		_ctrls.strip(id).unset_controllables ();
	}
}

bool
FaderPort8::assign_plugin_presets (boost::shared_ptr<PluginInsert> pi)
{
	if (!pi) {
		return false;
	}
	boost::shared_ptr<ARDOUR::Plugin> plugin = pi->plugin ();

	std::vector<ARDOUR::Plugin::PresetRecord> presets = plugin->get_presets ();
	if (presets.size () == 0) {
		return false;
	}

	int n_parameters = presets.size ();

	_parameter_off = std::min (_parameter_off, n_parameters - (N_STRIPS - 1));
	_parameter_off = std::max (0, _parameter_off);
	Plugin::PresetRecord active = plugin->last_preset ();

	uint8_t id = 0;
	for (size_t i = _parameter_off; i < (size_t)n_parameters; ++i) {
		_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT01 & ~FP8Strip::CTRL_TEXT3 & ~FP8Strip::CTRL_SELECT);
		 boost::function<void ()> cb (boost::bind (&FaderPort8::select_plugin_preset, this, i));
		_ctrls.strip(id).set_select_cb (cb);
		_ctrls.strip(id).select_button ().set_active (true);
		if (active != presets.at(i)) {
			_ctrls.strip(id).select_button ().set_color (0x0000ffff);
			_ctrls.strip(id).select_button ().set_blinking (false);
		} else {
			_ctrls.strip(id).select_button ().set_color (0x00ffffff);
			_ctrls.strip(id).select_button ().set_blinking (plugin->parameter_changed_since_last_preset ());
		}
		std::string label = presets.at(i).label;
		_ctrls.strip(id).set_text_line (0, label.substr (0, 9));
		_ctrls.strip(id).set_text_line (1, label.length () > 9 ? label.substr (9) : "");
		_ctrls.strip(id).set_text_line (3, "PRESET", true);
		if (++id == (N_STRIPS - 1)) {
			break;
		}
	}

	// clear remaining
	for (; id < (N_STRIPS - 1); ++id) {
		_ctrls.strip(id).unset_controllables ();
	}

	// pin clear-preset to the last slot
	assert (id == (N_STRIPS - 1));
	_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT0 & ~FP8Strip::CTRL_TEXT3 & ~FP8Strip::CTRL_SELECT);
	 boost::function<void ()> cb (boost::bind (&FaderPort8::select_plugin_preset, this, SIZE_MAX));
	_ctrls.strip(id).set_select_cb (cb);
	_ctrls.strip(id).select_button ().set_blinking (false);
	_ctrls.strip(id).select_button ().set_color (active.uri.empty() ? 0x00ffffff : 0x0000ffff);
	_ctrls.strip(id).select_button ().set_active (true);
	_ctrls.strip(id).set_text_line (0, _("(none)"));
	_ctrls.strip(id).set_text_line (3, "PRESET", true);
	return true;
}

void
FaderPort8::build_well_known_processor_ctrls (boost::shared_ptr<Stripable> s, bool eq)
{
#define PUSH_BACK_NON_NULL(N, C) do {if (C) { _proc_params.push_back (ProcessorCtrl (N, C)); }} while (0)

	_proc_params.clear ();
	if (eq) {
		int cnt = s->eq_band_cnt();

#ifdef MIXBUS32C
		PUSH_BACK_NON_NULL ("Flt In", s->filter_enable_controllable (true)); // both HP/LP
		PUSH_BACK_NON_NULL ("HP Freq", s->filter_freq_controllable (true));
		PUSH_BACK_NON_NULL ("LP Freq", s->filter_freq_controllable (false));
		PUSH_BACK_NON_NULL ("EQ In", s->eq_enable_controllable ());
#elif defined (MIXBUS)
		PUSH_BACK_NON_NULL ("EQ In", s->eq_enable_controllable ());
		PUSH_BACK_NON_NULL ("HP Freq", s->filter_freq_controllable (true));
#endif

		for (int band = 0; band < cnt; ++band) {
			std::string bn = s->eq_band_name (band);
			PUSH_BACK_NON_NULL (string_compose ("Gain %1", bn), s->eq_gain_controllable (band));
			PUSH_BACK_NON_NULL (string_compose ("Freq %1", bn), s->eq_freq_controllable (band));
			PUSH_BACK_NON_NULL (string_compose ("Band %1", bn), s->eq_q_controllable (band));
			PUSH_BACK_NON_NULL (string_compose ("Shape %1", bn), s->eq_shape_controllable (band));
		}
	} else {
		PUSH_BACK_NON_NULL ("Comp In", s->comp_enable_controllable ());
		PUSH_BACK_NON_NULL ("Threshold", s->comp_threshold_controllable ());
		PUSH_BACK_NON_NULL ("Makeup", s->comp_makeup_controllable ());
		PUSH_BACK_NON_NULL ("Speed", s->comp_speed_controllable ());
		PUSH_BACK_NON_NULL ("Mode", s->comp_mode_controllable ());
	}
}

void
FaderPort8::select_plugin (int num)
{
	// make sure drop_ctrl_connections() was called
	assert (_proc_params.size() == 0 && _showing_well_known == 0 && _plugin_insert.expired());

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (first_selected_stripable());
	if (!r) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}

	// Toggle Bypass
	if (shift_mod ()) {
		if (num >= 0) {
			boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (r->nth_plugin (num));
#ifdef MIXBUS
			if (pi && !pi->is_channelstrip () && pi->display_to_user ())
#else
			if (pi && pi->display_to_user ())
#endif
			{
				pi->enable (! pi->enabled ());
			}
		}
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

	// disconnect signals from spill_plugins: processors_changed and ActiveChanged
	processor_connections.drop_connections ();
	r->DropReferences.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FP8Controls::set_fader_mode, &_ctrls, ModeTrack), this);

	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
	assert (pi); // nth_plugin() always returns a PI.
	/* _plugin_insert is used for Bypass/Enable & presets */
#ifdef MIXBUS
	if (!pi->is_channelstrip () && pi->display_to_user ())
#else
	if (pi->display_to_user ())
#endif
	{
		_plugin_insert = boost::weak_ptr<ARDOUR::PluginInsert> (pi);
		pi->ActiveChanged.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_plugin_active_changed, this), this);
		boost::shared_ptr<ARDOUR::Plugin> plugin = pi->plugin ();

		plugin->PresetAdded.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::preset_changed, this), this);
		plugin->PresetRemoved.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::preset_changed, this), this);
		plugin->PresetLoaded.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::preset_changed, this), this);
		plugin->PresetDirty.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::preset_changed, this), this);

		if (_auto_pluginui) {
			pi->ShowUI (); /* EMIT SIGNAL */
		}
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

	/* sort by display priority */
	_proc_params.sort ();

	// TODO: open plugin GUI  if (_proc_params.size() > 0)

	// display
	assign_processor_ctrls ();
	notify_plugin_active_changed ();
}

void
FaderPort8::select_plugin_preset (size_t num)
{
	assert (_proc_params.size() > 0);
	boost::shared_ptr<PluginInsert> pi = _plugin_insert.lock();
	if (!pi) {
		_ctrls.set_fader_mode (ModeTrack);
		return;
	}
	if (num == SIZE_MAX) {
		pi->plugin ()->clear_preset ();
	} else {
		std::vector<ARDOUR::Plugin::PresetRecord> presets = pi->plugin ()->get_presets ();
		if (num < presets.size ()) {
			pi->load_preset (presets.at (num));
		}
	}
	_show_presets = false;
	assign_processor_ctrls ();
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
			continue;
		}
#ifdef MIXBUS
		/* don't show channelstrip plugins, use "well known" */
		if (boost::dynamic_pointer_cast<PluginInsert> (proc)->is_channelstrip ()) {
			continue;
		}
#endif
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
	int spillwidth = N_STRIPS;
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
		_ctrls.strip(id).select_button ().set_color (proc->enabled () ? 0x00ff00ff : 0xff0000ff);
		_ctrls.strip(id).select_button ().set_active (true);
		_ctrls.strip(id).select_button ().set_blinking (false);
		_ctrls.strip(id).set_text_line (0, proc->name());
		_ctrls.strip(id).set_text_line (1, pi->plugin()->maker());
		_ctrls.strip(id).set_text_line (2, PluginManager::plugin_type_name (pi->type()));
		_ctrls.strip(id).set_text_line (3, "");

		pi->ActiveChanged.connect (processor_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::spill_plugins, this), this);

		if (++id == spillwidth) {
			break;
		}
	}
	// clear remaining
	for (; id < spillwidth; ++id) {
		_ctrls.strip(id).unset_controllables ();
	}

	if (have_well_known_comp) {
			assert (id < N_STRIPS);
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
			assert (id < N_STRIPS);
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
	assert (id == N_STRIPS);
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

	_plugin_off = std::min (_plugin_off, n_sends - N_STRIPS);
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

		_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_FADER & ~FP8Strip::CTRL_TEXT01 & ~FP8Strip::CTRL_TEXT3 & ~FP8Strip::CTRL_SELECT);
		_ctrls.strip(id).set_fader_controllable (send);
		_ctrls.strip(id).set_text_line (0, s->send_name (i));
		_ctrls.strip(id).set_mute_controllable (s->send_enable_controllable (i));

		if (++id == N_STRIPS) {
			break;
		}
	}
	// clear remaining
	for (; id < N_STRIPS; ++id) {
		_ctrls.strip(id).unset_controllables (FP8Strip::CTRL_ALL & ~FP8Strip::CTRL_TEXT3 & ~FP8Strip::CTRL_SELECT);
	}
#ifdef MIXBUS // master-assign on last solo
	_ctrls.strip(N_STRIPS - 1).set_solo_controllable (s->master_send_enable_controllable ());
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
FaderPort8::assign_strips ()
{
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
	for (uint8_t id = 0; id < N_STRIPS; ++id) {
		_ctrls.strip(id).set_periodic_display_mode (m);
	}
}

void
FaderPort8::drop_ctrl_connections ()
{
	_proc_params.clear();
	if (_auto_pluginui) {
		boost::shared_ptr<PluginInsert> pi = _plugin_insert.lock ();
		if (pi) {
			pi->HideUI (); /* EMIT SIGNAL */
		}
	}
	_plugin_insert.reset ();
	_show_presets = false;
	processor_connections.drop_connections ();
	_showing_well_known = 0;
	notify_plugin_active_changed ();
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

# if 1 /* selecting a selected strip -> move fader to unity */
	if (s == first_selected_stripable () && !shift_mod ()) {
		if (_ctrls.fader_mode () == ModeTrack) {
			boost::shared_ptr<AutomationControl> ac = s->gain_control ();
			ac->start_touch (ac->session().transport_sample());
			ac->set_value (ac->normal (), PBD::Controllable::UseGroup);
		}
		return;
	}
# endif

	if (shift_mod ()) {
		toggle_stripable_selection (s);
	} else {
		set_stripable_selection (s);
	}
#else
	/* tri-state selection: This allows to set the "first selected"
	 * with a single click without clearing the selection.
	 * Single de/select via shift.
	 */
	if (shift_mod ()) {
		if (s->is_selected ()) {
			remove_stripable_from_selection (s);
		} else {
			set_stripable_selection (s);
		}
		return;
	}
	if (s->is_selected () && s != first_selected_stripable ()) {
		set_first_selected_stripable (s);
		stripable_selection_changed ();
	} else {
		toggle_stripable_selection (s);
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
			stop_link ();
			// force unset rec-arm button, see also FaderPort8::button_arm
			_ctrls.button (FP8Controls::BtnArm).set_active (false);
			ARMButtonChange (false);
			break;
	}
	assign_strips ();
	notify_route_state_changed ();
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
	assign_strips ();
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
		_ctrls.strip(id).set_select_button_color (s->presentation_info ().color());
	}

	if (what_changed.empty ()) {
		_ctrls.strip(id).set_stripable (s, _ctrls.fader_mode() == ModePan);
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

#ifdef FADERPORT2
void
FaderPort8::stripable_selection_changed ()
{
	if (!_device_active || _chan_locked) {
		return;
	}
	route_state_connections.drop_connections ();
	assign_stripables (false);
	subscribe_to_strip_signals ();
}

#else

void
FaderPort8::stripable_selection_changed ()
{
	if (!_device_active) {
		/* this can be called anytime from the static
		 * ControlProtocol::StripableSelectionChanged
		 */
		return;
	}
	route_state_connections.drop_connections();

	switch (_ctrls.fader_mode ()) {
		case ModePlugins:
			if (_proc_params.size () > 0 && _showing_well_known < 0) {
				/* w/well-known -> re-assign to new strip */
				int wk = _showing_well_known;
				drop_ctrl_connections ();
				select_plugin (wk);
			} else if (_proc_params.size() == 0) {
				/* selecting plugin, update available */
				spill_plugins ();
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

	subscribe_to_strip_signals ();
}
#endif

void
FaderPort8::subscribe_to_strip_signals ()
{
	/* keep track of automation-mode of primary selection, shared buttons */
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (s) {
		boost::shared_ptr<AutomationControl> ac;
		ac = s->gain_control();
		if (ac && ac->alist()) {
			ac->alist()->automation_state_changed.connect (route_state_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_route_state_changed, this), this);
		}
		ac = s->pan_azimuth_control();
		if (ac && ac->alist()) {
			ac->alist()->automation_state_changed.connect (route_state_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_route_state_changed, this), this);
		}
#ifdef FADERPORT2
		ac = s->rec_enable_control();
		if (ac) {
			ac->Changed.connect (route_state_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_route_state_changed, this), this);
		}
#endif
	}
	/* set lights */
	notify_route_state_changed ();
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

	int channel_off = get_channel_off (_ctrls.mix_mode ());
	if (channel_off <= off && off < channel_off + N_STRIPS) {
		return;
	}

	if (channel_off > off) {
		channel_off = off;
	} else {
		channel_off = off - (N_STRIPS - 1);
	}
	set_channel_off (_ctrls.mix_mode (), channel_off);
	assign_strips ();
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
				set_stripable_selection (strips.front ());
			} else {
				set_stripable_selection (strips.back ());
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
		set_stripable_selection (toselect);
	}
}

void
FaderPort8::bank (bool down, bool page)
{
#ifdef FADERPORT2
	// XXX this should preferably be in actions.cc
	AccessAction ("Editor", down ? "select-prev-stripable" : "select-next-stripable");
	return;
#endif

	int dt = page ? N_STRIPS : 1;
	if (down) {
		dt *= -1;
	}
	set_channel_off (_ctrls.mix_mode (), get_channel_off (_ctrls.mix_mode ()) + dt);
	assign_strips ();
}

void
FaderPort8::bank_param (bool down, bool page)
{
	int dt = page ? N_STRIPS : 1;
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
