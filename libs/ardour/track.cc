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

#include "ardour/amp.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/diskstream.h"
#include "ardour/io_processor.h"
#include "ardour/meter.h"
#include "ardour/port.h"
#include "ardour/processor.h"
#include "ardour/route_group_specialized.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Track::Track (Session& sess, string name, Route::Flag flag, TrackMode mode, DataType default_type)
	: Route (sess, name, flag, default_type)
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
	for (PortSet::iterator i = _input->ports().begin(); i != _input->ports().end(); ++i) {
		i->ensure_monitor_input(!i->monitoring_input());
	}
}

ARDOUR::nframes_t
Track::update_total_latency ()
{
	nframes_t old = _output->effective_latency();
	nframes_t own_latency = _output->user_latency();
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->active ()) {
			own_latency += (*i)->signal_latency ();
		}
	}

#undef DEBUG_LATENCY
#ifdef DEBUG_LATENCY
	cerr << _name << ": internal redirect (final) latency = " << own_latency << endl;
#endif

	_output->set_port_latency (own_latency);
	
	if (old != own_latency) {
		_output->set_latency_delay (own_latency);
		signal_latency_changed (); /* EMIT SIGNAL */
	}

	return _output->effective_latency();
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
	return _diskstream && _diskstream->record_enabled ();
}

bool
Track::can_record()
{
	bool will_record = true;
	for (PortSet::iterator i = _input->ports().begin(); i != _input->ports().end() && will_record; ++i) {
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

	if (_route_group && src != _route_group && _route_group->active_property (RouteGroup::RecEnable)) {
		_route_group->apply (&Track::set_record_enable, yn, _route_group);
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
	
	if ((ret = Route::set_name (str)) == 0) {
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

int 
Track::no_roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame, 
		bool session_state_changing, bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_total() == 0) {
		return 0;
	}

	if (!_active) {
		silence (nframes);
		return 0;
	}

	if (session_state_changing) {

		/* XXX is this safe to do against transport state changes? */

		passthru_silence (start_frame, end_frame, nframes, 0);
		return 0;
	}

	diskstream()->check_record_status (start_frame, nframes, can_record);

	bool send_silence;
	
	if (_have_internal_generator) {
		/* since the instrument has no input streams,
		   there is no reason to send any signal
		   into the route.
		*/
		send_silence = true;
	} else {
		if (!Config->get_tape_machine_mode()) {
			/* 
			   ADATs work in a strange way.. 
			   they monitor input always when stopped.and auto-input is engaged. 
			*/
			if ((Config->get_monitoring_model() == SoftwareMonitoring)
					&& (_session.config.get_auto_input () || _diskstream->record_enabled())) {
				send_silence = false;
			} else {
				send_silence = true;
			}
		} else {
			/* 
			   Other machines switch to input on stop if the track is record enabled,
			   regardless of the auto input setting (auto input only changes the 
			   monitoring state when the transport is rolling) 
			*/
			if ((Config->get_monitoring_model() == SoftwareMonitoring)
					&& _diskstream->record_enabled()) {
				send_silence = false;
			} else {
				send_silence = true;
			}
		}
	}

	_amp->apply_gain_automation(false);

	if (send_silence) {
		
		/* if we're sending silence, but we want the meters to show levels for the signal,
		   meter right here.
		*/
		
		if (_have_internal_generator) {
			passthru_silence (start_frame, end_frame, nframes, 0);
		} else {
			if (_meter_point == MeterInput) {
				_input->process_input (_meter, start_frame, end_frame, nframes);
			}
			passthru_silence (start_frame, end_frame, nframes, 0);
		}

	} else {
	
		/* we're sending signal, but we may still want to meter the input. 
		 */

		passthru (start_frame, end_frame, nframes, false);
	}

	return 0;
}

int
Track::silent_roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame,  
		    bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_total() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes);
		return 0;
	}

	_silent = true;
	_amp->apply_gain_automation(false);

	silence (nframes);

	return diskstream()->process (_session.transport_frame(), nframes, can_record, rec_monitors_input);
}
