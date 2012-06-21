/*
    Copyright (C) 2009 Paul Davis

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

#include "libardour-config.h"

#include <set>
#include <string>
#include <vector>
#include <jack/jack.h>
#include <boost/utility.hpp>
#include "pbd/signals.h"

#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {

class AudioEngine;
class Buffer;

class Port : public boost::noncopyable
{
public:
	enum Flags {
		IsInput = JackPortIsInput,
		IsOutput = JackPortIsOutput,
	};

	virtual ~Port ();

	static void set_connecting_blocked( bool yn ) {
		_connecting_blocked = yn;
	}
	static bool connecting_blocked() {
		return _connecting_blocked;
	}

	/** @return Port short name */
	std::string name () const {
		return _name;
	}

	int set_name (std::string const &);

	/** @return flags */
	Flags flags () const {
		return _flags;
	}

	/** @return true if this Port receives input, otherwise false */
	bool receives_input () const {
		return _flags & IsInput;
	}

	/** @return true if this Port sends output, otherwise false */
	bool sends_output () const {
		return _flags & IsOutput;
	}

	bool connected () const;
	int disconnect_all ();
	int get_connections (std::vector<std::string> &) const;

	/* connection by name */
	bool connected_to (std::string const &) const;
	int connect (std::string const &);
	int disconnect (std::string const &);

	/* connection by Port* */
	bool connected_to (Port *) const;
	virtual int connect (Port *);
	int disconnect (Port *);

	void ensure_jack_monitors_input (bool);
	bool jack_monitoring_input () const;
	int reestablish ();
	int reconnect ();
	void request_jack_monitors_input (bool);

	bool last_monitor() const { return _last_monitor; }
	void set_last_monitor (bool yn) { _last_monitor = yn; }

	jack_port_t* jack_port() const { return _jack_port; }

	void get_connected_latency_range (jack_latency_range_t& range, bool playback) const;

	void set_private_latency_range (jack_latency_range_t& range, bool playback);
	const jack_latency_range_t&  private_latency_range (bool playback) const;

	void set_public_latency_range (jack_latency_range_t& range, bool playback) const;
	jack_latency_range_t public_latency_range (bool playback) const;

	virtual void reset ();

	virtual DataType type () const = 0;
	virtual void cycle_start (pframes_t);
	virtual void cycle_end (pframes_t) = 0;
	virtual void cycle_split () = 0;
	virtual Buffer& get_buffer (pframes_t nframes) = 0;
	virtual void flush_buffers (pframes_t /*nframes*/) {}
	virtual void transport_stopped () {}
	virtual void realtime_locate () {}

	bool physically_connected () const;

	static void set_engine (AudioEngine *);

	PBD::Signal1<void,bool> MonitorInputChanged;
	static PBD::Signal2<void,boost::shared_ptr<Port>,boost::shared_ptr<Port> > PostDisconnect;
	static PBD::Signal0<void> PortDrop;

	static void set_cycle_framecnt (pframes_t n) {
		_cycle_nframes = n;
	}
	static framecnt_t port_offset() { return _global_port_buffer_offset; }
	static void set_global_port_buffer_offset (pframes_t off) {
		_global_port_buffer_offset = off;
	}
	static void increment_global_port_buffer_offset (pframes_t n) {
		_global_port_buffer_offset += n;
	}

	virtual void increment_port_buffer_offset (pframes_t n);

protected:

	Port (std::string const &, DataType, Flags);

	jack_port_t* _jack_port; ///< JACK port

	static bool	  _connecting_blocked;
	static pframes_t  _global_port_buffer_offset;   /* access only from process() tree */
	static pframes_t  _cycle_nframes; /* access only from process() tree */

	framecnt_t _port_buffer_offset; /* access only from process() tree */

	jack_latency_range_t _private_playback_latency;
	jack_latency_range_t _private_capture_latency;

	static AudioEngine* _engine; ///< the AudioEngine

private:
	std::string _name;  ///< port short name
	Flags       _flags; ///< flags
	bool        _last_monitor;

	/** ports that we are connected to, kept so that we can
	    reconnect to JACK when required
	*/
	std::set<std::string> _connections;

       void drop ();
       PBD::ScopedConnection drop_connection;
};

}

#endif /* __ardour_port_h__ */
