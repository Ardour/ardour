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

    $Id$
*/
#include <iostream>
#include <cstdio>
#include <fcntl.h>

#include <pbd/xml++.h>
#include <pbd/failed_constructor.h>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/channel.h>
#include <midi++/factory.h>

using namespace MIDI;
using namespace std;

size_t Port::nports = 0;

Port::Port (const XMLNode& node)
	: _currently_in_cycle(false)
	, _nframes_this_cycle(0)
{
	Descriptor desc (node);

	_ok = false;  /* derived class must set to true if constructor
			 succeeds.
		      */

	bytes_written = 0;
	bytes_read = 0;
	input_parser = 0;
	output_parser = 0;
	slowdown = 0;

	_devname = desc.device;
	_tagname = desc.tag;
	_mode = desc.mode;

	if (_mode == O_RDONLY || _mode == O_RDWR) {
		input_parser = new Parser (*this);
	} else {
		input_parser = 0;
	}

	if (_mode == O_WRONLY || _mode == O_RDWR) {
		output_parser = new Parser (*this);
	} else {
		output_parser = 0;
	}

	for (int i = 0; i < 16; i++) {
		_channel[i] =  new Channel (i, *this);

		if (input_parser) {
			_channel[i]->connect_input_signals ();
		}

		if (output_parser) {
			_channel[i]->connect_output_signals ();
		}
	}
}


Port::~Port ()
{
	for (int i = 0; i < 16; i++) {
		delete _channel[i];
	}
}

/** Send a clock tick message.
 * \return true on success.
 */
bool
Port::clock (timestamp_t timestamp)
{
	static byte clockmsg = 0xf8;
	
	if (_mode != O_RDONLY) {
		return midimsg (&clockmsg, 1, timestamp);
	}
	
	return false;
}

void
Port::cycle_start (nframes_t nframes)
{
	_currently_in_cycle = true;
	_nframes_this_cycle = nframes;
}

void
Port::cycle_end ()
{
	_currently_in_cycle = false;
	_nframes_this_cycle = 0;
}

XMLNode&
Port::get_state () const
{
	XMLNode* node = new XMLNode ("MIDI-port");
	node->add_property ("tag", _tagname);
	node->add_property ("device", _devname);
	node->add_property ("mode", PortFactory::mode_to_string (_mode));
	node->add_property ("type", get_typestring());

	return *node;
}

void
Port::set_state (const XMLNode& node)
{
	// relax
}

void
Port::gtk_read_callback (void *ptr, int fd, int cond)
{
	byte buf[64];
	
	((Port *)ptr)->read (buf, sizeof (buf), 0);
}

void
Port::write_callback (byte *msg, unsigned int len, void *ptr)
	
{
	((Port *)ptr)->write (msg, len, 0);
}

std::ostream & MIDI::operator << ( std::ostream & os, const MIDI::Port & port )
{
	using namespace std;
	os << "MIDI::Port { ";
	os << "device: " << port.device();
	os << "; ";
	os << "name: " << port.name();
	os << "; ";
	os << "type: " << port.type();
	os << "; ";
	os << "mode: " << port.mode();
	os << "; ";
	os << "ok: " << port.ok();
	os << "; ";
	os << " }";
	return os;
}

Port::Descriptor::Descriptor (const XMLNode& node)
{
	const XMLProperty *prop;
	bool have_tag = false;
	bool have_device = false;
	bool have_type = false;
	bool have_mode = false;

	if ((prop = node.property ("tag")) != 0) {
		tag = prop->value();
		have_tag = true;
	}

	if ((prop = node.property ("device")) != 0) {
		device = prop->value();
		have_device = true;
	}

	if ((prop = node.property ("type")) != 0) {
		type = PortFactory::string_to_type (prop->value());
		have_type = true;
	}

	if ((prop = node.property ("mode")) != 0) {
		mode = PortFactory::string_to_mode (prop->value());
		have_mode = true;
	}

	if (!have_tag || !have_device || !have_type || !have_mode) {
		throw failed_constructor();
	}
}

