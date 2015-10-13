/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#include <boost/shared_array.hpp>

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/ipmidi_port.h"
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/memento_command.h"
#include "pbd/convert.h"

#include "ardour/automation_control.h"
#include "ardour/async_midi_port.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/location.h"
#include "ardour/meter.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/track.h"
#include "ardour/types.h"
#include "ardour/audioengine.h"

#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "mackie_control_exception.h"
#include "device_profile.h"
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
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace Mackie;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

const int MackieControlProtocol::MODIFIER_OPTION = 0x1;
const int MackieControlProtocol::MODIFIER_CONTROL = 0x2;
const int MackieControlProtocol::MODIFIER_SHIFT = 0x4;
const int MackieControlProtocol::MODIFIER_CMDALT = 0x8;
const int MackieControlProtocol::MODIFIER_ZOOM = 0x10;
const int MackieControlProtocol::MODIFIER_SCRUB = 0x20;
const int MackieControlProtocol::MAIN_MODIFIER_MASK = (MackieControlProtocol::MODIFIER_OPTION|
						       MackieControlProtocol::MODIFIER_CONTROL|
						       MackieControlProtocol::MODIFIER_SHIFT|
						       MackieControlProtocol::MODIFIER_CMDALT);

MackieControlProtocol* MackieControlProtocol::_instance = 0;

bool MackieControlProtocol::probe()
{
	return true;
}

MackieControlProtocol::MackieControlProtocol (Session& session)
	: ControlProtocol (session, X_("Mackie"))
	, AbstractUI<MackieControlUIRequest> ("mackie")
	, _current_initial_bank (0)
	, _timecode_type (ARDOUR::AnyTime::BBT)
	, _gui (0)
	, _scrub_mode (false)
	, _flip_mode (Normal)
	, _view_mode (Mixer)
	, _pot_mode (Pan)
	, _current_selected_track (-1)
	, _modifier_state (0)
	, _ipmidi_base (MIDI::IPMIDIPort::lowest_ipmidi_port_default)
	, needs_ipmidi_restart (false)
	, _metering_active (true)
	, _initialized (false)
	, configuration_state (0)
	, state_version (0)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::MackieControlProtocol\n");

	DeviceInfo::reload_device_info ();
	DeviceProfile::reload_device_profiles ();

	TrackSelectionChanged.connect (gui_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::gui_track_selection_changed, this, _1, true), this);

	_instance = this;

	build_button_map ();
}

MackieControlProtocol::~MackieControlProtocol()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol init\n");

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		(*si)->reset ();
	}

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol drop_connections ()\n");
	drop_connections ();

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol tear_down_gui ()\n");
	tear_down_gui ();

	delete configuration_state;

	/* stop event loop */
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol BaseUI::quit ()\n");
	BaseUI::quit ();

	try {
		DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol close()\n");
		close();
	}
	catch (exception & e) {
		cout << "~MackieControlProtocol caught " << e.what() << endl;
	}
	catch (...) {
		cout << "~MackieControlProtocol caught unknown" << endl;
	}

	_instance = 0;

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol done\n");
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

void
MackieControlProtocol::ping_devices ()
{
	/* should not be called if surfaces are not connected, but will not
	 * malfunction if it is.
	 */

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		(*si)->connected ();
	}
}

// go to the previous track.
// Assume that get_sorted_routes().size() > route_table.size()
void
MackieControlProtocol::prev_track()
{
	if (_current_initial_bank >= 1) {
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
		switch_banks (_current_initial_bank + 1);
	}
}

bool
MackieControlProtocol::route_is_locked_to_strip (boost::shared_ptr<Route> r) const
{
	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		if ((*si)->route_is_locked_to_strip (r)) {
			return true;
		}
	}
	return false;
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

		boost::shared_ptr<Route> route = *it;

		if (remote_ids.find (route->remote_control_id()) != remote_ids.end()) {
			continue;
		}

		if (route->is_auditioner() || route->is_master() || route->is_monitor()) {
			continue;
		}

		/* don't include locked routes */

		if (route_is_locked_to_strip(route)) {
			continue;
		}
		/* This next section which is not used yet, looks wrong to me
			The first four belong here but the bottom five are not a selection
			of routes and belong elsewhere as they are v-pot modes.
		*/
		switch (_view_mode) {
		case Mixer:
			break;
		case AudioTracks:
			break;
		case Busses:
			break;
		case MidiTracks:
			break;
		case Loop:
			break;
		}

		sorted.push_back (*it);
		remote_ids.insert (route->remote_control_id());
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
MackieControlProtocol::n_strips (bool with_locked_strips) const
{
	uint32_t strip_count = 0;

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		strip_count += (*si)->n_strips (with_locked_strips);
	}

	return strip_count;
}

void
MackieControlProtocol::switch_banks (uint32_t initial, bool force)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch banking to start at %1 force ? %2 current = %3\n", initial, force, _current_initial_bank));

	if (initial == _current_initial_bank && !force) {
		return;
	}

	Sorted sorted = get_sorted_routes();
	uint32_t strip_cnt = n_strips (false); // do not include locked strips
					       // in this count

	if (sorted.size() <= strip_cnt && _current_initial_bank == 0 && !force) {
		/* no banking - not enough routes to fill all strips and we're
		 * not at the first one.
		 */
		return;
	}
	set_flip_mode (Normal);
	_current_initial_bank = initial;
	_current_selected_track = -1;

	// Map current bank of routes onto each surface(+strip)

	if (_current_initial_bank <= sorted.size()) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch to %1, %2, available routes %3 on %4 surfaces\n",
								   _current_initial_bank, strip_cnt, sorted.size(),
								   surfaces.size()));

		// link routes to strips

		Sorted::iterator r = sorted.begin() + _current_initial_bank;

		for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			vector<boost::shared_ptr<Route> > routes;
			uint32_t added = 0;

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface has %1 unlockedstrips\n", (*si)->n_strips (false)));

			for (; r != sorted.end() && added < (*si)->n_strips (false); ++r, ++added) {
				routes.push_back (*r);
			}

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("give surface %1 routes\n", routes.size()));

			(*si)->map_routes (routes);
		}
	}

	/* reset this to get the right display of view mode after the switch */
	set_view_mode (_view_mode);

	/* make sure selection is correct */

	_gui_track_selection_changed (&_last_selected_routes, false);

	/* current bank has not been saved */
	session->set_dirty();
}

int
MackieControlProtocol::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose("MackieControlProtocol::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		/* start event loop */

		BaseUI::run ();

		connect_session_signals ();

		if (!_device_info.name().empty()) {
			set_device (_device_info.name(), true);
		}

		/* set up periodic task for metering and automation
		 */

		Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (100); // milliseconds
		periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::periodic));
		periodic_timeout->attach (main_loop()->get_context());

		/* a faster periodic task used to display parameter updates */

		Glib::RefPtr<Glib::TimeoutSource> redisplay_timeout = Glib::TimeoutSource::create (10); // milliseconds
		redisplay_connection = redisplay_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::redisplay));
		redisplay_timeout->attach (main_loop()->get_context());

	} else {

		BaseUI::quit ();
		close ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose("MackieControlProtocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

bool
MackieControlProtocol::hui_heartbeat ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->hui_heartbeat ();
	}

	return true;
}

bool
MackieControlProtocol::periodic ()
{
	if (!active()) {
		return false;
	}

	if (needs_ipmidi_restart) {
		ipmidi_restart ();
		return true;
	}

	if (!_initialized) {
		initialize();
	}

	ARDOUR::microseconds_t now_usecs = ARDOUR::get_microseconds ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->periodic (now_usecs);
		}
	}

	update_timecode_display ();

	return true;
}

bool
MackieControlProtocol::redisplay ()
{
	if (!active()) {
		return false;
	}

	if (needs_ipmidi_restart) {
		ipmidi_restart ();
		return true;
	}

	if (!_initialized) {
		initialize();
	}

	ARDOUR::microseconds_t now = ARDOUR::get_microseconds ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->redisplay (now);
		}
	}

	return true;
}

void
MackieControlProtocol::update_timecode_beats_led()
{
	if (!_device_info.has_timecode_display()) {
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose("MackieControlProtocol::update_timecode_beats_led(): %1\n", _timecode_type));
	switch (_timecode_type) {
		case ARDOUR::AnyTime::BBT:
			update_global_led (Led::Beats, on);
			update_global_led (Led::Timecode, off);
			break;
		case ARDOUR::AnyTime::Timecode:
			update_global_led (Led::Timecode, on);
			update_global_led (Led::Beats, off);
			break;
		default:
			ostringstream os;
			os << "Unknown Anytime::Type " << _timecode_type;
			throw runtime_error (os.str());
	}
}

void
MackieControlProtocol::update_global_button (int id, LedState ls)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	if (surfaces.empty()) {
		return;
	}

	if (!_device_info.has_global_controls()) {
		return;
	}
	// surface needs to be master surface
	boost::shared_ptr<Surface> surface = _master_surface;

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (id);
	if (x != surface->controls_by_device_independent_id.end()) {
		Button * button = dynamic_cast<Button*> (x->second);
		surface->write (button->set_state (ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Button %1 not found\n", id));
	}
}

void
MackieControlProtocol::update_global_led (int id, LedState ls)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	if (surfaces.empty()) {
		return;
	}

	if (!_device_info.has_global_controls()) {
		return;
	}
	boost::shared_ptr<Surface> surface = _master_surface;

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (id);

	if (x != surface->controls_by_device_independent_id.end()) {
		Led * led = dynamic_cast<Led*> (x->second);
		DEBUG_TRACE (DEBUG::MackieControl, "Writing LedState\n");
		surface->write (led->set_state (ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Led %1 not found\n", id));
	}
}

void
MackieControlProtocol::device_ready ()
{
	/* this is not required to be called, but for devices which do
	 * handshaking, it can be called once the device has verified the
	 * connection.
	 */

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("device ready init (active=%1)\n", active()));
	update_surfaces ();
}

// send messages to surface to set controls to correct values
void
MackieControlProtocol::update_surfaces()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::update_surfaces() init (active=%1)\n", active()));
	if (!active()) {
		return;
	}

	// do the initial bank switch to connect signals
	// _current_initial_bank is initialised by set_state
	switch_banks (_current_initial_bank, true);

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::update_surfaces() finished\n");
}

void
MackieControlProtocol::initialize()
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}

		if (!_master_surface->active ()) {
			return;
		}

		// sometimes the jog wheel is a pot
		if (_device_info.has_jog_wheel()) {
			_master_surface->blank_jog_ring ();
		}
	}

	// update global buttons and displays

	notify_record_state_changed();
	notify_transport_state_changed();
	update_timecode_beats_led();

	_initialized = true;
}

void
MackieControlProtocol::connect_session_signals()
{
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_route_added, this, _1), this);
	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_record_state_changed, this), this);
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_solo_active_changed, this, _1), this);

	// make sure remote id changed signals reach here
	// see also notify_route_added
	Sorted sorted = get_sorted_routes();

	for (Sorted::iterator it = sorted.begin(); it != sorted.end(); ++it) {
		(*it)->RemoteControlIDChanged.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_remote_id_changed, this), this);
	}
}

void
MackieControlProtocol::set_profile (const string& profile_name)
{
	if (profile_name == "default") {
		/* reset to default */
		_device_profile = DeviceProfile (profile_name);
	}

	map<string,DeviceProfile>::iterator d = DeviceProfile::device_profiles.find (profile_name);

	if (d == DeviceProfile::device_profiles.end()) {
		return;
	}

	_device_profile = d->second;
}

int
MackieControlProtocol::set_device_info (const string& device_name)
{
	map<string,DeviceInfo>::iterator d = DeviceInfo::device_info.find (device_name);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("new device chosen %1\n", device_name));

	if (d == DeviceInfo::device_info.end()) {
		return -1;
	}

	_device_info = d->second;

	return 0;
}

int
MackieControlProtocol::set_device (const string& device_name, bool force)
{
	if (device_name == device_info().name() && !force) {
		/* already using that device, nothing to do */
		return 0;
	}
	/* get state from the current setup, and make sure it is stored in
	   the configuration_states node so that if we switch back to this device,
	   we will have its state available.
	*/

	update_configuration_state ();

	if (set_device_info (device_name)) {
		return -1;
	}

	clear_surfaces ();
	port_connection.disconnect ();
	hui_connection.disconnect ();

	if (_device_info.device_type() == DeviceInfo::HUI) {
		Glib::RefPtr<Glib::TimeoutSource> hui_timeout = Glib::TimeoutSource::create (1000); // milliseconds
		hui_connection = hui_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::hui_heartbeat));
		hui_timeout->attach (main_loop()->get_context());
	}

	if (!_device_info.uses_ipmidi()) {
		/* notice that the handler for this will execute in our event
		   loop, not in the thread where the
		   PortConnectedOrDisconnected signal is emitted.
		*/
		ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::connection_handler, this, _1, _2, _3, _4, _5), this);
	}

	if (create_surfaces ()) {
		return -1;
	}

	DeviceChanged ();

	return 0;
}

gboolean
ArdourSurface::ipmidi_input_handler (GIOChannel*, GIOCondition condition, void *data)
{
	ArdourSurface::MackieControlProtocol::ipMIDIHandler* ipm = static_cast<ArdourSurface::MackieControlProtocol::ipMIDIHandler*>(data);
	return ipm->mcp->midi_input_handler (Glib::IOCondition (condition), ipm->port);
}

int
MackieControlProtocol::create_surfaces ()
{
	string device_name;
	surface_type_t stype = mcu; // type not yet determined

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Create %1 surfaces for %2\n", 1 + _device_info.extenders(), _device_info.name()));

	if (!_device_info.uses_ipmidi()) {
		_input_bundle.reset (new ARDOUR::Bundle (_("Mackie Control In"), true));
		_output_bundle.reset (new ARDOUR::Bundle (_("Mackie Control Out"), false));
	} else {
		_input_bundle.reset ();
		_output_bundle.reset ();

	}
	for (uint32_t n = 0; n < 1 + _device_info.extenders(); ++n) {
		bool is_master = false;

		if (n == _device_info.master_position()) {
			is_master = true;
			if (_device_info.extenders() == 0) {
				device_name = _device_info.name();
			} else {
				device_name = X_("mackie control");
			}

		}

		if (!is_master) {
			device_name = string_compose (X_("mackie control ext %1"), n+1);
		}

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Port Name for surface %1 is %2\n", n, device_name));

		boost::shared_ptr<Surface> surface;

		if (is_master) {
			stype = mcu;
		} else {
			stype = ext;
		}
		try {
			surface.reset (new Surface (*this, device_name, n, stype));
		} catch (...) {
			return -1;
		}

		if (is_master) {
			_master_surface = surface;
		}

		if (configuration_state) {
			XMLNode* this_device = 0;
			XMLNodeList const& devices = configuration_state->children();
			for (XMLNodeList::const_iterator d = devices.begin(); d != devices.end(); ++d) {
				XMLProperty* prop = (*d)->property (X_("name"));
				if (prop && prop->value() == device_name) {
					this_device = *d;
					break;
				}
			}
			if (this_device) {
				XMLNode* surfaces = this_device->child (X_("Surfaces"));
				if (surfaces) {
					surface->set_state (*surfaces, state_version);
				}
			}
		}

		{
			Glib::Threads::Mutex::Lock lm (surfaces_lock);
			surfaces.push_back (surface);
		}

		if (!_device_info.uses_ipmidi()) {

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
		}

		MIDI::Port& input_port (surface->port().input_port());
		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*> (&input_port);

		if (asp) {

			/* async MIDI port */

			asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &MackieControlProtocol::midi_input_handler), &input_port));
			asp->xthread().attach (main_loop()->get_context());

		} else {

			/* ipMIDI port, no IOSource method at this time */

			int fd;

			if ((fd = input_port.selectable ()) >= 0) {

				GIOChannel* ioc = g_io_channel_unix_new (fd);
				surface->input_source = g_io_create_watch (ioc, GIOCondition (G_IO_IN|G_IO_HUP|G_IO_ERR));

				/* make surface's input source now hold the
				 * only reference on the IO channel
				 */
				g_io_channel_unref (ioc);

				/* hack up an object so that in the callback from the event loop
				   we have both the MackieControlProtocol and the input port.

				   If we were using C++ for this stuff we wouldn't need this
				   but a nasty, not-fixable bug in the binding between C
				   and C++ makes it necessary to avoid C++ for the IO
				   callback setup.
				*/

				ipMIDIHandler* ipm = new ipMIDIHandler (); /* we will leak this sizeof(pointer)*2 sized object */
				ipm->mcp = this;
				ipm->port = &input_port;

				g_source_set_callback (surface->input_source, (GSourceFunc) ipmidi_input_handler, ipm, NULL);
				g_source_attach (surface->input_source, main_loop()->get_context()->gobj());
			}
		}
	}

	if (!_device_info.uses_ipmidi()) {
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->port().reconnect ();
		}
	}

	session->BundleAddedOrRemoved ();

	assert (_master_surface);

	return 0;
}

void
MackieControlProtocol::close()
{
	port_connection.disconnect ();
	session_connections.drop_connections ();
	route_connections.drop_connections ();
	periodic_connection.disconnect ();

	clear_surfaces();
}

/** Ensure that the configuration_state XML node contains an up-to-date
 *  copy of the state node the current device. If configuration_state already
 *  contains a state node for the device, it will deleted and replaced.
 */
void
MackieControlProtocol::update_configuration_state ()
{
	if (!configuration_state) {
		configuration_state = new XMLNode (X_("Configurations"));
	}

	XMLNode* devnode = new XMLNode (X_("Configuration"));
	devnode->add_property (X_("name"), _device_info.name());

	configuration_state->remove_nodes_and_delete (X_("name"), _device_info.name());
	configuration_state->add_child_nocopy (*devnode);

	XMLNode* snode = new XMLNode (X_("Surfaces"));
	devnode->add_child_nocopy (*snode);

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			snode->add_child_nocopy ((*s)->get_state());
		}
	}
}

XMLNode&
MackieControlProtocol::get_state()
{
	XMLNode& node (ControlProtocol::get_state());

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::get_state init\n");
	char buf[16];

	// add current bank
	snprintf (buf, sizeof (buf), "%d", _current_initial_bank);
	node.add_property (X_("bank"), buf);

	// ipMIDI base port (possibly not used)
	snprintf (buf, sizeof (buf), "%d", _ipmidi_base);
	node.add_property (X_("ipmidi-base"), buf);

	node.add_property (X_("device-profile"), _device_profile.name());
	node.add_property (X_("device-name"), _device_info.name());

	update_configuration_state ();

	/* force a copy of the _surfaces_state node, because we want to retain ownership */
	node.add_child_copy (*configuration_state);

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::get_state done\n");

	return node;
}

int
MackieControlProtocol::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::set_state: active %1\n", active()));

	int retval = 0;
	const XMLProperty* prop;
	uint32_t bank = 0;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if ((prop = node.property (X_("ipmidi-base"))) != 0) {
		set_ipmidi_base (atoi (prop->value()));
	}

	// fetch current bank
	if ((prop = node.property (X_("bank"))) != 0) {
		bank = atoi (prop->value());
	}

	if ((prop = node.property (X_("device-name"))) != 0) {
		set_device_info (prop->value());
	}

	if ((prop = node.property (X_("device-profile"))) != 0) {
		set_profile (prop->value());
	}

	XMLNode* dnode = node.child (X_("Configurations"));

	delete configuration_state;
	configuration_state = 0;

	if (dnode) {
		configuration_state = new XMLNode (*dnode);
		state_version = version;
	}

	switch_banks (bank, true);

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::set_state done\n");

	return retval;
}

string
MackieControlProtocol::format_bbt_timecode (framepos_t now_frame)
{
	Timecode::BBT_Time bbt_time;

	session->bbt_time (now_frame, bbt_time);

	// The Mackie protocol spec is built around a BBT time display of
	//
	// digits:     888/88/88/888
	// semantics:  BBB/bb/ss/ttt
	//
	// The third field is "subdivisions" which is a concept found in Logic
	// but not present in Ardour. Instead Ardour displays a 4 digit tick
	// count, which we need to spread across the 5 digits of ss/ttt.

	ostringstream os;

	os << setw(3) << setfill('0') << bbt_time.bars;
	os << setw(2) << setfill('0') << bbt_time.beats;
	os << ' ';
	os << setw(1) << setfill('0') << bbt_time.ticks / 1000;
	os << setw(3) << setfill('0') << bbt_time.ticks % 1000;

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
	os << setw(2) << setfill('0') << timecode.hours;
	os << ' ';
	os << setw(2) << setfill('0') << timecode.minutes;
	os << setw(2) << setfill('0') << timecode.seconds;
	os << ' ';
	os << setw(2) << setfill('0') << timecode.frames;

	return os.str();
}

void
MackieControlProtocol::update_timecode_display()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	if (surfaces.empty()) {
		return;
	}

	boost::shared_ptr<Surface> surface = _master_surface;

	if (surface->type() != mcu || !_device_info.has_timecode_display() || !surface->active ()) {
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
		// no such button right now
		// update_global_button (Button::PunchIn, session->config.get_punch_in());
	} else if (p == "punch-out") {
		// no such button right now
		// update_global_button (Button::PunchOut, session->config.get_punch_out());
	} else if (p == "clicking") {
		update_global_button (Button::Click, Config->get_clicking());
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("parameter changed: %1\n", p));
	}
}

// RouteList is the set of routes that have just been added
void
MackieControlProtocol::notify_route_added (ARDOUR::RouteList & rl)
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}
	}

	// currently assigned banks are less than the full set of
	// strips, so activate the new strip now.

	refresh_current_bank();

	// otherwise route added, but current bank needs no updating

	// make sure remote id changes in the new route are handled
	typedef ARDOUR::RouteList ARS;

	for (ARS::iterator it = rl.begin(); it != rl.end(); ++it) {
		(*it)->RemoteControlIDChanged.connect (route_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_remote_id_changed, this), this);
	}
}

void
MackieControlProtocol::notify_solo_active_changed (bool active)
{
	boost::shared_ptr<Surface> surface;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}

		surface = _master_surface;
	}

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (Led::RudeSolo);
	if (x != surface->controls_by_device_independent_id.end()) {
		Led* rude_solo = dynamic_cast<Led*> (x->second);
		if (rude_solo) {
			surface->write (rude_solo->set_state (active ? flashing : off));
		}
	}
}

void
MackieControlProtocol::notify_remote_id_changed()
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}
	}

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
	update_global_button (Button::Loop, session->get_play_loop());
}

void
MackieControlProtocol::notify_transport_state_changed()
{
	if (!_device_info.has_global_controls()) {
		return;
	}

	// switch various play and stop buttons on / off
	update_global_button (Button::Loop, session->get_play_loop());
	update_global_button (Button::Play, session->transport_speed() == 1.0);
	update_global_button (Button::Stop, session->transport_stopped ());
	update_global_button (Button::Rewind, session->transport_speed() < 0.0);
	update_global_button (Button::Ffwd, session->transport_speed() > 1.0);

	notify_metering_state_changed ();
}

void
MackieControlProtocol::notify_metering_state_changed()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->notify_metering_state_changed ();
	}
}

void
MackieControlProtocol::notify_record_state_changed ()
{
	if (!_device_info.has_global_controls()) {
		return;
	}

	boost::shared_ptr<Surface> surface;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		if (surfaces.empty()) {
			return;
		}
		surface = _master_surface;
	}

	/* rec is a tristate */

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (Button::Record);
	if (x != surface->controls_by_device_independent_id.end()) {
		Button * rec = dynamic_cast<Button*> (x->second);
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

			surface->write (rec->set_state (ls));
		}
	}
}

list<boost::shared_ptr<ARDOUR::Bundle> >
MackieControlProtocol::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_input_bundle) {
		b.push_back (_input_bundle);
		b.push_back (_output_bundle);
	}

	return b;
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

void
MackieControlProtocol::update_led (Surface& surface, Button& button, Mackie::LedState ls)
{
	if (ls != none) {
		surface.port().write (button.set_state (ls));
	}
}

void
MackieControlProtocol::build_button_map ()
{
	/* this maps our device-independent button codes to the methods that handle them.
	 */

#define DEFINE_BUTTON_HANDLER(b,p,r) button_map.insert (pair<Button::ID,ButtonHandlers> ((b), ButtonHandlers ((p),(r))));

	DEFINE_BUTTON_HANDLER (Button::Track, &MackieControlProtocol::track_press, &MackieControlProtocol::track_release);
	DEFINE_BUTTON_HANDLER (Button::Send, &MackieControlProtocol::send_press, &MackieControlProtocol::send_release);
	DEFINE_BUTTON_HANDLER (Button::Pan, &MackieControlProtocol::pan_press, &MackieControlProtocol::pan_release);
	DEFINE_BUTTON_HANDLER (Button::Plugin, &MackieControlProtocol::plugin_press, &MackieControlProtocol::plugin_release);
	DEFINE_BUTTON_HANDLER (Button::Eq, &MackieControlProtocol::eq_press, &MackieControlProtocol::eq_release);
	DEFINE_BUTTON_HANDLER (Button::Dyn, &MackieControlProtocol::dyn_press, &MackieControlProtocol::dyn_release);
	DEFINE_BUTTON_HANDLER (Button::Left, &MackieControlProtocol::left_press, &MackieControlProtocol::left_release);
	DEFINE_BUTTON_HANDLER (Button::Right, &MackieControlProtocol::right_press, &MackieControlProtocol::right_release);
	DEFINE_BUTTON_HANDLER (Button::ChannelLeft, &MackieControlProtocol::channel_left_press, &MackieControlProtocol::channel_left_release);
	DEFINE_BUTTON_HANDLER (Button::ChannelRight, &MackieControlProtocol::channel_right_press, &MackieControlProtocol::channel_right_release);
	DEFINE_BUTTON_HANDLER (Button::Flip, &MackieControlProtocol::flip_press, &MackieControlProtocol::flip_release);
	DEFINE_BUTTON_HANDLER (Button::View, &MackieControlProtocol::view_press, &MackieControlProtocol::view_release);
	DEFINE_BUTTON_HANDLER (Button::NameValue, &MackieControlProtocol::name_value_press, &MackieControlProtocol::name_value_release);
	DEFINE_BUTTON_HANDLER (Button::TimecodeBeats, &MackieControlProtocol::timecode_beats_press, &MackieControlProtocol::timecode_beats_release);
	DEFINE_BUTTON_HANDLER (Button::F1, &MackieControlProtocol::F1_press, &MackieControlProtocol::F1_release);
	DEFINE_BUTTON_HANDLER (Button::F2, &MackieControlProtocol::F2_press, &MackieControlProtocol::F2_release);
	DEFINE_BUTTON_HANDLER (Button::F3, &MackieControlProtocol::F3_press, &MackieControlProtocol::F3_release);
	DEFINE_BUTTON_HANDLER (Button::F4, &MackieControlProtocol::F4_press, &MackieControlProtocol::F4_release);
	DEFINE_BUTTON_HANDLER (Button::F5, &MackieControlProtocol::F5_press, &MackieControlProtocol::F5_release);
	DEFINE_BUTTON_HANDLER (Button::F6, &MackieControlProtocol::F6_press, &MackieControlProtocol::F6_release);
	DEFINE_BUTTON_HANDLER (Button::F7, &MackieControlProtocol::F7_press, &MackieControlProtocol::F7_release);
	DEFINE_BUTTON_HANDLER (Button::F8, &MackieControlProtocol::F8_press, &MackieControlProtocol::F8_release);
	DEFINE_BUTTON_HANDLER (Button::MidiTracks, &MackieControlProtocol::miditracks_press, &MackieControlProtocol::miditracks_release);
	DEFINE_BUTTON_HANDLER (Button::Inputs, &MackieControlProtocol::inputs_press, &MackieControlProtocol::inputs_release);
	DEFINE_BUTTON_HANDLER (Button::AudioTracks, &MackieControlProtocol::audiotracks_press, &MackieControlProtocol::audiotracks_release);
	DEFINE_BUTTON_HANDLER (Button::AudioInstruments, &MackieControlProtocol::audioinstruments_press, &MackieControlProtocol::audioinstruments_release);
	DEFINE_BUTTON_HANDLER (Button::Aux, &MackieControlProtocol::aux_press, &MackieControlProtocol::aux_release);
	DEFINE_BUTTON_HANDLER (Button::Busses, &MackieControlProtocol::busses_press, &MackieControlProtocol::busses_release);
	DEFINE_BUTTON_HANDLER (Button::Outputs, &MackieControlProtocol::outputs_press, &MackieControlProtocol::outputs_release);
	DEFINE_BUTTON_HANDLER (Button::User, &MackieControlProtocol::user_press, &MackieControlProtocol::user_release);
	DEFINE_BUTTON_HANDLER (Button::Shift, &MackieControlProtocol::shift_press, &MackieControlProtocol::shift_release);
	DEFINE_BUTTON_HANDLER (Button::Option, &MackieControlProtocol::option_press, &MackieControlProtocol::option_release);
	DEFINE_BUTTON_HANDLER (Button::Ctrl, &MackieControlProtocol::control_press, &MackieControlProtocol::control_release);
	DEFINE_BUTTON_HANDLER (Button::CmdAlt, &MackieControlProtocol::cmd_alt_press, &MackieControlProtocol::cmd_alt_release);
	DEFINE_BUTTON_HANDLER (Button::Read, &MackieControlProtocol::read_press, &MackieControlProtocol::read_release);
	DEFINE_BUTTON_HANDLER (Button::Write, &MackieControlProtocol::write_press, &MackieControlProtocol::write_release);
	DEFINE_BUTTON_HANDLER (Button::Trim, &MackieControlProtocol::trim_press, &MackieControlProtocol::trim_release);
	DEFINE_BUTTON_HANDLER (Button::Touch, &MackieControlProtocol::touch_press, &MackieControlProtocol::touch_release);
	DEFINE_BUTTON_HANDLER (Button::Latch, &MackieControlProtocol::latch_press, &MackieControlProtocol::latch_release);
	DEFINE_BUTTON_HANDLER (Button::Grp, &MackieControlProtocol::grp_press, &MackieControlProtocol::grp_release);
	DEFINE_BUTTON_HANDLER (Button::Save, &MackieControlProtocol::save_press, &MackieControlProtocol::save_release);
	DEFINE_BUTTON_HANDLER (Button::Undo, &MackieControlProtocol::undo_press, &MackieControlProtocol::undo_release);
	DEFINE_BUTTON_HANDLER (Button::Cancel, &MackieControlProtocol::cancel_press, &MackieControlProtocol::cancel_release);
	DEFINE_BUTTON_HANDLER (Button::Enter, &MackieControlProtocol::enter_press, &MackieControlProtocol::enter_release);
	DEFINE_BUTTON_HANDLER (Button::Marker, &MackieControlProtocol::marker_press, &MackieControlProtocol::marker_release);
	DEFINE_BUTTON_HANDLER (Button::Nudge, &MackieControlProtocol::nudge_press, &MackieControlProtocol::nudge_release);
	DEFINE_BUTTON_HANDLER (Button::Loop, &MackieControlProtocol::loop_press, &MackieControlProtocol::loop_release);
	DEFINE_BUTTON_HANDLER (Button::Drop, &MackieControlProtocol::drop_press, &MackieControlProtocol::drop_release);
	DEFINE_BUTTON_HANDLER (Button::Replace, &MackieControlProtocol::replace_press, &MackieControlProtocol::replace_release);
	DEFINE_BUTTON_HANDLER (Button::Click, &MackieControlProtocol::click_press, &MackieControlProtocol::click_release);
	DEFINE_BUTTON_HANDLER (Button::ClearSolo, &MackieControlProtocol::clearsolo_press, &MackieControlProtocol::clearsolo_release);
	DEFINE_BUTTON_HANDLER (Button::Rewind, &MackieControlProtocol::rewind_press, &MackieControlProtocol::rewind_release);
	DEFINE_BUTTON_HANDLER (Button::Ffwd, &MackieControlProtocol::ffwd_press, &MackieControlProtocol::ffwd_release);
	DEFINE_BUTTON_HANDLER (Button::Stop, &MackieControlProtocol::stop_press, &MackieControlProtocol::stop_release);
	DEFINE_BUTTON_HANDLER (Button::Play, &MackieControlProtocol::play_press, &MackieControlProtocol::play_release);
	DEFINE_BUTTON_HANDLER (Button::Record, &MackieControlProtocol::record_press, &MackieControlProtocol::record_release);
	DEFINE_BUTTON_HANDLER (Button::CursorUp, &MackieControlProtocol::cursor_up_press, &MackieControlProtocol::cursor_up_release);
	DEFINE_BUTTON_HANDLER (Button::CursorDown, &MackieControlProtocol::cursor_down_press, &MackieControlProtocol::cursor_down_release);
	DEFINE_BUTTON_HANDLER (Button::CursorLeft, &MackieControlProtocol::cursor_left_press, &MackieControlProtocol::cursor_left_release);
	DEFINE_BUTTON_HANDLER (Button::CursorRight, &MackieControlProtocol::cursor_right_press, &MackieControlProtocol::cursor_right_release);
	DEFINE_BUTTON_HANDLER (Button::Zoom, &MackieControlProtocol::zoom_press, &MackieControlProtocol::zoom_release);
	DEFINE_BUTTON_HANDLER (Button::Scrub, &MackieControlProtocol::scrub_press, &MackieControlProtocol::scrub_release);
	DEFINE_BUTTON_HANDLER (Button::UserA, &MackieControlProtocol::user_a_press, &MackieControlProtocol::user_a_release);
	DEFINE_BUTTON_HANDLER (Button::UserB, &MackieControlProtocol::user_b_press, &MackieControlProtocol::user_b_release);
	DEFINE_BUTTON_HANDLER (Button::MasterFaderTouch, &MackieControlProtocol::master_fader_touch_press, &MackieControlProtocol::master_fader_touch_release);

}

void
MackieControlProtocol::handle_button_event (Surface& surface, Button& button, ButtonState bs)
{
	Button::ID button_id = button.bid();

	if  (bs != press && bs != release) {
		update_led (surface, button, none);
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Handling %1 for button %2 (%3)\n", (bs == press ? "press" : "release"), button.id(),
							   Button::id_to_name (button.bid())));

	/* check profile first */

	string action = _device_profile.get_button_action (button.bid(), _modifier_state);

	if (!action.empty()) {

		if (action.find ('/') != string::npos) { /* good chance that this is really an action */

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Looked up action for button %1 with modifier %2, got [%3]\n",
									   button.bid(), _modifier_state, action));

			/* if there is a bound action for this button, and this is a press event,
			   carry out the action. If its a release event, do nothing since we
			   don't bind to them at all but don't want any other handling to
			   occur either.
			*/
			if (bs == press) {
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("executing action %1\n", action));
				access_action (action);
			}

			return;

		} else {

			/* "action" is more likely to be a button name. We use this to
			 * allow remapping buttons to different (builtin) functionality
			 * associated with an existing button. This is similar to the
			 * way that (for example) Nuendo moves the "Shift" function to
			 * the "Enter" key of the MCU Pro.
			 */

			int bid = Button::name_to_id (action);

			if (bid < 0) {
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("apparent button name %1 not found\n", action));
				return;
			}

			button_id = (Button::ID) bid;
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handling button %1 as if it was %2 (%3)\n", Button::id_to_name (button.bid()), button_id, Button::id_to_name (button_id)));
		}
	}

	/* lookup using the device-INDEPENDENT button ID */

	ButtonMap::iterator b = button_map.find (button_id);

	if (b != button_map.end()) {

		ButtonHandlers& bh (b->second);

		switch  (bs) {
		case press:
			surface.write (button.set_state ((this->*(bh.press)) (button)));
			break;
		case release:
			surface.write (button.set_state ((this->*(bh.release)) (button)));
			break;
		default:
			break;
		}
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("no button handlers for button ID %1 (device ID %2)\n",
								   button.bid(), button.id()));
		error << string_compose ("no button handlers for button ID %1 (device ID %2)\n",
					 button.bid(), button.id()) << endmsg;
	}
}

bool
MackieControlProtocol::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		DEBUG_TRACE (DEBUG::MackieControl, "MIDI port closed\n");
		return false;
	}

	if (ioc & IO_IN) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("something happend on  %1\n", port->name()));

		/* Devices using regular JACK MIDI ports will need to have
		   the x-thread FIFO drained to avoid burning endless CPU.

		   Devices using ipMIDI have port->selectable() as the same
		   file descriptor that data arrives on, so doing this
		   for them will simply throw all incoming data away.
		*/

		if (!_device_info.uses_ipmidi()) {
			AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*>(port);
			if (asp) {
				asp->clear ();
			}
		}

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("data available on %1\n", port->name()));
		framepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}

void
MackieControlProtocol::clear_ports ()
{
	if (_input_bundle) {
		_input_bundle->remove_channels ();
		_output_bundle->remove_channels ();
	}
}

void
MackieControlProtocol::set_view_mode (ViewMode m)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	_view_mode = m;

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->update_view_mode_display ();
	}

}

void
MackieControlProtocol::set_flip_mode (FlipMode fm)
{
	if (_flip_mode != fm) {
		if (fm == Normal) {
			update_global_button (Button::Flip, off);
		} else {
			update_global_button (Button::Flip, on);
		}

		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		_flip_mode = fm;

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->update_flip_mode_display ();
		}
	}
}

void
MackieControlProtocol::set_pot_mode (PotMode m)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	_pot_mode = m;

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->update_potmode ();

	}

}

void
MackieControlProtocol::set_master_on_surface_strip (uint32_t surface, uint32_t strip_number)
{
	force_special_route_to_strip (session->master_out(), surface, strip_number);
}

void
MackieControlProtocol::set_monitor_on_surface_strip (uint32_t surface, uint32_t strip_number)
{
	force_special_route_to_strip (session->monitor_out(), surface, strip_number);
}

void
MackieControlProtocol::force_special_route_to_strip (boost::shared_ptr<Route> r, uint32_t surface, uint32_t strip_number)
{
	if (!r) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s)->number() == surface) {
			Strip* strip = (*s)->nth_strip (strip_number);
			if (strip) {
				strip->set_route (session->master_out());
				strip->lock_controls ();
			}
		}
	}
}

void
MackieControlProtocol::gui_track_selection_changed (ARDOUR::RouteNotificationListPtr rl, bool save_list)
{
	_gui_track_selection_changed (rl.get(), save_list);
}

void
MackieControlProtocol::_gui_track_selection_changed (ARDOUR::RouteNotificationList* rl, bool save_list)
{
	/* We need to keep a list of the most recently selected routes around,
	   but we are not allowed to keep shared_ptr<Route> unless we want to
	   handle the complexities of route deletion. So instead, the GUI sends
	   us a notification using weak_ptr<Route>, which we keep a copy
	   of. For efficiency's sake, however, we convert the weak_ptr's into
	   shared_ptr<Route> before passing them to however many surfaces (and
	   thus strips) that we have.
	*/

	StrongRouteNotificationList srl;

	for (ARDOUR::RouteNotificationList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<ARDOUR::Route> r = (*i).lock();
		if (r) {
			srl.push_back (r);
		}
	}

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->gui_selection_changed (srl);
		}
	}

	if (save_list) {
		_last_selected_routes = *rl;
	}
}

framepos_t
MackieControlProtocol::transport_frame() const
{
	return session->transport_frame();
}

void
MackieControlProtocol::add_down_select_button (int surface, int strip)
{
	_down_select_buttons.insert ((surface<<8)|(strip&0xf));
}

void
MackieControlProtocol::remove_down_select_button (int surface, int strip)
{
	DownButtonList::iterator x = find (_down_select_buttons.begin(), _down_select_buttons.end(), (uint32_t) (surface<<8)|(strip&0xf));
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("removing surface %1 strip %2 from down select buttons\n", surface, strip));
	if (x != _down_select_buttons.end()) {
		_down_select_buttons.erase (x);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface %1 strip %2 not found in down select buttons\n",
								   surface, strip));
	}
}

void
MackieControlProtocol::select_range ()
{
	RouteList routes;

	pull_route_range (_down_select_buttons, routes);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("select range: found %1 routes\n", routes.size()));

	if (!routes.empty()) {
		for (RouteList::iterator r = routes.begin(); r != routes.end(); ++r) {

			if (main_modifier_state() == MODIFIER_CONTROL) {
				ToggleRouteSelection ((*r)->remote_control_id ());
			} else {
				if (r == routes.begin()) {
					SetRouteSelection ((*r)->remote_control_id());
				} else {
					AddRouteToSelection ((*r)->remote_control_id());
				}
			}
		}
	}
}

void
MackieControlProtocol::add_down_button (AutomationType a, int surface, int strip)
{
	DownButtonMap::iterator m = _down_buttons.find (a);

	if (m == _down_buttons.end()) {
		_down_buttons[a] = DownButtonList();
	}

	_down_buttons[a].insert ((surface<<8)|(strip&0xf));
}

void
MackieControlProtocol::remove_down_button (AutomationType a, int surface, int strip)
{
	DownButtonMap::iterator m = _down_buttons.find (a);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("removing surface %1 strip %2 from down buttons for %3\n", surface, strip, (int) a));

	if (m == _down_buttons.end()) {
		return;
	}

	DownButtonList& l (m->second);
	DownButtonList::iterator x = find (l.begin(), l.end(), (surface<<8)|(strip&0xf));

	if (x != l.end()) {
		l.erase (x);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface %1 strip %2 not found in down buttons for %3\n",
								   surface, strip, (int) a));
	}
}

MackieControlProtocol::ControlList
MackieControlProtocol::down_controls (AutomationType p)
{
	ControlList controls;
	RouteList routes;

	DownButtonMap::iterator m = _down_buttons.find (p);

	if (m == _down_buttons.end()) {
		return controls;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("looking for down buttons for %1, got %2\n",
							   p, m->second.size()));

	pull_route_range (m->second, routes);

	switch (p) {
	case GainAutomation:
		for (RouteList::iterator r = routes.begin(); r != routes.end(); ++r) {
			controls.push_back ((*r)->gain_control());
		}
		break;
	case SoloAutomation:
		for (RouteList::iterator r = routes.begin(); r != routes.end(); ++r) {
			controls.push_back ((*r)->solo_control());
		}
		break;
	case MuteAutomation:
		for (RouteList::iterator r = routes.begin(); r != routes.end(); ++r) {
			controls.push_back ((*r)->mute_control());
		}
		break;
	case RecEnableAutomation:
		for (RouteList::iterator r = routes.begin(); r != routes.end(); ++r) {
			boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<Track> (*r);
			if (trk) {
				controls.push_back (trk->rec_enable_control());
			}
		}
		break;
	default:
		break;
	}

	return controls;

}

struct ButtonRangeSorter {
    bool operator() (const uint32_t& a, const uint32_t& b) {
	    return (a>>8) < (b>>8) // a.surface < b.surface
		    ||
		    ((a>>8) == (b>>8) && (a&0xf) < (b&0xf)); // a.surface == b.surface && a.strip < b.strip
    }
};

void
MackieControlProtocol::pull_route_range (DownButtonList& down, RouteList& selected)
{
	ButtonRangeSorter cmp;

	if (down.empty()) {
		return;
	}

	list<uint32_t> ldown;
	ldown.insert (ldown.end(), down.begin(), down.end());
	ldown.sort (cmp);

	uint32_t first = ldown.front();
	uint32_t last = ldown.back ();

	uint32_t first_surface = first>>8;
	uint32_t first_strip = first&0xf;

	uint32_t last_surface = last>>8;
	uint32_t last_strip = last&0xf;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("PRR %5 in list %1.%2 - %3.%4\n", first_surface, first_strip, last_surface, last_strip,
							   down.size()));

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {

		if ((*s)->number() >= first_surface && (*s)->number() <= last_surface) {

			uint32_t fs;
			uint32_t ls;

			if ((*s)->number() == first_surface) {
				fs = first_strip;
			} else {
				fs = 0;
			}

			if ((*s)->number() == last_surface) {
				ls = last_strip;
				ls += 1;
			} else {
				ls = (*s)->n_strips ();
			}

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("adding strips for surface %1 (%2 .. %3)\n",
									   (*s)->number(), fs, ls));

			for (uint32_t n = fs; n < ls; ++n) {
				boost::shared_ptr<Route> r = (*s)->nth_strip (n)->route();
				if (r) {
					selected.push_back (r);
				}
			}
		}
	}
}

void
MackieControlProtocol::set_ipmidi_base (int16_t portnum)
{
	/* this will not be saved without a session save, so .. */

	session->set_dirty ();

	_ipmidi_base = portnum;

	/* if the current device uses ipMIDI we need
	   to restart.
	*/

	if (active() && _device_info.uses_ipmidi()) {
		needs_ipmidi_restart = true;
	}
}

int
MackieControlProtocol::ipmidi_restart ()
{
	clear_surfaces ();
	if (create_surfaces ()) {
		return -1;
	}
	switch_banks (_current_initial_bank, true);
	needs_ipmidi_restart = false;
	return 0;
}

void
MackieControlProtocol::clear_surfaces ()
{
	clear_ports ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		_master_surface.reset ();
		surfaces.clear ();
	}
}

void
MackieControlProtocol::set_touch_sensitivity (int sensitivity)
{
	sensitivity = min (9, sensitivity);
	sensitivity = max (0, sensitivity);

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->set_touch_sensitivity (sensitivity);
	}
}

void
MackieControlProtocol::recalibrate_faders ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->recalibrate_faders ();
	}
}

void
MackieControlProtocol::toggle_backlight ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->toggle_backlight ();
	}
}

boost::shared_ptr<Surface>
MackieControlProtocol::get_surface_by_raw_pointer (void* ptr) const
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s).get() == (Surface*) ptr) {
			return *s;
		}
	}

	return boost::shared_ptr<Surface> ();
}

boost::shared_ptr<Surface>
MackieControlProtocol::nth_surface (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s, --n) {
		if (n == 0) {
			return *s;
		}
	}

	return boost::shared_ptr<Surface> ();
}

void
MackieControlProtocol::connection_handler (boost::weak_ptr<ARDOUR::Port> wp1, std::string name1, boost::weak_ptr<ARDOUR::Port> wp2, std::string name2, bool yn)
{
	Surfaces scopy;
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		scopy = surfaces;
	}

	for (Surfaces::const_iterator s = scopy.begin(); s != scopy.end(); ++s) {
		if ((*s)->connection_handler (wp1, name1, wp2, name2, yn)) {
			ConnectionChange (*s);
			break;
		}
	}
}
