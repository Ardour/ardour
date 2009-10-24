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

#include <algorithm>
#include <string>
#include <cmath>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <glibmm/thread.h>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"

#include "ardour/audio_diskstream.h"
#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/configuration.h"
#include "ardour/crossfade.h"
#include "ardour/io.h"
#include "ardour/midi_diskstream.h"
#include "ardour/session.h"
#include "ardour/timestamps.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* XXX put this in the right place */

static inline uint32_t next_power_of_two (uint32_t n)
{
	--n;
	n |= n >> 16;
	n |= n >> 8;
	n |= n >> 4;
	n |= n >> 2;
	n |= n >> 1;
	++n;
	return n;
}

/*---------------------------------------------------------------------------
 BUTLER THREAD
 ---------------------------------------------------------------------------*/

void
Session::schedule_curve_reallocation ()
{
	post_transport_work = PostTransportWork (post_transport_work | PostTransportCurveRealloc);
	_butler->schedule_transport_work ();
}

void
Session::request_overwrite_buffer (Diskstream* stream)
{
	Event *ev = new Event (Event::Overwrite, Event::Add, Event::Immediate, 0, 0, 0.0);
	ev->set_ptr (stream);
	queue_event (ev);
}

/** Process thread. */
void
Session::overwrite_some_buffers (Diskstream* ds)
{
	if (actively_recording()) {
		return;
	}

	if (ds) {

		ds->set_pending_overwrite (true);

	} else {

		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			(*i)->set_pending_overwrite (true);
		}
	}

	post_transport_work = PostTransportWork (post_transport_work | PostTransportOverWrite);
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

uint32_t
Session::playback_load_min ()
{
	return (uint32_t) g_atomic_int_get (&_playback_load_min);
}

uint32_t
Session::capture_load_min ()
{
	return (uint32_t) g_atomic_int_get (&_capture_load_min);
}

void
Session::reset_capture_load_min ()
{
	g_atomic_int_set (&_capture_load_min, 100);
}

void
Session::reset_playback_load_min ()
{
	g_atomic_int_set (&_playback_load_min, 100);
}

