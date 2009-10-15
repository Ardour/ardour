/*
    Copyright (C) 2009 Paul Davis

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

#include <glibmm/thread.h>

#include "pbd/failed_constructor.h"

#include "ardour/internal_return.h"
#include "ardour/mute_master.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;

sigc::signal<void,nframes_t> InternalReturn::CycleStart;

InternalReturn::InternalReturn (Session& s)
	: Return (s, true)
	, user_count (0)
{
	CycleStart.connect (mem_fun (*this, &InternalReturn::cycle_start));
}

InternalReturn::InternalReturn (Session& s, const XMLNode& node)
	: Return (s, node, true)
	, user_count (0)
{
	CycleStart.connect (mem_fun (*this, &InternalReturn::cycle_start));
}

void
InternalReturn::run (BufferSet& bufs, sframes_t /*start_frame*/, sframes_t /*end_frame*/, nframes_t nframes)
{
	if (!_active && !_pending_active) {
		return;
	}

	/* no lock here, just atomic fetch */

	if (g_atomic_int_get(&user_count) == 0) {
		/* nothing to do - nobody is feeding us anything */
		return;
	}

	bufs.merge_from (buffers, nframes);
	_active = _pending_active;
}

bool
InternalReturn::configure_io (ChanCount in, ChanCount out)
{
	IOProcessor::configure_io (in, out);
	allocate_buffers (_session.engine().frames_per_cycle());
	return true;
}

void
InternalReturn::set_block_size (nframes_t nframes)
{
	allocate_buffers (nframes);
}

void
InternalReturn::allocate_buffers (nframes_t nframes)
{
	buffers.ensure_buffers (_configured_input, nframes);
	buffers.set_count (_configured_input);
}

BufferSet*
InternalReturn::get_buffers ()
{
	Glib::Mutex::Lock lm (_session.engine().process_lock());
	/* use of g_atomic here is just for code consistency - its protected by the lock
	   for writing.
	*/
	g_atomic_int_inc (&user_count);
	return &buffers;
}

void
InternalReturn::release_buffers ()
{
	Glib::Mutex::Lock lm (_session.engine().process_lock());
	if (user_count) {
		/* use of g_atomic here is just for code consistency - its protected by the lock
		   for writing.
		*/
		(void) g_atomic_int_dec_and_test (&user_count);
	}
}

void
InternalReturn::cycle_start (nframes_t nframes)
{
	/* called from process cycle - no lock necessary */
	if (user_count) {
		/* don't bother with this if nobody is going to feed us anything */
		buffers.silence (nframes, 0);
	}
}

XMLNode&
InternalReturn::state (bool full)
{
	XMLNode& node (Return::state (full));
	/* override type */
	node.add_property("type", "intreturn");
	return node;
}

XMLNode&
InternalReturn::get_state()
{
	return state (true);
}

int
InternalReturn::set_state (const XMLNode& node, int version)
{
	return Return::set_state (node);
}

bool
InternalReturn::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
InternalReturn::visible () const
{
	return false;
}
