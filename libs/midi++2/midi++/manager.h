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
#include <string>

#include <midi++/types.h>
#include <midi++/port.h>

namespace MIDI {

/** Creates, stores, and manages system MIDI ports.
 */
class Manager {
  public:
	~Manager ();
	
	void set_api_data(void* data) { api_data = data; }

	/** Signal the start of an audio cycle.
	 * This MUST be called before any reading/writing for this cycle.
	 * Realtime safe.
	 */
	void cycle_start(nframes_t nframes);

	/** Signal the end of an audio cycle.
	 * This signifies that the cycle began with @ref cycle_start has ended.
	 * This MUST be called at the end of each cycle.
	 * Realtime safe.
	 */
	void cycle_end();

	Port *add_port (PortRequest &);
	int   remove_port (std::string port);

	Port *port (std::string name);
	Port *port (size_t number);

	size_t    nports () { return ports_by_device.size(); }

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

	static int parse_port_request (std::string str, Port::Type type);

  private:
	/* This is a SINGLETON pattern */
	
	Manager ();

	static Manager *theManager;
	PortMap         ports_by_device; /* canonical */
	PortMap         ports_by_tag;    /* may contain duplicate Ports */

	void *api_data;

	void close_ports ();
};

} // namespace MIDI

#endif  // __midi_manager_h__
