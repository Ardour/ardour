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
#include <jack/jack.h>

namespace ARDOUR {

class AudioEngine;

class Port : public sigc::trackable {
   public:
	virtual ~Port() { 
		free (_port);
	}

	Sample *get_buffer (jack_nframes_t nframes) {
		if (_flags & JackPortIsOutput) {
			return _buffer;
		} else {
			return (Sample *) jack_port_get_buffer (_port, nframes);
		}
	}

	void reset_buffer () {
		if (_flags & JackPortIsOutput) {
			_buffer = (Sample *) jack_port_get_buffer (_port, 0);
		} else {
			_buffer = 0; /* catch illegal attempts to use it */
		}
		_silent = false;
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

	float                       peak_db() const { return _peak_db; }
	jack_default_audio_sample_t peak()    const { return _peak; }

	uint32_t short_overs () const { return _short_overs; }
	uint32_t long_overs ()  const { return _long_overs; }
	
	static void set_short_over_length (jack_nframes_t);
	static void set_long_over_length (jack_nframes_t);

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

	sigc::signal<void,bool> MonitorInputChanged;
	sigc::signal<void,bool> ClockSyncChanged;

	bool is_silent() const { return _silent; }

	/** Assumes that the port is an audio output port */
	void silence (jack_nframes_t nframes, jack_nframes_t offset) {
		if (!_silent) {
			memset (_buffer + offset, 0, sizeof (Sample) * nframes);
			if (offset == 0) {
				/* XXX this isn't really true, but i am not sure
				   how to set this correctly. we really just
				   want to set it true when the entire port
				   buffer has been overrwritten.
				*/
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
	
	/* engine isn't supposed to below here */

	Sample *_buffer;

	/* cache these 3 from JACK so that we can
	   access them for reconnecting.
	*/

	JackPortFlags _flags;
	std::string   _type;
	std::string   _name;

	bool                         _last_monitor : 1;
	bool                         _silent : 1;
	jack_port_t                 *_port;
	jack_nframes_t               _overlen;
	jack_default_audio_sample_t  _peak;
	float                        _peak_db;
	uint32_t                     _short_overs;
	uint32_t                     _long_overs;
	unsigned short               _metering;
	
	static jack_nframes_t        _long_over_length;
	static jack_nframes_t        _short_over_length;
};
 
} // namespace ARDOUR

#endif /* __ardour_port_h__ */
