/*
    Copyright (C) 2006 Paul Davis 

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
#include <pbd/error.h>
#include <sigc++/retype.h>
#include <sigc++/retype_return.h>
#include <sigc++/bind.h>

#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/session.h>
#include <ardour/redirect.h>
#include <ardour/audioregion.h>
#include <ardour/audiosource.h>
#include <ardour/route_group_specialized.h>
#include <ardour/insert.h>
#include <ardour/audioplaylist.h>
#include <ardour/panner.h>
#include <ardour/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Track::Track (Session& sess, string name, Route::Flag flag, TrackMode mode, Buffer::Type default_type)
	: Route (sess, name, 1, -1, -1, -1, flag, default_type)
	, _diskstream (0)
	,  _rec_enable_control (*this)
{
	_declickable = true;
	_freeze_record.state = NoFreeze;
	_saved_meter_point = _meter_point;
	_mode = mode;
}

Track::Track (Session& sess, const XMLNode& node, Buffer::Type default_type)
	: Route (sess, "to be renamed", 0, 0, -1, -1, Route::Flag(0), default_type)
	, _diskstream (0)
	, _rec_enable_control (*this)
{
	_freeze_record.state = NoFreeze;
	_declickable = true;
	_saved_meter_point = _meter_point;
}

void
Track::set_meter_point (MeterPoint p, void *src)
{
	Route::set_meter_point (p, src);
}

XMLNode&
Track::get_state ()
{
	return state (true);
}

XMLNode&
Track::get_template ()
{
	return state (false);
}

void
Track::toggle_monitor_input ()
{
	for (vector<Port*>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		(*i)->request_monitor_input(!(*i)->monitoring_input());
	}
}

jack_nframes_t
Track::update_total_latency ()
{
	_own_latency = 0;

	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		if ((*i)->active ()) {
			_own_latency += (*i)->latency ();
		}
	}

	set_port_latency (_own_latency);

	return _own_latency;
}


Track::FreezeRecord::~FreezeRecord ()
{
	for (vector<FreezeRecordInsertInfo*>::iterator i = insert_info.begin(); i != insert_info.end(); ++i) {
		delete *i;
	}
}

Track::FreezeState
Track::freeze_state() const
{
	return _freeze_record.state;
}

Track::RecEnableControllable::RecEnableControllable (Track& s)
	: track (s)
{
}

void
Track::RecEnableControllable::set_value (float val)
{
	bool bval = ((val >= 0.5f) ? true: false);
	track.set_record_enable (bval, this);
}

float
Track::RecEnableControllable::get_value (void) const
{
	if (track.record_enabled()) { return 1.0f; }
	return 0.0f;
}

