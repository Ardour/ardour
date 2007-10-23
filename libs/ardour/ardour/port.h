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

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/data_type.h>
#include <jack/jack.h>

namespace ARDOUR {

class AudioEngine;
class Buffer;

/** Abstract base for ports
 */
class Port : public virtual sigc::trackable {
   public:
	enum Flags {
		IsInput = JackPortIsInput,
		IsOutput = JackPortIsOutput,
		IsPhysical = JackPortIsPhysical,
		IsTerminal = JackPortIsTerminal,
		CanMonitor = JackPortCanMonitor
	};

	virtual ~Port() {}

	std::string name() const { 
		return _name;
	}


	Flags flags() const {
		return _flags;
	}

	bool receives_input() const {
		return _flags & IsInput;
	}

	bool sends_output () const {
		return _flags & JackPortIsOutput;
	}

	bool can_monitor () const {
		return _flags & CanMonitor;
	}

	void enable_metering() {
		_metering++;
	}
	
	void disable_metering () {
		if (_metering) { _metering--; }
	}
	
	virtual DataType type() const = 0;
	virtual void cycle_start(nframes_t nframes) {}
	virtual void cycle_end() {}
	virtual Buffer& get_buffer() = 0;
	virtual std::string short_name() = 0;
	virtual int set_name (std::string str) = 0;
	virtual int reestablish () = 0;
	virtual int connected () const = 0;
	virtual bool connected_to (const std::string& portname) const = 0;
	virtual const char ** get_connections () const = 0;
	virtual bool monitoring_input () const = 0;
	virtual void ensure_monitor_input (bool yn) = 0;
	virtual void request_monitor_input (bool yn) = 0;
	virtual nframes_t latency () const = 0;
	virtual nframes_t total_latency () const = 0;
	virtual void set_latency (nframes_t nframes) = 0;

	sigc::signal<void,bool> MonitorInputChanged;
	sigc::signal<void,bool> ClockSyncChanged;

  protected:
	friend class AudioEngine;

	Port (Flags);

	virtual int disconnect () = 0;
	virtual void recompute_total_latency() const = 0;
	virtual void reset ();
	
	/* engine isn't supposed to access below here */

	Flags _flags;
	std::string    _type;
	std::string    _name;
	unsigned short _metering;
	bool           _last_monitor;
};
 
} // namespace ARDOUR

#endif /* __ardour_port_h__ */
