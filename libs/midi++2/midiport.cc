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

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/channel.h>
#include <midi++/port_request.h>

using namespace Select;
using namespace MIDI;

size_t Port::nports = 0;

Port::Port (PortRequest &req)
	: _currently_in_cycle(false)
	, _nframes_this_cycle(0)
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
	_number = nports++;

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

