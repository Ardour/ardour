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
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/memento_command.h"
#include "pbd/convert.h"

#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/location.h"
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
#include "mackie_midi_builder.h"
#include "surface_port.h"
#include "surface.h"

#include "strip.h"
#include "control_group.h"
#include "meter.h"
#include "button.h"
#include "fader.h"
#include "pot.h"

using namespace ARDOUR;
using namespace std;
using namespace Mackie;
using namespace PBD;
using namespace Glib;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

#define ui_bind(f, ...) boost::protect (boost::bind (f, __VA_ARGS__))

const int MackieControlProtocol::MODIFIER_OPTION = 0x1;
const int MackieControlProtocol::MODIFIER_CONTROL = 0x2;
const int MackieControlProtocol::MODIFIER_SHIFT = 0x3;
const int MackieControlProtocol::MODIFIER_CMDALT = 0x4;

MackieControlProtocol* MackieControlProtocol::_instance = 0;

bool MackieControlProtocol::probe()
{
	return true;
}

MackieControlProtocol::MackieControlProtocol (Session& session)
	: ControlProtocol (session, X_("Mackie"), this)
	, AbstractUI<MackieControlUIRequest> ("mackie")
	, _current_initial_bank (0)
	, _timecode_type (ARDOUR::AnyTime::BBT)
	, _input_bundle (new ARDOUR::Bundle (_("Mackie Control In"), true))
	, _output_bundle (new ARDOUR::Bundle (_("Mackie Control Out"), false))
	, _gui (0)
	, _zoom_mode (false)
	, _scrub_mode (false)
	, _flip_mode (false)
	, _current_selected_track (-1)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::MackieControlProtocol\n");

	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
		audio_engine_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::port_connected_or_disconnected, this, _2, _4, _5),
		this
		);

	_instance = this;
}

MackieControlProtocol::~MackieControlProtocol()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol\n");

	_active = false;

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

	_instance = 0;
}

void
MackieControlProtocol::thread_init ()
{
	struct sched_param rtparam;

	pthread_set_name (X_("MackieControl"));

	PBD::notify_gui_about_thread_creation (X_("gui"), pthread_self(), X_("MackieControl"), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (X_("MackieControl"), 128);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */

	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
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
	if (_current_initial_bank + n_strips() < sorted.size()) {
		session->set_dirty();
		switch_banks (_current_initial_bank + 1);
	}
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
	switch_banks (_current_initial_bank, true);
}

uint32_t
MackieControlProtocol::n_strips() const
{
	uint32_t strip_count = 0;

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		strip_count += (*si)->n_strips ();
	}

	return strip_count;
}

void 
MackieControlProtocol::switch_banks (uint32_t initial, bool force)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch banking to start at %1 force ? %2 current = %3\n", initial, force, _current_initial_bank));

	if (initial == _current_initial_bank && !force) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("not switching to %1\n", initial));
		return;
	}

	Sorted sorted = get_sorted_routes();
	uint32_t strip_cnt = n_strips();

	if (sorted.size() <= strip_cnt && !force) {
		/* no banking */
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("not switching to %1\n", initial));
		return;
	}

	uint32_t delta = sorted.size() - strip_cnt;

	if (delta > 0 && initial > delta) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("not switching to %1\n", initial));
		return;
	}

	_current_initial_bank = initial;
	_current_selected_track = -1;

	for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		(*si)->drop_routes ();
	}

	// Map current bank of routes onto each surface(+strip)

	if (_current_initial_bank <= sorted.size()) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch to %1, %2, available routes %3\n", _current_initial_bank, strip_cnt, sorted.size()));

		// link routes to strips

		Sorted::iterator r = sorted.begin() + _current_initial_bank;
		
		for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			vector<boost::shared_ptr<Route> > routes;
			uint32_t added = 0;

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface has %1 strips\n", (*si)->n_strips()));

			for (; r != sorted.end() && added < (*si)->n_strips(); ++r, ++added) {
				routes.push_back (*r);
				cerr << "\t\tadded " << (*r)->name() << endl;
			}

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("give surface %1 routes\n", routes.size()));

			(*si)->map_routes (routes);
		}
	}

	// display the current start bank.
	surfaces.front()->display_bank_start (_current_initial_bank);
}

int 
MackieControlProtocol::set_active (bool yn)
{
	if (yn == _active) {
		return 0;
	}

	try
	{
		if (yn) {

			/* start event loop */

			BaseUI::run ();

			create_surfaces ();
			connect_session_signals ();
			
			_active = true;
			update_surfaces ();

			/* set up periodic task for metering and automation
			 */

			Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (100); // milliseconds
			periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::periodic));
			periodic_timeout->attach (main_loop()->get_context());

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
MackieControlProtocol::periodic ()
{
	if (!_active) {
		return false;
	}

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->periodic ();
	}
	
	update_timecode_display();

	return true;
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
	boost::shared_ptr<Surface> surface = surfaces.front();

	if (!surface->type() == mcu) {
		return;
	}

	if (surface->controls_by_name.find (name) != surface->controls_by_name.end()) {
		Button * button = dynamic_cast<Button*> (surface->controls_by_name[name]);
		surface->write (builder.build_led (button->led(), ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Button %1 not found\n", name));
	}
}

void 
MackieControlProtocol::update_global_led (const string & name, LedState ls)
{
	boost::shared_ptr<Surface> surface = surfaces.front();

	if (!surface->type() == mcu) {
		return;
	}

	if (surface->controls_by_name.find (name) != surface->controls_by_name.end()) {
		Led * led = dynamic_cast<Led*> (surface->controls_by_name[name]);
		surface->write (builder.build_led (*led, ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Led %1 not found\n", name));
	}
}

// send messages to surface to set controls to correct values
void 
MackieControlProtocol::update_surfaces()
{
	if (!_active) {
		return;
	}

	// do the initial bank switch to connect signals
	// _current_initial_bank is initialised by set_state
	switch_banks (_current_initial_bank, true);
	
	// sometimes the jog wheel is a pot
	surfaces.front()->blank_jog_ring ();
	
	// update global buttons and displays

	notify_record_state_changed();
	notify_transport_state_changed();
	update_timecode_beats_led();
}

void 
MackieControlProtocol::connect_session_signals()
{
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_route_added, this, _1), this);
	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_record_state_changed, this), this);
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_solo_active_changed, this, _1), this);

	// make sure remote id changed signals reach here
	// see also notify_route_added
	Sorted sorted = get_sorted_routes();

	for (Sorted::iterator it = sorted.begin(); it != sorted.end(); ++it) {
		(*it)->RemoteControlIDChanged.connect (route_connections, MISSING_INVALIDATOR, ui_bind(&MackieControlProtocol::notify_remote_id_changed, this), this);
	}
}

void 
MackieControlProtocol::create_surfaces ()
{
	string device_name = "mcu";
	surface_type_t stype = mcu;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Create %1 surfaces\n",
							   1 + ARDOUR::Config->get_mackie_extenders()));

	for (uint32_t n = 0; n < 1 + ARDOUR::Config->get_mackie_extenders(); ++n) {

		boost::shared_ptr<Surface> surface (new Surface (*this, session->engine().jack(), device_name, n, stype));
		surfaces.push_back (surface);
		
		device_name = "mcu_xt";
		stype = ext;

		_input_bundle->add_channel (
			surface->port().input_port().name(),
			ARDOUR::DataType::MIDI,
			session->engine().make_port_name_non_relative (surface->port().input_port().name())
			);
		
		_output_bundle->add_channel (
			surface->port().output_port().name(),
			ARDOUR::DataType::MIDI,
			session->engine().make_port_name_non_relative (surface->port().output_port().name())
			);

		int fd;
		MIDI::Port& input_port (surface->port().input_port());
		
		if ((fd = input_port.selectable ()) >= 0) {
			Glib::RefPtr<IOSource> psrc = IOSource::create (fd, IO_IN|IO_HUP|IO_ERR);

			psrc->connect (sigc::bind (sigc::mem_fun (this, &MackieControlProtocol::midi_input_handler), &input_port));
			psrc->attach (main_loop()->get_context());
			
			// glibmm hack: for now, store only the GSource*

			port_sources.push_back (psrc->gobj());
			g_source_ref (psrc->gobj());
		}
	}
}

void 
MackieControlProtocol::close()
{
	clear_ports ();

	port_connections.drop_connections ();
	session_connections.drop_connections ();
	route_connections.drop_connections ();
	periodic_connection.disconnect ();

	surfaces.clear ();
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


/////////////////////////////////////////////////
// handlers for Route signals
// TODO should these be part of RouteSignal?
// They started off as signal/slot handlers for signals
// from Route, but they're also used in polling for automation
/////////////////////////////////////////////////

// TODO handle plugin automation polling
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
	boost::shared_ptr<Surface> surface = surfaces.front();

	if (surface->type() != mcu || !surface->has_timecode_display()) {
		return;
	}

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
		return;
	}
	
	// only write the timecode string to the MCU if it's changed
	// since last time. This is to reduce midi bandwidth used.
	if (timecode != _timecode_last) {
		surface->display_timecode (timecode, _timecode_last);
		_timecode_last = timecode;
	}
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

	refresh_current_bank();

	// otherwise route added, but current bank needs no updating

	// make sure remote id changes in the new route are handled
	typedef ARDOUR::RouteList ARS;

	for (ARS::iterator it = rl.begin(); it != rl.end(); ++it) {
		(*it)->RemoteControlIDChanged.connect (route_connections, MISSING_INVALIDATOR, ui_bind (&MackieControlProtocol::notify_remote_id_changed, this), this);
	}
}

void 
MackieControlProtocol::notify_solo_active_changed (bool active)
{
	boost::shared_ptr<Surface> surface = surfaces.front();
	
	Button * rude_solo = reinterpret_cast<Button*> (surface->controls_by_name["solo"]);

	if (rude_solo) {
		surface->write (builder.build_led (*rude_solo, active ? flashing : off));
	}
}

void 
MackieControlProtocol::notify_remote_id_changed()
{
	Sorted sorted = get_sorted_routes();
	uint32_t sz = n_strips();

	// if a remote id has been moved off the end, we need to shift
	// the current bank backwards.

	if (sorted.size() - _current_initial_bank < sz) {
		// but don't shift backwards past the zeroth channel
		switch_banks (max((Sorted::size_type) 0, sorted.size() - sz));
	} else {
		// Otherwise just refresh the current bank
		refresh_current_bank();
	}
}

///////////////////////////////////////////
// Transport signals
///////////////////////////////////////////

void 
MackieControlProtocol::notify_loop_state_changed()
{
	update_global_button ("loop", session->get_play_loop());
}

void 
MackieControlProtocol::notify_transport_state_changed()
{
	// switch various play and stop buttons on / off
	update_global_button ("play", session->transport_rolling());
	update_global_button ("stop", !session->transport_rolling());
	update_global_button ("rewind", session->transport_speed() < 0.0);
	update_global_button ("ffwd", session->transport_speed() > 1.0);

	_transport_previously_rolling = session->transport_rolling();
}

void
MackieControlProtocol::notify_record_state_changed ()
{
	/* rec is a tristate */


	Button * rec = reinterpret_cast<Button*> (surfaces.front()->controls_by_name["record"]);
	if (rec) {
		LedState ls;

		switch (session->record_status()) {
		case Session::Disabled:
			DEBUG_TRACE (DEBUG::MackieControl, "record state changed to disabled, LED off\n");
			ls = off;
			break;
		case Session::Recording:
			DEBUG_TRACE (DEBUG::MackieControl, "record state changed to recording, LED on\n");
			ls = on;
			break;
		case Session::Enabled:
			DEBUG_TRACE (DEBUG::MackieControl, "record state changed to enabled, LED flashing\n");
			ls = flashing;
			break;
		}

		surfaces.front()->write (builder.build_led (*rec, ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, "record button control not found\n");
	}
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

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		string const n = AudioEngine::instance()->make_port_name_non_relative ((*s)->port().output_port().name ());
		if (a == n || b == n) {
			update_surfaces ();
			return;
		}
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
MackieControlProtocol::add_in_use_timeout (Surface& surface, Control& in_use_control, Control* touch_control)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout (Glib::TimeoutSource::create (250)); // milliseconds

	in_use_control.in_use_connection.disconnect ();
	in_use_control.in_use_connection = timeout->connect (
		sigc::bind (sigc::mem_fun (*this, &MackieControlProtocol::control_in_use_timeout), &surface, &in_use_control, touch_control));
	in_use_control.in_use_touch_control = touch_control;
	
	timeout->attach (main_loop()->get_context());

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("timeout queued for surface %1, control %2 touch control %3\n",
							   surface.number(), &in_use_control, touch_control));}

/** Handle timeouts to reset in_use for controls that can't
 *  do this by themselves (e.g. pots, and faders without touch support).
 *  @param in_use_control the control whose in_use flag to reset.
 *  @param touch_control a touch control to emit an event for, or 0.
 */
bool
MackieControlProtocol::control_in_use_timeout (Surface* surface, Control* in_use_control, Control* touch_control)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("timeout elapsed for surface %1, control %2 touch control %3\n",
							   surface->number(), in_use_control, touch_control));

	in_use_control->set_in_use (false);

	if (touch_control) {
		// empty control_state
		ControlState control_state;
		surface->handle_control_event (*touch_control, control_state);
	}
	
	// only call this method once from the timer
	return false;
}

void 
MackieControlProtocol::update_led (Surface& surface, Button& button, Mackie::LedState ls)
{
	if (ls != none) {
		surface.port().write (builder.build_led (button, ls));
	}
}

void
MackieControlProtocol::build_button_map ()
{
	button_map.insert (pair<int,ButtonHandlers> (Button::Io, ButtonHandlers (&MackieControlProtocol::io_press, &MackieControlProtocol::io_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Sends, ButtonHandlers (&MackieControlProtocol::sends_press, &MackieControlProtocol::sends_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Pan, ButtonHandlers (&MackieControlProtocol::pan_press, &MackieControlProtocol::pan_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Plugin, ButtonHandlers (&MackieControlProtocol::plugin_press, &MackieControlProtocol::plugin_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Eq, ButtonHandlers (&MackieControlProtocol::eq_press, &MackieControlProtocol::eq_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Dyn, ButtonHandlers (&MackieControlProtocol::dyn_press, &MackieControlProtocol::dyn_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Left, ButtonHandlers (&MackieControlProtocol::left_press, &MackieControlProtocol::left_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Right, ButtonHandlers (&MackieControlProtocol::right_press, &MackieControlProtocol::right_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::ChannelLeft, ButtonHandlers (&MackieControlProtocol::channel_left_press, &MackieControlProtocol::channel_left_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::ChannelRight, ButtonHandlers (&MackieControlProtocol::channel_right_press, &MackieControlProtocol::channel_right_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Flip, ButtonHandlers (&MackieControlProtocol::flip_press, &MackieControlProtocol::flip_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Edit, ButtonHandlers (&MackieControlProtocol::edit_press, &MackieControlProtocol::edit_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::NameValue, ButtonHandlers (&MackieControlProtocol::name_value_press, &MackieControlProtocol::name_value_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::TimecodeBeats, ButtonHandlers (&MackieControlProtocol::timecode_beats_press, &MackieControlProtocol::timecode_beats_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F1, ButtonHandlers (&MackieControlProtocol::F1_press, &MackieControlProtocol::F1_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F2, ButtonHandlers (&MackieControlProtocol::F2_press, &MackieControlProtocol::F2_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F3, ButtonHandlers (&MackieControlProtocol::F3_press, &MackieControlProtocol::F3_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F4, ButtonHandlers (&MackieControlProtocol::F4_press, &MackieControlProtocol::F4_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F5, ButtonHandlers (&MackieControlProtocol::F5_press, &MackieControlProtocol::F5_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F6, ButtonHandlers (&MackieControlProtocol::F6_press, &MackieControlProtocol::F6_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F7, ButtonHandlers (&MackieControlProtocol::F7_press, &MackieControlProtocol::F7_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F8, ButtonHandlers (&MackieControlProtocol::F8_press, &MackieControlProtocol::F8_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F9, ButtonHandlers (&MackieControlProtocol::F9_press, &MackieControlProtocol::F9_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F10, ButtonHandlers (&MackieControlProtocol::F10_press, &MackieControlProtocol::F10_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F11, ButtonHandlers (&MackieControlProtocol::F11_press, &MackieControlProtocol::F11_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F12, ButtonHandlers (&MackieControlProtocol::F12_press, &MackieControlProtocol::F12_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F13, ButtonHandlers (&MackieControlProtocol::F13_press, &MackieControlProtocol::F13_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F14, ButtonHandlers (&MackieControlProtocol::F14_press, &MackieControlProtocol::F14_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F15, ButtonHandlers (&MackieControlProtocol::F15_press, &MackieControlProtocol::F15_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::F16, ButtonHandlers (&MackieControlProtocol::F16_press, &MackieControlProtocol::F16_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Shift, ButtonHandlers (&MackieControlProtocol::shift_press, &MackieControlProtocol::shift_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Option, ButtonHandlers (&MackieControlProtocol::option_press, &MackieControlProtocol::option_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Ctrl, ButtonHandlers (&MackieControlProtocol::control_press, &MackieControlProtocol::control_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::CmdAlt, ButtonHandlers (&MackieControlProtocol::cmd_alt_press, &MackieControlProtocol::cmd_alt_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::On, ButtonHandlers (&MackieControlProtocol::on_press, &MackieControlProtocol::on_release)));
	//button_map.insert (pair<int,ButtonHandlers> (Button::Rec, ButtonHandlers (&MackieControlProtocol::rec_press, &MackieControlProtocol::rec_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Undo, ButtonHandlers (&MackieControlProtocol::undo_press, &MackieControlProtocol::undo_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Save, ButtonHandlers (&MackieControlProtocol::save_press, &MackieControlProtocol::save_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Touch, ButtonHandlers (&MackieControlProtocol::touch_press, &MackieControlProtocol::touch_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Redo, ButtonHandlers (&MackieControlProtocol::redo_press, &MackieControlProtocol::redo_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Marker, ButtonHandlers (&MackieControlProtocol::marker_press, &MackieControlProtocol::marker_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Enter, ButtonHandlers (&MackieControlProtocol::enter_press, &MackieControlProtocol::enter_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Cancel, ButtonHandlers (&MackieControlProtocol::cancel_press, &MackieControlProtocol::cancel_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Mixer, ButtonHandlers (&MackieControlProtocol::mixer_press, &MackieControlProtocol::mixer_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::FrmLeft, ButtonHandlers (&MackieControlProtocol::frm_left_press, &MackieControlProtocol::frm_left_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::FrmRight, ButtonHandlers (&MackieControlProtocol::frm_right_press, &MackieControlProtocol::frm_right_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Loop, ButtonHandlers (&MackieControlProtocol::loop_press, &MackieControlProtocol::loop_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::PunchIn, ButtonHandlers (&MackieControlProtocol::punch_in_press, &MackieControlProtocol::punch_in_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::PunchOut, ButtonHandlers (&MackieControlProtocol::punch_out_press, &MackieControlProtocol::punch_out_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Home, ButtonHandlers (&MackieControlProtocol::home_press, &MackieControlProtocol::home_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::End, ButtonHandlers (&MackieControlProtocol::end_press, &MackieControlProtocol::end_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Rewind, ButtonHandlers (&MackieControlProtocol::rewind_press, &MackieControlProtocol::rewind_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Ffwd, ButtonHandlers (&MackieControlProtocol::ffwd_press, &MackieControlProtocol::ffwd_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Stop, ButtonHandlers (&MackieControlProtocol::stop_press, &MackieControlProtocol::stop_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Play, ButtonHandlers (&MackieControlProtocol::play_press, &MackieControlProtocol::play_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Record, ButtonHandlers (&MackieControlProtocol::record_press, &MackieControlProtocol::record_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::CursorUp, ButtonHandlers (&MackieControlProtocol::cursor_up_press, &MackieControlProtocol::cursor_up_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::CursorDown, ButtonHandlers (&MackieControlProtocol::cursor_down_press, &MackieControlProtocol::cursor_down_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::CursorLeft, ButtonHandlers (&MackieControlProtocol::cursor_left_press, &MackieControlProtocol::cursor_left_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::CursorRight, ButtonHandlers (&MackieControlProtocol::cursor_right_press, &MackieControlProtocol::cursor_right_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Zoom, ButtonHandlers (&MackieControlProtocol::zoom_press, &MackieControlProtocol::zoom_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::Scrub, ButtonHandlers (&MackieControlProtocol::scrub_press, &MackieControlProtocol::scrub_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::UserA, ButtonHandlers (&MackieControlProtocol::user_a_press, &MackieControlProtocol::user_a_release)));
	button_map.insert (pair<int,ButtonHandlers> (Button::UserB, ButtonHandlers (&MackieControlProtocol::user_b_press, &MackieControlProtocol::user_b_release)));
}

void 
MackieControlProtocol::handle_button_event (Surface& surface, Button& button, ButtonState bs)
{
	if  (bs != press && bs != release) {
		update_led (surface, button, none);
		return;
	}
	
	LedState ls;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Handling %1 for button %2\n", (bs == press ? "press" : "release"), button.raw_id()));

	ButtonMap::iterator b = button_map.find (button.raw_id());

	if (b != button_map.end()) {

		ButtonHandlers& bh (b->second);

		switch  (bs) {
		case press: ls = (this->*(bh.press)) (button); break;
		case release: ls = (this->*(bh.release)) (button); break;
		case neither: break;
		}
	}
		
	update_led (surface, button, ls);
}

void
MackieControlProtocol::select_track (boost::shared_ptr<Route> r)
{
	if (_modifier_state == MODIFIER_SHIFT) {
		r->gain_control()->set_value (0.0);
	} else {
		if (_current_selected_track > 0 && r->remote_control_id() == (uint32_t) _current_selected_track) {
			UnselectTrack (); /* EMIT SIGNAL */
			_current_selected_track = -1;
		} else {
			SelectByRID (r->remote_control_id()); /* EMIT SIGNAL */
			_current_selected_track = r->remote_control_id();;
		}
	}
}

bool
MackieControlProtocol::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("something happend on  %1\n", port->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		CrossThreadChannel::drain (port->selectable());

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", port->name()));
		framepos_t now = session->engine().frame_time();
		port->parse (now);
	}

	return true;
}

void
MackieControlProtocol::clear_ports ()
{
	for (PortSources::iterator i = port_sources.begin(); i != port_sources.end(); ++i) {
		g_source_destroy (*i);
		g_source_unref (*i);
	}

	port_sources.clear ();
}

