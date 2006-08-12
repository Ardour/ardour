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

#include <ardour/track.h>
#include <ardour/diskstream.h>
#include <ardour/session.h>
#include <ardour/redirect.h>
#include <ardour/audioregion.h>
#include <ardour/audiosource.h>
#include <ardour/route_group_specialized.h>
#include <ardour/insert.h>
#include <ardour/audioplaylist.h>
#include <ardour/panner.h>
#include <ardour/utils.h>
#include <ardour/connection.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Track::Track (Session& sess, string name, Route::Flag flag, TrackMode mode, DataType default_type)
	: Route (sess, name, 1, -1, -1, -1, flag, default_type)
	, _diskstream (0)
	,  _rec_enable_control (*this)
{
	_declickable = true;
	_freeze_record.state = NoFreeze;
	_saved_meter_point = _meter_point;
	_mode = mode;
}

Track::Track (Session& sess, const XMLNode& node, DataType default_type)
	: Route (sess, "to be renamed", 0, 0, -1, -1, Route::Flag(0), default_type)
	, _diskstream (0)
	, _rec_enable_control (*this)
{
	_freeze_record.state = NoFreeze;
	_declickable = true;
	_saved_meter_point = _meter_point;
}

Track::~Track ()
{
	if (_diskstream) {
		_diskstream->unref();
	}
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
	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		i->request_monitor_input(!i->monitoring_input());
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

bool
Track::record_enabled () const
{
	return _diskstream->record_enabled ();
}

bool
Track::can_record()
{
	bool will_record = true;
	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end() && will_record; ++i) {
		if (!i->connected())
			will_record = false;
	}

	return will_record;
}
	
void
Track::set_record_enable (bool yn, void *src)
{
	if (_freeze_record.state == Frozen) {
		return;
	}

	if (_mix_group && src != _mix_group && _mix_group->is_active()) {
		_mix_group->apply (&Track::set_record_enable, yn, _mix_group);
		return;
	}

	// Do not set rec enabled if the track can't record.
	if (yn && !can_record()) {
		error << string_compose( _("Can not arm track '%1'. Check the input connections"), name() ) << endmsg;
		return;
	}

	/* keep track of the meter point as it was before we rec-enabled */
	if (!_diskstream->record_enabled()) {
		_saved_meter_point = _meter_point;
	}
	
	_diskstream->set_record_enabled (yn);

	if (_diskstream->record_enabled()) {
		set_meter_point (MeterInput, this);
	} else {
		set_meter_point (_saved_meter_point, this);
	}

	_rec_enable_control.Changed ();
}

void
Track::set_mode (TrackMode m)
{
	if (_diskstream) {
		if (_mode != m) {
			_mode = m;
			_diskstream->set_destructive (m == Destructive);
			ModeChanged();
		}
	}
}

int
Track::set_name (string str, void *src)
{
	int ret;

	if (record_enabled() && _session.actively_recording()) {
		/* this messes things up if done while recording */
		return -1;
	}

	if (_diskstream->set_name (str)) {
		return -1;
	}

	/* save state so that the statefile fully reflects any filename changes */

	if ((ret = IO::set_name (str, src)) == 0) {
		_session.save_state ("");
                _session.save_history ("");
	}
	return ret;
}

void
Track::set_latency_delay (jack_nframes_t longest_session_latency)
{
	Route::set_latency_delay (longest_session_latency);
	_diskstream->set_roll_delay (_roll_delay);
}

