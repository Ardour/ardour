/*
    Copyright (C) 2000,2007 Paul Davis 

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

#include <string>

#include <sigc++/bind.h>

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>

#include <ardour/port_insert.h>
#include <ardour/plugin.h>
#include <ardour/port.h>
#include <ardour/route.h>
#include <ardour/buffer_set.h>

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/types.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PortInsert::PortInsert (Session& s, Placement p)
	: Redirect (s, string_compose (_("insert %1"), (bitslot = s.next_insert_id()) + 1), p, 1, -1, 1, -1)
{
	init ();
	InsertCreated (this); /* EMIT SIGNAL */
}

PortInsert::PortInsert (const PortInsert& other)
	: Redirect (other._session, string_compose (_("insert %1"), (bitslot = other._session.next_insert_id()) + 1), other.placement(), 1, -1, 1, -1)
{
	init ();
	InsertCreated (this); /* EMIT SIGNAL */
}

void
PortInsert::init ()
{
	if (_io->add_input_port ("", this)) {
		error << _("PortInsert: cannot add input port") << endmsg;
		throw failed_constructor();
	}
	
	if (_io->add_output_port ("", this)) {
		error << _("PortInsert: cannot add output port") << endmsg;
		throw failed_constructor();
	}
}

PortInsert::PortInsert (Session& s, const XMLNode& node)
	: Redirect (s, "unnamed port insert", PreFader)
{
	if (set_state (node)) {
		throw failed_constructor();
	}

	InsertCreated (this); /* EMIT SIGNAL */
}

PortInsert::~PortInsert ()
{
	GoingAway ();
}

void
PortInsert::run (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset)
{
	if (_io->n_outputs().n_total() == 0) {
		return;
	}

	if (!active()) {
		/* deliver silence */
		_io->silence (nframes, offset);
		return;
	}

	_io->deliver_output(bufs, start_frame, end_frame, nframes, offset);

	_io->collect_input(bufs, nframes, offset);
}

XMLNode&
PortInsert::get_state(void)
{
	return state (true);
}

XMLNode&
PortInsert::state (bool full)
{
	XMLNode& node = Redirect::state(full);
	char buf[32];
	node.add_property ("type", "port");
	snprintf (buf, sizeof (buf), "%" PRIu32, bitslot);
	node.add_property ("bitslot", buf);

	return node;
}

int
PortInsert::set_state(const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	const XMLProperty *prop;

	if ((prop = node.property ("type")) == 0) {
		error << _("XML node describing port insert is missing the `type' field") << endmsg;
		return -1;
	}
	
	if (prop->value() != "port") {
		error << _("non-port insert XML used for port plugin insert") << endmsg;
		return -1;
	}

	if ((prop = node.property ("bitslot")) == 0) {
		bitslot = _session.next_insert_id();
	} else {
		sscanf (prop->value().c_str(), "%" PRIu32, &bitslot);
		_session.mark_insert_id (bitslot);
	}

	const XMLNode* insert_node = &node;

	// legacy sessions: search for child Redirect node
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "Redirect") {
			insert_node = *niter;
			break;
		}
	}
	
	Redirect::set_state (*insert_node);

	return 0;
}

ARDOUR::nframes_t 
PortInsert::latency() 
{
	/* because we deliver and collect within the same cycle,
	   all I/O is necessarily delayed by at least frames_per_cycle().

	   if the return port for insert has its own latency, we
	   need to take that into account too.
	*/

	return _session.engine().frames_per_cycle() + _io->input_latency();
}

bool
PortInsert::can_support_input_configuration (ChanCount in) const
{
	if (_io->input_maximum() == ChanCount::INFINITE && _io->output_maximum() == ChanCount::INFINITE) {

		/* not configured yet */

		return true; /* we can support anything the first time we're asked */

	} else {

		/* the "input" config for a port insert corresponds to how
		   many output ports it will have.
		*/

		if (_io->output_maximum() == in) {

			return true;
		} 
	}

	return false;
}

ChanCount
PortInsert::output_for_input_configuration (ChanCount in) const
{
	return in;
}

bool
PortInsert::configure_io (ChanCount in, ChanCount out)
{
	/* do not allow configuration to be changed outside the range of
	   the last request config. or something like that.
	*/


	/* this is a bit odd: 

	   the number of inputs we are required to handle corresponds 
	   to the number of output ports we need.

	   the number of outputs we are required to have corresponds
	   to the number of input ports we need.
	*/

	_io->set_output_maximum (in);
	_io->set_output_minimum (in);
	_io->set_input_maximum (out);
	_io->set_input_minimum (out);

	bool success = (_io->ensure_io (out, in, false, this) == 0);

	if (success)
		return Insert::configure_io(in, out);
	else
		return false;
}

ChanCount
PortInsert::output_streams() const
{
	return _io->n_inputs ();
}

ChanCount
PortInsert::input_streams() const
{
	return _io->n_outputs ();
}

