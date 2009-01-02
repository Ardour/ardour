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

#include <set>
#include <vector>
#include <string>
#include <cstring>
#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <pbd/destructible.h>
#include <ardour/ardour.h>
#include <ardour/data_type.h>
#include <jack/jack.h>

namespace ARDOUR {

class AudioEngine;
class Buffer;

/** Abstract base for ports
 */
class Port : public virtual PBD::Destructible {
   public:
	enum Flags {
		IsInput = JackPortIsInput,
		IsOutput = JackPortIsOutput,
		IsPhysical = JackPortIsPhysical,
		IsTerminal = JackPortIsTerminal,
		CanMonitor = JackPortCanMonitor
	};

	virtual ~Port();

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
		return _flags & IsOutput;
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

	virtual void cycle_start (nframes_t nframes, nframes_t offset) {}
	virtual void cycle_end (nframes_t nframes, nframes_t offset) {}
	virtual void flush_buffers (nframes_t nframes, nframes_t offset ) {}
	virtual DataType type() const = 0;
	virtual Buffer& get_buffer( nframes_t nframes, nframes_t offset ) = 0;

	virtual bool connected () const;
	virtual bool connected_to (const std::string& portname) const;
	virtual int get_connections (std::vector<std::string>&) const;

	virtual int connect (Port& other);
	virtual int disconnect (Port& other);
	virtual int disconnect_all ();
	
	virtual void reset ();
	virtual int reestablish () {return 0; }
	virtual int reconnect () { return 0; }

	virtual int set_name (const std::string& str) {
		_name = str;
		return 0;
	}

	virtual std::string short_name() const = 0;
	virtual bool monitoring_input () const = 0;
	virtual void ensure_monitor_input (bool yn) = 0;
	virtual void request_monitor_input (bool yn) = 0;
	virtual nframes_t latency () const = 0;
	virtual nframes_t total_latency () const = 0;
	virtual void set_latency (nframes_t nframes) = 0;

	sigc::signal<void,bool> MonitorInputChanged;
	sigc::signal<void,bool> ClockSyncChanged;

	static void set_engine (AudioEngine*);

  protected:
	friend class AudioEngine;

	Port (const std::string& name, Flags flgs);

	virtual void recompute_total_latency() const {}
	
	/* engine isn't supposed to access below here */

	Flags          _flags;
	std::string    _type;
	std::string    _name;
	unsigned short _metering;
	bool           _last_monitor;
	nframes_t      _latency;

	std::set<Port*> _connections;

	static AudioEngine* engine;

   private:

        void port_going_away (Port *);
};

class PortConnectableByName {
  public:
	PortConnectableByName() {}
	virtual ~PortConnectableByName() {}

	virtual int connect (const std::string& other_name) = 0;
	virtual int disconnect (const std::string& other_name) = 0;
};
 
class PortFacade : public virtual Port, public PortConnectableByName { 
  public:
	PortFacade (const std::string& name, Flags flgs) : Port (name, flgs), _ext_port (0) {}
	~PortFacade() {}

	void reset ();
	int reestablish ();
	int reconnect ();

	int connect (Port& other);
	int disconnect (Port& other);
	int disconnect_all ();

	int connect (const std::string& other_name);
	int disconnect (const std::string& other_name);

	bool connected () const;
	bool connected_to (const std::string& portname) const;
	int get_connections (std::vector<std::string>&) const;

	std::string short_name() const;
	int         set_name (const std::string& str);
	bool        monitoring_input () const;
	void        ensure_monitor_input (bool yn);
	void        request_monitor_input (bool yn);
	nframes_t   latency () const;
	nframes_t   total_latency () const;
	void        set_latency (nframes_t nframes);

  protected:
	Port* _ext_port;
};

} // namespace ARDOUR

#endif /* __ardour_port_h__ */
