/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __ardour_internal_port_h__
#define __ardour_internal_port_h__

#include <list>

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/port.h>

namespace ARDOUR {

class AudioEngine;
class Buffer;

/** Abstract class representing internal (ardour<->ardour only) ports
 */
class InternalPort : public virtual Port {
   public:

	~InternalPort();

	std::string short_name();
	
	int set_name (std::string str);

	int connected () const;

	int reestablish ();
	
	bool connected_to (const std::string& portname) const;

	const char ** get_connections () const;
	bool monitoring_input () const { return false; }

	void ensure_monitor_input (bool yn) {}
	void request_monitor_input (bool yn) {}

	nframes_t latency () const { return _latency; }
	nframes_t total_latency() const { return _latency; }

	void set_latency (nframes_t nframes);

	static void connect (InternalPort& src, InternalPort& dst);
	static void disconnect (InternalPort& a, InternalPort& b);

  protected:
	friend class AudioEngine;

	InternalPort (const std::string&, DataType type, Flags flags);

 	int disconnect ();
	void recompute_total_latency() const;
	
	std::list<InternalPort*> _connections;
	nframes_t _latency;

	static AudioEngine* engine;
	static void set_engine (AudioEngine* e);
};
 
} // namespace ARDOUR

#endif /* __ardour_internal_port_h__ */
