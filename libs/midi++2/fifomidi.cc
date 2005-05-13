/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <midi++/types.h>
#include <midi++/fifomidi.h>

using namespace MIDI;

FIFO_MidiPort::FIFO_MidiPort (PortRequest &req) 
	: FD_MidiPort (req, ".", "midi")

{
}

void
FIFO_MidiPort::open (PortRequest &req)

{
	/* This is a placeholder for the fun-and-games I think we will
	   need to do with FIFO's.
	*/

	_fd = ::open (req.devname, req.mode|O_NDELAY);
}
