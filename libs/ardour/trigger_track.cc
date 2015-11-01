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

#include "ardour/trigger_track.h"

using namespace ARDOUR;

TriggerTrack::TriggerTrack (Session& s, std::string name, Route::Flag f, TrackMode m)
	: Track (s, name, f, m)
{
}

TriggerTrack::~TriggerTrack ()
{
}

int
TriggerTrack::init ()
{
	return 0;
}

int
TriggerTrack::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
{
	return 0;
}

void
TriggerTrack::realtime_handle_transport_stopped ()
{
}

void
TriggerTrack::realtime_locate ()
{
}

void
TriggerTrack::non_realtime_locate (framepos_t)
{
}

boost::shared_ptr<Diskstream>
TriggerTrack::create_diskstream ()
{
	return boost::shared_ptr<Diskstream> ();
}

void
TriggerTrack::set_diskstream (boost::shared_ptr<Diskstream>)
{
}

int
TriggerTrack::set_state (const XMLNode& root, int version)
{
	return Track::set_state (root, version);
}

XMLNode&
TriggerTrack::state (bool full_state)
{
	XMLNode& root (Track::state(full_state));
	return root;
}

int
TriggerTrack::no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool state_changing)
{
	return 0;
}

