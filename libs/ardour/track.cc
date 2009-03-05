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
#include "pbd/error.h"
#include <sigc++/retype.h>
#include <sigc++/retype_return.h>
#include <sigc++/bind.h>

#include "ardour/track.h"
#include "ardour/diskstream.h"
#include "ardour/session.h"
#include "ardour/io_processor.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/route_group_specialized.h"
#include "ardour/processor.h"
#include "ardour/audioplaylist.h"
#include "ardour/panner.h"
#include "ardour/utils.h"
#include "ardour/port.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Track::Track (Session& sess, string name, Route::Flag flag, TrackMode mode, DataType default_type)
	: Route (sess, name, 1, -1, -1, -1, flag, default_type)
	, _rec_enable_control (new RecEnableControllable(*this))
{
	_declickable = true;
	_freeze_record.state = NoFreeze;
	_saved_meter_point = _meter_point;
	_mode = mode;
}

Track::Track (Session& sess, const XMLNode& node, DataType default_type)
	: Route (sess, node, default_type)
	, _rec_enable_control (new RecEnableControllable(*this))
{
	_freeze_record.state = NoFreeze;
	_declickable = true;
	_saved_meter_point = _meter_point;
}

Track::~Track ()
{
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
		i->ensure_monitor_input(!i->monitoring_input());
	}
}

ARDOUR::nframes_t
Track::update_total_latency ()
{
	nframes_t old = _own_latency;

	if (_user_latency) {
		_own_latency = _user_latency;
	} else {
		_own_latency = 0;

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if ((*i)->active ()) {
				_own_latency += (*i)->signal_latency ();
			}
		}
	}

#undef DEBUG_LATENCY
#ifdef DEBUG_LATENCY
	cerr << _name << ": internal redirect (final) latency = " << _own_latency << endl;
#endif

	set_port_latency (_own_latency);

	if (old != _own_latency) {
		signal_latency_changed (); /* EMIT SIGNAL */
	}

	return _own_latency;
}

Track::FreezeRecord::~FreezeRecord ()
{
	for (vector<FreezeRecordProcessorInfo*>::iterator i = processor_info.begin(); i != processor_info.end(); ++i) {
		delete *i;
	}
}

Track::FreezeState
Track::freeze_state() const
{
	return _freeze_record.state;
}

Track::RecEnableControllable::RecEnableControllable (Track& s)
	: Controllable (X_("recenable")), track (s)
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

	_rec_enable_control->Changed ();
}


bool
Track::set_name (const string& str)
{
	bool ret;

	if (record_enabled() && _session.actively_recording()) {
		/* this messes things up if done while recording */
		return false;
	}

	if (_diskstream->set_name (str)) {
		return false;
	}

	/* save state so that the statefile fully reflects any filename changes */

	if ((ret = IO::set_name (str)) == 0) {
		_session.save_state ("");
	}

	return ret;
}

void
Track::set_latency_delay (nframes_t longest_session_latency)
{
	Route::set_latency_delay (longest_session_latency);
	_diskstream->set_roll_delay (_roll_delay);
}

void
Track::zero_diskstream_id_in_xml (XMLNode& node)
{
       if (node.property ("diskstream-id")) {
               node.add_property ("diskstream-id", "0");
       }
}
