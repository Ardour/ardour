/*
    Copyright (C) 2006 Paul Davis 

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser
    General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <ardour/session.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/meter.h>
#include <control_protocol/control_protocol.h>

using namespace ARDOUR;
using namespace std;

sigc::signal<void> ControlProtocol::ZoomToSession;
sigc::signal<void> ControlProtocol::ZoomOut;
sigc::signal<void> ControlProtocol::ZoomIn;
sigc::signal<void> ControlProtocol::Enter;
sigc::signal<void,float> ControlProtocol::ScrollTimeline;

ControlProtocol::ControlProtocol (Session& s, string str)
	: BasicUI (s),
	  _name (str)
{
	_active = false;
}

ControlProtocol::~ControlProtocol ()
{
}

void
ControlProtocol::next_track (uint32_t initial_id)
{
	uint32_t limit = session->nroutes();
	boost::shared_ptr<Route> cr = route_table[0];
	uint32_t id;

	if (cr) {
		id = cr->remote_control_id ();
	} else {
		id = 0;
	}

	if (id == limit) {
		id = 0;
	} else {
		id++;
	}

	while (id < limit) {
		if ((cr = session->route_by_remote_id (id)) != 0) {
			break;
		}
		id++;
	}

	if (id == limit) {
		id = 0;
		while (id != initial_id) {
			if ((cr = session->route_by_remote_id (id)) != 0) {
				break;
			}
			id++;
		}
	}

	route_table[0] = cr;
}

void
ControlProtocol::prev_track (uint32_t initial_id)
{
	uint32_t limit = session->nroutes() - 1;
	boost::shared_ptr<Route> cr = route_table[0];
	uint32_t id;

	if (cr) {
		id = cr->remote_control_id ();
	} else {
		id = 0;
	}

	if (id == 0) {
		id = session->nroutes() - 1;
	} else {
		id--;
	}

	while (id >= 0) {
		if ((cr = session->route_by_remote_id (id)) != 0) {
			break;
		}
		id--;
	}

	if (id < 0) {
		id = limit;
		while (id > initial_id) {
			if ((cr = session->route_by_remote_id (id)) != 0) {
				break;
			}
			id--;
		}
	}

	route_table[0] = cr;
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
	boost::shared_ptr<Route> r = session->route_by_remote_id (remote_control_id);

	if (!r) {
		return false;
	}

	set_route_table (table_index, r);

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
		at->set_record_enable (yn, this);
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
		return at->record_enabled ();
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

	return r->gain ();
}

void
ControlProtocol::route_set_gain (uint32_t table_index, float gain)
{
	if (table_index > route_table.size()) {
		return;
	}

	boost::shared_ptr<Route> r = route_table[table_index];
	
	if (r != 0) {
		r->set_gain (gain, this);
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

	return r->effective_gain ();
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

	return r->peak_meter().peak_power (which_input);
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

	return r->muted ();
}

void
ControlProtocol::route_set_muted (uint32_t table_index, bool yn)
{
	if (table_index > route_table.size()) {
		return;
	}

	boost::shared_ptr<Route> r = route_table[table_index];

	if (r != 0) {
		r->set_mute (yn, this);
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
		r->set_solo (yn, this);
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

