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

#include <cstdio>
#include <fcntl.h>

#include <pbd/xml++.h>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/channel.h>
#include <midi++/port_request.h>
#include <midi++/factory.h>

//using namespace Select;
using namespace MIDI;

size_t Port::nports = 0;

Port::Port (PortRequest &req)

{
	_ok = false;  /* derived class must set to true if constructor
			 succeeds.
		      */

	bytes_written = 0;
	bytes_read = 0;
	input_parser = 0;
	output_parser = 0;
	slowdown = 0;

	_devname = req.devname;
	_tagname = req.tagname;
	_mode = req.mode;

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

int
Port::clock ()
	
{
	static byte clockmsg = 0xf8;
	
	if (_mode != O_RDONLY) {
		return midimsg (&clockmsg, 1);
	}
	
	return 0;
}

/*
void
Port::selector_read_callback (Selectable *s, Select::Condition cond) 

{
	byte buf[64];
	read (buf, sizeof (buf));
}
*/

void
Port::xforms_read_callback (int cond, int fd, void *ptr) 

{
	byte buf[64];
	
	((Port *)ptr)->read (buf, sizeof (buf));
}

void
Port::gtk_read_callback (void *ptr, int fd, int cond)

{
	byte buf[64];
	
	((Port *)ptr)->read (buf, sizeof (buf));
}

void
Port::write_callback (byte *msg, unsigned int len, void *ptr)
	
{
	((Port *)ptr)->write (msg, len);
}

