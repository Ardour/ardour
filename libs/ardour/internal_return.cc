/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#include <glibmm/threads.h>

#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/route.h"

using namespace std;
using namespace ARDOUR;

InternalReturn::InternalReturn (Session& s, Temporal::TimeDomain td, std::string const& name)
	: Processor (s, name, td)
{
	_display_to_user = false;
}

void
InternalReturn::run (BufferSet& bufs, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double /*speed*/, pframes_t nframes, bool)
{
	if (!check_active()) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_sends_mutex, Glib::Threads::TRY_LOCK);

	if (!lm.locked ()) {
		return;
	}

	for (list<InternalSend*>::iterator i = _sends.begin(); i != _sends.end(); ++i) {
		if ((*i)->active () && (!(*i)->source_route() || (*i)->source_route()->active())) {
			bufs.merge_from ((*i)->get_buffers(), nframes);
		}
	}
}

void
InternalReturn::add_send (InternalSend* send)
{
	Glib::Threads::Mutex::Lock lm (_sends_mutex);
	_sends.push_back (send);
}

void
InternalReturn::remove_send (InternalSend* send)
{
	Glib::Threads::Mutex::Lock lm (_sends_mutex);
	_sends.remove (send);
}

void
InternalReturn::set_playback_offset (samplecnt_t cnt)
{
	Processor::set_playback_offset (cnt);

	Glib::Threads::Mutex::Lock lm (_sends_mutex); // TODO reader lock
	for (list<InternalSend*>::iterator i = _sends.begin(); i != _sends.end(); ++i) {
		(*i)->set_delay_out (cnt);
	}
}

XMLNode&
InternalReturn::state ()
{
	XMLNode& node (Processor::state ());
	/* override type */
	node.set_property("type", "intreturn");
	return node;
}

bool
InternalReturn::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
InternalReturn::configure_io (ChanCount in, ChanCount out)
{
	Processor::configure_io (in, out);
	return true;
}
