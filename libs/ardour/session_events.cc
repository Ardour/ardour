/*
    Copyright (C) 1999-2004 Paul Davis 

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

    $Id$
*/

#include <cmath>
#include <unistd.h>

#include <ardour/timestamps.h>

#include <pbd/error.h>
#include <glibmm/thread.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

MultiAllocSingleReleasePool Session::Event::pool ("event", sizeof (Session::Event), 512);

static const char* event_names[] = {
	"SetTransportSpeed",
	"SetDiskstreamSpeed",
	"Locate",
	"LocateRoll",
	"SetLoop",
	"PunchIn",
	"PunchOut",
	"RangeStop",
	"RangeLocate",
	"Overwrite",
	"SetSlaveSource",
	"Audition",
	"InputConfigurationChange",
	"SetAudioRange",
	"SetMusicRange",
	"SetPlayRange",
	"StopOnce",
	"AutoLoop"
};

void
Session::add_event (jack_nframes_t frame, Event::Type type, jack_nframes_t target_frame)
{
	Event* ev = new Event (type, Event::Add, frame, target_frame, 0);
	queue_event (ev);
}

void
Session::remove_event (jack_nframes_t frame, Event::Type type)
{
	Event* ev = new Event (type, Event::Remove, frame, 0, 0);
	queue_event (ev);
}

void
Session::replace_event (Event::Type type, jack_nframes_t frame, jack_nframes_t target)
{
	Event* ev = new Event (type, Event::Replace, frame, target, 0);
	queue_event (ev);
}

void
Session::clear_events (Event::Type type)
{
	Event* ev = new Event (type, Event::Clear, 0, 0, 0);
	queue_event (ev);
}


void
Session::dump_events () const
{
	cerr << "EVENT DUMP" << endl;
	for (Events::const_iterator i = events.begin(); i != events.end(); ++i) {
		cerr << "\tat " << (*i)->action_frame << ' ' << (*i)->type << " target = " << (*i)->target_frame << endl;
	}
	cerr << "Next event: ";

        if ((Events::const_iterator) next_event == events.end()) {
		cerr << "none" << endl;
	} else {
		cerr << "at " << (*next_event)->action_frame << ' ' 
		     << (*next_event)->type << " target = " 
		     << (*next_event)->target_frame << endl;
	}
	cerr << "Immediate events pending:\n";
	for (Events::const_iterator i = immediate_events.begin(); i != immediate_events.end(); ++i) {
		cerr << "\tat " << (*i)->action_frame << ' ' << (*i)->type << " target = " << (*i)->target_frame << endl;
	}
	cerr << "END EVENT_DUMP" << endl;
}

void
Session::queue_event (Event* ev)
{
	if (_state_of_the_state & Loading) {
		merge_event (ev);
	} else {
		pending_events.write (&ev, 1);
	}
}

void
Session::merge_event (Event* ev)
{
	switch (ev->action) {
	case Event::Remove:
		_remove_event (ev);
		delete ev;
		return;

	case Event::Replace:
		_replace_event (ev);
		return;

	case Event::Clear:
		_clear_event_type (ev->type);
		delete ev;
		return;
		
	case Event::Add:
		break;
	}

	/* try to handle immediate events right here */

	if (ev->action_frame == 0) {
		process_event (ev);
		return;
	}
	
	switch (ev->type) {
	case Event::AutoLoop:
	case Event::StopOnce:
		_clear_event_type (ev->type);
		break;

	default:
		for (Events::iterator i = events.begin(); i != events.end(); ++i) {
			if ((*i)->type == ev->type && (*i)->action_frame == ev->action_frame) {
			  error << string_compose(_("Session: cannot have two events of type %1 at the same frame (%2)."), 
						 event_names[ev->type], ev->action_frame) << endmsg;
				return;
			}
		}
	}

	events.insert (events.begin(), ev);
	events.sort (Event::compare);
	next_event = events.begin();
	set_next_event ();
}

bool
Session::_replace_event (Event* ev)
{
	// returns true when we deleted the passed in event
	bool ret = false;
	Events::iterator i;

	/* private, used only for events that can only exist once in the queue */

	for (i = events.begin(); i != events.end(); ++i) {
		if ((*i)->type == ev->type) {
			(*i)->action_frame = ev->action_frame;
			(*i)->target_frame = ev->target_frame;
			if ((*i) == ev) {
				ret = true;
			}
			delete ev;
			break;
		}
	}

	if (i == events.end()) {
		events.insert (events.begin(), ev);
	}

	events.sort (Event::compare);
	next_event = events.end();
	set_next_event ();

	return ret;
}

bool
Session::_remove_event (Session::Event* ev)
{
	// returns true when we deleted the passed in event
	bool ret = false;
	Events::iterator i;
	
	for (i = events.begin(); i != events.end(); ++i) {
		if ((*i)->type == ev->type && (*i)->action_frame == ev->action_frame) {
			if ((*i) == ev) {
				ret = true;
			}

			delete *i;
			if (i == next_event) {
				++next_event;
			}
			events.erase (i);
			break;
		}
	}

	if (i != events.end()) {
		set_next_event ();
	}

	return ret;
}

void
Session::_clear_event_type (Event::Type type)
{
	Events::iterator i, tmp;
	
	for (i = events.begin(); i != events.end(); ) {

		tmp = i;
		++tmp;

		if ((*i)->type == type) {
			delete *i;
			if (i == next_event) {
				++next_event;
			}
			events.erase (i);
		}

		i = tmp;
	}

	for (i = immediate_events.begin(); i != immediate_events.end(); ) {

		tmp = i;
		++tmp;

		if ((*i)->type == type) {
			delete *i;
			immediate_events.erase (i);
		}

		i = tmp;
	}

	set_next_event ();
}

void
Session::set_next_event ()
{
	if (events.empty()) {
		next_event = events.end();
		return;
	} 

	if (next_event == events.end()) {
		next_event = events.begin();
	}

	if ((*next_event)->action_frame > _transport_frame) {
		next_event = events.begin();
	}

	for (; next_event != events.end(); ++next_event) {
		if ((*next_event)->action_frame >= _transport_frame) {
			break;
		}
	}
}

void
Session::process_event (Event* ev)
{
	bool remove = true;
	bool del = true;

	/* if we're in the middle of a state change (i.e. waiting
	   for the butler thread to complete the non-realtime
	   part of the change), we'll just have to queue this
	   event for a time when the change is complete.
	*/

	if (non_realtime_work_pending()) {
		immediate_events.insert (immediate_events.end(), ev);
		_remove_event (ev);
		return;
	}

	switch (ev->type) {
	case Event::SetLoop:
		set_auto_loop (ev->yes_or_no);
		break;

	case Event::Locate:
		if (ev->yes_or_no) {
			// cerr << "forced locate to " << ev->target_frame << endl;
			locate (ev->target_frame, false, true, false);
		} else {
			// cerr << "soft locate to " << ev->target_frame << endl;
			start_locate (ev->target_frame, false, true, false);
		}
		break;

	case Event::LocateRoll:
		if (ev->yes_or_no) {
			// cerr << "forced locate to+roll " << ev->target_frame << endl;
			locate (ev->target_frame, true, true, false);
		} else {
			// cerr << "soft locate to+roll " << ev->target_frame << endl;
			start_locate (ev->target_frame, true, true, false);
		}
		break;

	case Event::SetTransportSpeed:
		set_transport_speed (ev->speed, ev->yes_or_no);
		break;
		
	case Event::PunchIn:
		// cerr << "PunchIN at " << transport_frame() << endl;
		if (punch_in && record_status() == Enabled) {
			enable_record ();
		}
		remove = false;
		del = false;
		break;
		
	case Event::PunchOut:
		// cerr << "PunchOUT at " << transport_frame() << endl;
		if (punch_out) {
			step_back_from_record ();
		}
		remove = false;
		del = false;
		break;

	case Event::StopOnce:
		if (!non_realtime_work_pending()) {
			stop_transport (ev->yes_or_no);
			_clear_event_type (Event::StopOnce);
		}
		remove = false;
		del = false;
		break;

	case Event::RangeStop:
		if (!non_realtime_work_pending()) {
			stop_transport (ev->yes_or_no);
		}
		remove = false;
		del = false;
		break;
		
	case Event::RangeLocate:
		start_locate (ev->target_frame, true, true, false);
		remove = false;
		del = false;
		break;

	case Event::AutoLoop:
		if (auto_loop) {
			start_locate (ev->target_frame, true, false, seamless_loop);
		}
		remove = false;
		del = false;
		break;

	case Event::Overwrite:
		overwrite_some_buffers (static_cast<AudioDiskstream*>(ev->ptr));
		break;

	case Event::SetDiskstreamSpeed:
		set_diskstream_speed (static_cast<AudioDiskstream*> (ev->ptr), ev->speed);
		break;

	case Event::SetSlaveSource:
		set_slave_source (ev->slave, ev->target_frame);
		break;

	case Event::Audition:
		// set_audition (static_cast<AudioRegion*> (ev->ptr)); AUDFIX
		break;

	case Event::InputConfigurationChange:
		post_transport_work = PostTransportWork (post_transport_work | PostTransportInputChange);
		schedule_butler_transport_work ();
		break;

	case Event::SetAudioRange:
		current_audio_range = ev->audio_range;
		setup_auto_play ();
		break;

	case Event::SetPlayRange:
		set_play_range (ev->yes_or_no);
		break;

	default:
	  fatal << string_compose(_("Programming error: illegal event type in process_event (%1)"), ev->type) << endmsg;
		/*NOTREACHED*/
		break;
	};

	if (remove) {
		del = del && !_remove_event (ev);
	}

	if (del) {
		delete ev;
	}
}
