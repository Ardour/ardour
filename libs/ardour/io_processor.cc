/*
 * Copyright (C) 2001-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <list>
#include <string>

#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/chan_count.h"
#include "ardour/data_type.h"
#include "ardour/io.h"
#include "ardour/io_processor.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "ardour/session_object.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR { class Session; }

/* create an IOProcessor that proxies to a new IO object */

IOProcessor::IOProcessor (Session& s, bool with_input, bool with_output,
			  const string& proc_name, const string io_name, DataType dtype, bool sendish)
	: Processor(s, proc_name)
{
	/* these are true in this constructor whether we actually create the associated
	   IO objects or not.
	*/

	_own_input = true;
	_own_output = true;

	if (with_input) {
		_input.reset (new IO(s, io_name.empty() ? proc_name : io_name, IO::Input, dtype, sendish));
	}

	if (with_output) {
		_output.reset (new IO(s, io_name.empty() ? proc_name : io_name, IO::Output, dtype, sendish));
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
IOProcessor::state ()
{
	XMLNode& node (Processor::state ());

	node.set_property ("own-input", _own_input);

	if (_input) {
		if (_own_input) {
			XMLNode& i (_input->get_state ());
			// i.name() = X_("output");
			node.add_child_nocopy (i);
		} else {
			node.set_property ("input", _input->name ());
		}
	}

	node.set_property ("own-output", _own_output);

	if (_output) {
		if (_own_output) {
			XMLNode& o (_output->get_state ());
			node.add_child_nocopy (o);
		} else {
			node.set_property ("output", _output->name ());
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

	XMLProperty const * prop;
	const XMLNode *io_node = 0;

	Processor::set_state(node, version);

	bool ignore_name = node.property ("ignore-name");

	node.get_property ("own-input", _own_input);
	node.get_property ("own-output", _own_output);

	/* don't attempt to set state for a proxied IO that we don't own */

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	const string instr = enum_2_string (IO::Input);
	const string outstr = enum_2_string (IO::Output);

	std::string str;
	if (_own_input && _input) {
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			if ((*niter)->get_property ("name", str) && (_name == str || ignore_name)) {
				if ((*niter)->get_property ("direction", str) && str == instr) {
					io_node = (*niter);
					break;
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

	if (_own_output && _output) {
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			if ((*niter)->name() == "IO") {
				if ((*niter)->get_property ("name", str) && (_name == str || ignore_name)) {
					if ((*niter)->get_property ("direction", str) && str == outstr) {
						io_node = (*niter);
						break;
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
IOProcessor::silence (samplecnt_t nframes, samplepos_t /* start_sample */)
{
	if (_own_output && _output) {
		_output->silence (nframes);
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
IOProcessor::set_name (const std::string& new_name)
{
	bool ret = true;

	if (name () == new_name) {
		return ret;
	}

	if (ret && _own_input && _input) {
		ret = _input->set_name (new_name);
	}

	if (ret && _own_output && _output) {
		ret = _output->set_name (new_name);
	}

	if (ret) {
		ret = SessionObject::set_name (new_name); // never fails
		assert (ret);
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
	state.set_property ("ignore-bitslot", true);
	state.set_property ("ignore-name", true);

	XMLNodeList nlist = state.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == IO::state_node_name.c_str()) {
			IO::prepare_for_reset (**niter, name);
		}
	}
}
