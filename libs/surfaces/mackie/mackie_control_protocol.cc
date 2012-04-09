/*
	Copyright (C) 2006,2007 John Anderson

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

#include <fcntl.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>
#include <iomanip>

#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>
#include <poll.h>

#include <boost/shared_array.hpp>

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/manager.h"
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/memento_command.h"
#include "pbd/convert.h"

#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/location.h"
#include "ardour/midi_ui.h"
#include "ardour/meter.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types.h"
#include "ardour/audioengine.h"

#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "mackie_control_exception.h"
#include "route_signal.h"
#include "mackie_midi_builder.h"
#include "surface_port.h"
#include "surface.h"
#include "bcf_surface.h"
#include "mackie_surface.h"

using namespace ARDOUR;
using namespace std;
using namespace Mackie;
using namespace PBD;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

#define NUCLEUS_DEBUG 1

MackieMidiBuilder builder;

#define midi_ui_context() MidiControlUI::instance() /* a UICallback-derived object that specifies the event loop for signal handling */
#define ui_bind(f, ...) boost::protect (boost::bind (f, __VA_ARGS__))

extern PBD::EventLoop::InvalidationRecord* __invalidator (sigc::trackable& trackable, const char*, int);
#define invalidator(x) __invalidator ((x), __FILE__, __LINE__)

MackieControlProtocol::MackieControlProtocol (Session& session)
	: ControlProtocol (session, X_("Mackie"), MidiControlUI::instance())
	, AbstractUI<MackieControlUIRequest> ("mackie")
	, _current_initial_bank (0)
	, _surface (0)
	, _jog_wheel (*this)
	, _timecode_type (ARDOUR::AnyTime::BBT)
	, _input_bundle (new ARDOUR::Bundle (_("Mackie Control In"), true))
	, _output_bundle (new ARDOUR::Bundle (_("Mackie Control Out"), false))
	, _gui (0)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::MackieControlProtocol\n");

	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
		audio_engine_connections, invalidator (*this), ui_bind (&MackieControlProtocol::port_connected_or_disconnected, this, _2, _4, _5),
		midi_ui_context ()
		);
}

MackieControlProtocol::~MackieControlProtocol()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol\n");

	try {
		close();
	}
	catch (exception & e) {
		cout << "~MackieControlProtocol caught " << e.what() << endl;
	}
	catch (...) {
		cout << "~MackieControlProtocol caught unknown" << endl;
	}

	DEBUG_TRACE (DEBUG::MackieControl, "finished ~MackieControlProtocol::MackieControlProtocol\n");
}

Mackie::Surface& 
MackieControlProtocol::surface()
{
	if (_surface == 0) {
		throw MackieControlException ("_surface is 0 in MackieControlProtocol::surface");
	}
	return *_surface;
}

const Mackie::SurfacePort& 
MackieControlProtocol::mcu_port() const
{
	if (_ports.size() < 1) {
		return _dummy_port;
	} else {
		return dynamic_cast<const MackiePort &> (*_ports[0]);
	}
}

Mackie::SurfacePort& 
MackieControlProtocol::mcu_port()
{
	if (_ports.size() < 1) {
		return _dummy_port;
	} else {
		return dynamic_cast<MackiePort &> (*_ports[0]);
	}
}

// go to the previous track.
// Assume that get_sorted_routes().size() > route_table.size()
void 
MackieControlProtocol::prev_track()
{
	if (_current_initial_bank >= 1) {
		session->set_dirty();
		switch_banks (_current_initial_bank - 1);
	}
}

// go to the next track.
// Assume that get_sorted_routes().size() > route_table.size()
void 
MackieControlProtocol::next_track()
{
	Sorted sorted = get_sorted_routes();
	if (_current_initial_bank + route_table.size() < sorted.size()) {
		session->set_dirty();
		switch_banks (_current_initial_bank + 1);
	}
}

void 
MackieControlProtocol::clear_route_signals()
{
	for (RouteSignals::iterator it = route_signals.begin(); it != route_signals.end(); ++it) {
		delete *it;
	}
	route_signals.clear();
}

// return the port for a given id - 0 based
// throws an exception if no port found
MackiePort& 
MackieControlProtocol::port_for_id (uint32_t index)
{
	uint32_t current_max = 0;

	for (MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it) {
		current_max += (*it)->strips();
		if (index < current_max) { 
			return **it;
		}
	}

	// oops - no matching port
	ostringstream os;
	os << "No port for index " << index;
	cerr << "No port for index " << index << endl;
	throw MackieControlException (os.str());
}

// predicate for sort call in get_sorted_routes
struct RouteByRemoteId
{
	bool operator () (const boost::shared_ptr<Route> & a, const boost::shared_ptr<Route> & b) const
	{
		return a->remote_control_id() < b->remote_control_id();
	}

	bool operator () (const Route & a, const Route & b) const
	{
		return a.remote_control_id() < b.remote_control_id();
	}

	bool operator () (const Route * a, const Route * b) const
	{
		return a->remote_control_id() < b->remote_control_id();
	}
};

MackieControlProtocol::Sorted 
MackieControlProtocol::get_sorted_routes()
{
	Sorted sorted;

	// fetch all routes
	boost::shared_ptr<RouteList> routes = session->get_routes();
	set<uint32_t> remote_ids;

	// routes with remote_id 0 should never be added
	// TODO verify this with ardour devs
	// remote_ids.insert (0);

	// sort in remote_id order, and exclude master, control and hidden routes
	// and any routes that are already set.
	for (RouteList::iterator it = routes->begin(); it != routes->end(); ++it) {
		Route & route = **it;
		if (
			route.active()
			&& !route.is_master()
			&& !route.is_hidden()
			&& !route.is_monitor()
			&& remote_ids.find (route.remote_control_id()) == remote_ids.end()
			) {
			sorted.push_back (*it);
			remote_ids.insert (route.remote_control_id());
		}
	}
	sort (sorted.begin(), sorted.end(), RouteByRemoteId());
	return sorted;
}

void 
MackieControlProtocol::refresh_current_bank()
{
	switch_banks (_current_initial_bank);
}

void 
MackieControlProtocol::switch_banks (int initial)
{
	// DON'T prevent bank switch if initial == _current_initial_bank
	// because then this method can't be used as a refresh

	Sorted sorted = get_sorted_routes();
	int delta = sorted.size() - route_table.size();

	if (initial < 0 || (delta > 0 && initial > delta)) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("not switching to %1\n", initial));
		return;
	}

	_current_initial_bank = initial;
	clear_route_signals();

	// now set the signals for new routes
	if (_current_initial_bank <= sorted.size()) {

		uint32_t end_pos = min (route_table.size(), sorted.size());
		uint32_t i = 0;
		Sorted::iterator it = sorted.begin() + _current_initial_bank;
		Sorted::iterator end = sorted.begin() + _current_initial_bank + end_pos;

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch to %1, %2\n", _current_initial_bank, end_pos));

		route_table.clear ();
		set_route_table_size (surface().strips.size());

		// link routes to strips

		for (; it != end && it != sorted.end(); ++it, ++i) {
			boost::shared_ptr<Route> route = *it;

			assert (surface().strips[i]);
			Strip & strip = *surface().strips[i];

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("remote id %1 connecting %2 to %3 with port %4\n", 
									   route->remote_control_id(), route->name(), strip.name(), port_for_id(i)));
			set_route_table (i, route);
			RouteSignal * rs = new RouteSignal (route, *this, strip, port_for_id(i));
			route_signals.push_back (rs);
			rs->notify_all ();
		}

		// create dead strips if there aren't enough routes to
		// fill a bank
		for (; i < route_table.size(); ++i) {
			Strip & strip = *surface().strips[i];
			// send zero for this strip
			MackiePort & port = port_for_id(i);
			port.write (builder.zero_strip (port, strip));
		}
	}

	// display the current start bank.
	surface().display_bank_start (mcu_port(), builder, _current_initial_bank);
}

void 
MackieControlProtocol::zero_all()
{
	// TODO turn off Timecode displays

	// zero all strips
	for (Surface::Strips::iterator it = surface().strips.begin(); it != surface().strips.end(); ++it) {
		MackiePort & port = port_for_id ((*it)->index());
		port.write (builder.zero_strip (port, **it));
	}

	// and the master strip
	mcu_port().write (builder.zero_strip (dynamic_cast<MackiePort&> (mcu_port()), master_strip()));

	// turn off global buttons and leds
	// global buttons are only ever on mcu_port, so we don't have
	// to figure out which port.
	for (Surface::Controls::iterator it = surface().controls.begin(); it != surface().controls.end(); ++it) {
		Control & control = **it;
		if (!control.group().is_strip() && control.accepts_feedback()) {
			mcu_port().write (builder.zero_control (control));
		}
	}

	// any hardware-specific stuff
	surface().zero_all (mcu_port(), builder);
}

int 
MackieControlProtocol::set_active (bool yn)
{
	if (yn == _active) {
		return 0;
	}

	try
	{
		// the reason for the locking and unlocking is that
		// glibmm can't do a condition wait on a RecMutex
		if (yn) {
			// TODO what happens if this fails half way?

			// start an event loop

			BaseUI::run ();
			
			// create MackiePorts
			{
				Glib::Mutex::Lock lock (update_mutex);
				create_ports();
			}
			
			// now initialise MackiePorts - ie exchange sysex messages
			for (MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it) {
				(*it)->open();
			}
			
			// wait until all ports are active
			// TODO a more sophisticated approach would
			// allow things to start up with only an MCU, even if
			// extenders were specified but not responding.
			for (MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it) {
				(*it)->wait_for_init();
			}
			
			// create surface object. This depends on the ports being
			// correctly initialised
			initialize_surface();
			connect_session_signals();
			
			// yeehah!
			_active = true;
			
			// send current control positions to surface
			// must come after _active = true otherwise it won't run
			update_surface();

			Glib::RefPtr<Glib::TimeoutSource> meter_timeout = Glib::TimeoutSource::create (25);

			meter_connection = meter_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::meter_update));

			meter_timeout->attach (main_loop()->get_context());

		} else {
			BaseUI::quit ();
			close();
			_active = false;
		}
	}
	
	catch (exception & e) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("set_active to false because exception caught: %1\n", e.what()));
		_active = false;
		throw;
	}

	return 0;
}

bool
MackieControlProtocol::meter_update ()
{
	for (std::vector<RouteSignal*>::iterator r = route_signals.begin(); r != route_signals.end(); ++r) {
		float dB;
		
		dB = const_cast<PeakMeter&> ((*r)->route()->peak_meter()).peak_power (0);
		Mackie::Meter& m = (*r)->strip().meter();

		float def = 0.0f; /* Meter deflection %age */
		
		if (dB < -70.0f) {
			def = 0.0f;
		} else if (dB < -60.0f) {
			def = (dB + 70.0f) * 0.25f;
		} else if (dB < -50.0f) {
			def = (dB + 60.0f) * 0.5f + 2.5f;
		} else if (dB < -40.0f) {
			def = (dB + 50.0f) * 0.75f + 7.5f;
		} else if (dB < -30.0f) {
			def = (dB + 40.0f) * 1.5f + 15.0f;
		} else if (dB < -20.0f) {
			def = (dB + 30.0f) * 2.0f + 30.0f;
		} else if (dB < 6.0f) {
			def = (dB + 20.0f) * 2.5f + 50.0f;
		} else {
			def = 115.0f;
		}
		
		/* 115 is the deflection %age that would be
		   when dB=6.0. this is an arbitrary
		   endpoint for our scaling.
		*/
		
		(*r)->port().write (builder.build_meter (m, def/115.0));
	}

	return true; // call it again
}

bool 
MackieControlProtocol::handle_strip_button (SurfacePort & port, Control & control, ButtonState bs, boost::shared_ptr<Route> route)
{
	bool state = false;

	if (bs == press) {
		if (control.name() == "recenable") {
			state = !route->record_enabled();
			route->set_record_enabled (state, this);
		} else if (control.name() == "mute") {
			state = !route->muted();
			route->set_mute (state, this);
		} else if (control.name() == "solo") {
			state = !route->soloed();
			route->set_solo (state, this);
		} else if (control.name() == "select") {
			// TODO make the track selected. Whatever that means.
			//state = default_button_press (dynamic_cast<Button&> (control));
		} else if (control.name() == "vselect") {
			// TODO could be used to select different things to apply the pot to?
			//state = default_button_press (dynamic_cast<Button&> (control));
		}
	}

	if (control.name() == "fader_touch") {
		state = bs == press;
		control.strip().gain().set_in_use (state);

		if (ARDOUR::Config->get_mackie_emulation() == "bcf" && state) {
			/* BCF faders don't support touch, so add a timeout to reset
			   their `in_use' state.
			*/
			add_in_use_timeout (port, control.strip().gain(), &control.strip().fader_touch());
		}
	}

	return state;
}

void 
MackieControlProtocol::update_led (Mackie::Button & button, Mackie::LedState ls)
{
	if (ls != none) {
		SurfacePort * port = 0;
		if (button.group().is_strip()) {
			if (button.group().is_master()) {
				port = &mcu_port();
			} else {
				port = &port_for_id (dynamic_cast<const Strip&> (button.group()).index());
			}
		} else {
			port = &mcu_port();
		}
		port->write (builder.build_led (button, ls));
	}
}

void 
MackieControlProtocol::update_timecode_beats_led()
{
	switch (_timecode_type) {
		case ARDOUR::AnyTime::BBT:
			update_global_led ("beats", on);
			update_global_led ("timecode", off);
			break;
		case ARDOUR::AnyTime::Timecode:
			update_global_led ("timecode", on);
			update_global_led ("beats", off);
			break;
		default:
			ostringstream os;
			os << "Unknown Anytime::Type " << _timecode_type;
			throw runtime_error (os.str());
	}
}

void 
MackieControlProtocol::update_global_button (const string & name, LedState ls)
{
	if (surface().controls_by_name.find (name) != surface().controls_by_name.end()) {
		Button * button = dynamic_cast<Button*> (surface().controls_by_name[name]);
		mcu_port().write (builder.build_led (button->led(), ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Button %1 not found\n", name));
	}
}

void 
MackieControlProtocol::update_global_led (const string & name, LedState ls)
{
	if (surface().controls_by_name.find (name) != surface().controls_by_name.end()) {
		Led * led = dynamic_cast<Led*> (surface().controls_by_name[name]);
		mcu_port().write (builder.build_led (*led, ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Led %1 not found\n", name));
	}
}

// send messages to surface to set controls to correct values
void 
MackieControlProtocol::update_surface()
{
	if (!_active) {
		return;
	}

	// do the initial bank switch to connect signals
	// _current_initial_bank is initialised by set_state
	switch_banks (_current_initial_bank);
	
	/* Create a RouteSignal for the master route, if we don't already have one */
	if (!master_route_signal) {
		boost::shared_ptr<Route> mr = master_route ();
		if (mr) {
			master_route_signal = boost::shared_ptr<RouteSignal> (new RouteSignal (mr, *this, master_strip(), mcu_port()));
			// update strip from route
			master_route_signal->notify_all();
		}
	}
	
	// sometimes the jog wheel is a pot
	surface().blank_jog_ring (mcu_port(), builder);
	
	// update global buttons and displays
	notify_record_state_changed();
	notify_transport_state_changed();
	update_timecode_beats_led();
}

void 
MackieControlProtocol::connect_session_signals()
{
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_route_added, this, _1), midi_ui_context());
	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_record_state_changed, this), midi_ui_context());
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_transport_state_changed, this), midi_ui_context());
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_parameter_changed, this, _1), midi_ui_context());
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_parameter_changed, this, _1), midi_ui_context());
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_solo_active_changed, this, _1), midi_ui_context());

	// make sure remote id changed signals reach here
	// see also notify_route_added
	Sorted sorted = get_sorted_routes();

	for (Sorted::iterator it = sorted.begin(); it != sorted.end(); ++it) {
		(*it)->RemoteControlIDChanged.connect (route_connections, MISSING_INVALIDATOR, ui_bind(&MackieControlProtocol::notify_remote_id_changed, this), midi_ui_context());
	}
}

void 
MackieControlProtocol::add_port (MIDI::Port & midi_input_port, MIDI::Port & midi_output_port, int number, MackiePort::port_type_t port_type)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("add port %1 %2\n", midi_input_port.name(), midi_output_port.name()));

	MackiePort * sport = new MackiePort (*this, midi_input_port, midi_output_port, number, port_type);
	_ports.push_back (sport);
	
	sport->init_event.connect_same_thread (port_connections, boost::bind (&MackieControlProtocol::handle_port_init, this, sport));
	sport->active_event.connect_same_thread (port_connections, boost::bind (&MackieControlProtocol::handle_port_active, this, sport));
	sport->inactive_event.connect_same_thread (port_connections, boost::bind (&MackieControlProtocol::handle_port_inactive, this, sport));

	_input_bundle->add_channel (
		midi_input_port.name(),
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (midi_input_port.name())
		);
	
	_output_bundle->add_channel (
		midi_output_port.name(),
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (midi_output_port.name())
		);
}

void 
MackieControlProtocol::create_ports()
{
	MIDI::Manager * mm = MIDI::Manager::instance();
	MIDI::Port * midi_input_port = mm->add_port (
		new MIDI::Port (string_compose (_("%1 in"), default_port_name), MIDI::Port::IsInput, session->engine().jack())
		);
	MIDI::Port * midi_output_port = mm->add_port (
		new MIDI::Port (string_compose (_("%1 out"), default_port_name), MIDI::Port::IsOutput, session->engine().jack())
		);

	/* Create main port */

	if (!midi_input_port->ok() || !midi_output_port->ok()) {
		ostringstream os;
		os << _("Mackie control MIDI ports could not be created; Mackie control disabled");
		error << os.str() << endmsg;
		throw MackieControlException (os.str());
	}

	add_port (*midi_input_port, *midi_output_port, 0, MackiePort::mcu);

	/* Create extender ports */

	for (uint32_t index = 1; index <= Config->get_mackie_extenders(); ++index) {
		MIDI::Port * midi_input_port = mm->add_port (
			new MIDI::Port (string_compose (_("mcu_xt_%1 in"), index), MIDI::Port::IsInput, session->engine().jack())
			);
		MIDI::Port * midi_output_port = mm->add_port (
			new MIDI::Port (string_compose (_("mcu_xt_%1 out"), index), MIDI::Port::IsOutput, session->engine().jack())
			);
		if (midi_input_port->ok() && midi_output_port->ok()) {
			add_port (*midi_input_port, *midi_output_port, index, MackiePort::ext);
		}
	}
}

boost::shared_ptr<Route> 
MackieControlProtocol::master_route()
{
	return session->master_out ();
}

Strip& 
MackieControlProtocol::master_strip()
{
	return dynamic_cast<Strip&> (*surface().groups["master"]);
}

void 
MackieControlProtocol::initialize_surface()
{
	// set up the route table
	int strips = 0;
	for (MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it) {
		strips += (*it)->strips();
	}

	set_route_table_size (strips);

	// TODO same as code in mackie_port.cc
	string emulation = ARDOUR::Config->get_mackie_emulation();
	if (emulation == "bcf") {
		_surface = new BcfSurface (strips);
	} else if (emulation == "mcu") {
		_surface = new MackieSurface (strips);
	} else {
		ostringstream os;
		os << "no Surface class found for emulation: " << emulation;
		throw MackieControlException (os.str());
	}

	_surface->init();
}

void 
MackieControlProtocol::close()
{

	// must be before other shutdown otherwise polling loop
	// calls methods on objects that are deleted

	port_connections.drop_connections ();
	session_connections.drop_connections ();
	route_connections.drop_connections ();

	if (_surface != 0) {
		// These will fail if the port has gone away.
		// So catch the exception and do the rest of the
		// close afterwards
		// because the bcf doesn't respond to the next 3 sysex messages
		try {
			zero_all();
		}

		catch (exception & e) {
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::close caught exception: %1\n", e.what()));
		}

		for (MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it) {
			try {
				MackiePort & port = **it;
				// faders to minimum
				port.write_sysex (0x61);
				// All LEDs off
				port.write_sysex (0x62);
				// Reset (reboot into offline mode)
				port.write_sysex (0x63);
			}
			catch (exception & e) {
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::close caught exception: %1\n", e.what()));
			}
		}

		// disconnect routes from strips
		clear_route_signals();
		delete _surface;
		_surface = 0;
	}

	// shut down MackiePorts
	for (MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it) {
		delete *it;
	}

	_ports.clear();
}

XMLNode& 
MackieControlProtocol::get_state()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::get_state\n");

	// add name of protocol
	XMLNode* node = new XMLNode (X_("Protocol"));
	node->add_property (X_("name"), ARDOUR::ControlProtocol::_name);

	// add current bank
	ostringstream os;
	os << _current_initial_bank;
	node->add_property (X_("bank"), os.str());

	return *node;
}

int 
MackieControlProtocol::set_state (const XMLNode & node, int /*version*/)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::set_state: active %1\n", _active));

	int retval = 0;

	// fetch current bank

	if (node.property (X_("bank")) != 0) {
		string bank = node.property (X_("bank"))->value();
		try {
			set_active (true);
			uint32_t new_bank = atoi (bank.c_str());
			if (_current_initial_bank != new_bank) {
				switch_banks (new_bank);
			}
		}
		catch (exception & e) {
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("exception in MackieControlProtocol::set_state: %1\n", e.what()));
			return -1;
		}
	}

	return retval;
}

void 
MackieControlProtocol::handle_control_event (SurfacePort & port, Control & control, const ControlState & state)
{
	// find the route for the control, if there is one
	boost::shared_ptr<Route> route;

	if (control.group().is_strip()) {
		if (control.group().is_master()) {
			DEBUG_TRACE (DEBUG::MackieControl, "master strip control event\n");
			route = master_route();
		} else {
			uint32_t index = control.ordinal() - 1 + (port.number() * port.strips());
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip control event, index = %1, rt size = %2\n",
									   index, route_table.size()));
			if (index < route_table.size()) {
				route = route_table[index];
				if (route) {
					DEBUG_TRACE (DEBUG::MackieControl, string_compose ("modifying %1\n", route->name()));
				} else {
					DEBUG_TRACE (DEBUG::MackieControl, "no route found!\n");
				}
			} else {
				cerr << "Warning: index is " << index << " which is not in the route table, size: " << route_table.size() << endl;
				DEBUG_TRACE (DEBUG::MackieControl, "illegal route index found!\n");
			}
		}
	}

	// This handles control element events from the surface
	// the state of the controls on the surface is usually updated
	// from UI events.
	switch (control.type()) {
		case Control::type_fader:
			// find the route in the route table for the id
			// if the route isn't available, skip it
			// at which point the fader should just reset itself
			if (route != 0)
			{
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("fader to %1\n", state.pos));

				route->gain_control()->set_value (slider_position_to_gain (state.pos));

				if (ARDOUR::Config->get_mackie_emulation() == "bcf") {
					/* reset the timeout while we're still moving the fader */
					add_in_use_timeout (port, control, control.in_use_touch_control);
				}

				// must echo bytes back to slider now, because
				// the notifier only works if the fader is not being
				// touched. Which it is if we're getting input.
				port.write (builder.build_fader ((Fader&)control, state.pos));
			}
			break;

		case Control::type_button:
			if (control.group().is_strip()) {
				// strips
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip button %1\n", control.id()));
				if (route != 0) {
					handle_strip_button (port, control, state.button_state, route);
				} else {
					// no route so always switch the light off
					// because no signals will be emitted by a non-route
					port.write (builder.build_led (control.led(), off));
				}
			} else if (control.group().is_master()) {
				// master fader touch
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("master strip button %1\n", control.id()));
				if (route != 0) {
					handle_strip_button (port, control, state.button_state, route);
				}
			} else {
				// handle all non-strip buttons
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("global button %1\n", control.id()));
				surface().handle_button (*this, state.button_state, dynamic_cast<Button&> (control));
			}
			break;

		// pot (jog wheel, external control)
		case Control::type_pot:
			if (control.group().is_strip()) {
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip pot %1\n", control.id()));
				if (route) {
                                        boost::shared_ptr<Panner> panner = route->panner_shell()->panner();
					// pan for mono input routes, or stereo linked panners
                                        if (panner) {
						double p = panner->position ();
                                                
						// calculate new value, and adjust
						p += state.delta * state.sign;
						p = min (1.0, p);
						p = max (0.0, p);
						panner->set_position (p);
					}
				} else {
					// it's a pot for an umnapped route, so turn all the lights off
					port.write (builder.build_led_ring (dynamic_cast<Pot &> (control), off));
				}
			} else {
				if (control.is_jog()) {
					DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Jog wheel moved %1\n", state.ticks));
					_jog_wheel.jog_event (port, control, state);
				} else {
					DEBUG_TRACE (DEBUG::MackieControl, string_compose ("External controller moved %1\n", state.ticks));
					cout << "external controller" << state.ticks * state.sign << endl;
				}
			}
			break;

		default:
			cout << "Control::type not handled: " << control.type() << endl;
	}
}

/////////////////////////////////////////////////
// handlers for Route signals
// TODO should these be part of RouteSignal?
// They started off as signal/slot handlers for signals
// from Route, but they're also used in polling for automation
/////////////////////////////////////////////////

void 
MackieControlProtocol::notify_solo_changed (RouteSignal * route_signal)
{
	try {
		Button & button = route_signal->strip().solo();
		route_signal->port().write (builder.build_led (button, route_signal->route()->soloed()));
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

void 
MackieControlProtocol::notify_mute_changed (RouteSignal * route_signal)
{
	try {
		Button & button = route_signal->strip().mute();
		route_signal->port().write (builder.build_led (button, route_signal->route()->muted()));
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

void 
MackieControlProtocol::notify_record_enable_changed (RouteSignal * route_signal)
{
	try {
		Button & button = route_signal->strip().recenable();
		route_signal->port().write (builder.build_led (button, route_signal->route()->record_enabled()));
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_active_changed (RouteSignal *)
{
	try {
		DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::notify_active_changed\n");
		refresh_current_bank();
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

void 
MackieControlProtocol::notify_gain_changed (RouteSignal * route_signal, bool force_update)
{
	try {
		Fader & fader = route_signal->strip().gain();
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("route %1 gain change, update fader %2 on port %3\n", 
								   route_signal->route()->name(), 
								   fader.raw_id(),
								   route_signal->port().output_port().name()));
		if (!fader.in_use()) {
			float gain_value = gain_to_slider_position (route_signal->route()->gain_control()->get_value());
			// check that something has actually changed
			if (force_update || gain_value != route_signal->last_gain_written()) {
				route_signal->port().write (builder.build_fader (fader, gain_value));
				route_signal->last_gain_written (gain_value);
			}
		}
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

void 
MackieControlProtocol::notify_property_changed (const PropertyChange& what_changed, RouteSignal * route_signal)
{
	if (!what_changed.contains (Properties::name)) {
		return;
	}

	try {
		Strip & strip = route_signal->strip();
		
		if (!strip.is_master()) {
			string line1;
			string fullname = route_signal->route()->name();

			if (fullname.length() <= 6) {
				line1 = fullname;
			} else {
				line1 = PBD::short_version (fullname, 6);
			}

#ifdef NUCLEUS_DEBUG
			cerr << "show strip name from " << fullname << " as " << line1 << endl;
#endif

			SurfacePort & port = route_signal->port();
			port.write (builder.strip_display (port, strip, 0, line1));
			port.write (builder.strip_display_blank (port, strip, 1));
		}
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

void 
MackieControlProtocol::notify_panner_changed (RouteSignal * route_signal, bool force_update)
{
	try {
		Pot & pot = route_signal->strip().vpot();
		boost::shared_ptr<Panner> panner = route_signal->route()->panner();
		if (panner) {
			double pos = panner->position ();

			// cache the MidiByteArray here, because the mackie led control is much lower
			// resolution than the panner control. So we save lots of byte
			// sends in spite of more work on the comparison
			MidiByteArray bytes = builder.build_led_ring (pot, ControlState (on, pos), MackieMidiBuilder::midi_pot_mode_dot);
			// check that something has actually changed
			if (force_update || bytes != route_signal->last_pan_written())
			{
				route_signal->port().write (bytes);
				route_signal->last_pan_written (bytes);
			}
		} else {
			route_signal->port().write (builder.zero_control (pot));
		}
	}
	catch (exception & e) {
		cout << e.what() << endl;
	}
}

// TODO handle plugin automation polling
void 
MackieControlProtocol::update_automation (RouteSignal & rs)
{
	ARDOUR::AutoState gain_state = rs.route()->gain_control()->automation_state();

	if (gain_state == Touch || gain_state == Play) {
		notify_gain_changed (&rs, false);
	}

	if (rs.route()->panner()) {
		ARDOUR::AutoState panner_state = rs.route()->panner()->automation_state();
		if (panner_state == Touch || panner_state == Play) {
			notify_panner_changed (&rs, false);
		}
	}
}

string 
MackieControlProtocol::format_bbt_timecode (framepos_t now_frame)
{
	Timecode::BBT_Time bbt_time;
	session->bbt_time (now_frame, bbt_time);

	// According to the Logic docs
	// digits: 888/88/88/888
	// BBT mode: Bars/Beats/Subdivisions/Ticks
	ostringstream os;
	os << setw(3) << setfill('0') << bbt_time.bars;
	os << setw(2) << setfill('0') << bbt_time.beats;

	// figure out subdivisions per beat
	const ARDOUR::Meter & meter = session->tempo_map().meter_at (now_frame);
	int subdiv = 2;
	if (meter.note_divisor() == 8 && (meter.divisions_per_bar() == 12.0 || meter.divisions_per_bar() == 9.0 || meter.divisions_per_bar() == 6.0)) {
		subdiv = 3;
	}

	uint32_t subdivisions = bbt_time.ticks / uint32_t (Timecode::BBT_Time::ticks_per_beat / subdiv);
	uint32_t ticks = bbt_time.ticks % uint32_t (Timecode::BBT_Time::ticks_per_beat / subdiv);

	os << setw(2) << setfill('0') << subdivisions + 1;
	os << setw(3) << setfill('0') << ticks;

	return os.str();
}

string 
MackieControlProtocol::format_timecode_timecode (framepos_t now_frame)
{
	Timecode::Time timecode;
	session->timecode_time (now_frame, timecode);

	// According to the Logic docs
	// digits: 888/88/88/888
	// Timecode mode: Hours/Minutes/Seconds/Frames
	ostringstream os;
	os << setw(3) << setfill('0') << timecode.hours;
	os << setw(2) << setfill('0') << timecode.minutes;
	os << setw(2) << setfill('0') << timecode.seconds;
	os << setw(3) << setfill('0') << timecode.frames;

	return os.str();
}

void 
MackieControlProtocol::update_timecode_display()
{
	if (surface().has_timecode_display()) {
		// do assignment here so current_frame is fixed
		framepos_t current_frame = session->transport_frame();
		string timecode;

		switch (_timecode_type) {
			case ARDOUR::AnyTime::BBT:
				timecode = format_bbt_timecode (current_frame);
				break;
			case ARDOUR::AnyTime::Timecode:
				timecode = format_timecode_timecode (current_frame);
				break;
			default:
				ostringstream os;
				os << "Unknown timecode: " << _timecode_type;
				throw runtime_error (os.str());
		}

		// only write the timecode string to the MCU if it's changed
		// since last time. This is to reduce midi bandwidth used.
		if (timecode != _timecode_last) {
			surface().display_timecode (mcu_port(), builder, timecode, _timecode_last);
			_timecode_last = timecode;
		}
	}
}

void 
MackieControlProtocol::poll_session_data()
{
	// XXX need to attach this to a timer in the MIDI UI event loop (20msec)

	if (_active) {
		// do all currently mapped routes
		for (RouteSignals::iterator it = route_signals.begin(); it != route_signals.end(); ++it) {
			update_automation (**it);
		}

		// and the master strip
		if (master_route_signal != 0) {
			update_automation (*master_route_signal);
		}

		update_timecode_display();
	}
}

/////////////////////////////////////
// Transport Buttons
/////////////////////////////////////

LedState 
MackieControlProtocol::frm_left_press (Button &)
{
	// can use first_mark_before/after as well
	unsigned long elapsed = _frm_left_last.restart();

	Location * loc = session->locations()->first_location_before (
		session->transport_frame()
	);

	// allow a quick double to go past a previous mark
	if (session->transport_rolling() && elapsed < 500 && loc != 0) {
		Location * loc_two_back = session->locations()->first_location_before (loc->start());
		if (loc_two_back != 0)
		{
			loc = loc_two_back;
		}
	}

	// move to the location, if it's valid
	if (loc != 0) {
		session->request_locate (loc->start(), session->transport_rolling());
	}

	return on;
}

LedState 
MackieControlProtocol::frm_left_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::frm_right_press (Button &)
{
	// can use first_mark_before/after as well
	Location * loc = session->locations()->first_location_after (session->transport_frame());
	
	if (loc != 0) {
		session->request_locate (loc->start(), session->transport_rolling());
	}
		
	return on;
}

LedState 
MackieControlProtocol::frm_right_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::stop_press (Button &)
{
	session->request_stop();
	return on;
}

LedState 
MackieControlProtocol::stop_release (Button &)
{
	return session->transport_stopped();
}

LedState 
MackieControlProtocol::play_press (Button &)
{
	session->request_transport_speed (1.0);
	return on;
}

LedState 
MackieControlProtocol::play_release (Button &)
{
	return session->transport_rolling();
}

LedState 
MackieControlProtocol::record_press (Button &)
{
	if (session->get_record_enabled()) {
		session->disable_record (false);
	} else {
		session->maybe_enable_record();
	}
	return on;
}

LedState 
MackieControlProtocol::record_release (Button &)
{
	if (session->get_record_enabled()) {
		if (session->transport_rolling()) {
			return on;
		} else {
			return flashing;
		}
	} else {
		return off;
	}
}

LedState 
MackieControlProtocol::rewind_press (Button &)
{
	_jog_wheel.push (JogWheel::speed);
	_jog_wheel.transport_direction (-1);
	session->request_transport_speed (-_jog_wheel.transport_speed());
	return on;
}

LedState 
MackieControlProtocol::rewind_release (Button &)
{
	_jog_wheel.pop();
	_jog_wheel.transport_direction (0);
	if (_transport_previously_rolling) {
		session->request_transport_speed (1.0);
	} else {
		session->request_stop();
	}
	return off;
}

LedState 
MackieControlProtocol::ffwd_press (Button &)
{
	_jog_wheel.push (JogWheel::speed);
	_jog_wheel.transport_direction (1);
	session->request_transport_speed (_jog_wheel.transport_speed());
	return on;
}

LedState 
MackieControlProtocol::ffwd_release (Button &)
{
	_jog_wheel.pop();
	_jog_wheel.transport_direction (0);
	if (_transport_previously_rolling) {
		session->request_transport_speed (1.0);
	} else {
		session->request_stop();
	}
	return off;
}

LedState 
MackieControlProtocol::loop_press (Button &)
{
	session->request_play_loop (!session->get_play_loop());
	return on;
}

LedState 
MackieControlProtocol::loop_release (Button &)
{
	return session->get_play_loop();
}

LedState 
MackieControlProtocol::punch_in_press (Button &)
{
	bool const state = !session->config.get_punch_in();
	session->config.set_punch_in (state);
	return state;
}

LedState 
MackieControlProtocol::punch_in_release (Button &)
{
	return session->config.get_punch_in();
}

LedState 
MackieControlProtocol::punch_out_press (Button &)
{
	bool const state = !session->config.get_punch_out();
	session->config.set_punch_out (state);
	return state;
}

LedState 
MackieControlProtocol::punch_out_release (Button &)
{
	return session->config.get_punch_out();
}

LedState 
MackieControlProtocol::home_press (Button &)
{
	session->goto_start();
	return on;
}

LedState 
MackieControlProtocol::home_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::end_press (Button &)
{
	session->goto_end();
	return on;
}

LedState 
MackieControlProtocol::end_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::clicking_press (Button &)
{
	bool state = !Config->get_clicking();
	Config->set_clicking (state);
	return state;
}

LedState 
MackieControlProtocol::clicking_release (Button &)
{
	return Config->get_clicking();
}

LedState MackieControlProtocol::global_solo_press (Button &)
{
	bool state = !session->soloing();
	session->set_solo (session->get_routes(), state);
	return state;
}

LedState MackieControlProtocol::global_solo_release (Button &)
{
	return session->soloing();
}

///////////////////////////////////////////
// Session signals
///////////////////////////////////////////

void MackieControlProtocol::notify_parameter_changed (std::string const & p)
{
	if (p == "punch-in") {
		update_global_button ("punch_in", session->config.get_punch_in());
	} else if (p == "punch-out") {
		update_global_button ("punch_out", session->config.get_punch_out());
	} else if (p == "clicking") {
		update_global_button ("clicking", Config->get_clicking());
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("parameter changed: %1\n", p));
	}
}

// RouteList is the set of routes that have just been added
void 
MackieControlProtocol::notify_route_added (ARDOUR::RouteList & rl)
{
	// currently assigned banks are less than the full set of
	// strips, so activate the new strip now.
	if (route_signals.size() < route_table.size()) {
		refresh_current_bank();
	}
	// otherwise route added, but current bank needs no updating

	// make sure remote id changes in the new route are handled
	typedef ARDOUR::RouteList ARS;

	for (ARS::iterator it = rl.begin(); it != rl.end(); ++it) {
		(*it)->RemoteControlIDChanged.connect (route_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_remote_id_changed, this), midi_ui_context());
	}
}

void 
MackieControlProtocol::notify_solo_active_changed (bool active)
{
	Button * rude_solo = reinterpret_cast<Button*> (surface().controls_by_name["solo"]);
	mcu_port().write (builder.build_led (*rude_solo, active ? flashing : off));
}

void 
MackieControlProtocol::notify_remote_id_changed()
{
	Sorted sorted = get_sorted_routes();

	// if a remote id has been moved off the end, we need to shift
	// the current bank backwards.
	if (sorted.size() - _current_initial_bank < route_signals.size()) {
		// but don't shift backwards past the zeroth channel
		switch_banks (max((Sorted::size_type) 0, sorted.size() - route_signals.size()));
	} else {
		// Otherwise just refresh the current bank
		refresh_current_bank();
	}
}

///////////////////////////////////////////
// Transport signals
///////////////////////////////////////////

void 
MackieControlProtocol::notify_record_state_changed()
{
	// switch rec button on / off / flashing
	Button * rec = reinterpret_cast<Button*> (surface().controls_by_name["record"]);
	mcu_port().write (builder.build_led (*rec, record_release (*rec)));
}

void 
MackieControlProtocol::notify_transport_state_changed()
{
	// switch various play and stop buttons on / off
	update_global_button ("play", session->transport_rolling());
	update_global_button ("stop", !session->transport_rolling());
	update_global_button ("loop", session->get_play_loop());

	_transport_previously_rolling = session->transport_rolling();

	// rec is special because it's tristate
	Button * rec = reinterpret_cast<Button*> (surface().controls_by_name["record"]);
	mcu_port().write (builder.build_led (*rec, record_release (*rec)));
}

/////////////////////////////////////
// Bank Switching
/////////////////////////////////////
LedState 
MackieControlProtocol::left_press (Button &)
{
	Sorted sorted = get_sorted_routes();
	if (sorted.size() > route_table.size()) {
		int new_initial = _current_initial_bank - route_table.size();
		if (new_initial < 0) {
			new_initial = 0;
		}
		
		if (new_initial != int (_current_initial_bank)) {
			session->set_dirty();
			switch_banks (new_initial);
		}

		return on;
	} else {
		return flashing;
	}
}

LedState 
MackieControlProtocol::left_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::right_press (Button &)
{
	Sorted sorted = get_sorted_routes();
	if (sorted.size() > route_table.size()) {
		uint32_t delta = sorted.size() - (route_table.size() + _current_initial_bank);

		if (delta > route_table.size()) {
			delta = route_table.size();
		}
		
		if (delta > 0) {
			session->set_dirty();
			switch_banks (_current_initial_bank + delta);
		}

		return on;
	} else {
		return flashing;
	}
}

LedState 
MackieControlProtocol::right_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::channel_left_press (Button &)
{
	Sorted sorted = get_sorted_routes();
	if (sorted.size() > route_table.size()) {
		prev_track();
		return on;
	} else {
		return flashing;
	}
}

LedState 
MackieControlProtocol::channel_left_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::channel_right_press (Button &)
{
	Sorted sorted = get_sorted_routes();
	if (sorted.size() > route_table.size()) {
		next_track();
		return on;
	} else {
		return flashing;
	}
}

LedState 
MackieControlProtocol::channel_right_release (Button &)
{
	return off;
}

/////////////////////////////////////
// Functions
/////////////////////////////////////
LedState 
MackieControlProtocol::marker_press (Button &)
{
	// cut'n'paste from LocationUI::add_new_location()
	string markername;
	framepos_t where = session->audible_frame();
	session->locations()->next_available_name(markername,"mcu");
	Location *location = new Location (*session, where, where, markername, Location::IsMark);
	session->begin_reversible_command (_("add marker"));
	XMLNode &before = session->locations()->get_state();
	session->locations()->add (location, true);
	XMLNode &after = session->locations()->get_state();
	session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
	return on;
}

LedState 
MackieControlProtocol::marker_release (Button &)
{
	return off;
}

void 
jog_wheel_state_display (JogWheel::State state, SurfacePort & port)
{
	switch (state) {
		case JogWheel::zoom:
			port.write (builder.two_char_display ("Zm"));
			break;
		case JogWheel::scroll:
			port.write (builder.two_char_display ("Sc"));
			break;
		case JogWheel::scrub:
			port.write (builder.two_char_display ("Sb"));
			break;
		case JogWheel::shuttle:
			port.write (builder.two_char_display ("Sh"));
			break;
		case JogWheel::speed:
			port.write (builder.two_char_display ("Sp"));
			break;
		case JogWheel::select:
			port.write (builder.two_char_display ("Se"));
			break;
	}
}

Mackie::LedState 
MackieControlProtocol::zoom_press (Mackie::Button &)
{
	_jog_wheel.zoom_state_toggle();
	update_global_button ("scrub", _jog_wheel.jog_wheel_state() == JogWheel::scrub);
	jog_wheel_state_display (_jog_wheel.jog_wheel_state(), mcu_port());
	return _jog_wheel.jog_wheel_state() == JogWheel::zoom;
}

Mackie::LedState 
MackieControlProtocol::zoom_release (Mackie::Button &)
{
	return _jog_wheel.jog_wheel_state() == JogWheel::zoom;
}

Mackie::LedState 
MackieControlProtocol::scrub_press (Mackie::Button &)
{
	_jog_wheel.scrub_state_cycle();
	update_global_button ("zoom", _jog_wheel.jog_wheel_state() == JogWheel::zoom);
	jog_wheel_state_display (_jog_wheel.jog_wheel_state(), mcu_port());
	return (
		_jog_wheel.jog_wheel_state() == JogWheel::scrub
		||
		_jog_wheel.jog_wheel_state() == JogWheel::shuttle
		);
}

Mackie::LedState 
MackieControlProtocol::scrub_release (Mackie::Button &)
{
	return (
		_jog_wheel.jog_wheel_state() == JogWheel::scrub
		||
		_jog_wheel.jog_wheel_state() == JogWheel::shuttle
		);
}

LedState 
MackieControlProtocol::drop_press (Button &)
{
	session->remove_last_capture();
	return on;
}

LedState 
MackieControlProtocol::drop_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::save_press (Button &)
{
	session->save_state ("");
	return on;
}

LedState 
MackieControlProtocol::save_release (Button &)
{
	return off;
}

LedState 
MackieControlProtocol::timecode_beats_press (Button &)
{
	switch (_timecode_type) {
	case ARDOUR::AnyTime::BBT:
		_timecode_type = ARDOUR::AnyTime::Timecode;
		break;
	case ARDOUR::AnyTime::Timecode:
		_timecode_type = ARDOUR::AnyTime::BBT;
		break;
	default:
		ostringstream os;
		os << "Unknown Anytime::Type " << _timecode_type;
		throw runtime_error (os.str());
	}
	update_timecode_beats_led();
	return on;
}

LedState 
MackieControlProtocol::timecode_beats_release (Button &)
{
	return off;
}

list<boost::shared_ptr<ARDOUR::Bundle> >
MackieControlProtocol::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;
	b.push_back (_input_bundle);
	b.push_back (_output_bundle);
	return b;
}

void
MackieControlProtocol::port_connected_or_disconnected (string a, string b, bool connected)
{
	/* If something is connected to one of our output ports, send MIDI to update the surface
	   to whatever state it should have.
	*/

	if (!connected) {
		return;
	}

	MackiePorts::const_iterator i = _ports.begin();
	while (i != _ports.end()) {

		string const n = AudioEngine::instance()->make_port_name_non_relative ((*i)->output_port().name ());

		if (a == n || b == n) {
			break;
		}

		++i;
	}

	if (i != _ports.end ()) {
		update_surface ();
	}
}

void
MackieControlProtocol::do_request (MackieControlUIRequest* req)
{
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
MackieControlProtocol::stop ()
{
	BaseUI::quit ();

	return 0;
}

/** Add a timeout so that a control's in_use flag will be reset some time in the future.
 *  @param in_use_control the control whose in_use flag to reset.
 *  @param touch_control a touch control to emit an event for, or 0.
 */
void
MackieControlProtocol::add_in_use_timeout (SurfacePort& port, Control& in_use_control, Control* touch_control)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout (Glib::TimeoutSource::create (250)); // milliseconds

	in_use_control.in_use_connection.disconnect ();
	in_use_control.in_use_connection = timeout->connect (
		sigc::bind (sigc::mem_fun (*this, &MackieControlProtocol::control_in_use_timeout), &port, &in_use_control, touch_control));
	in_use_control.in_use_touch_control = touch_control;

	timeout->attach (main_loop()->get_context());

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("timeout queued for port %1, control %2 touch control %3\n",
							   &port, &in_use_control, touch_control));}

/** Handle timeouts to reset in_use for controls that can't
 *  do this by themselves (e.g. pots, and faders without touch support).
 *  @param in_use_control the control whose in_use flag to reset.
 *  @param touch_control a touch control to emit an event for, or 0.
 */
bool
MackieControlProtocol::control_in_use_timeout (SurfacePort* port, Control* in_use_control, Control* touch_control)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("timeout elapsed for port %1, control %2 touch control %3\n",
							   port, in_use_control, touch_control));

	in_use_control->set_in_use (false);

	if (touch_control) {
		// empty control_state
		ControlState control_state;
		handle_control_event (*port, *touch_control, control_state);
	}
	
	// only call this method once from the timer
	return false;
}

