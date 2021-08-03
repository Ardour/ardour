/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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
#include <glibmm/miscutils.h>

#include "midi++/types.h"
#include "midi++/port.h"
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/memento_command.h"
#include "pbd/convert.h"

#include "ardour/audio_track.h"
#include "ardour/automation_control.h"
#include "ardour/async_midi_port.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/location.h"
#include "ardour/meter.h"
#include "ardour/midi_track.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/profile.h"
#include "ardour/record_enable_control.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/track.h"
#include "ardour/types.h"
#include "ardour/audioengine.h"
#include "ardour/vca_manager.h"

#include "us2400_control_protocol.h"

#include "midi_byte_array.h"
#include "us2400_control_exception.h"
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
using namespace US2400;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

const int US2400Protocol::MODIFIER_OPTION = 0x1;
const int US2400Protocol::MODIFIER_CONTROL = 0x2;
const int US2400Protocol::MODIFIER_SHIFT = 0x4;
const int US2400Protocol::MODIFIER_CMDALT = 0x8;
const int US2400Protocol::MODIFIER_ZOOM = 0x10;
const int US2400Protocol::MODIFIER_SCRUB = 0x20;
const int US2400Protocol::MODIFIER_MARKER = 0x40;
const int US2400Protocol::MODIFIER_DROP = 0x80; //US2400 Drop as a modifier for In/out
const int US2400Protocol::MAIN_MODIFIER_MASK = (US2400Protocol::MODIFIER_OPTION|
						       US2400Protocol::MODIFIER_CONTROL|
						       US2400Protocol::MODIFIER_SHIFT|
						       US2400Protocol::MODIFIER_CMDALT);

US2400Protocol* US2400Protocol::_instance = 0;

bool US2400Protocol::probe()
{
	return true;
}

US2400Protocol::US2400Protocol (Session& session)
	: ControlProtocol (session, X_("Tascam US-2400"))
	, AbstractUI<US2400ControlUIRequest> (name())
	, _current_initial_bank (0)
	, _sample_last (0)
	, _timecode_type (ARDOUR::AnyTime::BBT)
	, _gui (0)
	, _scrub_mode (false)
	, _view_mode (Mixer)
	, _subview_mode (None)
	, _modifier_state (0)
	, _metering_active (true)
	, _initialized (false)
	, configuration_state (0)
	, state_version (0)
	, marker_modifier_consumed_by_button (false)
	, nudge_modifier_consumed_by_button (false)
{
	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::US2400Protocol\n");

//	DeviceInfo::reload_device_info ();
	DeviceProfile::reload_device_profiles ();

	for (int i = 0; i < 9; i++) {
		_last_bank[i] = 0;
	}

	PresentationInfo::Change.connect (gui_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_presentation_info_changed, this, _1), this);

	_instance = this;

	build_button_map ();
}

US2400Protocol::~US2400Protocol()
{
	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::~US2400Protocol init\n");

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		(*si)->reset ();
	}

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::~US2400Protocol drop_connections ()\n");
	drop_connections ();

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::~US2400Protocol tear_down_gui ()\n");
	tear_down_gui ();

	delete configuration_state;

	/* stop event loop */
	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::~US2400Protocol BaseUI::quit ()\n");
	BaseUI::quit ();

	try {
		DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::~US2400Protocol close()\n");
		close();
	}
	catch (exception & e) {
		cout << "~US2400Protocol caught " << e.what() << endl;
	}
	catch (...) {
		cout << "~US2400Protocol caught unknown" << endl;
	}

	_instance = 0;

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::~US2400Protocol done\n");
}

void
US2400Protocol::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

// go to the previous track.
void
US2400Protocol::prev_track()
{
	if (_current_initial_bank >= 1) {
		switch_banks (_current_initial_bank - 1);
	}
}

// go to the next track.
void
US2400Protocol::next_track()
{
	Sorted sorted = get_sorted_stripables();
	if (_current_initial_bank + n_strips() < sorted.size()) {
		switch_banks (_current_initial_bank + 1);
	}
}

bool
US2400Protocol::stripable_is_locked_to_strip (boost::shared_ptr<Stripable> r) const
{
	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		if ((*si)->stripable_is_locked_to_strip (r)) {
			return true;
		}
	}
	return false;
}

#ifdef MIXBUS
struct StripableByMixbusOrder
{
	bool operator () (const boost::shared_ptr<Stripable> & a, const boost::shared_ptr<Stripable> & b) const
	{
		return a->mixbus() < b->mixbus();
	}

	bool operator () (const Stripable & a, const Stripable & b) const
	{
		return a.mixbus() < b.mixbus();
	}

	bool operator () (const Stripable * a, const Stripable * b) const
	{
		return a->mixbus() < b->mixbus();
	}
};
#endif

// predicate for sort call in get_sorted_stripables
struct StripableByPresentationOrder
{
	bool operator () (const boost::shared_ptr<Stripable> & a, const boost::shared_ptr<Stripable> & b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
	}

	bool operator () (const Stripable & a, const Stripable & b) const
	{
		return a.presentation_info().order() < b.presentation_info().order();
	}

	bool operator () (const Stripable * a, const Stripable * b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
	}
};

US2400Protocol::Sorted
US2400Protocol::get_sorted_stripables()
{
	Sorted sorted;

	// fetch all stripables
	StripableList stripables;

	session->get_stripables (stripables);

	// sort in presentation order, and exclude master, control and hidden stripables
	// and any stripables that are already set.

	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> s = *it;

		if (s->presentation_info().special()) {
			continue;
		}

		/* don't include locked routes */

		if (stripable_is_locked_to_strip (s)) {
			continue;
		}

		switch (_view_mode) {
		case Mixer:
#ifdef MIXBUS
			if (!s->presentation_info().hidden() && !s->mixbus()) {
#else
			if (is_track(s) && !s->presentation_info().hidden()) {
#endif
				sorted.push_back (s);
			}
			break;
		case Busses:
#ifdef MIXBUS
			if (s->mixbus()) {
				sorted.push_back (s);
			}
			break;
#else
			if (!is_track(s) && !s->presentation_info().hidden()) {
				sorted.push_back (s);
			}
			break;
#endif
		}
	}

#ifdef MIXBUS
	if (_view_mode == Busses) {
		sort (sorted.begin(), sorted.end(), StripableByMixbusOrder());
	} else {
		sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());
	}
#else
	sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());
#endif
	return sorted;
}

void
US2400Protocol::refresh_current_bank()
{
	switch_banks (_current_initial_bank, true);
}

uint32_t
US2400Protocol::n_strips (bool with_locked_strips) const
{
	uint32_t strip_count = 0;

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		strip_count += (*si)->n_strips (with_locked_strips);
	}

	return strip_count;
}

int
US2400Protocol::switch_banks (uint32_t initial, bool force)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("switch banking to start at %1 force ? %2 current = %3\n", initial, force, _current_initial_bank));

	if (initial == _current_initial_bank && !force) {
		/* everything is as it should be */
		return 0;
	}

	Sorted sorted = get_sorted_stripables();
	uint32_t strip_cnt = n_strips (false); // do not include locked strips
					       // in this count

	if (initial >= sorted.size() && !force) {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("bank target %1 exceeds route range %2\n",
		                                                   _current_initial_bank, sorted.size()));
		/* too high, we can't get there */
		return -1;
	}

	if (sorted.size() <= strip_cnt && _current_initial_bank == 0 && !force) {
		/* no banking - not enough stripables to fill all strips and we're
		 * not at the first one.
		 */
		DEBUG_TRACE (DEBUG::US2400, string_compose ("less routes (%1) than strips (%2) and we're at the end already (%3)\n",
		                                                   sorted.size(), strip_cnt, _current_initial_bank));
		return -1;
	}

	_current_initial_bank = initial;

	// Map current bank of stripables onto each surface(+strip)

	if (_current_initial_bank < sorted.size()) {

		DEBUG_TRACE (DEBUG::US2400, string_compose ("switch to %1, %2, available stripables %3 on %4 surfaces\n",
								   _current_initial_bank, strip_cnt, sorted.size(),
								   surfaces.size()));

		// link stripables to strips

		Sorted::iterator r = sorted.begin() + _current_initial_bank;

		for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			vector<boost::shared_ptr<Stripable> > stripables;
			uint32_t added = 0;

			DEBUG_TRACE (DEBUG::US2400, string_compose ("surface has %1 unlocked strips\n", (*si)->n_strips (false)));

			for (; r != sorted.end() && added < (*si)->n_strips (false); ++r, ++added) {
				stripables.push_back (*r);
			}

			DEBUG_TRACE (DEBUG::US2400, string_compose ("give surface #%1 %2 stripables\n", (*si)->number(), stripables.size()));

			(*si)->map_stripables (stripables);
		}

	} else {
		/* all strips need to be reset */
		DEBUG_TRACE (DEBUG::US2400, string_compose ("clear all strips, bank target %1  is outside route range %2\n",
		                                                   _current_initial_bank, sorted.size()));
		for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			vector<boost::shared_ptr<Stripable> > stripables;
			/* pass in an empty stripables list, so that all strips will be reset */
			(*si)->map_stripables (stripables);
		}
		return -1;
	}

	/* current bank has not been saved */
	session->set_dirty();

	return 0;
}

int
US2400Protocol::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose("US2400Protocol::set_active init with yn: '%1'\n", yn));

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

		/* set up periodic task for timecode display and metering and automation
		 */

		Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (10); // milliseconds
		periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &US2400Protocol::periodic));
		periodic_timeout->attach (main_loop()->get_context());

		/* periodic task used to update strip displays */

		Glib::RefPtr<Glib::TimeoutSource> redisplay_timeout = Glib::TimeoutSource::create (10); // milliseconds
		redisplay_connection = redisplay_timeout->connect (sigc::mem_fun (*this, &US2400Protocol::redisplay));
		redisplay_timeout->attach (main_loop()->get_context());

	} else {

		BaseUI::quit ();
		close ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::US2400, string_compose("US2400Protocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

bool
US2400Protocol::hui_heartbeat ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->hui_heartbeat ();
	}

	return true;
}

bool
US2400Protocol::periodic ()
{
	if (!active()) {
		return false;
	}

	if (!_initialized) {
		initialize();
	}

	PBD::microseconds_t now_usecs = PBD::get_microseconds ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->periodic (now_usecs);
		}
	}

	return true;
}

bool
US2400Protocol::redisplay ()
{
	return true;
}

void
US2400Protocol::update_timecode_beats_led()
{
}

void
US2400Protocol::update_global_button (int id, LedState ls)
{
	boost::shared_ptr<Surface> surface;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}

		if (!_device_info.has_global_controls()) {
			return;
		}
		// surface needs to be master surface
		surface = _master_surface;
	}

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (id);
	if (x != surface->controls_by_device_independent_id.end()) {
		Button * button = dynamic_cast<Button*> (x->second);
		surface->write (button->set_state (ls));
	} else {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("Button %1 not found\n", id));
	}
}

void
US2400Protocol::update_global_led (int id, LedState ls)
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
		DEBUG_TRACE (DEBUG::US2400, "Writing LedState\n");
		surface->write (led->set_state (ls));
	} else {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("Led %1 not found\n", id));
	}
}

void
US2400Protocol::device_ready ()
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("device ready init (active=%1)\n", active()));

	//this gets called every time a new surface appears; we have to do this to reset the banking etc
	//particularly when the user is setting it up the first time; we can't guarantee the order that they will be connected
	
	update_surfaces ();

	set_subview_mode (US2400Protocol::None, first_selected_stripable());
}

// send messages to surface to set controls to correct values
void
US2400Protocol::update_surfaces()
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("US2400Protocol::update_surfaces() init (active=%1)\n", active()));
	if (!active()) {
		return;
	}

	// do the initial bank switch to connect signals
	// _current_initial_bank is initialised by set_state
	(void) switch_banks (_current_initial_bank, true);

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::update_surfaces() finished\n");
}

void
US2400Protocol::initialize()
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}

		if (!_master_surface->active ()) {
			return;
		}

	}

	update_global_button (Button::Send, on);
	update_global_button (Button::Send, off);

	update_global_button (Button::Scrub, on);
	update_global_button (Button::Scrub, off);

	notify_solo_active_changed(false);

	update_global_button (Button::Pan, off);
	update_global_button (Button::Pan, on);

	update_global_button (Button::Flip, on);
	update_global_button (Button::Flip, off);

	update_global_button (Button::MstrSelect, on);
	update_global_button (Button::MstrSelect, off);

	notify_transport_state_changed();

	_initialized = true;
}

void
US2400Protocol::connect_session_signals()
{
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_routes_added, this, _1), this);
	// receive VCAs added
	session->vca_manager().VCAAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_vca_added, this, _1), this);

	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_record_state_changed, this), this);
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::notify_solo_active_changed, this, _1), this);

	// make sure remote id changed signals reach here
	// see also notify_stripable_added
	Sorted sorted = get_sorted_stripables();
}

void
US2400Protocol::set_profile (const string& profile_name)
{
	map<string,DeviceProfile>::iterator d = DeviceProfile::device_profiles.find (profile_name);

	if (d == DeviceProfile::device_profiles.end()) {
		_device_profile = DeviceProfile (profile_name);
		return;
	}

	_device_profile = d->second;
}

int
US2400Protocol::set_device_info (const string& device_name)
{
return 0;
}

int
US2400Protocol::set_device (const string& device_name, bool force)
{
	if (device_name == device_info().name() && !force) {
		/* already using that device, nothing to do */
		return 0;
	}
	/* get state from the current setup, and make sure it is stored in
	   the configuration_states node so that if we switch back to this device,
	   we will have its state available.
	*/

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		if (!surfaces.empty()) {
			update_configuration_state ();
		}
	}

	if (set_device_info (device_name)) {
		return -1;
	}

	clear_surfaces ();
	port_connection.disconnect ();
	hui_connection.disconnect ();

	if (_device_info.device_type() == DeviceInfo::HUI) {
		Glib::RefPtr<Glib::TimeoutSource> hui_timeout = Glib::TimeoutSource::create (1000); // milliseconds
		hui_connection = hui_timeout->connect (sigc::mem_fun (*this, &US2400Protocol::hui_heartbeat));
		hui_timeout->attach (main_loop()->get_context());
	}

	/* notice that the handler for this will execute in our event
	   loop, not in the thread where the
	   PortConnectedOrDisconnected signal is emitted.
	*/
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&US2400Protocol::connection_handler, this, _1, _2, _3, _4, _5), this);

	if (create_surfaces ()) {
		return -1;
	}

	DeviceChanged ();

	return 0;
}

int
US2400Protocol::create_surfaces ()
{
	string device_name;
	surface_type_t stype = st_mcu; // type not yet determined

	DEBUG_TRACE (DEBUG::US2400, string_compose ("Create %1 surfaces for %2\n", 1 + _device_info.extenders(), _device_info.name()));

	_input_bundle.reset (new ARDOUR::Bundle (_("US2400 Control In"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("US2400 Control Out"), false));

	for (uint32_t n = 0; n < 1 + _device_info.extenders(); ++n) {
		bool is_master = false;

		if (n == _device_info.master_position()) {
			is_master = true;
		}

		device_name = string_compose (X_("US-2400 Control %1"), n+1);

		DEBUG_TRACE (DEBUG::US2400, string_compose ("Port Name for surface %1 is %2\n", n, device_name));

		boost::shared_ptr<Surface> surface;

		if (n ==0) {
			stype = st_mcu;
		} else if (n ==1) {
			stype = st_ext;   //ch8..16
		} else if (n ==2) {
			stype = st_ext;   //ch17..24
		} else if (n ==3) {
			stype = st_joy;   //joystick
		} else if (n ==4) {
			stype = st_knb;   //chan knobs ???
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
				XMLProperty const * prop = (*d)->property (X_("name"));
				if (prop && prop->value() == _device_info.name()) {
					this_device = *d;
					break;
				}
			}
			if (this_device) {
				XMLNode* snode = this_device->child (X_("Surfaces"));
				if (snode) {
					surface->set_state (*snode, state_version);
				}
			}
		}

		{
			Glib::Threads::Mutex::Lock lm (surfaces_lock);
			surfaces.push_back (surface);
		}

		if ( n <=3 ) {   //ports 5&6 are not really used by us2400

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

			asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &US2400Protocol::midi_input_handler), &input_port));
			asp->xthread().attach (main_loop()->get_context());

		}
	}

	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->port().reconnect ();
	}

	session->BundleAddedOrRemoved ();

	assert (_master_surface);

	return 0;
}

void
US2400Protocol::close()
{
	port_connection.disconnect ();
	session_connections.drop_connections ();
	stripable_connections.drop_connections ();
	periodic_connection.disconnect ();

	clear_surfaces();
}

/** Ensure that the configuration_state XML node contains an up-to-date
 *  copy of the state node the current device. If configuration_state already
 *  contains a state node for the device, it will deleted and replaced.
 */
void
US2400Protocol::update_configuration_state ()
{
	/* CALLER MUST HOLD SURFACES LOCK */

	if (!configuration_state) {
		configuration_state = new XMLNode (X_("Configurations"));
	}

	XMLNode* devnode = new XMLNode (X_("Configuration"));
	devnode->set_property (X_("name"), _device_info.name());

	configuration_state->remove_nodes_and_delete (X_("name"), _device_info.name());
	configuration_state->add_child_nocopy (*devnode);

	XMLNode* snode = new XMLNode (X_("Surfaces"));

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		snode->add_child_nocopy ((*s)->get_state());
	}

	devnode->add_child_nocopy (*snode);
}

XMLNode&
US2400Protocol::get_state()
{
	XMLNode& node (ControlProtocol::get_state());

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::get_state init\n");

	// add current bank
	node.set_property (X_("bank"), _current_initial_bank);

	node.set_property (X_("device-profile"), _device_profile.name());
	node.set_property (X_("device-name"), _device_info.name());

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		update_configuration_state ();
	}

	/* force a copy of the _surfaces_state node, because we want to retain ownership */
	node.add_child_copy (*configuration_state);

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::get_state done\n");

	return node;
}

bool
US2400Protocol::profile_exists (string const & name) const
{
	return DeviceProfile::device_profiles.find (name) != DeviceProfile::device_profiles.end();
}

int
US2400Protocol::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("US2400Protocol::set_state: active %1\n", active()));

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	uint32_t bank = 0;
	// fetch current bank
	node.get_property (X_("bank"), bank);

	std::string device_name;
	if (node.get_property (X_("device-name"), device_name)) {
		set_device_info (device_name);
	}

	std::string device_profile_name;
	if (node.get_property (X_("device-profile"), device_profile_name)) {
		if (device_profile_name.empty()) {
			string default_profile_name;

			/* start by looking for a user-edited profile for the current device name */

			default_profile_name = DeviceProfile::name_when_edited (_device_info.name());

			if (!profile_exists (default_profile_name)) {

				/* no user-edited profile for this device name, so try the user-edited default profile */

				default_profile_name = DeviceProfile::name_when_edited (DeviceProfile::default_profile_name);

				if (!profile_exists (default_profile_name)) {

					/* no user-edited version, so just try the device name */

					default_profile_name = _device_info.name();

					if (!profile_exists (default_profile_name)) {

						/* no generic device specific profile, just try the fixed default */
						default_profile_name = DeviceProfile::default_profile_name;
					}
				}
			}

			set_profile (default_profile_name);

		} else {
			if (profile_exists (device_profile_name)) {
				set_profile (device_profile_name);
			} else {
				set_profile (DeviceProfile::default_profile_name);
			}
		}
	}

	XMLNode* dnode = node.child (X_("Configurations"));

	delete configuration_state;
	configuration_state = 0;

	if (dnode) {
		configuration_state = new XMLNode (*dnode);
		state_version = version;
	}

	(void) switch_banks (bank, true);

	DEBUG_TRACE (DEBUG::US2400, "US2400Protocol::set_state done\n");

	return 0;
}

///////////////////////////////////////////
// Session signals
///////////////////////////////////////////

void US2400Protocol::notify_parameter_changed (std::string const & p)
{
}

void
US2400Protocol::notify_stripable_removed ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->master_monitor_may_have_changed ();
	}
}

void
US2400Protocol::notify_vca_added (ARDOUR::VCAList& vl)
{
	refresh_current_bank ();
}

// RouteList is the set of Routes that have just been added
void
US2400Protocol::notify_routes_added (ARDOUR::RouteList & rl)
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}
	}

	/* special case: single route, and it is the monitor or master out */

	if (rl.size() == 1 && (rl.front()->is_monitor() || rl.front()->is_master())) {
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->master_monitor_may_have_changed ();
		}
	}

	// currently assigned banks are less than the full set of
	// strips, so activate the new strip now.

	refresh_current_bank();

	// otherwise route added, but current bank needs no updating
}

void
US2400Protocol::notify_solo_active_changed (bool active)
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
US2400Protocol::notify_presentation_info_changed (PBD::PropertyChange const & what_changed)
{
	PBD::PropertyChange order_or_hidden;

	order_or_hidden.add (Properties::hidden);
	order_or_hidden.add (Properties::order);

	if (!what_changed.contains (order_or_hidden)) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}
	}

	refresh_current_bank();
}

///////////////////////////////////////////
// Transport signals
///////////////////////////////////////////

void
US2400Protocol::notify_loop_state_changed()
{
}

void
US2400Protocol::notify_transport_state_changed()
{
	if (!_device_info.has_global_controls()) {
		return;
	}

	// switch various play and stop buttons on / off
	update_global_button (Button::Play, play_button_onoff());
	update_global_button (Button::Stop, stop_button_onoff());
	update_global_button (Button::Rewind, rewind_button_onoff());
	update_global_button (Button::Ffwd, ffwd_button_onoff());

	// sometimes a return to start leaves time code at old time
	_timecode_last = string (10, ' ');

	notify_metering_state_changed ();
}

void
US2400Protocol::notify_metering_state_changed()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->notify_metering_state_changed ();
	}
}

void
US2400Protocol::notify_record_state_changed ()
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
				DEBUG_TRACE (DEBUG::US2400, "record state changed to disabled, LED off\n");
				ls = off;
				break;
			case Session::Recording:
				DEBUG_TRACE (DEBUG::US2400, "record state changed to recording, LED on\n");
				ls = on;
				break;
			case Session::Enabled:
				DEBUG_TRACE (DEBUG::US2400, "record state changed to enabled, LED flashing\n");
				ls = flashing;
				break;
			}

			surface->write (rec->set_state (ls));
		}
	}
}

list<boost::shared_ptr<ARDOUR::Bundle> >
US2400Protocol::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_input_bundle) {
		b.push_back (_input_bundle);
		b.push_back (_output_bundle);
	}

	return b;
}

void
US2400Protocol::do_request (US2400ControlUIRequest* req)
{
	DEBUG_TRACE (DEBUG::US2400, string_compose ("doing request type %1\n", req->type));
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
US2400Protocol::stop ()
{
	BaseUI::quit ();

	return 0;
}

void
US2400Protocol::update_led (Surface& surface, Button& button, US2400::LedState ls)
{
	if (ls != none) {
		surface.port().write (button.set_state (ls));
	}
}

void
US2400Protocol::build_button_map ()
{
	/* this maps our device-independent button codes to the methods that handle them.
	 */

#define DEFINE_BUTTON_HANDLER(b,p,r) button_map.insert (pair<Button::ID,ButtonHandlers> ((b), ButtonHandlers ((p),(r))));

	DEFINE_BUTTON_HANDLER (Button::Solo, &US2400Protocol::clearsolo_press, &US2400Protocol::clearsolo_release);  // ClearSolo button == Option+Solo lands here.

	DEFINE_BUTTON_HANDLER (Button::Send, &US2400Protocol::send_press, &US2400Protocol::send_release);
	DEFINE_BUTTON_HANDLER (Button::Pan, &US2400Protocol::pan_press, &US2400Protocol::pan_release);
	DEFINE_BUTTON_HANDLER (Button::Left, &US2400Protocol::left_press, &US2400Protocol::left_release);
	DEFINE_BUTTON_HANDLER (Button::Right, &US2400Protocol::right_press, &US2400Protocol::right_release);
	DEFINE_BUTTON_HANDLER (Button::Flip, &US2400Protocol::flip_press, &US2400Protocol::flip_release);
	DEFINE_BUTTON_HANDLER (Button::MstrSelect, &US2400Protocol::mstr_press, &US2400Protocol::mstr_release);
//	DEFINE_BUTTON_HANDLER (Button::F1, &US2400Protocol::F1_press, &US2400Protocol::F1_release);
//	DEFINE_BUTTON_HANDLER (Button::F2, &US2400Protocol::F2_press, &US2400Protocol::F2_release);
//	DEFINE_BUTTON_HANDLER (Button::F3, &US2400Protocol::F3_press, &US2400Protocol::F3_release);
//	DEFINE_BUTTON_HANDLER (Button::F4, &US2400Protocol::F4_press, &US2400Protocol::F4_release);
//	DEFINE_BUTTON_HANDLER (Button::F5, &US2400Protocol::F5_press, &US2400Protocol::F5_release);
//	DEFINE_BUTTON_HANDLER (Button::F6, &US2400Protocol::F6_press, &US2400Protocol::F6_release);
	DEFINE_BUTTON_HANDLER (Button::Shift, &US2400Protocol::shift_press, &US2400Protocol::shift_release);
	DEFINE_BUTTON_HANDLER (Button::Option, &US2400Protocol::option_press, &US2400Protocol::option_release);
	DEFINE_BUTTON_HANDLER (Button::Drop, &US2400Protocol::drop_press, &US2400Protocol::drop_release);
	DEFINE_BUTTON_HANDLER (Button::Rewind, &US2400Protocol::rewind_press, &US2400Protocol::rewind_release);
	DEFINE_BUTTON_HANDLER (Button::Ffwd, &US2400Protocol::ffwd_press, &US2400Protocol::ffwd_release);
	DEFINE_BUTTON_HANDLER (Button::Stop, &US2400Protocol::stop_press, &US2400Protocol::stop_release);
	DEFINE_BUTTON_HANDLER (Button::Play, &US2400Protocol::play_press, &US2400Protocol::play_release);
	DEFINE_BUTTON_HANDLER (Button::Record, &US2400Protocol::record_press, &US2400Protocol::record_release);
	DEFINE_BUTTON_HANDLER (Button::Scrub, &US2400Protocol::scrub_press, &US2400Protocol::scrub_release);
	DEFINE_BUTTON_HANDLER (Button::MasterFaderTouch, &US2400Protocol::master_fader_touch_press, &US2400Protocol::master_fader_touch_release);
}

void
US2400Protocol::handle_button_event (Surface& surface, Button& button, ButtonState bs)
{
	Button::ID button_id = button.bid();

	if  (bs != press && bs != release) {
		update_led (surface, button, none);
		return;
	}

	DEBUG_TRACE (DEBUG::US2400, string_compose ("Handling %1 for button %2 (%3)\n", (bs == press ? "press" : "release"), button.id(),
							   Button::id_to_name (button.bid())));

	/* check profile first */

	string action = _device_profile.get_button_action (button.bid(), _modifier_state);

	DEBUG_TRACE (DEBUG::US2400, string_compose ("device profile returned [%1] for that button\n", action));

	if (!action.empty()) {

		if (action.find ('/') != string::npos) { /* good chance that this is really an action */

			DEBUG_TRACE (DEBUG::US2400, string_compose ("Looked up action for button %1 with modifier %2, got [%3]\n",
									   button.bid(), _modifier_state, action));

			/* if there is a bound action for this button, and this is a press event,
			   carry out the action. If its a release event, do nothing since we
			   don't bind to them at all but don't want any other handling to
			   occur either.
			*/
			if (bs == press) {
				update_led (surface, button, on);
				DEBUG_TRACE (DEBUG::US2400, string_compose ("executing action %1\n", action));
				access_action (action);
			} else {
				update_led (surface, button, off);
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
				DEBUG_TRACE (DEBUG::US2400, string_compose ("apparent button name %1 not found\n", action));
				return;
			}

			button_id = (Button::ID) bid;
			DEBUG_TRACE (DEBUG::US2400, string_compose ("handling button %1 as if it was %2 (%3)\n", Button::id_to_name (button.bid()), button_id, Button::id_to_name (button_id)));
		}
	}

	/* Now that we have the correct (maybe remapped) button ID, do these
	 * checks on it.
	 */

	/* lookup using the device-INDEPENDENT button ID */

	DEBUG_TRACE (DEBUG::US2400, string_compose ("now looking up button ID %1\n", button_id));

	ButtonMap::iterator b = button_map.find (button_id);

	if (b != button_map.end()) {

		ButtonHandlers& bh (b->second);

		DEBUG_TRACE (DEBUG::US2400, string_compose ("button found in map, now invoking %1\n", (bs == press ? "press" : "release")));

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
		DEBUG_TRACE (DEBUG::US2400, string_compose ("no button handlers for button ID %1 (device ID %2)\n",
								   button.bid(), button.id()));
		error << string_compose ("no button handlers for button ID %1 (device ID %2)\n",
					 button.bid(), button.id()) << endmsg;
	}
}

bool
US2400Protocol::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		DEBUG_TRACE (DEBUG::US2400, "MIDI port closed\n");
		return false;
	}

	if (ioc & IO_IN) {

		// DEBUG_TRACE (DEBUG::US2400, string_compose ("something happend on  %1\n", port->name()));

		/* Devices using regular JACK MIDI ports will need to have
		   the x-thread FIFO drained to avoid burning endless CPU.
		*/

		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*>(port);
		if (asp) {
			asp->clear ();
		}

		// DEBUG_TRACE (DEBUG::US2400, string_compose ("data available on %1\n", port->name()));
		samplepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}

void
US2400Protocol::clear_ports ()
{
	if (_input_bundle) {
		_input_bundle->remove_channels ();
		_output_bundle->remove_channels ();
	}
}

void
US2400Protocol::notify_subview_stripable_deleted ()
{
	/* return to global/mixer view */
	_subview_stripable.reset ();
	set_view_mode (Mixer);
}

bool
US2400Protocol::subview_mode_would_be_ok (SubViewMode mode, boost::shared_ptr<Stripable> r)
{
	switch (mode) {
	case None:
		return true;
		break;

	case TrackView:
		if (r) {
			return true;
		}
	}

	return false;
}

bool
US2400Protocol::redisplay_subview_mode ()
{
	Surfaces copy; /* can't hold surfaces lock while calling Strip::subview_mode_changed */

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		copy = surfaces;
	}

	for (Surfaces::iterator s = copy.begin(); s != copy.end(); ++s) {
		(*s)->subview_mode_changed ();
	}

	/* don't call this again from a timeout */
	return false;
}

int
US2400Protocol::set_subview_mode (SubViewMode sm, boost::shared_ptr<Stripable> r)
{
	if (!subview_mode_would_be_ok (sm, r)) {

		DEBUG_TRACE (DEBUG::US2400, "subview mode not OK\n");

		if (r) {

			Glib::Threads::Mutex::Lock lm (surfaces_lock);

			if (!surfaces.empty()) {

				string msg;

				switch (sm) {
				case TrackView:
					msg = _("no track view possible");
				default:
					break;
				}
			}
		}

		return -1;
	}

	boost::shared_ptr<Stripable> old_stripable = _subview_stripable;

	_subview_mode = sm;
	_subview_stripable = r;

	if (_subview_stripable != old_stripable) {
		subview_stripable_connections.drop_connections ();

		/* Catch the current subview stripable going away */
		if (_subview_stripable) {
			_subview_stripable->DropReferences.connect (subview_stripable_connections, MISSING_INVALIDATOR,
			                                            boost::bind (&US2400Protocol::notify_subview_stripable_deleted, this),
			                                            this);
		}
	}

	redisplay_subview_mode ();

	/* turn buttons related to vpot mode on or off as required */

	switch (_subview_mode) {
	case US2400Protocol::None:
		update_global_button (Button::Send, off);
		update_global_button (Button::Pan, on);
		break;
	case US2400Protocol::TrackView:
		update_global_button (Button::Send, off);
		update_global_button (Button::Pan, off);
		break;
	}

	return 0;
}

void
US2400Protocol::set_view_mode (ViewMode m)
{
	ViewMode old_view_mode = _view_mode;

	_view_mode = m;
	_last_bank[old_view_mode] = _current_initial_bank;

	if (switch_banks(_last_bank[m], true)) {
		_view_mode = old_view_mode;
		return;
	}

	/* leave subview mode, whatever it was */
	set_subview_mode (None, boost::shared_ptr<Stripable>());
}

void
US2400Protocol::display_view_mode ()
{

}

void
US2400Protocol::set_master_on_surface_strip (uint32_t surface, uint32_t strip_number)
{
	force_special_stripable_to_strip (session->master_out(), surface, strip_number);
}

void
US2400Protocol::set_monitor_on_surface_strip (uint32_t surface, uint32_t strip_number)
{
	force_special_stripable_to_strip (session->monitor_out(), surface, strip_number);
}

void
US2400Protocol::force_special_stripable_to_strip (boost::shared_ptr<Stripable> r, uint32_t surface, uint32_t strip_number)
{
	if (!r) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s)->number() == surface) {
			Strip* strip = (*s)->nth_strip (strip_number);
			if (strip) {
				strip->set_stripable (session->master_out());
				strip->lock_controls ();
			}
		}
	}
}

void
US2400Protocol::check_fader_automation_state ()
{
}

void
US2400Protocol::update_fader_automation_state ()
{
}

samplepos_t
US2400Protocol::transport_sample() const
{
	return session->transport_sample();
}

void
US2400Protocol::add_down_select_button (int surface, int strip)
{
	_down_select_buttons.insert ((surface<<8)|(strip&0xf));
}

void
US2400Protocol::remove_down_select_button (int surface, int strip)
{
	DownButtonList::iterator x = find (_down_select_buttons.begin(), _down_select_buttons.end(), (uint32_t) (surface<<8)|(strip&0xf));
	DEBUG_TRACE (DEBUG::US2400, string_compose ("removing surface %1 strip %2 from down select buttons\n", surface, strip));
	if (x != _down_select_buttons.end()) {
		_down_select_buttons.erase (x);
	} else {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface %1 strip %2 not found in down select buttons\n",
								   surface, strip));
	}
}

void
US2400Protocol::select_range (uint32_t pressed)
{
	StripableList stripables;

	pull_stripable_range (_down_select_buttons, stripables, pressed);

	DEBUG_TRACE (DEBUG::US2400, string_compose ("select range: found %1 stripables, first = %2\n", stripables.size(),
	                                                   (stripables.empty() ? "null" : stripables.front()->name())));

	if (stripables.empty()) {
		return;
	}

	if (stripables.size() == 1 && ControlProtocol::last_selected().size() == 1 && stripables.front()->is_selected()) {
		/* cancel selection for one and only selected stripable */
		toggle_stripable_selection (stripables.front());
	} else {
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {

			if (main_modifier_state() == MODIFIER_SHIFT) {
				toggle_stripable_selection (*s);
			} else {
				if (s == stripables.begin()) {
					set_stripable_selection (*s);
				} else {
					add_stripable_to_selection (*s);
				}
			}
		}
	}
}

void
US2400Protocol::add_down_button (AutomationType a, int surface, int strip)
{
	DownButtonMap::iterator m = _down_buttons.find (a);

	if (m == _down_buttons.end()) {
		_down_buttons[a] = DownButtonList();
	}

	_down_buttons[a].insert ((surface<<8)|(strip&0xf));
}

void
US2400Protocol::remove_down_button (AutomationType a, int surface, int strip)
{
	DownButtonMap::iterator m = _down_buttons.find (a);

	DEBUG_TRACE (DEBUG::US2400, string_compose ("removing surface %1 strip %2 from down buttons for %3\n", surface, strip, (int) a));

	if (m == _down_buttons.end()) {
		return;
	}

	DownButtonList& l (m->second);
	DownButtonList::iterator x = find (l.begin(), l.end(), (surface<<8)|(strip&0xf));

	if (x != l.end()) {
		l.erase (x);
	} else {
		DEBUG_TRACE (DEBUG::US2400, string_compose ("surface %1 strip %2 not found in down buttons for %3\n",
								   surface, strip, (int) a));
	}
}

US2400Protocol::ControlList
US2400Protocol::down_controls (AutomationType p, uint32_t pressed)
{
	ControlList controls;
	StripableList stripables;

	DownButtonMap::iterator m = _down_buttons.find (p);

	if (m == _down_buttons.end()) {
		return controls;
	}

	DEBUG_TRACE (DEBUG::US2400, string_compose ("looking for down buttons for %1, got %2\n",
							   p, m->second.size()));

	pull_stripable_range (m->second, stripables, pressed);

	switch (p) {
	case GainAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			controls.push_back ((*s)->gain_control());
		}
		break;
	case SoloAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			controls.push_back ((*s)->solo_control());
		}
		break;
	case MuteAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			controls.push_back ((*s)->mute_control());
		}
		break;
	case RecEnableAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			boost::shared_ptr<AutomationControl> ac = (*s)->rec_enable_control();
			if (ac) {
				controls.push_back (ac);
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
US2400Protocol::pull_stripable_range (DownButtonList& down, StripableList& selected, uint32_t pressed)
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

	DEBUG_TRACE (DEBUG::US2400, string_compose ("PRR %5 in list %1.%2 - %3.%4\n", first_surface, first_strip, last_surface, last_strip,
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

			DEBUG_TRACE (DEBUG::US2400, string_compose ("adding strips for surface %1 (%2 .. %3)\n",
									   (*s)->number(), fs, ls));

			for (uint32_t n = fs; n < ls; ++n) {
				Strip* strip = (*s)->nth_strip (n);
				boost::shared_ptr<Stripable> r = strip->stripable();
				if (r) {
					if (global_index_locked (*strip) == pressed) {
						selected.push_front (r);
					} else {
						selected.push_back (r);
					}
				}
			}
		}
	}

}

void
US2400Protocol::clear_surfaces ()
{
	clear_ports ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		_master_surface.reset ();
		surfaces.clear ();
	}
}

void
US2400Protocol::set_touch_sensitivity (int sensitivity)
{
	sensitivity = min (9, sensitivity);
	sensitivity = max (0, sensitivity);

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->set_touch_sensitivity (sensitivity);
	}
}

void
US2400Protocol::recalibrate_faders ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->recalibrate_faders ();
	}
}

void
US2400Protocol::toggle_backlight ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->toggle_backlight ();
	}
}

boost::shared_ptr<Surface>
US2400Protocol::get_surface_by_raw_pointer (void* ptr) const
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
US2400Protocol::nth_surface (uint32_t n) const
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
US2400Protocol::connection_handler (boost::weak_ptr<ARDOUR::Port> wp1, std::string name1, boost::weak_ptr<ARDOUR::Port> wp2, std::string name2, bool yn)
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

bool
US2400Protocol::is_track (boost::shared_ptr<Stripable> r) const
{
	return boost::dynamic_pointer_cast<Track>(r) != 0;
}

bool
US2400Protocol::is_audio_track (boost::shared_ptr<Stripable> r) const
{
	return boost::dynamic_pointer_cast<AudioTrack>(r) != 0;
}

bool
US2400Protocol::is_midi_track (boost::shared_ptr<Stripable> r) const
{
	return boost::dynamic_pointer_cast<MidiTrack>(r) != 0;
}

bool
US2400Protocol::is_mapped (boost::shared_ptr<Stripable> r) const
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s)->stripable_is_mapped (r)) {
			return true;
		}
	}

	return false;
}

void
US2400Protocol::stripable_selection_changed ()
{
	//this function is called after the stripable selection is "stable", so this is the place to check surface selection state
	for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		(*si)->update_strip_selection ();
	}

	//first check for the dedicated Master strip
	boost::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable();
	if (s && s->is_master()) {
		update_global_button(Button::MstrSelect, on);  //NOTE:  surface does not respond to this
	} else {
		update_global_button(Button::MstrSelect, off);

		//not the master;  now check for other strips ( this will only allow a selection if the strip is mapped on our surface )
		s = first_selected_stripable ();
	}
	
	if (s) {
		check_fader_automation_state ();

		/* It is possible that first_selected_route() may return null if we
		 * are no longer displaying/mapping that route. In that case,
		 * we will exit subview mode. If first_selected_route() is
		 * null, and subview mode is not None, then the first call to
		 * set_subview_mode() will fail, and we will reset to None.
		 */

		if (set_subview_mode (TrackView, s)) {
			set_subview_mode (None, boost::shared_ptr<Stripable>());
		}

	} else
		set_subview_mode (None, boost::shared_ptr<Stripable>());
}

boost::shared_ptr<Stripable>
US2400Protocol::first_selected_stripable () const
{
	boost::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable();

	if (s) {
		/* check it is on one of our surfaces */

		if (is_mapped (s)) {
			return s;
		}

		/* stripable is not mapped. thus, the currently selected stripable is
		 * not on the surfaces, and so from our perspective, there is
		 * no currently selected stripable.
		 */

		s.reset ();
	}

	return s; /* may be null */
}

boost::shared_ptr<Stripable>
US2400Protocol::subview_stripable () const
{
	return _subview_stripable;
}

uint32_t
US2400Protocol::global_index (Strip& strip)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	return global_index_locked (strip);
}

uint32_t
US2400Protocol::global_index_locked (Strip& strip)
{
	uint32_t global = 0;

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s).get() == strip.surface()) {
			return global + strip.index();
		}
		global += (*s)->n_strips ();
	}

	return global;
}

void*
US2400Protocol::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
US2400Protocol::set_automation_state (AutoState as)
{
	boost::shared_ptr<Stripable> r = first_selected_stripable ();

	if (!r) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = r->gain_control();

	if (!ac) {
		return;
	}

	ac->set_automation_state (as);
}
