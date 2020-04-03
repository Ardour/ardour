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

#ifndef __libardour_port_engine_shared_h__
#define __libardour_port_engine_shared_h__

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


class LIBARDOUR_API BackendPort
{
   protected:
	BackendPort (PortEngineSharedImpl &b, const std::string&, PortFlags);

   public:
	virtual ~BackendPort ();

	const std::string& name () const { return _name; }
	const std::string& pretty_name () const { return _pretty_name; }
	PortFlags flags () const { return _flags; }

	int set_name (const std::string &name) { _name = name; return 0; }
	int set_pretty_name (const std::string &name) { _pretty_name = name; return 0; }

	virtual DataType type () const = 0;

	bool is_input ()     const { return flags () & IsInput; }
	bool is_output ()    const { return flags () & IsOutput; }
	bool is_physical ()  const { return flags () & IsPhysical; }
	bool is_terminal ()  const { return flags () & IsTerminal; }
	bool is_connected () const { return _connections.size () != 0; }
	bool is_connected (const BackendPort *port) const;
	bool is_physically_connected () const;

	const std::set<BackendPort *>& get_connections () const { return _connections; }

	int connect (BackendPort *port);
	int disconnect (BackendPort *port);
	void disconnect_all ();

	virtual void* get_buffer (pframes_t nframes) = 0;

	const LatencyRange latency_range (bool for_playback) const
	{
		return for_playback ? _playback_latency_range : _capture_latency_range;
	}

	void set_latency_range (const LatencyRange &latency_range, bool for_playback);

	void update_connected_latency (bool for_playback);

  protected:
	PortEngineSharedImpl &_backend;

  private:
	std::string _name;
	std::string _pretty_name;
	const PortFlags _flags;
	LatencyRange _capture_latency_range;
	LatencyRange _playback_latency_range;
	std::set<BackendPort*> _connections;

	void _connect (BackendPort* , bool);
	void _disconnect (BackendPort* , bool);

}; // class BackendPort

class LIBARDOUR_API PortEngineSharedImpl
{
  public:
	PortEngineSharedImpl (PortManager& mgr, std::string const & instance_name);
	virtual ~PortEngineSharedImpl();

	/* Discovering physical ports */

	bool      port_is_physical (PortEngine::PortHandle) const;
	void      get_physical_outputs (DataType type, std::vector<std::string>&);
	void      get_physical_inputs (DataType type, std::vector<std::string>&);
	ChanCount n_physical_outputs () const;
	ChanCount n_physical_inputs () const;

	uint32_t port_name_size () const;

	int         set_port_name (PortEngine::PortHandle, const std::string&);
	std::string get_port_name (PortEngine::PortHandle) const;
	PortFlags get_port_flags (PortEngine::PortHandle) const;
	PortEngine::PortHandle  get_port_by_name (const std::string&) const;

	int get_port_property (PortEngine::PortHandle, const std::string& key, std::string& value, std::string& type) const;
	int set_port_property (PortEngine::PortHandle, const std::string& key, const std::string& value, const std::string& type);

	int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

	DataType port_data_type (PortEngine::PortHandle) const;

	PortEngine::PortHandle register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
	virtual void unregister_port (PortEngine::PortHandle);

	int  connect (const std::string& src, const std::string& dst);
	int  disconnect (const std::string& src, const std::string& dst);
	int  connect (PortEngine::PortHandle, const std::string&);
	int  disconnect (PortEngine::PortHandle, const std::string&);
	int  disconnect_all (PortEngine::PortHandle);

	bool connected (PortEngine::PortHandle, bool process_callback_safe);
	bool connected_to (PortEngine::PortHandle, const std::string&, bool process_callback_safe);
	bool physically_connected (PortEngine::PortHandle, bool process_callback_safe);
	int  get_connections (PortEngine::PortHandle, std::vector<std::string>&, bool process_callback_safe);

	virtual void port_connect_callback (const std::string& a, const std::string& b, bool conn) = 0;
	virtual void port_connect_add_remove_callback () = 0;

  protected:
	std::string _instance_name;

	std::vector<BackendPort *> _system_inputs;
	std::vector<BackendPort *> _system_outputs;
	std::vector<BackendPort *> _system_midi_in;
	std::vector<BackendPort *> _system_midi_out;

	void clear_ports ();

	PortEngine::PortHandle add_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
	void unregister_ports (bool system_only = false);

	struct SortByPortName
	{
		bool operator ()(const BackendPort* lhs, const BackendPort* rhs) const
		{
			return PBD::naturally_less (lhs->name ().c_str (), rhs->name ().c_str ());
		}
	};

	typedef std::map<std::string, BackendPort *> PortMap; // fast lookup in _ports
	typedef std::set<BackendPort *, SortByPortName> PortIndex; // fast lookup in _ports
	SerializedRCUManager<PortMap> _portmap;
	SerializedRCUManager<PortIndex> _ports;

	bool valid_port (PortEngine::PortHandle port) const {
		boost::shared_ptr<PortIndex> p = _ports.reader();
		return std::find (p->begin(), p->end(), static_cast<BackendPort*>(port)) != p->end ();
	}

	BackendPort* find_port (const std::string& port_name) const {
		boost::shared_ptr<PortMap> p = _portmap.reader();
		PortMap::const_iterator it = p->find (port_name);
			if (it == p->end()) {
				return NULL;
			}
			return (*it).second;
	}

	virtual BackendPort* port_factory (std::string const & name, ARDOUR::DataType dt, ARDOUR::PortFlags flags) = 0;
};

} /* namespace ARDOUR */

#endif /* __libardour_port_engine_shared_h__ */
