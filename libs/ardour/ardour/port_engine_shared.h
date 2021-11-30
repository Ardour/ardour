/*
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2020 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _libardour_port_engine_shared_h_
#define _libardour_port_engine_shared_h_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "pbd/natsort.h"
#include "pbd/rcu.h"

#include "ardour/libardour_visibility.h"
#include "ardour/port_engine.h"
#include "ardour/types.h"

namespace ARDOUR {

class PortEngineSharedImpl;
class PortManager;

class BackendPort;

typedef boost::shared_ptr<BackendPort> BackendPortPtr;
typedef boost::shared_ptr<BackendPort> const & BackendPortHandle;

class LIBARDOUR_API BackendPort : public ProtoPort
{
  protected:
	BackendPort (PortEngineSharedImpl& b, const std::string&, PortFlags);

  public:
	virtual ~BackendPort ();

	const std::string& name ()        const { return _name; }
	const std::string& pretty_name () const { return _pretty_name; }
	const std::string& hw_port_name () const { return _hw_port_name; }

	int set_name (const std::string& name) {
		_name = name;
		return 0;
	}

	/* called from PortEngineSharedImpl */
	int set_pretty_name (const std::string& name) {
		_pretty_name = name;
		return 0;
	}

	/* called from backends only */
	int set_hw_port_name (const std::string& name) {
		_hw_port_name = name;
		return 0;
	}

	virtual DataType type () const = 0;

	PortFlags flags ()   const { return _flags; }
	bool is_input ()     const { return flags () & IsInput; }
	bool is_output ()    const { return flags () & IsOutput; }
	bool is_physical ()  const { return flags () & IsPhysical; }
	bool is_terminal ()  const { return flags () & IsTerminal; }
	bool is_connected () const { return _connections.size () != 0; }

	bool is_connected (BackendPortHandle port) const;
	bool is_physically_connected () const;

	const std::set<BackendPortPtr>& get_connections () const {
		return _connections;
	}

	int  connect (BackendPortHandle port, BackendPortHandle self);
	int  disconnect (BackendPortHandle port, BackendPortHandle self);
	void disconnect_all (BackendPortHandle self);

	virtual void* get_buffer (pframes_t nframes) = 0;

	const LatencyRange latency_range (bool for_playback) const
	{
		return for_playback ? _playback_latency_range : _capture_latency_range;
	}

	void set_latency_range (const LatencyRange& latency_range, bool for_playback);

	void update_connected_latency (bool for_playback);

protected:
	PortEngineSharedImpl& _backend;

private:
	std::string            _name;
	std::string            _pretty_name;
	std::string            _hw_port_name;
	const PortFlags        _flags;
	LatencyRange           _capture_latency_range;
	LatencyRange           _playback_latency_range;
	std::set<BackendPortPtr> _connections;

	void store_connection (BackendPortHandle);
	void remove_connection (BackendPortHandle);

}; // class BackendPort

class LIBARDOUR_API BackendMIDIEvent
{
public:
	virtual ~BackendMIDIEvent () {}
	virtual size_t size () const = 0;
	virtual pframes_t timestamp () const = 0;
	virtual const uint8_t* data () const = 0;
	bool operator< (const BackendMIDIEvent &other) const;
};

class LIBARDOUR_API PortEngineSharedImpl
{
public:
	PortEngineSharedImpl (PortManager& mgr, std::string const& instance_name);
	virtual ~PortEngineSharedImpl ();

	/* Discovering physical ports */

	bool      port_is_physical (PortEngine::PortHandle) const;
	void      get_physical_outputs (DataType type, std::vector<std::string>&);
	void      get_physical_inputs (DataType type, std::vector<std::string>&);
	ChanCount n_physical_outputs () const;
	ChanCount n_physical_inputs () const;

	uint32_t port_name_size () const;

	int                    set_port_name (PortEngine::PortHandle, const std::string&);
	std::string            get_port_name (PortEngine::PortHandle) const;
	PortFlags              get_port_flags (PortEngine::PortHandle) const;
	PortEngine::PortPtr    get_port_by_name (const std::string&) const;

	int get_port_property (PortEngine::PortHandle, const std::string& key, std::string& value, std::string& type) const;
	int set_port_property (PortEngine::PortHandle, const std::string& key, const std::string& value, const std::string& type);

	int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

	DataType port_data_type (PortEngine::PortHandle) const;

	PortEngine::PortPtr register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
	virtual void        unregister_port (PortEngine::PortHandle);

	int connect (const std::string& src, const std::string& dst);
	int disconnect (const std::string& src, const std::string& dst);
	int connect (PortEngine::PortHandle, const std::string&);
	int disconnect (PortEngine::PortHandle, const std::string&);
	int disconnect_all (PortEngine::PortHandle);

	bool connected (PortEngine::PortHandle, bool process_callback_safe);
	bool connected_to (PortEngine::PortHandle, const std::string&, bool process_callback_safe);
	bool physically_connected (PortEngine::PortHandle, bool process_callback_safe);
	int  get_connections (PortEngine::PortHandle, std::vector<std::string>&, bool process_callback_safe);

protected:
	friend class BackendPort;
	std::string _instance_name;

	std::vector<BackendPortPtr> _system_inputs;
	std::vector<BackendPortPtr> _system_outputs;
	std::vector<BackendPortPtr> _system_midi_in;
	std::vector<BackendPortPtr> _system_midi_out;

	struct PortConnectData {
		std::string a;
		std::string b;
		bool c;

		PortConnectData (const std::string& a, const std::string& b, bool c)
			: a (a) , b (b) , c (c) {}
	};

	std::vector<PortConnectData *> _port_connection_queue;
	pthread_mutex_t _port_callback_mutex;

	GATOMIC_QUAL gint _port_change_flag; /* atomic */

	void port_connect_callback (const std::string& a, const std::string& b, bool conn) {
		pthread_mutex_lock (&_port_callback_mutex);
		_port_connection_queue.push_back(new PortConnectData(a, b, conn));
		pthread_mutex_unlock (&_port_callback_mutex);
	}

	void port_connect_add_remove_callback () {
		g_atomic_int_set (&_port_change_flag, 1);
	}

	virtual void update_system_port_latencies ();

	void clear_ports ();

	BackendPortPtr add_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
	void                unregister_ports (bool system_only = false);

	struct SortByPortName {
		bool operator() (BackendPortHandle lhs, BackendPortHandle rhs) const {
			return PBD::naturally_less (lhs->name ().c_str (), rhs->name ().c_str ());
		}
	};

	typedef std::map<std::string, BackendPortPtr>    PortMap;   // fast lookup in _ports
	typedef std::set<BackendPortPtr, SortByPortName> PortIndex; // fast lookup in _ports
	SerializedRCUManager<PortMap>                  _portmap;
	SerializedRCUManager<PortIndex>                _ports;

	bool valid_port (BackendPortHandle port) const {
		boost::shared_ptr<PortIndex> p = _ports.reader ();
		return std::find (p->begin (), p->end (), port) != p->end ();
	}

	BackendPortPtr find_port (const std::string& port_name) const {
		boost::shared_ptr<PortMap> p  = _portmap.reader ();
		PortMap::const_iterator    it = p->find (port_name);
		if (it == p->end ()) {
			return BackendPortPtr();
		}
		return (*it).second;
	}

	virtual BackendPort* port_factory (std::string const& name, ARDOUR::DataType dt, ARDOUR::PortFlags flags) = 0;

#ifndef NDEBUG
	void list_ports () const;
#endif
};

} /* namespace ARDOUR */

#endif
