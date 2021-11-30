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

#include "ardour/port_engine_shared.h"

#include "pbd/i18n.h"

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

	_backend.port_connect_add_remove_callback (); // XXX -> RT
}

BackendPort::~BackendPort ()
{
	_backend.port_connect_add_remove_callback (); // XXX -> RT
	assert (_connections.empty());
}

int
BackendPort::connect (BackendPortHandle port, BackendPortHandle self)
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

	if (this == port.get()) {
		PBD::error << _("BackendPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("BackendPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return 0;
	}

	store_connection (port);
	port->store_connection (self);

	_backend.port_connect_callback (name(),  port->name(), true);

	return 0;
}

void
BackendPort::store_connection (BackendPortHandle port)
{
	_connections.insert (port);
}

int
BackendPort::disconnect (BackendPortHandle port, BackendPortHandle self)
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

	remove_connection (port);
	port->remove_connection (self);
	_backend.port_connect_callback (name(),  port->name(), false);

	return 0;
}

void BackendPort::remove_connection (BackendPortHandle port)
{
	std::set<BackendPortPtr>::iterator it = _connections.find (port);
	assert (it != _connections.end ());
	_connections.erase (it);
}


void BackendPort::disconnect_all (BackendPortHandle self)
{
	while (!_connections.empty ()) {
		std::set<BackendPortPtr>::iterator it = _connections.begin ();
		(*it)->remove_connection (self);
		_backend.port_connect_callback (name(), (*it)->name(), false);
		_connections.erase (it);
	}
}

bool
BackendPort::is_connected (BackendPortHandle port) const
{
	return _connections.find (port) != _connections.end ();
}

bool BackendPort::is_physically_connected () const
{
	for (std::set<BackendPortPtr>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

void
BackendPort::set_latency_range (const LatencyRange &latency_range, bool for_playback)
{
	LatencyRange& lr = for_playback ? _playback_latency_range : _capture_latency_range;

	if (lr == latency_range) {
		return;
	}

	lr = latency_range;

	for (std::set<BackendPortPtr>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
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
	for (std::set<BackendPortPtr>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		LatencyRange l;
		l = (*it)->latency_range (for_playback);
		lr.min = std::max (lr.min, l.min);
		lr.max = std::max (lr.max, l.max);
	}
	set_latency_range (lr, for_playback);
}

bool
BackendMIDIEvent::operator< (const BackendMIDIEvent &other) const {
	if (timestamp() == other.timestamp ()) {
		/* concurrent MIDI events, sort
		 * - CC first (may include bank/patch)
		 * - Program Change
		 * - Note Off
		 * - Note On
		 * - Note Pressure
		 * - Channel Pressure
		 * - Pitch Bend
		 * - SysEx/RT, etc
		 *
		 * see Evoral::Sequence<Time>::const_iterator::choose_next
		 * and MidiBuffer::second_simultaneous_midi_byte_is_first
		 */
		uint8_t ta = 9;
		uint8_t tb = 9;
		if (size () > 0 && size () < 4) {
			switch (data ()[0] & 0xf0) {
				case 0xB0: /* Control Change */
					ta = 1;
					break;
				case 0xC0: /* Program Change */
					ta = 2;
					break;
				case 0x80: /* Note Off */
					ta = 3;
					break;
				case 0x90: /* Note On */
					ta = 4;
					break;
				case 0xA0: /* Key Pressure */
					ta = 5;
					break;
				case 0xD0: /* Channel Pressure */
					ta = 6;
					break;
				case 0xE0: /* Pitch Bend */
					ta = 7;
					break;
				default:
					ta = 8;
					break;
			}
		}

		if (other.size () > 0 && other.size () < 4) {
			switch (other.data ()[0] & 0xf0) {
				case 0xB0: /* Control Change */
					tb = 1;
					break;
				case 0xC0: /* Program Change */
					tb = 2;
					break;
				case 0x80: /* Note Off */
					tb = 3;
					break;
				case 0x90: /* Note On */
					tb = 4;
					break;
				case 0xA0: /* Key Pressure */
					tb = 5;
					break;
				case 0xD0: /* Channel Pressure */
					tb = 6;
					break;
				case 0xE0: /* Pitch Bend */
					tb = 7;
					break;
				default:
					tb = 8;
					break;
			}
		}
		return ta < tb;
	}
	return timestamp () < other.timestamp ();
};

PortEngineSharedImpl::PortEngineSharedImpl (PortManager& mgr, std::string const & str)
	: _instance_name (str)
	, _portmap (new PortMap)
	, _ports (new PortIndex)
{
	g_atomic_int_set (&_port_change_flag, 0);
	pthread_mutex_init (&_port_callback_mutex, 0);
}

PortEngineSharedImpl::~PortEngineSharedImpl ()
{
	pthread_mutex_destroy (&_port_callback_mutex);
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
		BackendPortPtr port = *i;
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
	if (!valid_port (boost::dynamic_pointer_cast<BackendPort>(port))) {
		PBD::warning << _("BackendPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return boost::dynamic_pointer_cast<BackendPort>(port)->is_physical ();
}

void
PortEngineSharedImpl::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	boost::shared_ptr<PortIndex> p = _ports.reader();

	for (PortIndex::iterator i = p->begin (); i != p->end (); ++i) {
		BackendPortPtr port = *i;
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
		BackendPortPtr port = *i;
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
		BackendPortPtr port = *i;
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
		BackendPortPtr port = *i;
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

BackendPortPtr
PortEngineSharedImpl::add_port (const std::string& name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	assert(name.size ());

	if (find_port (name)) {
		PBD::error << string_compose (_("%1::register_port: Port already exists: (%2)"), _instance_name, name) << endmsg;
		return BackendPortPtr();
	}

	BackendPortPtr port (port_factory (name, type, flags));

	if (!port) {
		return BackendPortPtr ();
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
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort>(port_handle);

	{
		RCUWriter<PortIndex> index_writer (_ports);
		RCUWriter<PortMap> map_writer (_portmap);

		boost::shared_ptr<PortIndex> ps = index_writer.get_copy ();
		boost::shared_ptr<PortMap> pm = map_writer.get_copy ();

		PortIndex::iterator i = std::find (ps->begin(), ps->end(), boost::dynamic_pointer_cast<BackendPort> (port_handle));

		if (i == ps->end ()) {
			PBD::error << string_compose (_("%1::unregister_port: Failed to find port"), _instance_name) << endmsg;
			return;
		}

		disconnect_all (port_handle);

		pm->erase (port->name());
		ps->erase (i);
	}

	_ports.flush ();
	_portmap.flush ();
}


void
PortEngineSharedImpl::unregister_ports (bool system_only)
{
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();

	{
		RCUWriter<PortIndex> index_writer (_ports);
		RCUWriter<PortMap> map_writer (_portmap);

		boost::shared_ptr<PortIndex> ps = index_writer.get_copy ();
		boost::shared_ptr<PortMap> pm = map_writer.get_copy ();


		for (PortIndex::iterator i = ps->begin (); i != ps->end ();) {
			PortIndex::iterator cur = i++;
			BackendPortPtr port = *cur;
			if (! system_only || (port->is_physical () && port->is_terminal ())) {
				port->disconnect_all (port);
				pm->erase (port->name());
				ps->erase (cur);
			}
		}
	}

	_ports.flush ();
	_portmap.flush ();
}

void
PortEngineSharedImpl::clear_ports ()
{
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

	_ports.flush ();
	_portmap.flush ();

	g_atomic_int_set (&_port_change_flag, 0);
	pthread_mutex_lock (&_port_callback_mutex);
	_port_connection_queue.clear();
	pthread_mutex_unlock (&_port_callback_mutex);
}

uint32_t
PortEngineSharedImpl::port_name_size () const
{
	return 256;
}

int
PortEngineSharedImpl::set_port_name (PortEngine::PortHandle port_handle, const std::string& name)
{
	std::string newname (_instance_name + ":" + name);
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort>(port_handle);

	if (!valid_port (port)) {
		PBD::error << string_compose (_("%1::set_port_name: Invalid Port"), _instance_name) << endmsg;
		return -1;
	}

	if (find_port (newname)) {
		PBD::error << string_compose (_("%1::set_port_name: Port with given name already exists"), _instance_name) << endmsg;
		return -1;
	}

	const std::string old_name = port->name();
	int ret =  port->set_name (newname);

	if (ret == 0) {

		RCUWriter<PortMap> map_writer (_portmap);
		boost::shared_ptr<PortMap> pm = map_writer.get_copy ();

		pm->erase (old_name);
		pm->insert (make_pair (newname, port));
	}

	return ret;
}

std::string
PortEngineSharedImpl::get_port_name (PortEngine::PortHandle port_handle) const
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort>(port_handle);

	if (!valid_port (port)) {
		PBD::warning << string_compose (_("%1::get_port_name: invalid port"), _instance_name) << endmsg;
		return std::string ();
	}

	return port->name ();
}

PortFlags
PortEngineSharedImpl::get_port_flags (PortEngine::PortHandle port) const
{
	if (!valid_port (boost::dynamic_pointer_cast<BackendPort>(port))) {
		PBD::warning << string_compose (_("%1::get_port_flags: invalid port"), _instance_name) << endmsg;
		return PortFlags (0);
	}
	return boost::static_pointer_cast<BackendPort>(port)->flags ();
}

int
PortEngineSharedImpl::get_port_property (PortEngine::PortHandle port, const std::string& key, std::string& value, std::string& type) const
{
	if (!valid_port (boost::dynamic_pointer_cast<BackendPort>(port))) {
		PBD::warning << string_compose (_("%1::get_port_property: invalid port"), _instance_name) << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name") {
		type = "";
		value = boost::static_pointer_cast<BackendPort>(port)->pretty_name ();
		if (!value.empty()) {
			return 0;
		}
		value = boost::static_pointer_cast<BackendPort>(port)->hw_port_name ();
		if (!value.empty()) {
			return 0;
		}
	}
	return -1;
}

int
PortEngineSharedImpl::set_port_property (PortEngine::PortHandle port, const std::string& key, const std::string& value, const std::string& type)
{
	if (!valid_port (boost::dynamic_pointer_cast<BackendPort>(port))) {
		PBD::warning << string_compose (_("%1::set_port_property: invalid port"), _instance_name) << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name" && type.empty ()) {
		boost::static_pointer_cast<BackendPort>(port)->set_pretty_name (value);
		return 0;
	}
	return -1;
}

PortEngine::PortPtr
PortEngineSharedImpl::get_port_by_name (const std::string& name) const
{
	return find_port (name);
}

DataType
PortEngineSharedImpl::port_data_type (PortEngine::PortHandle port) const
{
	BackendPortPtr p = boost::dynamic_pointer_cast<BackendPort> (port);

	if (!valid_port (p)) {
		return DataType::NIL;
	}

	return p->type ();
}

PortEngine::PortPtr
PortEngineSharedImpl::register_port (
	const std::string& name,
	ARDOUR::DataType type,
	ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return PortEngine::PortPtr(); }
	if (flags & IsPhysical) { return PortEngine::PortPtr(); }
	return add_port (_instance_name + ":" + name, type, flags);
}

int
PortEngineSharedImpl::connect (const std::string& src, const std::string& dst)
{
	BackendPortPtr src_port = find_port (src);
	BackendPortPtr dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << string_compose (_("%1::connect: Invalid Source port: (%2)"), _instance_name, src) << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << string_compose (_("%1::connect: Invalid Destination port: (%2)"), _instance_name, dst) << endmsg;
		return -1;
	}

	src_port->connect (dst_port, src_port);

	return 0;
}

int
PortEngineSharedImpl::disconnect (const std::string& src, const std::string& dst)
{
	BackendPortPtr src_port = find_port (src);
	BackendPortPtr dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::warning << string_compose (_("%1::disconnect: invalid port"), _instance_name) << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port, src_port);
}

int
PortEngineSharedImpl::connect (PortEngine::PortHandle src, const std::string& dst)
{
	BackendPortPtr src_port = boost::dynamic_pointer_cast<BackendPort> (src);

	if (!valid_port (src_port)) {
		PBD::error << string_compose (_("%1::connect: Invalid Source Port Handle"), _instance_name) << endmsg;
		return -1;
	}

	BackendPortPtr dst_port = find_port (dst);

	if (!dst_port) {
		PBD::error << string_compose (_("%1::connect: Invalid Destination Port: (%2)"), _instance_name, dst) << endmsg;
		return -1;
	}

	src_port->connect (dst_port, src_port);

	return 0;
}

int
PortEngineSharedImpl::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	BackendPortPtr src_port = boost::dynamic_pointer_cast<BackendPort>(src);
	BackendPortPtr dst_port = find_port (dst);

	if (!valid_port (src_port) || !dst_port) {
		PBD::warning << string_compose (_("%1::disconnect: invalid port"), _instance_name) << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port, src_port);
}

int
PortEngineSharedImpl::disconnect_all (PortEngine::PortHandle port_handle)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);

	if (!valid_port (port)) {
		PBD::warning << string_compose (_("%1::disconnect_all: invalid port"), _instance_name) << endmsg;
		return -1;
	}

	port->disconnect_all (port);

	return 0;
}

bool
PortEngineSharedImpl::connected (PortEngine::PortHandle port_handle, bool /* process_callback_safe*/)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);

	if (!valid_port (port)) {
		PBD::error << string_compose (_("%1::disconnect_all: Invalid Port"), _instance_name) << endmsg;
		return false;
	}
	return port->is_connected ();
}

bool
PortEngineSharedImpl::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	BackendPortPtr src_port = boost::dynamic_pointer_cast<BackendPort> (src);
	BackendPortPtr dst_port = find_port (dst);
#ifndef NDEBUG
	if (!valid_port (src_port) || !dst_port) {
		PBD::error << string_compose (_("%1::connected_to: Invalid Port"), _instance_name) << endmsg;
		return false;
	}
#endif
	return boost::static_pointer_cast<BackendPort>(src)->is_connected (dst_port);
}

bool
PortEngineSharedImpl::physically_connected (PortEngine::PortHandle port_handle, bool /*process_callback_safe*/)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);

	if (!valid_port (port)) {
		PBD::error << string_compose (_("%1::physically_connected: Invalid Port"), _instance_name) << endmsg;
		return false;
	}
	return port->is_physically_connected ();
}

int
PortEngineSharedImpl::get_connections (PortEngine::PortHandle port_handle, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);

	if (!valid_port (port)) {
		PBD::error << string_compose (_("%1::get_connections: Invalid Port"), _instance_name) << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::set<BackendPortPtr>& connected_ports = port->get_connections ();

	for (std::set<BackendPortPtr>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

void
PortEngineSharedImpl::update_system_port_latencies ()
{
	for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
		(*it)->update_connected_latency (true);
	}
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		(*it)->update_connected_latency (false);
	}

	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
		(*it)->update_connected_latency (true);
	}
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
		(*it)->update_connected_latency (false);
	}
}

#ifndef NDEBUG
void
PortEngineSharedImpl::list_ports () const
{
	boost::shared_ptr<PortIndex> p = _ports.reader ();
	for (PortIndex::const_iterator i = p->begin (); i != p->end (); ++i) {
		std::cout << (*i)->name () << "\n";
	}
}
#endif
