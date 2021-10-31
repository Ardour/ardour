/*
 * Copyright (C) 2013-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <algorithm>
#include <vector>

#ifdef COMPILER_MSVC
#include <ardourext/misc.h>
#include <io.h> // Microsoft's nearest equivalent to <unistd.h>
#else
#include <regex.h>
#endif

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/strsplit.h"
#include "pbd/unwind.h"

#include "ardour/async_midi_port.h"
#include "ardour/audio_backend.h"
#include "ardour/audio_port.h"
#include "ardour/circular_buffer.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"
#include "ardour/port_manager.h"
#include "ardour/profile.h"
#include "ardour/rt_tasklist.h"
#include "ardour/session.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;
using std::vector;

PortManager::AudioInputPort::AudioInputPort (samplecnt_t sz)
	: scope (AudioPortScope (new CircularSampleBuffer (sz)))
	, meter (AudioPortMeter (new DPM))
{
}

PortManager::MIDIInputPort::MIDIInputPort (samplecnt_t sz)
	: monitor (MIDIPortMonitor (new CircularEventBuffer (sz)))
	, meter (MIDIPortMeter (new MPM))
{
}

PortManager::PortID::PortID (boost::shared_ptr<AudioBackend> b, DataType dt, bool in, std::string const& pn)
	: backend (b->name ())
	, port_name (pn)
	, data_type (dt)
	, input (in)
{
	if (dt == DataType::MIDI) {
		/* Audio device name is not applicable for MIDI ports */
		device_name = "";
	} else if (b->use_separate_input_and_output_devices ()) {
		device_name = in ? b->input_device_name () : b->output_device_name ();
	} else {
		device_name = b->device_name ();
	}
}

PortManager::PortID::PortID (XMLNode const& node, bool old_midi_format)
	: data_type (DataType::NIL)
	, input (false)
{
	bool err = false;

	if (node.name () != (old_midi_format ? "port" : "PortID")) {
		throw failed_constructor ();
	}

	err |= !node.get_property ("backend", backend);
	err |= !node.get_property ("input", input);

	if (old_midi_format) {
		err |= !node.get_property ("name", port_name);
		data_type   = DataType::MIDI;
		device_name = "";
	} else {
		err |= !node.get_property ("device-name", device_name);
		err |= !node.get_property ("port-name", port_name);
		err |= !node.get_property ("data-type", data_type);
	}

	if (err) {
		throw failed_constructor ();
	}
}

XMLNode&
PortManager::PortID::state () const
{
	XMLNode* node = new XMLNode ("PortID");
	node->set_property ("backend",     backend);
	node->set_property ("device-name", device_name);
	node->set_property ("port-name",   port_name);
	node->set_property ("data-type",   data_type);
	node->set_property ("input",       input);
	return *node;
}

PortManager::PortMetaData::PortMetaData (XMLNode const& node)
{
	bool err = false;

	err |= !node.get_property ("pretty-name", pretty_name);
	err |= !node.get_property ("properties", properties);

	if (err) {
		throw failed_constructor ();
	}
}

/* ****************************************************************************/

PortManager::PortManager ()
	: _ports (new Ports)
	, _port_remove_in_progress (false)
	, _port_deletions_pending (8192) /* ick, arbitrary sizing */
	, _midi_info_dirty (true)
	, _audio_input_ports (new AudioInputPorts)
	, _midi_input_ports (new MIDIInputPorts)
{
	g_atomic_int_set (&_reset_meters, 1);
	load_port_info ();
}

void
PortManager::clear_pending_port_deletions ()
{
	Port* p;

	DEBUG_TRACE (DEBUG::Ports, string_compose ("pending port deletions: %1\n", _port_deletions_pending.read_space ()));

	while (_port_deletions_pending.read (&p, 1) == 1) {
		delete p;
	}
}

void
PortManager::remove_all_ports ()
{
	/* make sure that JACK callbacks that will be invoked as we cleanup
	 * ports know that they have nothing to do.
	 */

	PBD::Unwinder<bool> uw (_port_remove_in_progress, true);

	/* process lock MUST be held by caller
	*/

	{
		RCUWriter<Ports>         writer (_ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}

	/* clear dead wood list in RCU */

	_ports.flush ();

	/* clear out pending port deletion list. we know this is safe because
	 * the auto connect thread in Session is already dead when this is
	 * done. It doesn't use shared_ptr<Port> anyway.
	 */

	_port_deletions_pending.reset ();
}

string
PortManager::make_port_name_relative (const string& portname) const
{
	if (!_backend) {
		return portname;
	}

	string::size_type colon = portname.find (':');

	if (colon == string::npos) {
		return portname;
	}

	if (portname.substr (0, colon) == _backend->my_name ()) {
		return portname.substr (colon + 1);
	}

	return portname;
}

string
PortManager::make_port_name_non_relative (const string& portname) const
{
	string str;

	if (portname.find_first_of (':') != string::npos) {
		return portname;
	}

	str = _backend->my_name ();
	str += ':';
	str += portname;

	return str;
}

std::string
PortManager::get_pretty_name_by_name (const std::string& portname) const
{
	PortEngine::PortHandle ph = _backend->get_port_by_name (portname);

	if (ph) {
		std::string value;
		std::string type;
		if (0 == _backend->get_port_property (ph, "http://jackaudio.org/metadata/pretty-name", value, type)) {
			return value;
		}
	}

	return string ();
}

bool
PortManager::port_is_mine (const string& portname) const
{
	if (!_backend) {
		return true;
	}

	string self = _backend->my_name ();

	if (portname.find_first_of (':') != string::npos) {
		if (portname.substr (0, self.length ()) != self) {
			return false;
		}
	}

	return true;
}

bool
PortManager::port_is_physical (const std::string& portname) const
{
	if (!_backend) {
		return false;
	}

	PortEngine::PortHandle ph = _backend->get_port_by_name (portname);
	if (!ph) {
		return false;
	}

	return _backend->port_is_physical (ph);
}

void
PortManager::filter_midi_ports (vector<string>& ports, MidiPortFlags include, MidiPortFlags exclude)
{
	if (!include && !exclude) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_port_info_mutex);
	fill_midi_port_info_locked ();

	for (vector<string>::iterator si = ports.begin (); si != ports.end ();) {
		PortInfo::iterator x;
		for (x = _port_info.begin (); x != _port_info.end (); ++x) {
			if (x->first.data_type != DataType::MIDI) {
				continue;
			}
			if (x->first.backend != _backend->name ()) {
				continue;
			}
			if (x->first.port_name == *si) {
				break;
			}
		}

		if (x == _port_info.end ()) {
			++si;
			continue;
		}

		if (include) {
			if ((x->second.properties & include) != include) {
				/* properties do not include requested ones */
				si = ports.erase (si);
				continue;
			}
		}

		if (exclude) {
			if ((x->second.properties & exclude)) {
				/* properties include ones to avoid */
				si = ports.erase (si);
				continue;
			}
		}

		++si;
	}
}

void
PortManager::get_physical_outputs (DataType type, std::vector<std::string>& s, MidiPortFlags include, MidiPortFlags exclude)
{
	if (!_backend) {
		s.clear ();
		return;
	}
	_backend->get_physical_outputs (type, s);
	filter_midi_ports (s, include, exclude);
}

void
PortManager::get_physical_inputs (DataType type, std::vector<std::string>& s, MidiPortFlags include, MidiPortFlags exclude)
{
	if (!_backend) {
		s.clear ();
		return;
	}

	_backend->get_physical_inputs (type, s);
	filter_midi_ports (s, include, exclude);
}

ChanCount
PortManager::n_physical_outputs () const
{
	if (!_backend) {
		return ChanCount::ZERO;
	}

	return _backend->n_physical_outputs ();
}

ChanCount
PortManager::n_physical_inputs () const
{
	if (!_backend) {
		return ChanCount::ZERO;
	}
	return _backend->n_physical_inputs ();
}

/** @param name Full or short name of port
 *  @return Corresponding Port or 0.
 */
boost::shared_ptr<Port>
PortManager::get_port_by_name (const string& portname)
{
	if (!_backend) {
		return boost::shared_ptr<Port> ();
	}

	if (!port_is_mine (portname)) {
		/* not an ardour port */
		return boost::shared_ptr<Port> ();
	}

	boost::shared_ptr<Ports> pr  = _ports.reader ();
	std::string              rel = make_port_name_relative (portname);
	Ports::iterator          x   = pr->find (rel);

	if (x != pr->end ()) {
		/* its possible that the port was renamed by some 3rd party and
		 * we don't know about it. check for this (the check is quick
		 * and cheap), and if so, rename the port (which will alter
		 * the port map as a side effect).
		 */
		const std::string check = make_port_name_relative (_backend->get_port_name (x->second->port_handle ()));
		if (check != rel) {
			x->second->set_name (check);
		}
		return x->second;
	}

	return boost::shared_ptr<Port> ();
}

void
PortManager::port_renamed (const std::string& old_relative_name, const std::string& new_relative_name)
{
	RCUWriter<Ports>         writer (_ports);
	boost::shared_ptr<Ports> p = writer.get_copy ();
	Ports::iterator          x = p->find (old_relative_name);

	if (x != p->end ()) {
		boost::shared_ptr<Port> port = x->second;
		p->erase (x);
		p->insert (make_pair (new_relative_name, port));
	}
}

int
PortManager::get_ports (DataType type, PortList& pl)
{
	boost::shared_ptr<Ports> plist = _ports.reader ();
	for (Ports::iterator p = plist->begin (); p != plist->end (); ++p) {
		if (p->second->type () == type) {
			pl.push_back (p->second);
		}
	}
	return pl.size ();
}

int
PortManager::get_ports (const string& port_name_pattern, DataType type, PortFlags flags, vector<string>& s)
{
	s.clear ();

	if (!_backend) {
		return 0;
	}

	return _backend->get_ports (port_name_pattern, type, flags, s);
}

void
PortManager::port_registration_failure (const std::string& portname)
{
	if (!_backend) {
		return;
	}

	string full_portname = _backend->my_name ();
	full_portname += ':';
	full_portname += portname;

	PortEngine::PortHandle p = _backend->get_port_by_name (full_portname);
	string                 reason;

	if (p) {
		reason = string_compose (_("a port with the name \"%1\" already exists: check for duplicated track/bus names"), portname);
	} else {
		reason = string_compose (_("No more ports are available. You will need to stop %1 and restart with more ports if you need this many tracks."), PROGRAM_NAME);
	}

	throw PortRegistrationFailure (string_compose (_("AudioEngine: cannot register port \"%1\": %2"), portname, reason).c_str ());
}

struct PortDeleter {
	void operator() (Port* p)
	{
		AudioEngine::instance ()->add_pending_port_deletion (p);
	}
};

boost::shared_ptr<Port>
PortManager::register_port (DataType dtype, const string& portname, bool input, bool async, PortFlags flags)
{
	boost::shared_ptr<Port> newport;

	/* limit the possible flags that can be set */

	flags = PortFlags (flags & (Hidden | Shadow | IsTerminal | TransportSyncPort));

	try {
		if (dtype == DataType::AUDIO) {
			DEBUG_TRACE (DEBUG::Ports, string_compose ("registering AUDIO port %1, input %2\n",
			                                           portname, input));
			newport.reset (new AudioPort (portname, PortFlags ((input ? IsInput : IsOutput) | flags)),
			               PortDeleter ());
		} else if (dtype == DataType::MIDI) {
			if (async) {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("registering ASYNC MIDI port %1, input %2\n",
				                                           portname, input));
				newport.reset (new AsyncMIDIPort (portname, PortFlags ((input ? IsInput : IsOutput) | flags)),
				               PortDeleter ());
				_midi_info_dirty = true;
			} else {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("registering MIDI port %1, input %2\n",
				                                           portname, input));
				newport.reset (new MidiPort (portname, PortFlags ((input ? IsInput : IsOutput) | flags)),
				               PortDeleter ());
			}
		} else {
			throw PortRegistrationFailure (string_compose ("unable to create port '%1': %2", portname, _("(unknown type)")));
		}

		newport->set_buffer_size (AudioEngine::instance ()->samples_per_cycle ());

		RCUWriter<Ports>         writer (_ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->insert (make_pair (make_port_name_relative (portname), newport));

		/* writer goes out of scope, forces update */
	}

	catch (PortRegistrationFailure& err) {
		throw err;
	} catch (std::exception& e) {
		throw PortRegistrationFailure (string_compose ("unable to create port '%1': %2", portname, e.what ()).c_str ());
	} catch (...) {
		throw PortRegistrationFailure (string_compose ("unable to create port '%1': %2", portname, _("(unknown error)")));
	}

	DEBUG_TRACE (DEBUG::Ports, string_compose ("\t%2 port registration success, ports now = %1\n", _ports.reader ()->size (), this));
	return newport;
}

boost::shared_ptr<Port>
PortManager::register_input_port (DataType type, const string& portname, bool async, PortFlags extra_flags)
{
	return register_port (type, portname, true, async, extra_flags);
}

boost::shared_ptr<Port>
PortManager::register_output_port (DataType type, const string& portname, bool async, PortFlags extra_flags)
{
	return register_port (type, portname, false, async, extra_flags);
}

int
PortManager::unregister_port (boost::shared_ptr<Port> port)
{
	/* This is a little subtle. We do not call the backend's port
	 * unregistration code from here. That is left for the Port
	 * destructor. We are trying to drop references to the Port object
	 * here, so that its destructor will run and it will unregister itself.
	 */

	/* caller must hold process lock */

	{
		RCUWriter<Ports>         writer (_ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		Ports::iterator          x  = ps->find (make_port_name_relative (port->name ()));

		if (x != ps->end ()) {
			DEBUG_TRACE (DEBUG::Ports, string_compose ("removing %1 from port map (uc=%2)\n", port->name (), port.use_count ()));
			ps->erase (x);
		}

		/* writer goes out of scope, forces update */
	}

	_ports.flush ();

	return 0;
}

bool
PortManager::connected (const string& port_name)
{
	if (!_backend) {
		return false;
	}

	PortEngine::PortHandle handle = _backend->get_port_by_name (port_name);

	if (!handle) {
		return false;
	}

	return _backend->connected (handle);
}

bool
PortManager::physically_connected (const string& port_name)
{
	if (!_backend) {
		return false;
	}

	PortEngine::PortHandle handle = _backend->get_port_by_name (port_name);

	if (!handle) {
		return false;
	}

	return _backend->physically_connected (handle);
}

int
PortManager::get_connections (const string& port_name, std::vector<std::string>& s)
{
	if (!_backend) {
		s.clear ();
		return 0;
	}

	PortEngine::PortHandle handle = _backend->get_port_by_name (port_name);

	if (!handle) {
		s.clear ();
		return 0;
	}

	return _backend->get_connections (handle, s);
}

int
PortManager::connect (const string& source, const string& destination)
{
	int ret;

	string s = make_port_name_non_relative (source);
	string d = make_port_name_non_relative (destination);

	boost::shared_ptr<Port> src = get_port_by_name (s);
	boost::shared_ptr<Port> dst = get_port_by_name (d);

	if (src) {
		ret = src->connect (d);
	} else if (dst) {
		ret = dst->connect (s);
	} else {
		/* neither port is known to us ...hand-off to the PortEngine
		 */
		if (_backend) {
			ret = _backend->connect (s, d);
		} else {
			ret = -1;
		}
	}

	if (ret > 0) {
		/* already exists - no error, no warning */
	} else if (ret < 0) {
		error << string_compose (_("AudioEngine: cannot connect %1 (%2) to %3 (%4)"),
		                         source, s, destination, d)
		      << endmsg;
	}

	return ret;
}

int
PortManager::disconnect (const string& source, const string& destination)
{
	int ret;

	string s = make_port_name_non_relative (source);
	string d = make_port_name_non_relative (destination);

	boost::shared_ptr<Port> src = get_port_by_name (s);
	boost::shared_ptr<Port> dst = get_port_by_name (d);

	if (src) {
		ret = src->disconnect (d);
	} else if (dst) {
		ret = dst->disconnect (s);
	} else {
		/* neither port is known to us ...hand-off to the PortEngine
		 */
		if (_backend) {
			ret = _backend->disconnect (s, d);
		} else {
			ret = -1;
		}
	}
	return ret;
}

int
PortManager::disconnect (boost::shared_ptr<Port> port)
{
	return port->disconnect_all ();
}

int
PortManager::disconnect (std::string const& name)
{
	PortEngine::PortHandle ph = _backend->get_port_by_name (name);
	if (ph) {
		return _backend->disconnect_all (ph);
	}
	return -2;
}

int
PortManager::reestablish_ports ()
{
	Ports::iterator i;
	_midi_info_dirty           = true;
	boost::shared_ptr<Ports> p = _ports.reader ();
	DEBUG_TRACE (DEBUG::Ports, string_compose ("reestablish %1 ports\n", p->size ()));

	for (i = p->begin (); i != p->end (); ++i) {
		if (i->second->reestablish ()) {
			error << string_compose (_("Re-establising port %1 failed"), i->second->name ()) << endmsg;
			std::cerr << string_compose (_("Re-establising port %1 failed"), i->second->name ()) << std::endl;
			break;
		}
	}

	if (i != p->end ()) {
		/* failed */
		remove_all_ports ();
		return -1;
	}

	if (!_backend->info ().already_configured ()) {
		std::vector<std::string> port_names;
		get_physical_inputs (DataType::AUDIO, port_names);
		set_pretty_names (port_names, DataType::AUDIO, true);

		port_names.clear ();
		get_physical_outputs (DataType::AUDIO, port_names);
		set_pretty_names (port_names, DataType::AUDIO, false);

		port_names.clear ();
		get_physical_inputs (DataType::MIDI, port_names);
		set_pretty_names (port_names, DataType::MIDI, true);

		port_names.clear ();
		get_physical_outputs (DataType::MIDI, port_names);
		set_pretty_names (port_names, DataType::MIDI, false);
	}

	if (Config->get_work_around_jack_no_copy_optimization () && AudioEngine::instance ()->current_backend_name () == X_("JACK")) {
		port_engine ().register_port (X_("physical_audio_input_monitor_enable"), DataType::AUDIO, ARDOUR::PortFlags (IsInput | IsTerminal | Hidden));
		port_engine ().register_port (X_("physical_midi_input_monitor_enable"), DataType::MIDI, ARDOUR::PortFlags (IsInput | IsTerminal | Hidden));
	}

	update_input_ports (true);
	return 0;
}

void
PortManager::set_pretty_names (std::vector<std::string> const& port_names, DataType dt, bool input)
{
	Glib::Threads::Mutex::Lock lm (_port_info_mutex);
	for (std::vector<std::string>::const_iterator p = port_names.begin (); p != port_names.end (); ++p) {
		if (port_is_mine (*p)) {
			continue;
		}
		PortEngine::PortHandle ph = _backend->get_port_by_name (*p);
		if (!ph) {
			continue;
		}
		PortID             pid (_backend, dt, input, *p);
		PortInfo::iterator x = _port_info.find (pid);
		if (x == _port_info.end ()) {
			continue;
		}
		_backend->set_port_property (ph, "http://jackaudio.org/metadata/pretty-name", x->second.pretty_name, string ());
	}
}

int
PortManager::reconnect_ports ()
{
	boost::shared_ptr<Ports> p = _ports.reader ();

	/* re-establish connections */

	DEBUG_TRACE (DEBUG::Ports, string_compose ("reconnect %1 ports\n", p->size ()));

	for (Ports::iterator i = p->begin (); i != p->end (); ++i) {
		if (i->second->reconnect ()) {
			PortConnectedOrDisconnected (i->second, i->first, boost::weak_ptr<Port> (), "", false);
		}
	}

	if (Config->get_work_around_jack_no_copy_optimization () && AudioEngine::instance ()->current_backend_name () == X_("JACK")) {
		std::string const        audio_port = AudioEngine::instance ()->make_port_name_non_relative (X_("physical_audio_input_monitor_enable"));
		std::string const        midi_port  = AudioEngine::instance ()->make_port_name_non_relative (X_("physical_midi_input_monitor_enable"));
		std::vector<std::string> audio_ports;
		std::vector<std::string> midi_ports;
		get_physical_inputs (DataType::AUDIO, audio_ports);
		get_physical_inputs (DataType::MIDI, midi_ports);
		for (std::vector<std::string>::iterator p = audio_ports.begin (); p != audio_ports.end (); ++p) {
			port_engine ().connect (*p, audio_port);
		}
		for (std::vector<std::string>::iterator p = midi_ports.begin (); p != midi_ports.end (); ++p) {
			port_engine ().connect (*p, midi_port);
		}
	}

	return 0;
}

void
PortManager::connect_callback (const string& a, const string& b, bool conn)
{
	DEBUG_TRACE (DEBUG::BackendCallbacks, string_compose (X_("connect callback %1 + %2 connected ? %3\n"), a, b, conn));

	boost::shared_ptr<Port>  port_a;
	boost::shared_ptr<Port>  port_b;
	Ports::iterator          x;
	boost::shared_ptr<Ports> pr = _ports.reader ();

	x = pr->find (make_port_name_relative (a));
	if (x != pr->end ()) {
		port_a = x->second;
	}

	x = pr->find (make_port_name_relative (b));
	if (x != pr->end ()) {
		port_b = x->second;
	}

	if (conn) {
		if (port_a && !port_b) {
			port_a->increment_external_connections ();
		} else if (port_b && !port_a) {
			port_b->increment_external_connections ();
		}
	} else {
		if (port_a && !port_b) {
			port_a->decrement_external_connections ();
		} else if (port_b && !port_a) {
			port_b->decrement_external_connections ();
		}
	}

	PortConnectedOrDisconnected (
	    port_a, a,
	    port_b, b,
	    conn); /* EMIT SIGNAL */
}

void
PortManager::registration_callback ()
{
	DEBUG_TRACE (DEBUG::BackendCallbacks, "port registration callback\n");

	if (_port_remove_in_progress) {
		return;
	}

	update_input_ports (false);

	PortRegisteredOrUnregistered (); /* EMIT SIGNAL */
}

struct MIDIConnectCall {
	MIDIConnectCall (std::vector<std::string> const& pl)
		: port_list (pl)
	{
	}
	std::vector<std::string> port_list;
};

static void*
_midi_connect (void* arg)
{
	MIDIConnectCall*  mcl      = static_cast<MIDIConnectCall*> (arg);
	std::string const our_name = AudioEngine::instance ()->make_port_name_non_relative (X_("physical_midi_input_monitor_enable"));

	for (vector<string>::const_iterator p = mcl->port_list.begin (); p != mcl->port_list.end (); ++p) {
		AudioEngine::instance ()->connect (*p, our_name);
	}
	delete mcl;
	return 0;
}

void
PortManager::update_input_ports (bool clear)
{
	std::vector<std::string> audio_ports;
	std::vector<std::string> midi_ports;

	std::vector<std::string> new_audio;
	std::vector<std::string> old_audio;
	std::vector<std::string> new_midi;
	std::vector<std::string> old_midi;

	get_physical_inputs (DataType::AUDIO, audio_ports);
	get_physical_inputs (DataType::MIDI, midi_ports);

	if (clear) {
		new_audio = audio_ports;
		new_midi  = midi_ports;
		_monitor_port.clear_ports (true);
	} else {
		boost::shared_ptr<AudioInputPorts> aip = _audio_input_ports.reader ();
		/* find new audio ports */
		for (std::vector<std::string>::iterator p = audio_ports.begin (); p != audio_ports.end (); ++p) {
			if (port_is_mine (*p) || !_backend->get_port_by_name (*p)) {
				continue;
			}
			if (aip->find (*p) == aip->end ()) {
				new_audio.push_back (*p);
			}
		}

		/* find stale audio ports */
		for (AudioInputPorts::iterator p = aip->begin (); p != aip->end (); ++p) {
			if (std::find (audio_ports.begin (), audio_ports.end (), p->first) == audio_ports.end ()) {
				old_audio.push_back (p->first);
			}
		}

		boost::shared_ptr<MIDIInputPorts> mip = _midi_input_ports.reader ();
		/* find new MIDI ports */
		for (std::vector<std::string>::iterator p = midi_ports.begin (); p != midi_ports.end (); ++p) {
			if (port_is_mine (*p) || !_backend->get_port_by_name (*p)) {
				continue;
			}
#ifdef HAVE_ALSA
			if ((*p).find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos) {
				continue;
			}
#endif
			if (mip->find (*p) == mip->end ()) {
				new_midi.push_back (*p);
			}
		}

		/* find stale audio ports */
		for (MIDIInputPorts::iterator p = mip->begin (); p != mip->end (); ++p) {
			if (std::find (midi_ports.begin (), midi_ports.end (), p->first) == midi_ports.end ()) {
				old_midi.push_back (p->first);
			}
		}
	}

	if (!new_audio.empty () || !old_audio.empty () || clear) {
		RCUWriter<AudioInputPorts>         apwr (_audio_input_ports);
		boost::shared_ptr<AudioInputPorts> apw = apwr.get_copy ();
		if (clear) {
			apw->clear ();
		} else {
			for (std::vector<std::string>::const_iterator p = old_audio.begin (); p != old_audio.end (); ++p) {
				apw->erase (*p);
				_monitor_port.remove_port (*p, true);
			}
		}
		for (std::vector<std::string>::const_iterator p = new_audio.begin (); p != new_audio.end (); ++p) {
			if (port_is_mine (*p) || !_backend->get_port_by_name (*p)) {
				continue;
			}
			apw->insert (make_pair (*p, AudioInputPort (24288))); // 2^19 ~ 1MB / port
		}
	}

	std::vector<std::string> physical_midi_connection_list;

	if (!new_midi.empty () || !old_midi.empty () || clear) {
		RCUWriter<MIDIInputPorts>         mpwr (_midi_input_ports);
		boost::shared_ptr<MIDIInputPorts> mpw = mpwr.get_copy ();
		if (clear) {
			mpw->clear ();
		} else {
			for (std::vector<std::string>::const_iterator p = old_midi.begin (); p != old_midi.end (); ++p) {
				mpw->erase (*p);
			}
		}
		for (std::vector<std::string>::const_iterator p = new_midi.begin (); p != new_midi.end (); ++p) {
			if (port_is_mine (*p) || !_backend->get_port_by_name (*p)) {
				continue;
			}
#ifdef HAVE_ALSA
			if ((*p).find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos) {
				continue;
			}
#endif
			mpw->insert (make_pair (*p, MIDIInputPort (32)));

			if (Config->get_work_around_jack_no_copy_optimization () && AudioEngine::instance ()->current_backend_name () == X_("JACK")) {
				physical_midi_connection_list.push_back (*p);
			}
		}
	}

	if (clear) {
		/* don't send notifcation for initial setup.
		 * Physical I/O is initially connected in
		 * reconnect_ports(), it is too early to
		 * do this when called from ::reestablish_ports()
		 * "JACK: Cannot connect ports owned by inactive clients"
		 */
		return;
	}

	if (!physical_midi_connection_list.empty ()) {
		/* handle hotplug, connect in bg thread, because
		 * "JACK: Cannot callback the server in notification thread!"
		 */
		pthread_t        thread;
		MIDIConnectCall* mcl = new MIDIConnectCall (physical_midi_connection_list);
		pthread_create_and_store ("midi-connect", &thread, _midi_connect, mcl);
		pthread_detach (thread);
	}

	if (!old_audio.empty ()) {
		PhysInputChanged (DataType::AUDIO, old_audio, false);
	}
	if (!old_midi.empty ()) {
		PhysInputChanged (DataType::MIDI, old_midi, false);
	}
	if (!new_audio.empty ()) {
		PhysInputChanged (DataType::AUDIO, new_audio, true);
	}
	if (!new_midi.empty ()) {
		PhysInputChanged (DataType::MIDI, new_midi, true);
	}
}

bool
PortManager::can_request_input_monitoring () const
{
	if (!_backend) {
		return false;
	}

	return _backend->can_monitor_input ();
}

void
PortManager::request_input_monitoring (const string& name, bool yn) const
{
	if (!_backend) {
		return;
	}

	PortEngine::PortHandle ph = _backend->get_port_by_name (name);

	if (ph) {
		_backend->request_input_monitoring (ph, yn);
	}
}

void
PortManager::ensure_input_monitoring (const string& name, bool yn) const
{
	if (!_backend) {
		return;
	}

	PortEngine::PortHandle ph = _backend->get_port_by_name (name);

	if (ph) {
		_backend->ensure_input_monitoring (ph, yn);
	}
}

uint32_t
PortManager::port_name_size () const
{
	if (!_backend) {
		return 0;
	}

	return _backend->port_name_size ();
}

string
PortManager::my_name () const
{
	if (!_backend) {
		return string ();
	}

	return _backend->my_name ();
}

int
PortManager::graph_order_callback ()
{
	DEBUG_TRACE (DEBUG::BackendCallbacks, "graph order callback\n");

	if (!_port_remove_in_progress) {
		GraphReordered (); /* EMIT SIGNAL */
	}

	return 0;
}

void
PortManager::cycle_start (pframes_t nframes, Session* s)
{
	Port::set_global_port_buffer_offset (0);
	Port::set_cycle_samplecnt (nframes);

	_cycle_ports = _ports.reader ();

	/* TODO optimize
	 *  - when speed == 1.0, the resampler copies data without processing
	 *   it may (or may not) be more efficient to just run all in sequence.
	 *
	 *  - single sequential task for 'lightweight' tasks would make sense
	 *    (run it in parallel with 'heavy' resampling.
	 *    * output ports (sends_output()) only set a flag
	 *    * midi-ports only scale event timestamps
	 *
	 *  - a threshold parallel vs searial processing may be appropriate.
	 *    amount of work (how many connected ports are there, how
	 *    many resamplers need to run) vs. available CPU cores and semaphore
	 *    synchronization overhead.
	 *
	 *  - input ports: it would make sense to resample each input only once
	 *    (rather than resample into each ardour-owned input port).
	 *    A single external source-port may be connected to many ardour
	 *    input-ports. Currently re-sampling is per input.
	 */
	if (s && s->rt_tasklist () && fabs (Port::speed_ratio ()) != 1.0) {
		RTTaskList::TaskList tl;
		for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
			if (!(p->second->flags () & TransportSyncPort)) {
				tl.push_back (boost::bind (&Port::cycle_start, p->second, nframes));
			}
		}
		tl.push_back (boost::bind (&PortManager::run_input_meters, this, nframes, s ? s->nominal_sample_rate () : 0));
		s->rt_tasklist ()->process (tl);
	} else {
		for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
			if (!(p->second->flags () & TransportSyncPort)) {
				p->second->cycle_start (nframes);
			}
		}
		run_input_meters (nframes, s ? s->nominal_sample_rate () : 0);
	}
}

void
PortManager::cycle_end (pframes_t nframes, Session* s)
{
	// see optimzation note in ::cycle_start()
	if (0 && s && s->rt_tasklist () && fabs (Port::speed_ratio ()) != 1.0) {
		RTTaskList::TaskList tl;
		for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
			if (!(p->second->flags () & TransportSyncPort)) {
				tl.push_back (boost::bind (&Port::cycle_end, p->second, nframes));
			}
		}
		s->rt_tasklist ()->process (tl);
	} else {
		for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
			if (!(p->second->flags () & TransportSyncPort)) {
				p->second->cycle_end (nframes);
			}
		}
	}

	for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
		/* AudioEngine::split_cycle flushes buffers until Port::port_offset.
		 * Now only flush remaining events (after Port::port_offset) */
		p->second->flush_buffers (nframes * Port::speed_ratio () - Port::port_offset ());
	}

	_cycle_ports.reset ();

	/* we are done */
}

void
PortManager::silence (pframes_t nframes, Session* s)
{
	for (Ports::iterator i = _cycle_ports->begin (); i != _cycle_ports->end (); ++i) {
		if (s && i->second == s->mtc_output_port ()) {
			continue;
		}
		if (s && i->second == s->midi_clock_output_port ()) {
			continue;
		}
		if (s && i->second == s->ltc_output_port ()) {
			continue;
		}
		if (boost::dynamic_pointer_cast<AsyncMIDIPort> (i->second)) {
			continue;
		}
		if (i->second->sends_output ()) {
			i->second->get_buffer (nframes).silence (nframes);
		}
	}
}

void
PortManager::silence_outputs (pframes_t nframes)
{
	std::vector<std::string> port_names;
	if (get_ports ("", DataType::AUDIO, IsOutput, port_names)) {
		for (std::vector<std::string>::iterator p = port_names.begin (); p != port_names.end (); ++p) {
			if (!port_is_mine (*p)) {
				continue;
			}
			PortEngine::PortHandle ph = _backend->get_port_by_name (*p);
			if (!ph) {
				continue;
			}
			void* buf = _backend->get_buffer (ph, nframes);
			if (!buf) {
				continue;
			}
			memset (buf, 0, sizeof (float) * nframes);
		}
	}

	if (get_ports ("", DataType::MIDI, IsOutput, port_names)) {
		for (std::vector<std::string>::iterator p = port_names.begin (); p != port_names.end (); ++p) {
			if (!port_is_mine (*p)) {
				continue;
			}
			PortEngine::PortHandle ph = _backend->get_port_by_name (*p);
			if (!ph) {
				continue;
			}
			void* buf = _backend->get_buffer (ph, nframes);
			if (!buf) {
				continue;
			}
			_backend->midi_clear (buf);
		}
	}
}

void
PortManager::check_monitoring ()
{
	for (Ports::iterator i = _cycle_ports->begin (); i != _cycle_ports->end (); ++i) {
		bool x;

		if (i->second->last_monitor () != (x = i->second->monitoring_input ())) {
			i->second->set_last_monitor (x);
			/* XXX I think this is dangerous, due to
			   a likely mutex in the signal handlers ...
			*/
			i->second->MonitorInputChanged (x); /* EMIT SIGNAL */
		}
	}
}

void
PortManager::cycle_end_fade_out (gain_t base_gain, gain_t gain_step, pframes_t nframes, Session* s)
{
	// see optimzation note in ::cycle_start()
	if (0 && s && s->rt_tasklist () && fabs (Port::speed_ratio ()) != 1.0) {
		RTTaskList::TaskList tl;
		for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
			if (!(p->second->flags () & TransportSyncPort)) {
				tl.push_back (boost::bind (&Port::cycle_end, p->second, nframes));
			}
		}
		s->rt_tasklist ()->process (tl);
	} else {
		for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
			if (!(p->second->flags () & TransportSyncPort)) {
				p->second->cycle_end (nframes);
			}
		}
	}

	for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
		p->second->flush_buffers (nframes);

		if (p->second->sends_output ()) {
			boost::shared_ptr<AudioPort> ap = boost::dynamic_pointer_cast<AudioPort> (p->second);
			if (ap) {
				Sample* s = ap->engine_get_whole_audio_buffer ();
				gain_t  g = base_gain;

				for (pframes_t n = 0; n < nframes; ++n) {
					*s++ *= g;
					g -= gain_step;
				}
			}
		}
	}
	_cycle_ports.reset ();
	/* we are done */
}

PortEngine&
PortManager::port_engine ()
{
	assert (_backend);
	return *_backend;
}

bool
PortManager::port_is_control_only (std::string const& name)
{
	static regex_t compiled_pattern;
	static string  pattern;

	if (pattern.empty ()) {
		/* This is a list of regular expressions that match ports
		 * related to physical MIDI devices that we do not want to
		 * expose as normal physical ports.
		 */

		const char* const control_only_ports[] = {
			X_(".*Ableton Push.*"),
			X_(".*FaderPort .*"),
			X_(".*FaderPort8 .*"),
			X_(".*FaderPort16 .*"),
			X_(".*FaderPort2 .*"),
			X_(".*US-2400 .*"),
			X_(".*Mackie .*"),
			X_(".*MIDI Control .*"),
		};

		pattern = "(";
		for (size_t n = 0; n < sizeof (control_only_ports) / sizeof (control_only_ports[0]); ++n) {
			if (n > 0) {
				pattern += '|';
			}
			pattern += control_only_ports[n];
		}
		pattern += ')';

		regcomp (&compiled_pattern, pattern.c_str (), REG_EXTENDED | REG_NOSUB);
	}

	return regexec (&compiled_pattern, name.c_str (), 0, 0, 0) == 0;
}

static bool
ends_with (std::string const& str, std::string const& end)
{
	const size_t str_size = str.size ();
	const size_t end_size = end.size ();
	if (str_size < end_size) {
		return false;
	}
	return 0 == str.compare (str_size - end_size, end_size, end);
}

bool
PortManager::port_is_virtual_piano (std::string const& name)
{
	return ends_with (name, X_(":x-virtual-keyboard"));
}

bool
PortManager::port_is_physical_input_monitor_enable (std::string const& name)
{
	if (Config->get_work_around_jack_no_copy_optimization () && AudioEngine::instance ()->current_backend_name () == X_("JACK")) {
		if (ends_with (name, X_(":physical_midi_input_monitor_enable"))) {
			return true;
		}
		if (ends_with (name, X_(":physical_audio_input_monitor_enable"))) {
			return true;
		}
	}
	return false;
}

MidiPortFlags
PortManager::midi_port_metadata (std::string const& name)
{
	Glib::Threads::Mutex::Lock lm (_port_info_mutex);
	fill_midi_port_info_locked ();

	PortID             pid (_backend, DataType::MIDI, true, name);
	PortInfo::iterator x = _port_info.find (pid);
	if (x != _port_info.end ()) {
		return x->second.properties;
	}

	pid.input = false;
	x         = _port_info.find (pid);
	if (x != _port_info.end ()) {
		return x->second.properties;
	}

	return MidiPortFlags (0);
}

void
PortManager::get_configurable_midi_ports (vector<string>& copy, bool for_input)
{
	if (!_backend) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (_port_info_mutex);
		fill_midi_port_info_locked ();
	}

	PortFlags flags = PortFlags ((for_input ? IsOutput : IsInput) | IsPhysical);

	std::vector<string> ports;
	AudioEngine::instance ()->get_ports (string (), DataType::MIDI, flags, ports);
	for (vector<string>::iterator p = ports.begin (); p != ports.end (); ++p) {
		if (port_is_mine (*p) && !port_is_virtual_piano (*p)) {
			continue;
		}
		if ((*p).find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos) {
			continue;
		}
		copy.push_back (*p);
	}
}

void
PortManager::get_midi_selection_ports (vector<string>& copy)
{
	Glib::Threads::Mutex::Lock lm (_port_info_mutex);
	fill_midi_port_info_locked ();

	for (PortInfo::const_iterator x = _port_info.begin (); x != _port_info.end (); ++x) {
		if (x->first.data_type != DataType::MIDI || !x->first.input) {
			continue;
		}
		if (x->second.properties & MidiPortSelection) {
			copy.push_back (x->first.port_name);
		}
	}
}

void
PortManager::set_port_pretty_name (string const& port, string const& pretty)
{
	PortEngine::PortHandle ph = _backend->get_port_by_name (port);
	if (!ph) {
		return;
	}

	/* port-manager only handles physical I/O names */
	assert (_backend->get_port_flags (ph) & IsPhysical);

	_backend->set_port_property (ph, "http://jackaudio.org/metadata/pretty-name", pretty, string ());

	{
		/* backend IsOutput ports = capture = input ports for libardour */
		PortID                     pid (_backend, _backend->port_data_type (ph), _backend->get_port_flags (ph) & IsOutput, port);
		Glib::Threads::Mutex::Lock lm (_port_info_mutex);
		fill_midi_port_info_locked ();

		if (!pretty.empty ()) {
			_port_info[pid].pretty_name = pretty;
		} else {
			/* remove empty */
			PortInfo::iterator x = _port_info.find (pid);
			if (x != _port_info.end () && x->second.properties == MidiPortFlags (0)) {
				_port_info.erase (x);
			}
		}
	}

	save_port_info ();
	MidiPortInfoChanged ();       /* EMIT SIGNAL*/
	PortPrettyNameChanged (port); /* EMIT SIGNAL */
}

void
PortManager::add_midi_port_flags (string const& port, MidiPortFlags flags)
{
	assert (flags != MidiPortFlags (0));
	PortEngine::PortHandle ph = _backend->get_port_by_name (port);
	if (!ph) {
		return;
	}

	bool emit = false;

	{
		PortID                     pid (_backend, _backend->port_data_type (ph), _backend->get_port_flags (ph) & IsOutput, port);
		Glib::Threads::Mutex::Lock lm (_port_info_mutex);
		fill_midi_port_info_locked ();

		/* Add MIDI port if present */
		if (_port_info[pid].properties != flags) {
			_port_info[pid].properties = MidiPortFlags (_port_info[pid].properties | flags);
			emit                       = true;
		}
	}

	if (emit) {
		if (flags & MidiPortSelection) {
			MidiSelectionPortsChanged (); /* EMIT SIGNAL */
		}

		if (flags != MidiPortSelection) {
			MidiPortInfoChanged (); /* EMIT SIGNAL */
		}

		save_port_info ();
	}
}

void
PortManager::remove_midi_port_flags (string const& port, MidiPortFlags flags)
{
	assert (flags != MidiPortFlags (0));
	PortEngine::PortHandle ph = _backend->get_port_by_name (port);
	if (!ph) {
		return;
	}

	bool emit = false;

	{
		PortID                     pid (_backend, _backend->port_data_type (ph), _backend->get_port_flags (ph) & IsOutput, port);
		Glib::Threads::Mutex::Lock lm (_port_info_mutex);
		fill_midi_port_info_locked ();
		PortInfo::iterator x = _port_info.find (pid);

		if (x != _port_info.end ()) {
			if (x->second.properties & flags) { // at least one is set
				x->second.properties = MidiPortFlags (x->second.properties & ~flags);
				emit                 = true;
			}
			/* remove empty */
			if (x->second.properties == MidiPortFlags (0) && x->second.pretty_name.empty ()) {
				_port_info.erase (x);
			}
		}
	}

	if (emit) {
		if (flags & MidiPortSelection) {
			MidiSelectionPortsChanged (); /* EMIT SIGNAL */
		}

		if (flags != MidiPortSelection) {
			MidiPortInfoChanged (); /* EMIT SIGNAL */
		}

		save_port_info ();
	}
}

string
PortManager::port_info_file ()
{
	return Glib::build_filename (user_config_directory (), X_("port_metadata"));
}

#if CURRENT_SESSION_FILE_VERSION < 6999
string
PortManager::midi_port_info_file ()
{
	return Glib::build_filename (user_config_directory (), X_("midi_port_info"));
}
#endif

void
PortManager::save_port_info ()
{
	XMLNode* root = new XMLNode ("PortMeta");
	root->set_property ("version", 1);

	{
		Glib::Threads::Mutex::Lock lm (_port_info_mutex);
		for (PortInfo::const_iterator i = _port_info.begin (); i != _port_info.end (); ++i) {
			if (port_is_virtual_piano (i->first.port_name)) {
				continue;
			}

			XMLNode& node = i->first.state ();
			node.set_property ("pretty-name", i->second.pretty_name);
			node.set_property ("properties", i->second.properties);
			root->add_child_nocopy (node);
		}
	}

	XMLTree tree;
	tree.set_root (root);

	if (!tree.write (port_info_file ())) {
		error << string_compose (_("Could not save port info to %1"), port_info_file ()) << endmsg;
	}
}

void
PortManager::load_port_info ()
{
	_port_info.clear ();

#if CURRENT_SESSION_FILE_VERSION < 6999
	/* import old Ardour 6 MIDI meta-data */
	string a6path = midi_port_info_file ();

	if (Glib::file_test (a6path, Glib::FILE_TEST_EXISTS)) {
		XMLTree tree;
		if (!tree.read (a6path)) {
			warning << string_compose (_("Cannot load/convert MIDI port info from '%1'."), a6path) << endmsg;
		} else {
			for (XMLNodeConstIterator i = tree.root ()->children ().begin (); i != tree.root ()->children ().end (); ++i) {
				string name;
				string backend;
				bool   input;
				if (!(*i)->get_property (X_("name"), name) ||
				    !(*i)->get_property (X_("backend"), backend) ||
				    !(*i)->get_property (X_("input"), input)) {
					error << string_compose (_("MIDI port info file '%1' contains invalid port description - please remove it."), a6path) << endmsg;
					continue;
				}
				try {
					PortID       id (**i, true);
					PortMetaData meta (**i);
					_port_info[id] = meta;
				} catch (...) {
					error << string_compose (_("MIDI port info file '%1' contains invalid meta data - please remove it."), a6path) << endmsg;
				}
			}
		}
	}
#endif

	XMLTree tree;
	string  path = port_info_file ();
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return;
	}
	if (!tree.read (path)) {
		error << string_compose (_("Cannot load port info from '%1'."), path) << endmsg;
		return;
	}

	for (XMLNodeConstIterator i = tree.root ()->children ().begin (); i != tree.root ()->children ().end (); ++i) {
		try {
			PortID       id (**i);
			PortMetaData meta (**i);
			_port_info[id] = meta;
		} catch (...) {
			error << string_compose (_("port info file '%1' contains invalid information - please remove it."), path) << endmsg;
		}
	}
}

string
PortManager::short_port_name_from_port_name (std::string const& full_name) const
{
	string::size_type colon = full_name.find_first_of (':');
	if (colon == string::npos || colon == full_name.length ()) {
		return full_name;
	}
	return full_name.substr (colon + 1);
}

void
PortManager::fill_midi_port_info_locked ()
{
	/* MIDI info mutex MUST be held */

	if (!_midi_info_dirty || !_backend) {
		return;
	}

	std::vector<string> ports;
	AudioEngine::instance ()->get_ports (string (), DataType::MIDI, IsOutput, ports);
	for (vector<string>::iterator p = ports.begin (); p != ports.end (); ++p) {
		if (port_is_mine (*p) && !port_is_virtual_piano (*p)) {
			continue;
		}

		PortID             pid (_backend, DataType::MIDI, true, *p);
		PortInfo::iterator x = _port_info.find (pid);
		if (x != _port_info.end ()) {
			continue;
		}

		MidiPortFlags flags (MidiPortFlags (0));

		if (port_is_control_only (*p)) {
			flags = MidiPortControl;
		} else if (port_is_virtual_piano (*p)) {
			flags = MidiPortFlags (MidiPortSelection | MidiPortMusic);
		}

#ifdef HAVE_ALSA
		if ((*p).find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos) {
			flags = MidiPortFlags (flags | MidiPortVirtual);
		}
#endif

		if (flags != MidiPortFlags (0)) {
			_port_info[pid].properties = flags;
		}
	}

	AudioEngine::instance ()->get_ports (string (), DataType::MIDI, IsInput, ports);
	for (vector<string>::iterator p = ports.begin (); p != ports.end (); ++p) {
		if (port_is_mine (*p)) {
			continue;
		}

		PortID             pid (_backend, DataType::MIDI, false, *p);
		PortInfo::iterator x = _port_info.find (pid);
		if (x != _port_info.end ()) {
			continue;
		}

		MidiPortFlags flags (MidiPortFlags (0));

		if (port_is_control_only (*p)) {
			flags = MidiPortControl;
		}

#ifdef HAVE_ALSA
		if ((*p).find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos) {
			flags = MidiPortFlags (flags | MidiPortVirtual);
		}
#endif

		if (flags != MidiPortFlags (0)) {
			_port_info[pid].properties = flags;
		}
	}

	_midi_info_dirty = false;
}

void
PortManager::set_port_buffer_sizes (pframes_t n)
{
	boost::shared_ptr<Ports> all = _ports.reader ();

	for (Ports::iterator p = all->begin (); p != all->end (); ++p) {
		p->second->set_buffer_size (n);
	}
	_monitor_port.set_buffer_size (n);
}

bool
PortManager::check_for_ambiguous_latency (bool log) const
{
	bool                     rv    = false;
	boost::shared_ptr<Ports> plist = _ports.reader ();
	for (Ports::iterator pi = plist->begin (); pi != plist->end (); ++pi) {
		boost::shared_ptr<Port> const& p (pi->second);
		/* check one to many connections */
		if (!p->sends_output () || (p->flags () & IsTerminal) || !p->connected ()) {
			continue;
		}
		if (boost::dynamic_pointer_cast<AsyncMIDIPort> (p)) {
			continue;
		}
		assert (port_is_mine (p->name ()));

		LatencyRange range;
		range.min = ~((pframes_t)0);
		range.max = 0;
		p->collect_latency_from_backend (range, true);
		if (range.min != range.max) {
			if (log) {
				warning << string_compose (_("Ambiguous latency for port '%1' (%2, %3)"), p->name (), range.min, range.max) << endmsg;
				rv = true;
			} else {
				return true;
			}
		}
	}
	return rv;
}

void
PortManager::reset_input_meters ()
{
	g_atomic_int_set (&_reset_meters, 1);
}

PortManager::AudioInputPorts
PortManager::audio_input_ports () const
{
	boost::shared_ptr<AudioInputPorts> p = _audio_input_ports.reader ();
	return *p;
}

PortManager::MIDIInputPorts
PortManager::midi_input_ports () const
{
	boost::shared_ptr<MIDIInputPorts> p = _midi_input_ports.reader ();
	return *p;
}

/* Cache dB -> coefficient calculation for dB/sec falloff.
 * @n_samples engine-buffer-size
 * @rate engine sample-rate
 * @return coefficiant taking user preferences meter_falloff (dB/sec) into account
 */
struct FallOffCache {
	FallOffCache ()
		: _falloff (1.0)
		, _cfg_db_s (0)
		, _n_samples (0)
		, _rate (0)
	{
	}

	float calc (pframes_t n_samples, samplecnt_t rate)
	{
		if (n_samples == 0 || rate == 0) {
			return 1.0;
		}

		if (Config->get_meter_falloff () != _cfg_db_s || n_samples != _n_samples || rate != _rate) {
			_cfg_db_s  = Config->get_meter_falloff ();
			_n_samples = n_samples;
			_rate      = rate;
#ifdef _GNU_SOURCE
			_falloff = exp10f (-0.05f * _cfg_db_s * _n_samples / _rate);
#else
			_falloff = powf (10.f, -0.05f * _cfg_db_s * _n_samples / _rate);
#endif
		}

		return _falloff;
	}

private:
	float       _falloff;
	float       _cfg_db_s;
	pframes_t   _n_samples;
	samplecnt_t _rate;
};

static FallOffCache falloff_cache;

void
PortManager::run_input_meters (pframes_t n_samples, samplecnt_t rate)
{
	if (n_samples == 0) {
		return;
	}

	const bool  reset   = g_atomic_int_compare_and_exchange (&_reset_meters, 1, 0);
	const float falloff = falloff_cache.calc (n_samples, rate);

	_monitor_port.monitor (port_engine (), n_samples);

	/* calculate peak of all physical inputs (readable ports) */
	boost::shared_ptr<AudioInputPorts> aip = _audio_input_ports.reader ();

	for (AudioInputPorts::iterator p = aip->begin (); p != aip->end (); ++p) {
		assert (!port_is_mine (p->first));
		AudioInputPort& ai (p->second);

		if (reset) {
			ai.meter->reset ();
		}

		PortEngine::PortHandle ph = _backend->get_port_by_name (p->first);
		if (!ph) {
			continue;
		}

		Sample* buf = (Sample*)_backend->get_buffer (ph, n_samples);
		if (!buf) {
			/* can this happen? */
			ai.meter->level = 0;
			ai.scope->silence (n_samples);
			continue;
		}

		ai.scope->write (buf, n_samples);

		/* falloff */
		if (ai.meter->level > 1e-10) {
			ai.meter->level *= falloff;
		} else {
			ai.meter->level = 0;
		}

		float level     = ai.meter->level;
		level           = compute_peak (buf, n_samples, reset ? 0 : level);
		ai.meter->level = std::min (level, 100.f); // cut off at +40dBFS for falloff.
		ai.meter->peak  = std::max (ai.meter->peak, level);
	}

	/* MIDI */
	boost::shared_ptr<MIDIInputPorts> mip = _midi_input_ports.reader ();
	for (MIDIInputPorts::iterator p = mip->begin (); p != mip->end (); ++p) {
		assert (!port_is_mine (p->first));

		PortEngine::PortHandle ph = _backend->get_port_by_name (p->first);
		if (!ph || !_backend->connected (ph)) {
			continue;
		}

		MIDIInputPort& mi (p->second);

		for (size_t i = 0; i < 17; ++i) {
			/* falloff */
			if (mi.meter->chn_active[i] > 1e-10) {
				mi.meter->chn_active[i] *= falloff;
			} else {
				mi.meter->chn_active[i] = 0;
			}
		}

		void*           buffer      = _backend->get_buffer (ph, n_samples);
		const pframes_t event_count = _backend->get_midi_event_count (buffer);

		for (pframes_t i = 0; i < event_count; ++i) {
			pframes_t      timestamp;
			size_t         size;
			uint8_t const* buf;
			_backend->midi_event_get (timestamp, size, &buf, buffer, i);
			if (buf[0] == 0xfe) {
				/* ignore active sensing */
				continue;
			}
			if ((buf[0] & 0xf0) == 0xf0) {
				mi.meter->chn_active[16] = 1.0;
			} else {
				int chn                   = (buf[0] & 0x0f);
				mi.meter->chn_active[chn] = 1.0;
			}
			mi.monitor->write (buf, size);
		}
	}
}

#ifndef NDEBUG
void
PortManager::list_all_ports () const
{
	boost::shared_ptr<Ports> plist = _ports.reader ();
	for (Ports::iterator p = plist->begin (); p != plist->end (); ++p) {
		std::cout << p->first << "\n";
	}
}

void
PortManager::list_cycle_ports () const
{
	for (Ports::iterator p = _cycle_ports->begin (); p != _cycle_ports->end (); ++p) {
		std::cout << p->first << "\n";
	}
}
#endif
