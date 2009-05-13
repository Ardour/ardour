/*
    Copyright (C) 2001 Paul Davis 

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

#include <fstream>
#include <algorithm>
#include <string>
#include <cerrno>
#include <unistd.h>
#include <sstream>

#include <sigc++/bind.h>

#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/io_processor.h"
#include "ardour/session.h"
#include "ardour/utils.h"
#include "ardour/send.h"
#include "ardour/port_insert.h"
#include "ardour/plugin_insert.h"
#include "ardour/io.h"
#include "ardour/route.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* create an IOProcessor that proxies to a new IO object */

IOProcessor::IOProcessor (Session& s, const string& proc_name, const string io_name, DataType dtype)
	: Processor(s, proc_name)
	, _io (new IO(s, io_name.empty() ? proc_name : io_name, dtype))
{
	_own_io = true;
}

/* create an IOProcessor that proxies to an existing IO object */

IOProcessor::IOProcessor (Session& s, IO* io, const string& proc_name, DataType dtype)
	: Processor(s, proc_name)
	, _io (io)
{
	_own_io = false;
}

IOProcessor::~IOProcessor ()
{
	notify_callbacks ();
}

void
IOProcessor::set_io (boost::shared_ptr<IO> io)
{
	/* CALLER MUST HOLD PROCESS LOCK */

	_io = io;
	_own_io = false;
}

XMLNode&
IOProcessor::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	
	if (_own_io) {
		node.add_child_nocopy (_io->state (full_state));
		node.add_property ("own-io", "yes");
	} else {
		node.add_property ("own-io", "no");
		node.add_property ("io", _io->name());
	}

	return node;
}

int
IOProcessor::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	const XMLNode *io_node = 0;

	Processor::set_state(node);

	if ((prop = node.property ("own-io")) != 0) {
		_own_io = prop->value() == "yes";
	}

	/* don't attempt to set state for a proxied IO that we don't own */

	if (!_own_io) {

		/* look up the IO object we're supposed to proxy to */

		if ((prop = node.property ("io")) == 0) {
			fatal << "IOProcessor has no named IO object" << endmsg;
			/*NOTREACHED*/
		}

		boost::shared_ptr<Route> r = _session.route_by_name (prop->value());

		if (!r) {
			fatal << string_compose ("IOProcessor uses an unknown IO object called %1", prop->value()) << endmsg;
			/*NOTREACHED*/
		}

		/* gotcha */

		_io = boost::static_pointer_cast<IO> (r);

		return 0;
	}

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == IO::state_node_name) {
			io_node = (*niter);
			break;
		} else if ((*niter)->name() == "Redirect") {
			XMLNodeList rlist = (*niter)->children();
			XMLNodeIterator riter;

			for (riter = rlist.begin(); riter != rlist.end(); ++riter) {
				if ( (*riter)->name() == IO::state_node_name) {
					warning << _("Found legacy IO in a redirect") << endmsg;
					io_node = (*riter);
					break;
				}
			}
		}
	}

	if (io_node) {
		_io->set_state(*io_node);

		// legacy sessions: use IO name
		if ((prop = node.property ("name")) == 0) {
			set_name (_io->name());
		}

	} else {
		error << _("XML node describing a redirect is missing an IO node") << endmsg;
		return -1;
	}

	return 0;
}

void
IOProcessor::silence (nframes_t nframes)
{
	if (_own_io) {
		_io->silence (nframes);
	}
}

ChanCount
IOProcessor::output_streams() const
{
	return _io->n_outputs();
}

ChanCount
IOProcessor::input_streams () const
{
	return _io->n_inputs();
}

ChanCount
IOProcessor::natural_output_streams() const
{
	return _io->n_outputs();
}

ChanCount
IOProcessor::natural_input_streams () const
{
	return _io->n_inputs();
}

void
IOProcessor::automation_snapshot (nframes_t now, bool force)
{
	if (_own_io) {
		_io->automation_snapshot(now, force);
	}
}

bool
IOProcessor::set_name (const std::string& name)
{
	bool ret = SessionObject::set_name (name);

	if (ret && _own_io) {
		ret = _io->set_name (name);
	}

	return ret;
}
