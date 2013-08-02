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

#include <glibmm/threads.h>

#include "ardour/internal_return.h"
#include "ardour/internal_send.h"

using namespace std;
using namespace ARDOUR;

InternalReturn::InternalReturn (Session& s)
	: Return (s, true)
{
        _display_to_user = false;
}

void
InternalReturn::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_sends_mutex, Glib::Threads::TRY_LOCK);
	
	if (lm.locked ()) {
		for (list<InternalSend*>::iterator i = _sends.begin(); i != _sends.end(); ++i) {
			if ((*i)->active ()) {
				bufs.merge_from ((*i)->get_buffers(), nframes);
			}
		}
	}

	_active = _pending_active;
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

bool
InternalReturn::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
InternalReturn::configure_io (ChanCount in, ChanCount out)
{
	IOProcessor::configure_io (in, out);
	return true;
}
