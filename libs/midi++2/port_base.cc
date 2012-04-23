/*
    Copyright (C) 1998 Paul Barton-Davis
    
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

    $Id: port.cc 11871 2012-04-10 16:27:01Z paul $
*/
#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <errno.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/convert.h"
#include "pbd/strsplit.h"
#include "pbd/stacktrace.h"

#include "midi++/types.h"
#include "midi++/port_base.h"
#include "midi++/channel.h"

using namespace MIDI;
using namespace std;
using namespace PBD;

string PortBase::state_node_name = "MIDI-port";

PortBase::PortBase (string const & name, Flags flags)
	: _flags (flags)
	, _centrally_parsed (true)
{
	init (name, flags);
}

PortBase::PortBase (const XMLNode& node)
	: _centrally_parsed (true)
{
	
	Descriptor desc (node);

	init (desc.tag, desc.flags);

	set_state (node);
}

void
PortBase::init (string const & name, Flags flags)
{
	_ok = false;  /* derived class must set to true if constructor
			 succeeds.
		      */

	_parser = 0;

	_tagname = name;
	_flags = flags;

	_parser = new Parser (*this);

	for (int i = 0; i < 16; i++) {
		_channel[i] = new Channel (i, *this);
		_channel[i]->connect_signals ();
	}
}

PortBase::~PortBase ()
{
	for (int i = 0; i < 16; i++) {
		delete _channel[i];
	}
}

/** Send a clock tick message.
 * \return true on success.
 */
bool
PortBase::clock (timestamp_t timestamp)
{
	static byte clockmsg = 0xf8;
	
	if (sends_output()) {
		return midimsg (&clockmsg, 1, timestamp);
	}
	
	return false;
}

std::ostream & MIDI::operator << ( std::ostream & os, const MIDI::PortBase & port )
{
	using namespace std;
	os << "MIDI::Port { ";
	os << "name: " << port.name();
	os << "; ";
	os << "ok: " << port.ok();
	os << "; ";
	os << " }";
	return os;
}

PortBase::Descriptor::Descriptor (const XMLNode& node)
{
	const XMLProperty *prop;
	bool have_tag = false;
	bool have_mode = false;

	if ((prop = node.property ("tag")) != 0) {
		tag = prop->value();
		have_tag = true;
	}

	if ((prop = node.property ("mode")) != 0) {

		if (strings_equal_ignore_case (prop->value(), "output") || strings_equal_ignore_case (prop->value(), "out")) {
			flags = IsOutput;
		} else if (strings_equal_ignore_case (prop->value(), "input") || strings_equal_ignore_case (prop->value(), "in")) {
			flags = IsInput;
		}

		have_mode = true;
	}

	if (!have_tag || !have_mode) {
		throw failed_constructor();
	}
}

XMLNode& 
PortBase::get_state () const
{
	XMLNode* root = new XMLNode (state_node_name);
	root->add_property ("tag", _tagname);

	if (_flags == IsInput) {
		root->add_property ("mode", "input");
	} else {
		root->add_property ("mode", "output");
	}
	
#if 0
	byte device_inquiry[6];

	device_inquiry[0] = 0xf0;
	device_inquiry[0] = 0x7e;
	device_inquiry[0] = 0x7f;
	device_inquiry[0] = 0x06;
	device_inquiry[0] = 0x02;
	device_inquiry[0] = 0xf7;
	
	write (device_inquiry, sizeof (device_inquiry), 0);
#endif

	return *root;
}

void
PortBase::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("tag")) == 0 || prop->value() != _tagname) {
		return;
	}
}

bool
PortBase::centrally_parsed() const
{
	return _centrally_parsed;
}
