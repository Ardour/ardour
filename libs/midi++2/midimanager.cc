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
#include <midi++/port_request.h>

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
Manager::add_port (PortRequest &req)

{
	PortFactory factory;
	Port *port;
	PortMap::iterator existing;
	pair<string, Port *> newpair;

	if (!PortFactory::ignore_duplicate_devices (req.type)) {

		if ((existing = ports_by_device.find (req.devname)) != ports_by_device.end()) {
			
			port = (*existing).second;
			
			if (port->mode() == req.mode) {
				
				/* Same mode - reuse the port, and just
				   create a new tag entry.
				*/
				
				newpair.first = req.tagname;
				newpair.second = port;
				
				ports_by_tag.insert (newpair);
				return port;
			}
			
			/* If the existing is duplex, and this request
			   is not, then fail, because most drivers won't
			   allow opening twice with duplex and non-duplex
			   operation.
			*/
			
			if ((req.mode == O_RDWR && port->mode() != O_RDWR) ||
			    (req.mode != O_RDWR && port->mode() == O_RDWR)) {
				error << "MIDIManager: port tagged \""
				      << req.tagname
				      << "\" cannot be opened duplex and non-duplex"
				      << endmsg;
				return 0;
			}
			
			/* modes must be different or complementary */
		}
	}
	
	port = factory.create_port (req);
	
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
Manager::remove_port (string name)
{
	PortMap::iterator res;

	if ((res = ports_by_device.find (name)) == ports_by_device.end()) {
		return -1;
	}
	
	ports_by_device.erase (res);
	ports_by_device.erase ((*res).second->name());

	delete (*res).second;

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
Manager::set_input_port (size_t portnum)

{
	PortMap::iterator res;

	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); res++) {
		if ((*res).second->number() == portnum) {
			inputPort = (*res).second;
			return 0;
		}
	}

	return -1;
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

int
Manager::set_output_port (size_t portnum)

{
	PortMap::iterator res;

	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); res++) {
		if ((*res).second->number() == portnum) {
			outputPort = (*res).second;
			return 0;
		}
	}

	return -1;
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

Port *
Manager::port (size_t portnum)

{
	PortMap::iterator res;

	for (res = ports_by_tag.begin(); res != ports_by_tag.end(); res++) {
		if ((*res).second->number() == portnum) {
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
Manager::parse_port_request (string str, Port::Type type)
{
	PortRequest *req;
	string::size_type colon;
	string tag;

	if (str.length() == 0) {
		error << "MIDI: missing port specification" << endmsg;
		return -1;
	}

	/* Port specifications look like:

	   devicename
	   devicename:tagname
	   devicename:tagname:mode

	   where 

	   "devicename" is the full path to the requested file
	   
	   "tagname" (optional) is the name used to refer to the
		         port. If not given, g_path_get_basename (devicename)
			 will be used.

	   "mode" (optional) is either "r" or "w" or something else.
		        if it is "r", the port will be opened
			read-only, if "w", the port will be opened
			write-only. Any other value, or no mode
			specification at all, will cause the port to
			be opened for reading and writing.
	*/
			
	req = new PortRequest;
	colon = str.find_first_of (':');

	if (colon != string::npos) {
		req->devname = strdup (str.substr (0, colon).c_str());
	} else {
		req->devname = strdup (str.c_str());
	}

	if (colon < str.length()) {

		tag = str.substr (colon+1);

		/* see if there is a mode specification in the tag part */
		
		colon = tag.find_first_of (':');

		if (colon != string::npos) {
			string modestr;

			req->tagname = strdup (tag.substr (0, colon).c_str());

			modestr = tag.substr (colon+1);
			if (modestr == "r") {
				req->mode = O_RDONLY;
			} else if (modestr == "w") {
				req->mode = O_WRONLY;
			} else {
				req->mode = O_RDWR;
			}

		} else {
			req->tagname = strdup (tag.c_str());
			req->mode = O_RDWR;
		}

	} else {
                // check when tagname is freed
		req->tagname = g_path_get_basename (req->devname);
		req->mode = O_RDWR;
	}

	req->type = type;

	if (MIDI::Manager::instance()->add_port (*req) == 0) {
		return -1;
	}

	return 0;
}
