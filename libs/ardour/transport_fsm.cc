/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/debug.h"
#include "ardour/session.h"
#include "ardour/transport_fsm.h"

using namespace ARDOUR;
using namespace PBD;

/* transition actions */

void
TransportFSM::start_playback (TransportFSM::start_transport const& p)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_playback\n");
	api->start_transport();
}

void
TransportFSM::start_declick (TransportFSM::stop_transport const &s)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_declick\n");
	_last_stop = s;
}

void
TransportFSM::stop_playback (TransportFSM::declick_done const& /*ignored*/)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::stop_playback\n");
	api->stop_transport (_last_stop.abort, _last_stop.clear_state);
}

void
TransportFSM::save_locate_and_start_declick (TransportFSM::locate const & l)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::save_locate_and_stop\n");
	_last_locate = l;
	start_declick (stop_transport (false, false));
}

void
TransportFSM::start_locate (TransportFSM::locate const& l)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_locate\n");
	api->locate (l.target, l.with_roll, l.with_flush, l.with_loop, l.force);
}

void
TransportFSM::start_saved_locate (TransportFSM::declick_done const&)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_save\n");
	api->locate (_last_locate.target, _last_locate.with_roll, _last_locate.with_flush, _last_locate.with_loop, _last_locate.force);
}

void
TransportFSM::interrupt_locate (TransportFSM::locate const& l)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::interrupt\n");
	/* maintain original "with-roll" choice of initial locate, even though
	 * we are interrupting the locate to start a new one.
	 */
	api->locate (l.target, _last_locate.with_roll, l.with_flush, l.with_loop, l.force);
}

void
TransportFSM::schedule_butler_for_transport_work (TransportFSM::butler_required const&)
{
	api->schedule_butler_for_transport_work ();
}

bool
TransportFSM::should_roll_after_locate (TransportFSM::locate_done const &)
{
	bool ret = api->should_roll_after_locate ();
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("tfsm::should_roll_after_locate() ? %1\n", ret));
	return ret;
}

void
TransportFSM::roll_after_locate (TransportFSM::locate_done const &)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "rolling after locate\n");
	api->start_transport ();
}

