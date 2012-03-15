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

IOProcessor::IOProcessor (Session& s, bool with_input, bool with_output,
			  const string& proc_name, const string io_name, DataType dtype)
	: Processor(s, proc_name)
{
	/* these are true in this constructor whether we actually create the associated
	   IO objects or not.
	*/

	_own_input = true;
	_own_output = true;

	if (with_input) {
		_input.reset (new IO(s, io_name.empty() ? proc_name : io_name, IO::Input, dtype));
	}

	if (with_output) {
		_output.reset (new IO(s, io_name.empty() ? proc_name : io_name, IO::Output, dtype));
	}
}

/* create an IOProcessor that proxies to an existing IO object */

IOProcessor::IOProcessor (Session& s, boost::shared_ptr<IO> in, boost::shared_ptr<IO> out,
			  const string& proc_name, DataType /*dtype*/)
	: Processor(s, proc_name)
	, _input (in)
	, _output (out)
{
	if (in) {
		_own_input = false;
	} else {
		_own_input = true;
	}

	if (out) {
		_own_output = false;
	} else {
		_own_output = true;
	}
}

IOProcessor::~IOProcessor ()
{
}

void
IOProcessor::set_input (boost::shared_ptr<IO> io)
{
	/* CALLER MUST HOLD PROCESS LOCK */

	_input = io;
	_own_input = false;
}

void
IOProcessor::set_output (boost::shared_ptr<IO> io)
{
	/* CALLER MUST HOLD PROCESS LOCK */

	_output = io;
	_own_output = false;
}

XMLNode&
IOProcessor::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));

	if (_own_input) {
		node.add_property ("own-input", "yes");
		if (_input) {
			XMLNode& i (_input->state (full_state));
			// i.name() = X_("output");
			node.add_child_nocopy (i);
		}
	} else {
		node.add_property ("own-input", "no");
		if (_input) {
			node.add_property ("input", _input->name());
		}
	}

	if (_own_output) {
		node.add_property ("own-output", "yes");
		if (_output) {
			XMLNode& o (_output->state (full_state));
			node.add_child_nocopy (o);
		}
	} else {
		node.add_property ("own-output", "no");
		if (_output) {
			node.add_property ("output", _output->name());
		}
	}

	return node;
}

int
IOProcessor::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	const XMLProperty *prop;
	const XMLNode *io_node = 0;

	Processor::set_state(node, version);


	if ((prop = node.property ("own-input")) != 0) {
		_own_input = string_is_affirmative (prop->value());
	}

	if ((prop = node.property ("own-output")) != 0) {
		_own_output = string_is_affirmative (prop->value());
	}

	/* don't attempt to set state for a proxied IO that we don't own */

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	const string instr = enum_2_string (IO::Input);
	const string outstr = enum_2_string (IO::Output);
	
	if (_own_input) {
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			const XMLProperty* prop;
			if ((prop = (*niter)->property ("name")) != 0) {
				if (_name == prop->value()) {
					if ((prop = (*niter)->property ("direction")) != 0) {
						if (prop->value() == instr) {
							io_node = (*niter);
							break;
						}
					}
				}
			}
		}
		
		if (io_node) {
			_input->set_state(*io_node, version);
			
			// legacy sessions: use IO name
			if ((prop = node.property ("name")) == 0) {
				set_name (_input->name());
			}
			
		} else {
			/* no input, which is OK */
		}
		
	}
	
	if (_own_output) {
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			if ((*niter)->name() == "IO") {
				const XMLProperty* prop;
				if ((prop = (*niter)->property ("name")) != 0) {
					if (_name == prop->value()) {
						if ((prop = (*niter)->property ("direction")) != 0) {
							if (prop->value() == outstr) {
								io_node = (*niter);
								break;
							}
						}
					}
				}
			}
		}
		
		if (io_node) {
			_output->set_state(*io_node, version);
			
			// legacy sessions: use IO name
			if ((prop = node.property ("name")) == 0) {
				set_name (_output->name());
			}
		} else {
			/* no output, which is OK */
		}
	}

	return 0;
}

int
IOProcessor::set_state_2X (const XMLNode& node, int version)
{
	_own_input = _own_output = true;

	Processor::set_state_2X (node, version);

	return 0;
}

void
IOProcessor::silence (framecnt_t nframes)
{
	if (_own_output && _output) {
		_output->silence (nframes);
	}
}

void
IOProcessor::increment_port_buffer_offset (pframes_t offset)
{
        if (_own_output && _output) {
                _output->increment_port_buffer_offset (offset);
        }
}

ChanCount
IOProcessor::natural_output_streams() const
{
	return _output ? _output->n_ports() : ChanCount::ZERO;
}

ChanCount
IOProcessor::natural_input_streams () const
{
	return _input ? _input->n_ports() : ChanCount::ZERO;
}

bool
IOProcessor::set_name (const std::string& name)
{
	bool ret = SessionObject::set_name (name);

	if (ret && _own_input && _input) {
		ret = _input->set_name (name);
	}

	if (ret && _own_output && _output) {
		ret = _output->set_name (name);
	}

	return ret;
}

bool
IOProcessor::feeds (boost::shared_ptr<Route> other) const
{
	return _output && _output->connected_to (other->input());
}

void
IOProcessor::disconnect ()
{
	if (_input) {
		_input->disconnect (this);
	}

	if (_output) {
		_output->disconnect (this);
	}
}

/** Set up the XML description of a send so that we will not
 *  reset its name or bitslot during ::set_state()
 *  @param state XML send state.
 *  @param session Session.
 */
void
IOProcessor::prepare_for_reset (XMLNode &state, const std::string& name)
{
	state.add_property ("ignore-bitslot", "1");
	state.add_property ("ignore-name", "1");

	XMLNode* io_node = state.child (IO::state_node_name.c_str());

	if (io_node) {
		io_node->add_property ("name", name);
	}
}
