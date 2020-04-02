/*
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2020 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <regex.h>

#include "pbd/error.h"
#include "pbd/i18n.h"

#include "ardour/port_engine_shared.h"

using namespace ARDOUR;

BackendPort::BackendPort (PortEngineSharedImpl &b, const std::string& name, PortFlags flags)
	: _backend (b)
	, _name  (name)
	, _flags (flags)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
}

BackendPort::~BackendPort () {
	disconnect_all ();
}

int BackendPort::connect (BackendPort *port)
{
	if (!port) {
		PBD::error << _("BackendPort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("BackendPort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("BackendPort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("BackendPort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("BackendPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("BackendPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return -1;
	}

	_connect (port, true);
	return 0;
}

void BackendPort::_connect (BackendPort *port, bool callback)
{
	_connections.insert (port);
	if (callback) {
		port->_connect (this, false);
		_backend.port_connect_callback (name(),  port->name(), true);
	}
}

int BackendPort::disconnect (BackendPort *port)
{
	if (!port) {
		PBD::error << _("BackendPort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("BackendPort::disconnect (): ports are not connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void BackendPort::_disconnect (BackendPort *port, bool callback)
{
	std::set<BackendPort*>::iterator it = _connections.find (port);
	assert (it != _connections.end ());
	_connections.erase (it);
	if (callback) {
		port->_disconnect (this, false);
		_backend.port_connect_callback (name(),  port->name(), false);
	}
}


void BackendPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		std::set<BackendPort*>::iterator it = _connections.begin ();
		(*it)->_disconnect (this, false);
		_backend.port_connect_callback (name(), (*it)->name(), false);
		_connections.erase (it);
	}
}

bool
BackendPort::is_connected (const BackendPort *port) const
{
	return _connections.find (const_cast<BackendPort *>(port)) != _connections.end ();
}

bool BackendPort::is_physically_connected () const
{
	for (std::set<BackendPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

void
BackendPort::set_latency_range (const LatencyRange &latency_range, bool for_playback)
{
	if (for_playback) {
		_playback_latency_range = latency_range;
	} else {
		_capture_latency_range = latency_range;
	}

	for (std::set<BackendPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			(*it)->update_connected_latency (is_input ());
		}
	}
}

void
BackendPort::update_connected_latency (bool for_playback)
{
	LatencyRange lr;
	lr.min = lr.max = 0;
	for (std::set<BackendPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		LatencyRange l;
		l = (*it)->latency_range (for_playback);
		lr.min = std::max (lr.min, l.min);
		lr.max = std::max (lr.max, l.max);
	}
	set_latency_range (lr, for_playback);
}



PortEngineSharedImpl::PortEngineSharedImpl (PortManager& mgr, std::string const & str)
	:  _instance_name (str)
	, _portmap (new PortMap)
	, _ports (new PortIndex)
{
}

PortEngineSharedImpl::~PortEngineSharedImpl ()
{
}

int
PortEngineSharedImpl::get_ports (
	const std::string& port_name_pattern,
	DataType type, PortFlags flags,
	std::vector<std::string>& port_names) const
{
	int rv = 0;
	regex_t port_regex;
	bool use_regexp = false;
	if (port_name_pattern.size () > 0) {
		if (!regcomp (&port_regex, port_name_pattern.c_str (), REG_EXTENDED|REG_NOSUB)) {
			use_regexp = true;
		}
	}

	boost::shared_ptr<PortIndex> p = _ports.reader ();

	for (PortIndex::const_iterator i = p->begin (); i != p->end (); ++i) {
		BackendPort* port = *i;
		if ((port->type () == type) && flags == (port->flags () & flags)) {
			if (!use_regexp || !regexec (&port_regex, port->name ().c_str (), 0, NULL, 0)) {
				port_names.push_back (port->name ());
				++rv;
			}
		}
	}
	if (use_regexp) {
		regfree (&port_regex);
	}
	return rv;
}

/* Discovering physical ports */

bool
PortEngineSharedImpl::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("BackendPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<BackendPort*>(port)->is_physical ();
}

void
PortEngineSharedImpl::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	boost::shared_ptr<PortIndex> p = _ports.reader();

	for (PortIndex::iterator i = p->begin (); i != p->end (); ++i) {
		BackendPort* port = *i;
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
PortEngineSharedImpl::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	boost::shared_ptr<PortIndex> p = _ports.reader();

	for (PortIndex::iterator i = p->begin (); i != p->end (); ++i) {
		BackendPort* port = *i;
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

ChanCount
PortEngineSharedImpl::n_physical_outputs () const
{
	int n_midi = 0;
	int n_audio = 0;

	boost::shared_ptr<PortIndex> p = _ports.reader();

	for (PortIndex::const_iterator i = p->begin (); i != p->end (); ++i) {
		BackendPort* port = *i;
		if (port->is_output () && port->is_physical ()) {
			switch (port->type ()) {
			case DataType::AUDIO: ++n_audio; break;
			case DataType::MIDI: ++n_midi; break;
			default: break;
			}
		}
	}
	ChanCount cc;
	cc.set (DataType::AUDIO, n_audio);
	cc.set (DataType::MIDI, n_midi);
	return cc;
}

ChanCount
PortEngineSharedImpl::n_physical_inputs () const
{
	int n_midi = 0;
	int n_audio = 0;

	boost::shared_ptr<PortIndex> p = _ports.reader();

	for (PortIndex::const_iterator i = p->begin (); i != p->end (); ++i) {
		BackendPort* port = *i;
		if (port->is_input () && port->is_physical ()) {
			switch (port->type ()) {
			case DataType::AUDIO: ++n_audio; break;
			case DataType::MIDI: ++n_midi; break;
			default: break;
			}
		}
	}
	ChanCount cc;
	cc.set (DataType::AUDIO, n_audio);
	cc.set (DataType::MIDI, n_midi);
	return cc;
}

PortEngine::PortHandle
PortEngineSharedImpl::add_port (
	const std::string& name,
	ARDOUR::DataType type,
	ARDOUR::PortFlags flags)
{
	assert(name.size ());
	if (find_port (name)) {
		PBD::error << _("AlsaBackend::register_port: Port already exists:")
		           << " (" << name << ")" << endmsg;
		return 0;
	}

	BackendPort* port = port_factory (name, type, flags);

	if (!port) {
		return 0;
	}

	{
		RCUWriter<PortIndex> index_writer (_ports);
		RCUWriter<PortMap> map_writer (_portmap);

		boost::shared_ptr<PortIndex> ps = index_writer.get_copy ();
		boost::shared_ptr<PortMap> pm = map_writer.get_copy ();

		ps->insert (port);
		pm->insert (make_pair (name, port));
	}

	return port;
}

void
PortEngineSharedImpl::unregister_port (PortEngine::PortHandle port_handle)
{
	BackendPort* port = static_cast<BackendPort*>(port_handle);

	{
		RCUWriter<PortIndex> index_writer (_ports);
		RCUWriter<PortMap> map_writer (_portmap);

		boost::shared_ptr<PortIndex> ps = index_writer.get_copy ();
		boost::shared_ptr<PortMap> pm = map_writer.get_copy ();

		PortIndex::iterator i = std::find (ps->begin(), ps->end(), static_cast<BackendPort*>(port_handle));

		if (i == ps->end ()) {
			PBD::error << _("AlsaBackend::unregister_port: Failed to find port") << endmsg;
			return;
		}

		disconnect_all(port_handle);

		pm->erase (port->name());
		ps->erase (i);
	}

	delete port;
}


void
PortEngineSharedImpl::unregister_ports (bool system_only)
{
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();

	RCUWriter<PortIndex> index_writer (_ports);
	RCUWriter<PortMap> map_writer (_portmap);

	boost::shared_ptr<PortIndex> ps = index_writer.get_copy ();
	boost::shared_ptr<PortMap> pm = map_writer.get_copy ();


	for (PortIndex::iterator i = ps->begin (); i != ps->end ();) {
		PortIndex::iterator cur = i++;
		BackendPort* port = *cur;
		if (! system_only || (port->is_physical () && port->is_terminal ())) {
			port->disconnect_all ();
			pm->erase (port->name());
			delete port;
			ps->erase (cur);
		}
	}
}

void
PortEngineSharedImpl::clear_ports ()
{
	RCUWriter<PortIndex> index_writer (_ports);
	RCUWriter<PortMap> map_writer (_portmap);

	boost::shared_ptr<PortIndex> ps = index_writer.get_copy();
	boost::shared_ptr<PortMap> pm = map_writer.get_copy ();

	if (ps->size () || pm->size ()) {
		PBD::warning << _("PortEngineSharedImpl: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		_system_inputs.clear();
		_system_outputs.clear();
		_system_midi_in.clear();
		_system_midi_out.clear();
		ps->clear();
		pm->clear();
	}
}

uint32_t
PortEngineSharedImpl::port_name_size () const
{
	return 256;
}

int
PortEngineSharedImpl::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	std::string newname (_instance_name + ":" + name);

	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::set_port_name: Invalid Port") << endmsg;
		return -1;
	}

	if (find_port (newname)) {
		PBD::error << _("AlsaBackend::set_port_name: Port with given name already exists") << endmsg;
		return -1;
	}

	BackendPort* p = static_cast<BackendPort*>(port);

	{
		RCUWriter<PortMap> map_writer (_portmap);
		boost::shared_ptr<PortMap> pm = map_writer.get_copy ();

		pm->erase (p->name());
		pm->insert (make_pair (newname, p));
	}

	return p->set_name (newname);
}

std::string
PortEngineSharedImpl::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::warning << _("AlsaBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<BackendPort*>(port)->name ();
}

PortFlags
PortEngineSharedImpl::get_port_flags (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::warning << _("AlsaBackend::get_port_flags: Invalid Port(s)") << endmsg;
		return PortFlags (0);
	}
	return static_cast<BackendPort*>(port)->flags ();
}

int
PortEngineSharedImpl::get_port_property (PortEngine::PortHandle port, const std::string& key, std::string& value, std::string& type) const
{
	if (!valid_port (port)) {
		PBD::warning << _("AlsaBackend::get_port_property: Invalid Port(s)") << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name") {
		type = "";
		value = static_cast<BackendPort*>(port)->pretty_name ();
		if (!value.empty()) {
			return 0;
		}
	}
	return -1;
}

int
PortEngineSharedImpl::set_port_property (PortEngine::PortHandle port, const std::string& key, const std::string& value, const std::string& type)
{
	if (!valid_port (port)) {
		PBD::warning << _("AlsaBackend::set_port_property: Invalid Port(s)") << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name" && type.empty ()) {
		static_cast<BackendPort*>(port)->set_pretty_name (value);
		return 0;
	}
	return -1;
}

PortEngine::PortHandle
PortEngineSharedImpl::get_port_by_name (const std::string& name) const
{
	PortEngine::PortHandle port = (PortEngine::PortHandle) find_port (name);
	return port;
}

DataType
PortEngineSharedImpl::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<BackendPort*>(port)->type ();
}

PortEngine::PortHandle
PortEngineSharedImpl::register_port (
	const std::string& name,
	ARDOUR::DataType type,
	ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return 0; }
	if (flags & IsPhysical) { return 0; }
	return add_port (_instance_name + ":" + name, type, flags);
}


int
PortEngineSharedImpl::connect (const std::string& src, const std::string& dst)
{
	BackendPort* src_port = find_port (src);
	BackendPort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("AlsaBackend::connect: Invalid Source port:")
		           << " (" << src <<")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("AlsaBackend::connect: Invalid Destination port:")
		           << " (" << dst <<")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
PortEngineSharedImpl::disconnect (const std::string& src, const std::string& dst)
{
	BackendPort* src_port = find_port (src);
	BackendPort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("AlsaBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
PortEngineSharedImpl::connect (PortEngine::PortHandle src, const std::string& dst)
{
	BackendPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("AlsaBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("AlsaBackend::connect: Invalid Destination Port")
		           << " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<BackendPort*>(src)->connect (dst_port);
}

int
PortEngineSharedImpl::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	BackendPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("AlsaBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<BackendPort*>(src)->disconnect (dst_port);
}

int
PortEngineSharedImpl::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<BackendPort*>(port)->disconnect_all ();
	return 0;
}

bool
PortEngineSharedImpl::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<BackendPort*>(port)->is_connected ();
}

bool
PortEngineSharedImpl::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	BackendPort* dst_port = find_port (dst);
#ifndef NDEBUG
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("AlsaBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
#endif
	return static_cast<BackendPort*>(src)->is_connected (dst_port);
}

bool
PortEngineSharedImpl::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<BackendPort*>(port)->is_physically_connected ();
}

int
PortEngineSharedImpl::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::set<BackendPort*>& connected_ports = static_cast<BackendPort*>(port)->get_connections ();

	for (std::set<BackendPort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}
