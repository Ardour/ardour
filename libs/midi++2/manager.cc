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

#include <glib.h>

#include "pbd/error.h"

#include "midi++/types.h"
#include "midi++/manager.h"
#include "midi++/factory.h"
#include "midi++/channel.h"

using namespace std;
using namespace MIDI;
using namespace PBD;

/* XXX check for strdup leaks */

Manager *Manager::theManager = 0;

Manager::Manager () 
{
	inputPort = 0;
	outputPort = 0;
	inputChannelNumber = 0;
	outputChannelNumber = 0;
	api_data = 0;
}

Manager::~Manager ()
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		delete *p;
	}

	if (theManager == this) {
		theManager = 0;
	}
}

Port *
Manager::add_port (const XMLNode& node)
{
	Port::Descriptor desc (node);
	PortFactory factory;
	Port *port;
	PortList::iterator p;

	for (p = _ports.begin(); p != _ports.end(); ++p) {

		if (desc.tag == (*p)->name()) {
			break;
		} 

		if (!PortFactory::ignore_duplicate_devices (desc.type)) {
			if (desc.device == (*p)->device()) {
				/* If the existing is duplex, and this request
				   is not, then fail, because most drivers won't
				   allow opening twice with duplex and non-duplex
				   operation.
				*/

				if ((desc.mode == O_RDWR && port->mode() != O_RDWR) ||
				    (desc.mode != O_RDWR && port->mode() == O_RDWR)) {
					break;
				}
			}
		}
	}

	if (p != _ports.end()) {
		return 0;
	}
	
	port = factory.create_port (node, api_data);
	
	if (port == 0) {
		return 0;
	}

	if (!port->ok()) {
		delete port;
		return 0;
	}

	_ports.push_back (port);

	/* first port added becomes the default input
	   port.
	*/

	if (inputPort == 0) {
		inputPort = port;
	} 

	if (outputPort == 0) {
		outputPort = port;
	}

	return port;
}

int 
Manager::remove_port (Port* port)
{
	if (inputPort == port) {
		inputPort = 0;
	}
	if (outputPort == port) {
		outputPort = 0;
	}
	_ports.remove (port);
	delete port;
	return 0;
}

int
Manager::set_input_port (string tag)
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		if ((*p)->name() == tag) {
			inputPort = (*p);
			return 0;
		}
	}

	return -1;
}

int
Manager::set_output_port (string tag)
{
	PortList::iterator p;

	for (p = _ports.begin(); p != _ports.end(); ++p) {
		if ((*p)->name() == tag) {
			inputPort = (*p);
			break;
		}
	}

	if (p == _ports.end()) {
		return -1;
	}

	// XXX send a signal to say we're about to change output ports

	if (outputPort) {
		for (channel_t chan = 0; chan < 16; chan++) {
			outputPort->channel (chan)->all_notes_off (0);
		}
	}
	
	outputPort = (*p);

	// XXX send a signal to say we've changed output ports

	return 0;
}

Port *
Manager::port (string name)
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		if (name == (*p)->name()) {
			return (*p);
		}
	}

	return 0;
}

int
Manager::foreach_port (int (*func)(const Port &, size_t, void *),
			   void *arg)
{
	int n = 0;
		
	for (PortList::const_iterator p = _ports.begin(); p != _ports.end(); ++p, ++n) {
		int retval;

		if ((retval = func (**p, n, arg)) != 0) {
			return retval;
		}
	}

	return 0;
}

void
Manager::cycle_start(nframes_t nframes)
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		(*p)->cycle_start (nframes);
	}
}

void
Manager::cycle_end()
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		(*p)->cycle_end ();
	}
}


int
Manager::get_known_ports (vector<PortSet>& ports)
{
	return PortFactory::get_known_ports (ports);
}
