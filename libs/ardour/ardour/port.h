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

#include "ardour/data_type.h"
#include "ardour/types.h"
#include <sigc++/trackable.h>
#include <jack/jack.h>
#include <string>
#include <set>
#include <vector>

namespace ARDOUR {

class AudioEngine;
class Buffer;	

class Port : public sigc::trackable
{
public:
	enum Flags {
		IsInput = JackPortIsInput,
		IsOutput = JackPortIsOutput,
	};

	virtual ~Port ();

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

	/* @return true if this port is visible outside Ardour (via JACK) */
	bool external () const {
		return _jack_port != 0;
	}

	bool connected () const;
	bool externally_connected () const;
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
	void set_latency (nframes_t);
	void request_monitor_input (bool);
	void make_external ();

	virtual void reset ();

	virtual DataType type () const = 0;
	virtual void cycle_start (nframes_t, nframes_t) = 0;
	virtual void cycle_end (nframes_t, nframes_t) = 0;
	virtual Buffer& get_buffer (nframes_t, nframes_t) = 0;
	virtual void flush_buffers (nframes_t, nframes_t) {}

	static void set_engine (AudioEngine *);

	sigc::signal<void, bool> MonitorInputChanged;

protected:
	
	Port (std::string const &, DataType, Flags, bool);

	jack_port_t* _jack_port; ///< JACK port, or 0 if we don't have one
	std::set<Port*> _connections; ///< internal Ports that we are connected to

	static AudioEngine* _engine; ///< the AudioEngine

	virtual bool using_internal_data() const { return false; }
	virtual void use_internal_data () {}
	virtual void use_external_data () {} 

	void check_buffer_status ();
	
private:
	friend class AudioEngine;

	void recompute_total_latency () const;
	void do_make_external (DataType);
	
	/* XXX */
	bool _last_monitor;
	nframes_t _latency;

	std::string _name; ///< port short name
	Flags _flags; ///< flags

	/// list of JACK ports that we are connected to; we only keep this around
	/// so that we can implement ::reconnect ()
	std::set<std::string> _named_connections;

};

}

#endif
