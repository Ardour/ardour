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

#include <ardour/redirect.h>
#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/send.h>
#include <ardour/port_insert.h>
#include <ardour/plugin_insert.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Redirect::Redirect (Session& s, const string& name, Placement p,
                    int input_min, int input_max,
                    int output_min, int output_max)
	: Insert(s, name, p)
	, _io(new IO(s, name, input_min, input_max, output_min, output_max))
{
	_active = false;
	_sort_key = 0;
	_gui = 0;
	_extra_xml = 0;
}

Redirect::~Redirect ()
{
	notify_callbacks ();
}

XMLNode&
Redirect::state (bool full_state)
{
	XMLNode& node = Insert::state(full_state);
	
	node.add_child_nocopy (_io->state (full_state));

	return node;
}

int
Redirect::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	Insert::set_state(node);

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	bool have_io = false;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == IO::state_node_name) {
			have_io = true;
			_io->set_state(**niter);

			// legacy sessions: use IO name
			if ((prop = node.property ("name")) == 0) {
				set_name(_io->name());
			}
		}
	}

	if (!have_io) {
		error << _("XML node describing a redirect is missing an IO node") << endmsg;
		return -1;
	}

	return 0;
}

void
Redirect::silence (nframes_t nframes, nframes_t offset)
{
	_io->silence(nframes, offset);
}
