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

#include <pbd/error.h>

#include <midi++/types.h>
#include <midi++/manager.h>
#include <midi++/factory.h>
#include <midi++/channel.h>

using namespace std;
using namespace MIDI;
using namespace PBD;

/* XXX check for strdup leaks */

Manager *Manager::theManager = 0;

sigc::signal0<void> Manager::PreRead;

Manager::Manager () 
{
	inputPort = 0;
	outputPort = 0;
	inputChannelNumber = 0;
	outputChannelNumber = 0;
}

Manager::~Manager ()

{
	PortMap::iterator i;

	for (i = ports_by_device.begin(); i != ports_by_device.end(); i++) {
		delete (*i).second;
	}

	ports_by_device.erase (ports_by_device.begin(), ports_by_device.end());
	ports_by_tag.erase (ports_by_tag.begin(), ports_by_tag.end());

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
	PortMap::iterator existing;
	pair<string, Port *> newpair;

	/* do not allow multiple ports with the same tag. if attempted, just return the existing
	   port with the same tag. XXX this is really caused by the mess of setup_midi() being
	   called twice in Ardour, once in the global init() function and once after the user RC file
	   has been loaded (there may be extra ports in it). 
	 */

	if ((existing = ports_by_tag.find (desc.tag)) != ports_by_tag.end()) {

		port = (*existing).second;
		
		if (port->mode() == desc.mode) {
			
			/* Same mode - reuse the port, and just
			   create a new tag entry.
			*/
			
			newpair.first = desc.tag;
			newpair.second = port;
			
			ports_by_tag.insert (newpair);
			return port;
		}
		
		/* If the existing is duplex, and this request
		   is not, then fail, because most drivers won't
		   allow opening twice with duplex and non-duplex
		   operation.
		*/
		
		if ((desc.mode == O_RDWR && port->mode() != O_RDWR) ||
		    (desc.mode != O_RDWR && port->mode() == O_RDWR)) {
			error << "MIDIManager: port tagged \""
			      << desc.tag
			      << "\" cannot be opened duplex and non-duplex"
			      << endmsg;
			return 0;
		}
		
		/* modes must be different or complementary */
	}

	if (!PortFactory::ignore_duplicate_devices (desc.type)) {

		if ((existing = ports_by_device.find (desc.device)) != ports_by_device.end()) {
			
			port = (*existing).second;
			
			if (port->mode() == desc.mode) {
				
				/* Same mode - reuse the port, and just
				   create a new tag entry.
				*/
				
				newpair.first = desc.tag;
				newpair.second = port;
				
				ports_by_tag.insert (newpair);
				return port;
			}
			
			/* If the existing is duplex, and this request
			   is not, then fail, because most drivers won't
			   allow opening twice with duplex and non-duplex
			   operation.
			*/
			
			if ((desc.mode == O_RDWR && port->mode() != O_RDWR) ||
			    (desc.mode != O_RDWR && port->mode() == O_RDWR)) {
				error << "MIDIManager: port tagged \""
				      << desc.tag
				      << "\" cannot be opened duplex and non-duplex"
				      << endmsg;
				return 0;
			}
			
			/* modes must be different or complementary */
		}
	}
	
	port = factory.create_port (node);
	
	if (port == 0) {
		return 0;
	}

	if (!port->ok()) {
		delete port;
		return 0;
	}

	newpair.first = port->name();
	newpair.second = port;
	ports_by_tag.insert (newpair);

	newpair.first = port->device();
	newpair.second = port;
	ports_by_device.insert (newpair);

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
	PortMap::iterator res;

	for (res = ports_by_device.begin(); res != ports_by_device.end(); ) {
		PortMap::iterator tmp;
		tmp = res;
		++tmp;
		if (res->second == port) {
			ports_by_device.erase (res);
		} 
		res = tmp;
	}


	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); ) {
		PortMap::iterator tmp;
		tmp = res;
		++tmp;
		if (res->second == port) {
			ports_by_tag.erase (res);
		} 
		res = tmp;
	}
	
	delete port;

	return 0;
}

int
Manager::set_input_port (string tag)
{
	PortMap::iterator res;
	bool found = false;

	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); res++) {
		if (tag == (*res).first) {
			found = true;
			break;
		}
	}
	
	if (!found) {
		return -1;
	}

	inputPort = (*res).second;

	return 0;
}

int
Manager::set_output_port (string tag)

{
	PortMap::iterator res;
	bool found = false;

	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); res++) {
		if (tag == (*res).first) {
			found = true;
			break;
		}
	}
	
	if (!found) {
		return -1;
	}

	// XXX send a signal to say we're about to change output ports

	if (outputPort) {
		for (channel_t chan = 0; chan < 16; chan++) {
			outputPort->channel (chan)->all_notes_off ();
		}
	}
	outputPort = (*res).second;

	// XXX send a signal to say we've changed output ports

	return 0;
}

Port *
Manager::port (string name)
{
	PortMap::iterator res;

	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); res++) {
		if (name == (*res).first) {
			return (*res).second;
		}
	}

	return 0;
}

int
Manager::foreach_port (int (*func)(const Port &, size_t, void *),
			   void *arg)
{
	PortMap::const_iterator i;
	int retval;
	int n;
		
	for (n = 0, i = ports_by_device.begin(); 
	            i != ports_by_device.end(); i++, n++) {

		if ((retval = func (*((*i).second), n, arg)) != 0) {
			return retval;
		}
	}

	return 0;
}


int
Manager::get_known_ports (vector<PortSet>& ports)
{
	return PortFactory::get_known_ports (ports);
}
