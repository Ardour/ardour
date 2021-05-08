/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
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

#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "ardour/butler.h"
#include "ardour/disk_reader.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/session_route.h"
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
	if (!loading()) {
		request_stop (false, false);
	}
	SessionEvent *ev = new SessionEvent (SessionEvent::AdjustPlaybackBuffering, SessionEvent::Add, SessionEvent::Immediate, 0, 0, 0.0);
	queue_event (ev);
}

void
Session::adjust_capture_buffering ()
{
	if (!loading()) {
		request_stop (false, false);
	}
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
Session::request_overwrite_buffer (boost::shared_ptr<Track> t, OverwriteReason why)
{
	assert (t);
	SessionEvent *ev = new SessionEvent (SessionEvent::Overwrite, SessionEvent::Replace, SessionEvent::Immediate, 0, 0, 0.0);
	ev->set_track (t);
	ev->overwrite = why;
	queue_event (ev);
}

void
Session::overwrite_some_buffers (boost::shared_ptr<Route> r, OverwriteReason why)
{
	/* this is called from the process thread while handling queued
	 * SessionEvents. Therefore neither playback sample or read offsets in
	 * tracks will change while we "queue" them all for an upcoming
	 * overwrite.
	 */

	if (actively_recording()) {
		return;
	}


	if (r) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (r);
		assert (t);
		t->set_pending_overwrite (why);

	} else {

		foreach_track (&Track::set_pending_overwrite, why);
	}

	if (why == LoopChanged) {
		add_post_transport_work (PostTransportWork (PostTransportOverWrite|PostTransportLoopChanged));
	} else {
		add_post_transport_work (PostTransportOverWrite);
	}

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
