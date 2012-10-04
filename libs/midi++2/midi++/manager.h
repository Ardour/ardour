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

*/

#ifndef __midi_manager_h__
#define __midi_manager_h__

#include <map>
#include <vector>

#include <string>

#include <midi++/types.h>
#include <midi++/port.h>

namespace MIDI {

class Manager {
  public:
	~Manager ();
	
	Port *add_port (const XMLNode& node);
	int   remove_port (Port*);

	Port *port (std::string name);

	size_t    nports () { return ports_by_device.size(); }

	/* defaults for clients who are not picky */
	
	Port *inputPort;
	Port *outputPort;
	channel_t inputChannelNumber;
	channel_t outputChannelNumber;

	int set_input_port (std::string);
	int set_output_port (std::string);
	int set_input_channel (channel_t);
	int set_output_channel (channel_t);

	int foreach_port (int (*func)(const Port &, size_t n, void *), 
			  void *arg);

	typedef std::map<std::string, Port *> PortMap;

	const PortMap& get_midi_ports() const { return ports_by_tag; } 

	static Manager *instance () {
		if (theManager == 0) {
			theManager = new Manager;
		}
		return theManager;
	}

	int get_known_ports (std::vector<PortSet>&);

        static sigc::signal0<void> PreRead;

  private:
	/* This is a SINGLETON pattern */
	
	Manager ();

	static Manager *theManager;
	PortMap         ports_by_device; /* canonical */
	PortMap         ports_by_tag;    /* may contain duplicate Ports */

	void close_ports ();
};

} // namespace MIDI

#endif  // __midi_manager_h__
