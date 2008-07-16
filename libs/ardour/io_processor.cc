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

#include <pbd/xml++.h>
#include <pbd/enumwriter.h>

#include <ardour/io_processor.h>
#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/send.h>
#include <ardour/port_insert.h>
#include <ardour/plugin_insert.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

IOProcessor::IOProcessor (Session& s, const string& name, Placement p,
                    int input_min, int input_max,
                    int output_min, int output_max)
	: Processor(s, name, p)
	, _io(new IO(s, name, input_min, input_max, output_min, output_max))
{
	_active = false;
	_sort_key = 0;
	_gui = 0;
	_extra_xml = 0;
}

IOProcessor::~IOProcessor ()
{
	notify_callbacks ();
}

XMLNode&
IOProcessor::state (bool full_state)
{
	XMLNode& node = Processor::state(full_state);
	
	node.add_child_nocopy (_io->state (full_state));

	return node;
}

int
IOProcessor::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	const XMLNode *io_node = 0;

	Processor::set_state(node);

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
			set_name(_io->name());
		}

	} else {
		error << _("XML node describing a redirect is missing an IO node") << endmsg;
		return -1;
	}

	return 0;
}

void
IOProcessor::silence (nframes_t nframes, nframes_t offset)
{
	_io->silence(nframes, offset);
}
