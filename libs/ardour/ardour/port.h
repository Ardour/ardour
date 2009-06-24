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

#ifndef __ardour_port_h__
#define __ardour_port_h__

#include <cstring>
#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <jack/jack.h>

namespace ARDOUR {

class AudioEngine;

class Port : public sigc::trackable {
   public:
	virtual ~Port() { 
		// Port is an opaque pointer, so should be be deallocated ??
	}

	static nframes_t port_offset() { return _port_offset; }

	static void set_port_offset (nframes_t off) {
		_port_offset = off;
	}
	static void increment_port_offset (nframes_t n) {
		_port_offset += n;
	}
	static void set_buffer_size (nframes_t sz) {
		_buffer_size = sz;
	}

	std::string name() { 
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

	const char* type() const {
		return _type.c_str();
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

	void reset_overs () {
		_short_overs = 0;
		_long_overs = 0;
		_overlen = 0;
	}

	void reset_peak_meter () {
		_peak = 0;
	}
	
	void reset_meters () {
		reset_peak_meter ();
		reset_overs ();
	}

	void enable_metering() {
		_metering++;
	}
	
	void disable_metering () {
		if (_metering) { _metering--; }
	}

	float  peak_db() const { return _peak_db; }
	Sample peak()    const { return _peak; }

	uint32_t short_overs () const { return _short_overs; }
	uint32_t long_overs ()  const { return _long_overs; }
	
	static void set_short_over_length (nframes_t);
	static void set_long_over_length (nframes_t);

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

	void set_latency (nframes_t nframes) {
		jack_port_set_latency (_port, nframes);
	}

	sigc::signal<void,bool> MonitorInputChanged;
	sigc::signal<void,bool> ClockSyncChanged;

	bool is_silent() const { return _silent; }

	/** Assumes that the port is an audio output port */
	void silence (nframes_t nframes) {
		if (!_silent) {
			memset ((Sample *) jack_port_get_buffer (_port, nframes) + _port_offset, 0, sizeof (Sample) * nframes);
			if (nframes == _buffer_size) {
				/* its a heuristic. its not perfect */
				_silent = true;
			}
		}
	}
	
	void mark_silence (bool yn) {
		_silent = yn;
	}

  private:
	friend class AudioEngine;

	Port (jack_port_t *port);
	void reset ();
	
	/* engine isn't supposed to use anything below here */

	/* cache these 3 from JACK so that we can
	   access them for reconnecting.
	*/

	JackPortFlags _flags;
	std::string   _type;
	std::string   _name;

	bool                         _last_monitor : 1;
	bool                         _silent : 1;
	jack_port_t                 *_port;
	nframes_t                    _overlen;
	Sample                       _peak;
	float                        _peak_db;
	uint32_t                     _short_overs;
	uint32_t                     _long_overs;
	unsigned short               _metering;

	static nframes_t        _long_over_length;
	static nframes_t        _short_over_length;

	static nframes_t _port_offset;
	static nframes_t _buffer_size;

  private:
	friend class IO;      // get_(input|output)_buffer
	friend class Session; // session_export.cc

	Sample *get_buffer (nframes_t nframes) {
		
		/* ignore requested size, because jack always
		   reads the entire port buffer. 
		*/

		return (Sample *) jack_port_get_buffer (_port, _buffer_size) + _port_offset;
	}


};
 
} // namespace ARDOUR

#endif /* __ardour_port_h__ */
