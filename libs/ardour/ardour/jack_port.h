/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __ardour_jack_port_h__
#define __ardour_jack_port_h__

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/port.h>
#include <jack/jack.h>

namespace ARDOUR {

class AudioEngine;
class Buffer;

/** Abstract class representing JACK ports
 */
class JackPort : public virtual Port {
   public:

	~JackPort();

	std::string short_name() { 
		return jack_port_short_name (_port);
	}
	
	int set_name (std::string str);

	bool is_mine () const;

	int connected () const {
		return jack_port_connected (_port);
	}
	
	int reestablish ();
	
	bool connected_to (const std::string& portname) const {
		return jack_port_connected_to (_port, portname.c_str());
	}

	const char ** get_connections () const {
		return jack_port_get_connections (_port);
	}
	
	bool monitoring_input () const {
		return jack_port_monitoring_input (_port);
	}

	void ensure_monitor_input (bool yn) {

#ifdef HAVE_JACK_PORT_ENSURE_MONITOR
		jack_port_ensure_monitor (_port, yn);
#else
		jack_port_request_monitor(_port, yn);
#endif

	}

	/*XXX completely bloody useless imho*/
	void request_monitor_input (bool yn) {
		jack_port_request_monitor (_port, yn);
	}

	nframes_t latency () const {
		return jack_port_get_latency (_port);
	}

	nframes_t total_latency() const;

	void set_latency (nframes_t nframes) {
		jack_port_set_latency (_port, nframes);
	}


  protected:
	friend class AudioEngine;

	JackPort (const std::string&, DataType type, Flags flags);
	jack_port_t*  _port;

 	int disconnect ();
	void recompute_total_latency() const;

	static void set_engine (AudioEngine*);
	static AudioEngine* engine;
};
 
} // namespace ARDOUR

#endif /* __ardour_jack_port_h__ */
