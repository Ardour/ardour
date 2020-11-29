/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/convert.h"
#include "pbd/error.h"

#include "temporal/superclock.h"
#include "temporal/tempo.h"

#include "ardour/control_protocol_manager.h"
#include "ardour/gain_control.h"
#include "ardour/session.h"
#include "ardour/record_enable_control.h"
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/meter.h"
#include "ardour/amp.h"
#include "ardour/selection.h"
#include "control_protocol/control_protocol.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

Signal0<void>       ControlProtocol::ZoomToSession;
Signal0<void>       ControlProtocol::ZoomOut;
Signal0<void>       ControlProtocol::ZoomIn;
Signal0<void>       ControlProtocol::Enter;
Signal0<void>       ControlProtocol::Undo;
Signal0<void>       ControlProtocol::Redo;
Signal1<void,float> ControlProtocol::ScrollTimeline;
Signal1<void,uint32_t> ControlProtocol::GotoView;
Signal0<void> ControlProtocol::CloseDialog;
PBD::Signal0<void> ControlProtocol::VerticalZoomInAll;
PBD::Signal0<void> ControlProtocol::VerticalZoomOutAll;
PBD::Signal0<void> ControlProtocol::VerticalZoomInSelected;
PBD::Signal0<void> ControlProtocol::VerticalZoomOutSelected;
PBD::Signal0<void>          ControlProtocol::StepTracksDown;
PBD::Signal0<void>          ControlProtocol::StepTracksUp;

StripableNotificationList ControlProtocol::_last_selected;
PBD::ScopedConnection ControlProtocol::selection_connection;
bool ControlProtocol::selection_connected = false;

const std::string ControlProtocol::state_node_name ("Protocol");

ControlProtocol::ControlProtocol (Session& s, string str)
	: BasicUI (s)
	, _name (str)
	, _active (false)
{
	if (!selection_connected) {
		/* this is all static, connect it only once (and early), for all ControlProtocols */
		ControlProtocolManager::StripableSelectionChanged.connect_same_thread (selection_connection, boost::bind (&ControlProtocol::notify_stripable_selection_changed, _1));
		selection_connected = true;
	}
}

ControlProtocol::~ControlProtocol ()
{
}

int
ControlProtocol::set_active (bool yn)
{
	_active = yn;
	return 0;
}

void
ControlProtocol::next_track (uint32_t initial_id)
{
	// STRIPABLE route_table[0] = _session->get_nth_stripable (++initial_id, RemoteControlID::Route);
}

void
ControlProtocol::prev_track (uint32_t initial_id)
{
	if (!initial_id) {
		return;
	}
	// STRIPABLE route_table[0] = _session->get_nth_stripable (--initial_id, RemoteControlID::Route);
}

void
ControlProtocol::set_route_table_size (uint32_t size)
{
	while (route_table.size() < size) {
		route_table.push_back (boost::shared_ptr<Route> ((Route*) 0));
	}
}

void
ControlProtocol::set_route_table (uint32_t table_index, boost::shared_ptr<ARDOUR::Route> r)
{
	if (table_index >= route_table.size()) {
		return;
	}

	route_table[table_index] = r;

	// XXX SHAREDPTR need to handle r->GoingAway
}

bool
ControlProtocol::set_route_table (uint32_t table_index, uint32_t remote_control_id)
{
#if 0 // STRIPABLE
	boost::shared_ptr<Route> r = session->route_by_remote_id (remote_control_id);

	if (!r) {
		return false;
	}

	set_route_table (table_index, r);
#endif
	return true;
}

void
ControlProtocol::route_set_rec_enable (uint32_t table_index, bool yn)
{
	if (table_index > route_table.size()) {
		return;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack>(r);

	if (at) {
		at->rec_enable_control()->set_value (1.0, Controllable::UseGroup);
	}
}

bool
ControlProtocol::route_get_rec_enable (uint32_t table_index)
{
	if (table_index > route_table.size()) {
		return false;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack>(r);

	if (at) {
		return at->rec_enable_control()->get_value();
	}

	return false;
}


float
ControlProtocol::route_get_gain (uint32_t table_index)
{
	if (table_index > route_table.size()) {
		return 0.0f;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r == 0) {
		return 0.0f;
	}

	return r->gain_control()->get_value();
}

void
ControlProtocol::route_set_gain (uint32_t table_index, float gain)
{
	if (table_index > route_table.size()) {
		return;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r != 0) {
		r->gain_control()->set_value (gain, Controllable::UseGroup);
	}
}

float
ControlProtocol::route_get_effective_gain (uint32_t table_index)
{
	if (table_index > route_table.size()) {
		return 0.0f;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r == 0) {
		return 0.0f;
	}

	return r->amp()->gain_control()->get_value();
}


float
ControlProtocol::route_get_peak_input_power (uint32_t table_index, uint32_t which_input)
{
	if (table_index > route_table.size()) {
		return 0.0f;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r == 0) {
		return 0.0f;
	}

	return r->peak_meter()->meter_level (which_input, MeterPeak);
}

bool
ControlProtocol::route_get_muted (uint32_t table_index)
{
	if (table_index > route_table.size()) {
		return false;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r == 0) {
		return false;
	}

	return r->mute_control()->muted ();
}

void
ControlProtocol::route_set_muted (uint32_t table_index, bool yn)
{
	if (table_index > route_table.size()) {
		return;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r != 0) {
		r->mute_control()->set_value (yn ? 1.0 : 0.0, Controllable::UseGroup);
	}
}


bool
ControlProtocol::route_get_soloed (uint32_t table_index)
{
	if (table_index > route_table.size()) {
		return false;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r == 0) {
		return false;
	}

	return r->soloed ();
}

void
ControlProtocol::route_set_soloed (uint32_t table_index, bool yn)
{
	if (table_index > route_table.size()) {
		return;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r != 0) {
		session->set_control (r->solo_control(), yn ? 1.0 : 0.0, Controllable::UseGroup);
	}
}

string
ControlProtocol:: route_get_name (uint32_t table_index)
{
	if (table_index > route_table.size()) {
		return "";
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r == 0) {
		return "";
	}

	return r->name();
}

list<boost::shared_ptr<Bundle> >
ControlProtocol::bundles ()
{
	return list<boost::shared_ptr<Bundle> > ();
}

XMLNode&
ControlProtocol::get_state ()
{
	XMLNode* node = new XMLNode (state_node_name);

	node->set_property ("name", _name);
	node->set_property ("feedback", get_feedback());

	return *node;
}

int
ControlProtocol::set_state (XMLNode const & node, int /* version */)
{
	bool feedback;
	if (node.get_property ("feedback", feedback)) {
		set_feedback (feedback);
	}

	return 0;
}

boost::shared_ptr<Stripable>
ControlProtocol::first_selected_stripable () const
{
	return session->selection().first_selected_stripable ();
}

void
ControlProtocol::add_stripable_to_selection (boost::shared_ptr<ARDOUR::Stripable> s)
{
	session->selection().add (s, boost::shared_ptr<AutomationControl>());
}

void
ControlProtocol::set_stripable_selection (boost::shared_ptr<ARDOUR::Stripable> s)
{
	session->selection().select_stripable_and_maybe_group (s, true, true, 0);
}

void
ControlProtocol::toggle_stripable_selection (boost::shared_ptr<ARDOUR::Stripable> s)
{
	session->selection().toggle (s, boost::shared_ptr<AutomationControl>());
}

void
ControlProtocol::remove_stripable_from_selection (boost::shared_ptr<ARDOUR::Stripable> s)
{
	session->selection().remove (s, boost::shared_ptr<AutomationControl>());
}

void
ControlProtocol::clear_stripable_selection ()
{
	session->selection().clear_stripables ();
}

void
ControlProtocol::notify_stripable_selection_changed (StripableNotificationListPtr sp)
{
	_last_selected = *sp;
}

void
ControlProtocol::event_loop_precall ()
{
	Temporal::set_thread_sample_rate (session->sample_rate());
	Temporal::TempoMap::fetch ();
}

