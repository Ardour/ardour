/*
    Copyright (C) 1999-2002 Paul Davis

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
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"

#include "ardour/butler.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/track.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/*---------------------------------------------------------------------------
 BUTLER THREAD
 ---------------------------------------------------------------------------*/

void
Session::adjust_playback_buffering ()
{
        request_stop (false, false);
	SessionEvent *ev = new SessionEvent (SessionEvent::AdjustPlaybackBuffering, SessionEvent::Add, SessionEvent::Immediate, 0, 0, 0.0);
	queue_event (ev);
}

void
Session::adjust_capture_buffering ()
{
        request_stop (false, false);
	SessionEvent *ev = new SessionEvent (SessionEvent::AdjustCaptureBuffering, SessionEvent::Add, SessionEvent::Immediate, 0, 0, 0.0);
	queue_event (ev);
}

void
Session::schedule_playback_buffering_adjustment ()
{
	add_post_transport_work (PostTransportAdjustPlaybackBuffering);
	_butler->schedule_transport_work ();
}

void
Session::schedule_capture_buffering_adjustment ()
{
	add_post_transport_work (PostTransportAdjustCaptureBuffering);
	_butler->schedule_transport_work ();
}

void
Session::schedule_curve_reallocation ()
{
	add_post_transport_work (PostTransportCurveRealloc);
	_butler->schedule_transport_work ();
}

void
Session::request_overwrite_buffer (boost::shared_ptr<Route> r)
{
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (r);
	if (!t) {
		return;
	}

	SessionEvent *ev = new SessionEvent (SessionEvent::Overwrite, SessionEvent::Add, SessionEvent::Immediate, 0, 0, 0.0);
	ev->set_ptr (t.get());
	queue_event (ev);
}

/** Process thread. */
void
Session::overwrite_some_buffers (Track* t)
{
	if (actively_recording()) {
		return;
	}

	if (t) {

		t->set_pending_overwrite (true);

	} else {

		boost::shared_ptr<RouteList> rl = routes.reader();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->set_pending_overwrite (true);
			}
		}
	}

	add_post_transport_work (PostTransportOverWrite);
	_butler->schedule_transport_work ();
}

uint32_t
Session::playback_load ()
{
	return (uint32_t) g_atomic_int_get (&_playback_load);
}

uint32_t
Session::capture_load ()
{
	return (uint32_t) g_atomic_int_get (&_capture_load);
}
