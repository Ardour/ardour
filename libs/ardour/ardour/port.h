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
#include <sigc++/trackable.h>
#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {

class AudioEngine;
class Buffer;

class Port : public sigc::trackable, public boost::noncopyable
{
public:
	enum Flags {
		IsInput = JackPortIsInput,
		IsOutput = JackPortIsOutput,
	};

	virtual ~Port ();

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
	nframes_t total_latency () const;
	int reestablish ();
	int reconnect ();
	void request_monitor_input (bool);
	void set_latency (nframes_t);

	virtual void reset ();

	/** @return the size of the raw buffer (bytes) for duration @a nframes (audio frames) */
	virtual size_t raw_buffer_size(jack_nframes_t nframes) const = 0;

	virtual DataType type () const = 0;
	virtual void cycle_start (nframes_t) = 0;
	virtual void cycle_end (nframes_t) = 0;
	virtual void cycle_split () = 0;
	virtual Buffer& get_buffer (nframes_t nframes, nframes_t offset = 0) = 0;
	virtual void flush_buffers (nframes_t, nframes_t offset = 0) {
		(void) offset;
	}

	static void set_engine (AudioEngine *);

	sigc::signal<void, bool> MonitorInputChanged;

protected:

	Port (std::string const &, DataType, Flags);

	jack_port_t* _jack_port; ///< JACK port

	static nframes_t _port_offset;
	static nframes_t _buffer_size;

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
