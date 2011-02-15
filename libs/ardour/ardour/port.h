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

	static void set_buffer_size (pframes_t sz) {
		_buffer_size = sz;
	}
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

	void ensure_monitor_input (bool);
	bool monitoring_input () const;
	framecnt_t total_latency () const;
	int reestablish ();
	int reconnect ();
	void request_monitor_input (bool);
	void set_latency (framecnt_t);
        
        void get_connected_latency_range (jack_latency_range_t& range, jack_latency_callback_mode_t mode) const;
        void set_latency_range (jack_latency_range_t& range, jack_latency_callback_mode_t mode) const;

	virtual void reset ();

	/** @return the size of the raw buffer (bytes) for duration @a nframes (audio frames) */
	virtual size_t raw_buffer_size (pframes_t nframes) const = 0;

	virtual DataType type () const = 0;
	virtual void cycle_start (pframes_t) = 0;
	virtual void cycle_end (pframes_t) = 0;
	virtual void cycle_split () = 0;
	virtual Buffer& get_buffer (framecnt_t nframes, framecnt_t offset = 0) = 0;
	virtual void flush_buffers (pframes_t nframes, framepos_t /*time*/, framecnt_t offset = 0) {
		assert (offset < nframes);
	}
	virtual void transport_stopped () {}

        bool physically_connected () const;

	static void set_engine (AudioEngine *);

	PBD::Signal1<void,bool> MonitorInputChanged;

protected:

	Port (std::string const &, DataType, Flags);

	jack_port_t* _jack_port; ///< JACK port

	static pframes_t  _buffer_size;
	static bool	  _connecting_blocked;
        
	static AudioEngine* _engine; ///< the AudioEngine

private:
	friend class AudioEngine;

	void recompute_total_latency () const;

	/* XXX */
	bool _last_monitor;

	std::string _name;  ///< port short name
	Flags       _flags; ///< flags

	/** ports that we are connected to, kept so that we can
	    reconnect to JACK when required */
	std::set<std::string> _connections;
};

}

#endif /* __ardour_port_h__ */
