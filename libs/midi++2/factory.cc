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

#include <cassert>
#include <stdint.h>

#include "pbd/error.h"
#include "pbd/convert.h"

#include "midi++/types.h"
#include "midi++/factory.h"

#include "midi++/jack.h"

using namespace std;
using namespace MIDI;
using namespace PBD;


bool 
PortFactory::ignore_duplicate_devices (Port::Type type)
{
	bool ret = false;

	switch (type) {

	case Port::JACK_Midi:
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

std::string
PortFactory::default_port_type ()
{
	return "jack";
}

Port::Type
PortFactory::string_to_type (const string& xtype)
{
	if (strings_equal_ignore_case (xtype, JACK_MidiPort::typestring)) {
		return Port::JACK_Midi;
	}

	return Port::Unknown;
}

string
PortFactory::mode_to_string (int mode)
{
	if (mode == O_RDONLY) {
		return "input";
	} else if (mode == O_WRONLY) {
		return "output";
	} 

	return "duplex";
}

int
PortFactory::string_to_mode (const string& str)
{
	if (strings_equal_ignore_case (str, "output") || strings_equal_ignore_case (str, "out")) {
		return O_WRONLY;
	} else if (strings_equal_ignore_case (str, "input") || strings_equal_ignore_case (str, "in")) {
		return O_RDONLY;
	}

	return O_RDWR;
}
