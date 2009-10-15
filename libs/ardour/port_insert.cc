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

#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"

#include "ardour/delivery.h"
#include "ardour/port_insert.h"
#include "ardour/plugin.h"
#include "ardour/port.h"
#include "ardour/route.h"
#include "ardour/buffer_set.h"

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PortInsert::PortInsert (Session& s, boost::shared_ptr<MuteMaster> mm)
	: IOProcessor (s, true, true, string_compose (_("insert %1"), (bitslot = s.next_insert_id()) + 1), "")
	, _out (new Delivery (s, _output, mm, _name, Delivery::Insert))
{
	ProcessorCreated (this); /* EMIT SIGNAL */
}

PortInsert::PortInsert (Session& s, boost::shared_ptr<MuteMaster> mm, const XMLNode& node)
	: IOProcessor (s, true, true, "unnamed port insert")
	, _out (new Delivery (s, _output, mm, _name, Delivery::Insert))

{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}

	ProcessorCreated (this); /* EMIT SIGNAL */
}

PortInsert::~PortInsert ()
{
	GoingAway ();
}

void
PortInsert::run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	if (_output->n_ports().n_total() == 0) {
		return;
	}

	if (!_active && !_pending_active) {
		/* deliver silence */
		silence (nframes);
		goto out;
	}

	_out->run (bufs, start_frame, end_frame, nframes);
	_input->collect_input (bufs, nframes, ChanCount::ZERO);

  out:
	_active = _pending_active;
}

XMLNode&
PortInsert::get_state(void)
{
	return state (true);
}

XMLNode&
PortInsert::state (bool full)
{
	XMLNode& node = Processor::state(full);
	char buf[32];
	node.add_property ("type", "port");
	snprintf (buf, sizeof (buf), "%" PRIu32, bitslot);
	node.add_property ("bitslot", buf);

	return node;
}

int
PortInsert::set_state (const XMLNode& node, int version)
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

	// legacy sessions: search for child IOProcessor node
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "IOProcessor") {
			insert_node = *niter;
			break;
		}
	}

	Processor::set_state (*insert_node, version);

	return 0;
}

ARDOUR::nframes_t
PortInsert::signal_latency() const
{
	/* because we deliver and collect within the same cycle,
	   all I/O is necessarily delayed by at least frames_per_cycle().

	   if the return port for insert has its own latency, we
	   need to take that into account too.
	*/

	return _session.engine().frames_per_cycle() + _input->signal_latency();
}

bool
PortInsert::configure_io (ChanCount in, ChanCount out)
{
	/* for an insert, processor input corresponds to IO output, and vice versa */

	if (_input->ensure_io (in, false, this) != 0) {
		return false;
	}

	if (_output->ensure_io (out, false, this) != 0) {
		return false;
	}

	return Processor::configure_io (in, out);
}

bool
PortInsert::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
PortInsert::set_name (const std::string& name)
{
	bool ret = Processor::set_name (name);

	ret = (_input->set_name (name) || _output->set_name (name));

	return ret;
}
