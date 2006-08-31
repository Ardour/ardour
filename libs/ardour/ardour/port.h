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

    $Id$
*/

#ifndef __ardour_port_h__
#define __ardour_port_h__

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/data_type.h>
#include <jack/jack.h>

namespace ARDOUR {

class AudioEngine;
class Buffer;

/** Abstract base for all outside ports (eg Jack ports)
 */
class Port : public sigc::trackable {
   public:
	virtual ~Port() { 
		free (_port);
	}

	virtual DataType type() const = 0;

	virtual void cycle_start(jack_nframes_t nframes) {}
	virtual void cycle_end() {}

	virtual Buffer& get_buffer() = 0;
	
	/** Silence/Empty the port, output ports only */
	virtual void silence (jack_nframes_t nframes, jack_nframes_t offset) = 0;


	std::string name() const { 
		return _name;
	}

	std::string short_name() { 
		return jack_port_short_name (_port);
	}
	
	int set_name (std::string str);

	JackPortFlags flags() const {
		return _flags;
	}

	bool is_mine (jack_client_t *client) { 
		return jack_port_is_mine (client, _port);
	}

	int connected () const {
		return jack_port_connected (_port);
	}
	
	bool connected_to (const std::string& portname) const {
		return jack_port_connected_to (_port, portname.c_str());
	}

	const char ** get_connections () const {
		return jack_port_get_connections (_port);
	}

	bool receives_input() const {
		return _flags & JackPortIsInput;
	}

	bool sends_output () const {
		return _flags & JackPortIsOutput;
	}
	
	bool monitoring_input () const {
		return jack_port_monitoring_input (_port);
	}

	bool can_monitor () const {
		return _flags & JackPortCanMonitor;
	}

	void enable_metering() {
		_metering++;
	}
	
	void disable_metering () {
		if (_metering) { _metering--; }
	}
	
	void ensure_monitor_input (bool yn) {

#ifdef WITH_JACK_PORT_ENSURE_MONITOR
		jack_port_ensure_monitor (_port, yn);
#else
		jack_port_request_monitor(_port, yn);
#endif

	}

	/*XXX completely bloody useless imho*/
	void request_monitor_input (bool yn) {
		jack_port_request_monitor (_port, yn);
	}

	jack_nframes_t latency () const {
		return jack_port_get_latency (_port);
	}

	void set_latency (jack_nframes_t nframes) {
		jack_port_set_latency (_port, nframes);
	}

	bool is_silent() const { return _silent; }

	void mark_silence (bool yn) {
		_silent = yn;
	}
	
	sigc::signal<void,bool> MonitorInputChanged;
	sigc::signal<void,bool> ClockSyncChanged;

  protected:
	friend class AudioEngine;

	Port (jack_port_t *port);
	
	virtual void reset ();
	
	/* engine isn't supposed to access below here */

	/* cache these 3 from JACK so we can access them for reconnecting */
	JackPortFlags _flags;
	std::string   _type;
	std::string   _name;

	jack_port_t*  _port;

	unsigned short _metering;

	bool          _last_monitor : 1;
	bool          _silent : 1;
};
 
} // namespace ARDOUR

#endif /* __ardour_port_h__ */
