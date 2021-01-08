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

#include <vector>

#ifdef COMPILER_MSVC
#include <io.h> // Microsoft's nearest equivalent to <unistd.h>
#include <ardourext/misc.h>
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

PortManager::PortManager ()
	: ports (new Ports)
	, _port_remove_in_progress (false)
	, _port_deletions_pending (8192) /* ick, arbitrary sizing */
	, midi_info_dirty (true)
{
	load_midi_port_info ();
}

void
PortManager::clear_pending_port_deletions ()
{
	Port* p;

	DEBUG_TRACE (DEBUG::Ports, string_compose ("pending port deletions: %1\n", _port_deletions_pending.read_space()));

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
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}

	/* clear dead wood list in RCU */

	ports.flush ();

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

	if (portname.substr (0, colon) == _backend->my_name()) {
		return portname.substr (colon+1);
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

	str  = _backend->my_name();
	str += ':';
	str += portname;

	return str;
}

std::string
PortManager::get_pretty_name_by_name(const std::string& portname) const
{
	PortEngine::PortHandle ph = _backend->get_port_by_name (portname);

	if (ph) {
		std::string value;
		std::string type;
		if (0 == _backend->get_port_property (ph, "http://jackaudio.org/metadata/pretty-name", value, type)) {
			return value;
		}
	}

	return string();
}

bool
PortManager::port_is_mine (const string& portname) const
{
	if (!_backend) {
		return true;
	}

	string self = _backend->my_name();

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

	{
		Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

		fill_midi_port_info_locked ();

		for (vector<string>::iterator si = ports.begin(); si != ports.end(); ) {

			MidiPortInfo::iterator x = midi_port_info.find (*si);

			if (x == midi_port_info.end()) {
				++si;
				continue;
			}

			MidiPortInformation& mpi (x->second);

			if (mpi.pretty_name.empty()) {
				/* no information !!! */
				++si;
				continue;
			}

			if (include) {
				if ((mpi.properties & include) != include) {
					/* properties do not include requested ones */
					si = ports.erase (si);
					continue;
				}
			}

			if (exclude) {
				if ((mpi.properties & exclude)) {
					/* properties include ones to avoid */
					si = ports.erase (si);
					continue;
				}
			}

			++si;
		}
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
		return boost::shared_ptr<Port>();
	}

	if (!port_is_mine (portname)) {
		/* not an ardour port */
		return boost::shared_ptr<Port> ();
	}

	boost::shared_ptr<Ports> pr = ports.reader();
	std::string rel = make_port_name_relative (portname);
	Ports::iterator x = pr->find (rel);

	if (x != pr->end()) {
		/* its possible that the port was renamed by some 3rd party and
		 * we don't know about it. check for this (the check is quick
		 * and cheap), and if so, rename the port (which will alter
		 * the port map as a side effect).
		 */
		const std::string check = make_port_name_relative (_backend->get_port_name (x->second->port_handle()));
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
	RCUWriter<Ports> writer (ports);
	boost::shared_ptr<Ports> p = writer.get_copy();
	Ports::iterator x = p->find (old_relative_name);

	if (x != p->end()) {
		boost::shared_ptr<Port> port = x->second;
		p->erase (x);
		p->insert (make_pair (new_relative_name, port));
	}
}

int
PortManager::get_ports (DataType type, PortList& pl)
{
	boost::shared_ptr<Ports> plist = ports.reader();
	for (Ports::iterator p = plist->begin(); p != plist->end(); ++p) {
		if (p->second->type() == type) {
			pl.push_back (p->second);
		}
	}
	return pl.size();
}

int
PortManager::get_ports (const string& port_name_pattern, DataType type, PortFlags flags, vector<string>& s)
{
	s.clear();

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

	string full_portname = _backend->my_name();
	full_portname += ':';
	full_portname += portname;


	PortEngine::PortHandle p = _backend->get_port_by_name (full_portname);
	string reason;

	if (p) {
		reason = string_compose (_("a port with the name \"%1\" already exists: check for duplicated track/bus names"), portname);
	} else {
		reason = string_compose (_("No more ports are available. You will need to stop %1 and restart with more ports if you need this many tracks."), PROGRAM_NAME);
	}

	throw PortRegistrationFailure (string_compose (_("AudioEngine: cannot register port \"%1\": %2"), portname, reason).c_str());
}

struct PortDeleter
{
	void operator() (Port* p) {
		AudioEngine::instance()->add_pending_port_deletion (p);
	}
};

boost::shared_ptr<Port>
PortManager::register_port (DataType dtype, const string& portname, bool input, bool async, PortFlags flags)
{
	boost::shared_ptr<Port> newport;

	/* limit the possible flags that can be set */

	flags = PortFlags (flags & (Hidden|Shadow|IsTerminal|TransportSyncPort));

	try {
		if (dtype == DataType::AUDIO) {
			DEBUG_TRACE (DEBUG::Ports, string_compose ("registering AUDIO port %1, input %2\n",
								   portname, input));
			newport.reset (new AudioPort (portname, PortFlags ((input ? IsInput : IsOutput) | flags)),
			               PortDeleter());
		} else if (dtype == DataType::MIDI) {
			if (async) {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("registering ASYNC MIDI port %1, input %2\n",
									   portname, input));
				newport.reset (new AsyncMIDIPort (portname, PortFlags ((input ? IsInput : IsOutput) | flags)),
				               PortDeleter());
			} else {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("registering MIDI port %1, input %2\n",
									   portname, input));
				newport.reset (new MidiPort (portname, PortFlags ((input ? IsInput : IsOutput) | flags)),
				               PortDeleter());
			}
		} else {
			throw PortRegistrationFailure (string_compose ("unable to create port '%1': %2", portname, _("(unknown type)")));
		}

		newport->set_buffer_size (AudioEngine::instance()->samples_per_cycle());

		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->insert (make_pair (make_port_name_relative (portname), newport));

		/* writer goes out of scope, forces update */
	}

	catch (PortRegistrationFailure& err) {
		throw err;
	} catch (std::exception& e) {
		throw PortRegistrationFailure (string_compose ("unable to create port '%1': %2", portname, e.what()).c_str());
	} catch (...) {
		throw PortRegistrationFailure (string_compose ("unable to create port '%1': %2", portname, _("(unknown error)")));
	}

	DEBUG_TRACE (DEBUG::Ports, string_compose ("\t%2 port registration success, ports now = %1\n", ports.reader()->size(), this));
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
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		Ports::iterator x = ps->find (make_port_name_relative (port->name()));

		if (x != ps->end()) {
			DEBUG_TRACE (DEBUG::Ports, string_compose ("removing %1 from port map (uc=%2)\n", port->name(), port.use_count()));
			ps->erase (x);
		}

		/* writer goes out of scope, forces update */
	}

	ports.flush ();

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
		error << string_compose(_("AudioEngine: cannot connect %1 (%2) to %3 (%4)"),
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
PortManager::disconnect (std::string const & name)
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

	boost::shared_ptr<Ports> p = ports.reader ();

	DEBUG_TRACE (DEBUG::Ports, string_compose ("reestablish %1 ports\n", p->size()));

	for (i = p->begin(); i != p->end(); ++i) {
		if (i->second->reestablish ()) {
			error << string_compose (_("Re-establising port %1 failed"), i->second->name()) << endmsg;
			std::cerr << string_compose (_("Re-establising port %1 failed"), i->second->name()) << std::endl;
			break;
		}
	}

	if (i != p->end()) {
		/* failed */
		remove_all_ports ();
		return -1;
	}

	return 0;
}

int
PortManager::reconnect_ports ()
{
	boost::shared_ptr<Ports> p = ports.reader ();

	/* re-establish connections */

	DEBUG_TRACE (DEBUG::Ports, string_compose ("reconnect %1 ports\n", p->size()));

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		i->second->reconnect ();
	}

	return 0;
}

void
PortManager::connect_callback (const string& a, const string& b, bool conn)
{
	DEBUG_TRACE (DEBUG::BackendCallbacks, string_compose (X_("connect callback %1 + %2 connected ? %3\n"), a, b, conn));

	boost::shared_ptr<Port> port_a;
	boost::shared_ptr<Port> port_b;
	Ports::iterator x;
	boost::shared_ptr<Ports> pr = ports.reader ();

	x = pr->find (make_port_name_relative (a));
	if (x != pr->end()) {
		port_a = x->second;
	}

	x = pr->find (make_port_name_relative (b));
	if (x != pr->end()) {
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
		conn
		); /* EMIT SIGNAL */
}

void
PortManager::registration_callback ()
{
	DEBUG_TRACE (DEBUG::BackendCallbacks, "port registration callback\n");

	if (!_port_remove_in_progress) {

		{
			Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);
			midi_info_dirty = true;
		}

		PortRegisteredOrUnregistered (); /* EMIT SIGNAL */
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
PortManager::port_name_size() const
{
	if (!_backend) {
		return 0;
	}

	return _backend->port_name_size ();
}

string
PortManager::my_name() const
{
	if (!_backend) {
		return string();
	}

	return _backend->my_name();
}

int
PortManager::graph_order_callback ()
{
	DEBUG_TRACE (DEBUG::BackendCallbacks, "graph order callback\n");

	if (!_port_remove_in_progress) {
		GraphReordered(); /* EMIT SIGNAL */
	}

	return 0;
}

void
PortManager::cycle_start (pframes_t nframes, Session* s)
{
	Port::set_global_port_buffer_offset (0);
	Port::set_cycle_samplecnt (nframes);

	_cycle_ports = ports.reader ();

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
		for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
			if (!(p->second->flags() & TransportSyncPort)) {
				tl.push_back (boost::bind (&Port::cycle_start, p->second, nframes));
			}
		}
		s->rt_tasklist()->process (tl);
	} else {
		for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
			if (!(p->second->flags() & TransportSyncPort)) {
				p->second->cycle_start (nframes);
			}
		}
	}
}

void
PortManager::cycle_end (pframes_t nframes, Session* s)
{
	// see optimzation note in ::cycle_start()
	if (0 && s && s->rt_tasklist () && fabs (Port::speed_ratio ()) != 1.0) {
		RTTaskList::TaskList tl;
		for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
			if (!(p->second->flags() & TransportSyncPort)) {
				tl.push_back (boost::bind (&Port::cycle_end, p->second, nframes));
			}
		}
		s->rt_tasklist()->process (tl);
	} else {
		for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
			if (!(p->second->flags() & TransportSyncPort)) {
				p->second->cycle_end (nframes);
			}
		}
	}

	for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
		/* AudioEngine::split_cycle flushes buffers until Port::port_offset.
		 * Now only flush remaining events (after Port::port_offset) */
		p->second->flush_buffers (nframes * Port::speed_ratio() - Port::port_offset ());
	}

	_cycle_ports.reset ();

	/* we are done */
}

void
PortManager::silence (pframes_t nframes, Session *s)
{
	for (Ports::iterator i = _cycle_ports->begin(); i != _cycle_ports->end(); ++i) {
		if (s && i->second == s->mtc_output_port ()) {
			continue;
		}
		if (s && i->second == s->midi_clock_output_port ()) {
			continue;
		}
		if (s && i->second == s->ltc_output_port ()) {
			continue;
		}
		if (boost::dynamic_pointer_cast<AsyncMIDIPort>(i->second)) {
			continue;
		}
		if (i->second->sends_output()) {
			i->second->get_buffer(nframes).silence(nframes);
		}
	}
}

void
PortManager::silence_outputs (pframes_t nframes)
{
	std::vector<std::string> port_names;
	if (get_ports("", DataType::AUDIO, IsOutput, port_names)) {
		for (std::vector<std::string>::iterator p = port_names.begin(); p != port_names.end(); ++p) {
			if (!port_is_mine(*p)) {
				continue;
			}
			PortEngine::PortHandle ph = _backend->get_port_by_name (*p);
			if (!ph) {
				continue;
			}
			void *buf = _backend->get_buffer(ph, nframes);
			if (!buf) {
				continue;
			}
			memset (buf, 0, sizeof(float) * nframes);
		}
	}

	if (get_ports("", DataType::MIDI, IsOutput, port_names)) {
		for (std::vector<std::string>::iterator p = port_names.begin(); p != port_names.end(); ++p) {
			if (!port_is_mine(*p)) {
				continue;
			}
			PortEngine::PortHandle ph = _backend->get_port_by_name (*p);
			if (!ph) {
				continue;
			}
			void *buf = _backend->get_buffer(ph, nframes);
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
	for (Ports::iterator i = _cycle_ports->begin(); i != _cycle_ports->end(); ++i) {

		bool x;

		if (i->second->last_monitor() != (x = i->second->monitoring_input ())) {
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
		for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
			if (!(p->second->flags() & TransportSyncPort)) {
				tl.push_back (boost::bind (&Port::cycle_end, p->second, nframes));
			}
		}
		s->rt_tasklist()->process (tl);
	} else {
		for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
			if (!(p->second->flags() & TransportSyncPort)) {
				p->second->cycle_end (nframes);
			}
		}
	}

	for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
		p->second->flush_buffers (nframes);

		if (p->second->sends_output()) {

			boost::shared_ptr<AudioPort> ap = boost::dynamic_pointer_cast<AudioPort> (p->second);
			if (ap) {
				Sample* s = ap->engine_get_whole_audio_buffer ();
				gain_t g = base_gain;

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
PortManager::port_engine()
{
	assert (_backend);
	return *_backend;
}

bool
PortManager::port_is_control_only (std::string const& name)
{
	static regex_t compiled_pattern;
	static string pattern;

	if (pattern.empty()) {

		/* This is a list of regular expressions that match ports
		 * related to physical MIDI devices that we do not want to
		 * expose as normal physical ports.
		 */

		const char * const control_only_ports[] = {
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
		for (size_t n = 0; n < sizeof (control_only_ports)/sizeof (control_only_ports[0]); ++n) {
			if (n > 0) {
				pattern += '|';
			}
			pattern += control_only_ports[n];
		}
		pattern += ')';

		regcomp (&compiled_pattern, pattern.c_str(), REG_EXTENDED|REG_NOSUB);
	}

	return regexec (&compiled_pattern, name.c_str(), 0, 0, 0) == 0;
}

PortManager::MidiPortInformation
PortManager::midi_port_information (std::string const & name)
{
	Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

	fill_midi_port_info_locked ();

	MidiPortInfo::iterator x = midi_port_info.find (name);

	if (x != midi_port_info.end()) {
		return x->second;
	}

	return MidiPortInformation ();
}

void
PortManager::get_known_midi_ports (vector<string>& copy)
{
	Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

	fill_midi_port_info_locked ();

	for (MidiPortInfo::const_iterator x = midi_port_info.begin(); x != midi_port_info.end(); ++x) {
		copy.push_back (x->first);
	}
}

void
PortManager::get_midi_selection_ports (vector<string>& copy)
{
	Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

	fill_midi_port_info_locked ();

	for (MidiPortInfo::const_iterator x = midi_port_info.begin(); x != midi_port_info.end(); ++x) {
		if (x->second.properties & MidiPortSelection) {
			copy.push_back (x->first);
		}
	}
}

void
PortManager::set_port_pretty_name (string const & port, string const & pretty)
{
	{
		Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

		fill_midi_port_info_locked ();

		MidiPortInfo::iterator x = midi_port_info.find (port);
		if (x == midi_port_info.end()) {
			return;
		}
		x->second.pretty_name = pretty;
	}

	/* push into back end */

	PortEngine::PortHandle ph = _backend->get_port_by_name (port);

	if (ph) {
		_backend->set_port_property (ph, "http://jackaudio.org/metadata/pretty-name", pretty, string());
	}

	save_midi_port_info ();
	MidiPortInfoChanged (); /* EMIT SIGNAL*/
}

void
PortManager::add_midi_port_flags (string const & port, MidiPortFlags flags)
{
	bool emit = false;

	{
		Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

		fill_midi_port_info_locked ();

		MidiPortInfo::iterator x = midi_port_info.find (port);

		if (x != midi_port_info.end()) {
			if ((x->second.properties & flags) != flags) { // at least one missing
				x->second.properties = MidiPortFlags (x->second.properties | flags);
				emit = true;
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

		save_midi_port_info ();
	}
}

void
PortManager::remove_midi_port_flags (string const & port, MidiPortFlags flags)
{
	bool emit = false;

	{
		Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

		fill_midi_port_info_locked ();

		MidiPortInfo::iterator x = midi_port_info.find (port);

		if (x != midi_port_info.end()) {
			if (x->second.properties & flags) { // at least one is set
				x->second.properties = MidiPortFlags (x->second.properties & ~flags);
				emit = true;
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

		save_midi_port_info ();
	}
}

string
PortManager::midi_port_info_file ()
{
	return Glib::build_filename (user_config_directory(), X_("midi_port_info"));
}

void
PortManager::save_midi_port_info ()
{
	string path = midi_port_info_file ();

	XMLNode* root = new XMLNode (X_("MidiPortInfo"));

	{
		Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);

		if (midi_port_info.empty()) {
			delete root;
			return;
		}

		for (MidiPortInfo::iterator i = midi_port_info.begin(); i != midi_port_info.end(); ++i) {
			XMLNode* node = new XMLNode (X_("port"));
			node->set_property (X_("name"), i->first);
			node->set_property (X_("backend"), i->second.backend);
			node->set_property (X_("pretty-name"), i->second.pretty_name);
			node->set_property (X_("input"), i->second.input);
			node->set_property (X_("properties"), i->second.properties);
			root->add_child_nocopy (*node);
		}
	}

	XMLTree tree;

	tree.set_root (root);

	if (!tree.write (path)) {
		error << string_compose (_("Could not save MIDI port info to %1"), path) << endmsg;
	}
}

void
PortManager::load_midi_port_info ()
{
	string path = midi_port_info_file ();
	XMLTree tree;

	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	if (!tree.read (path)) {
		error << string_compose (_("Cannot load MIDI port info from %1"), path) << endmsg;
		return;
	}

	midi_port_info.clear ();

	for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
		string name;
		string backend;
		string pretty;
		bool  input;
		MidiPortFlags properties;


		if (!(*i)->get_property (X_("name"), name) ||
		    !(*i)->get_property (X_("backend"), backend) ||
		    !(*i)->get_property (X_("pretty-name"), pretty) ||
		    !(*i)->get_property (X_("input"), input) ||
		    !(*i)->get_property (X_("properties"), properties)) {
			/* should only affect version changes */
			error << string_compose (_("MIDI port info file %1 contains invalid information - please remove it."), path) << endmsg;
			continue;
		}

		MidiPortInformation mpi (backend, pretty, input, properties, false);

		midi_port_info.insert (make_pair (name, mpi));
	}
}

void
PortManager::fill_midi_port_info ()
{
	{
		Glib::Threads::Mutex::Lock lm (midi_port_info_mutex);
		fill_midi_port_info_locked ();
	}
}

string
PortManager::short_port_name_from_port_name (std::string const & full_name) const
{
	string::size_type colon = full_name.find_first_of (':');
	if (colon == string::npos || colon == full_name.length()) {
		return full_name;
	}
	return full_name.substr (colon+1);
}

void
PortManager::fill_midi_port_info_locked ()
{
	/* MIDI info mutex MUST be held */

	if (!midi_info_dirty || !_backend) {
		return;
	}

	std::vector<string> ports;

	AudioEngine::instance()->get_ports (string(), DataType::MIDI, IsOutput, ports);

	for (vector<string>::iterator p = ports.begin(); p != ports.end(); ++p) {

		/* ugly hack, ideally we'd use a port-flag, or vkbd_output_port()->name() */
		if (port_is_mine (*p) && *p != _backend->my_name() + ":x-virtual-keyboard") {
			continue;
		}

		if (midi_port_info.find (*p) == midi_port_info.end()) {

			MidiPortFlags flags (MidiPortFlags (0));

			if (port_is_control_only (*p)) {
				flags = MidiPortControl;
			} else if (*p == _backend->my_name() + ":x-virtual-keyboard") {
				flags = MidiPortFlags(MidiPortSelection | MidiPortMusic);
			}

			MidiPortInformation mpi (_backend->name(), *p, true, flags, true);

#ifdef LINUX
			if ((*p.find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos)) {
				mpi.properties = MidiPortFlags (mpi.properties | MidiPortVirtual);
			}
#endif
			midi_port_info.insert (make_pair (*p, mpi));
		}
	}

	AudioEngine::instance()->get_ports (string(), DataType::MIDI, IsInput, ports);

	for (vector<string>::iterator p = ports.begin(); p != ports.end(); ++p) {

		if (port_is_mine (*p)) {
			continue;
		}

		if (midi_port_info.find (*p) == midi_port_info.end()) {

			MidiPortFlags flags (MidiPortFlags (0));

			if (port_is_control_only (*p)) {
				flags = MidiPortControl;
			}

			MidiPortInformation mpi (_backend->name(), *p, false, flags, true);

#ifdef LINUX
			if ((*p.find (X_("Midi Through")) != string::npos || (*p).find (X_("Midi-Through")) != string::npos)) {
				mpi.properties = MidiPortFlags (mpi.properties | MidiPortVirtual);
			}
#endif
			midi_port_info.insert (make_pair (*p, mpi));
		}
	}

	/* now check with backend about which ports are present and pull
	 * pretty-name if it exists
	 */

	for (MidiPortInfo::iterator x = midi_port_info.begin(); x != midi_port_info.end(); ++x) {

		if (x->second.backend != _backend->name()) {
			/* this port (info) comes from a different
			 * backend. While there's a reasonable chance that it
			 * refers to the same physical (or virtual) endpoint, we
			 * don't allow its use with this backend.
			*/
			x->second.exists = false;
			continue;
		}

		PortEngine::PortHandle ph = _backend->get_port_by_name (x->first);

		if (!ph) {
			/* port info saved from some condition where this port
			 * existed, but no longer does (i.e. device unplugged
			 * at present). We don't remove it from midi_port_info.
			 */
			x->second.exists = false;

		} else {
			x->second.exists = true;

			/* check with backend for pre-existing pretty name */

			string value = AudioEngine::instance()->get_pretty_name_by_name (x->first);

			if (!value.empty()) {
				x->second.pretty_name = value;
			}
		}
	}

	midi_info_dirty = false;
}

void
PortManager::set_port_buffer_sizes (pframes_t n)
{

	boost::shared_ptr<Ports> all = ports.reader();

	for (Ports::iterator p = all->begin(); p != all->end(); ++p) {
		p->second->set_buffer_size (n);
	}
}

bool
PortManager::check_for_ambiguous_latency (bool log) const
{
	bool rv = false;
	boost::shared_ptr<Ports> plist = ports.reader();
	for (Ports::iterator pi = plist->begin(); pi != plist->end(); ++pi) {
		boost::shared_ptr<Port> const& p (pi->second);
		if (! p->sends_output () || (p->flags () & IsTerminal)) {
			continue;
		}
		if (boost::dynamic_pointer_cast<AsyncMIDIPort>(p)) {
			continue;
		}
		assert (port_is_mine (p->name ()));

		LatencyRange range;
		p->get_connected_latency_range (range, true);
		if (range.min != range.max) {
			if (log) {
				warning << string_compose(_("Ambiguous latency for port '%1' (%2, %3)"), p->name(), range.min, range.max) << endmsg;
				rv = true;
			} else {
				return true;
			}
		}
	}
	return rv;
}
