/*
    Copyright (C) 2013 Paul Davis

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

#include "pbd/error.h"

#include "ardour/async_midi_port.h"
#include "ardour/debug.h"
#include "ardour/port_manager.h"
#include "ardour/audio_port.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;
using std::vector;

PortManager::PortManager ()
	: ports (new Ports)
	, _port_remove_in_progress (false)
{
}

void
PortManager::remove_all_ports ()
{
	/* make sure that JACK callbacks that will be invoked as we cleanup
	 * ports know that they have nothing to do.
	 */

	_port_remove_in_progress = true;

	/* process lock MUST be held by caller
	*/

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}

	/* clear dead wood list in RCU */

	ports.flush ();

	_port_remove_in_progress = false;
}


string
PortManager::make_port_name_relative (const string& portname) const
{
	if (!_impl) {
		return portname;
	}

	string::size_type len;
	string::size_type n;
	string self = _impl->my_name();

	len = portname.length();

	for (n = 0; n < len; ++n) {
		if (portname[n] == ':') {
			break;
		}
	}

	if ((n != len) && (portname.substr (0, n) == self)) {
		return portname.substr (n+1);
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

	str  = _impl->my_name();
	str += ':';
	str += portname;

	return str;
}

bool
PortManager::port_is_mine (const string& portname) const
{
	if (!_impl) {
		return true;
	}

	string self = _impl->my_name();

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
	if (!_impl) {
		return false;
	}

	PortEngine::PortHandle ph = _impl->get_port_by_name (portname);
	if (!ph) {
		return false;
	}

	return _impl->port_is_physical (ph);
}

void
PortManager::get_physical_outputs (DataType type, std::vector<std::string>& s)
{
	if (!_impl) {
		return;
	}
	_impl->get_physical_outputs (type, s);
}
 
void
PortManager::get_physical_inputs (DataType type, std::vector<std::string>& s)
{
	if (!_impl) {
		return;
	}

	_impl->get_physical_inputs (type, s);
}
 
ChanCount
PortManager::n_physical_outputs () const
{
	if (!_impl) {
		return ChanCount::ZERO;
	}

	return _impl->n_physical_outputs ();
}
 
ChanCount
PortManager::n_physical_inputs () const
{
	if (!_impl) {
		return ChanCount::ZERO;
	}
	return _impl->n_physical_inputs ();
}

/** @param name Full or short name of port
 *  @return Corresponding Port or 0.
 */

boost::shared_ptr<Port>
PortManager::get_port_by_name (const string& portname)
{
	if (!_impl) {
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
		   we don't know about it. check for this (the check is quick
		   and cheap), and if so, rename the port (which will alter
		   the port map as a side effect).
		*/
		const std::string check = make_port_name_relative (_impl->get_port_name (x->second->port_handle()));
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
	if (!_impl) {
		return 0;
	}

	return _impl->get_ports (port_name_pattern, type, flags, s);
}

void
PortManager::port_registration_failure (const std::string& portname)
{
	if (!_impl) {
		return;
	}

	string full_portname = _impl->my_name();
	full_portname += ':';
	full_portname += portname;


	PortEngine::PortHandle p = _impl->get_port_by_name (full_portname);
	string reason;

	if (p) {
		reason = string_compose (_("a port with the name \"%1\" already exists: check for duplicated track/bus names"), portname);
	} else {
		reason = string_compose (_("No more ports are available. You will need to stop %1 and restart with more ports if you need this many tracks."), PROGRAM_NAME);
	}

	throw PortRegistrationFailure (string_compose (_("AudioEngine: cannot register port \"%1\": %2"), portname, reason).c_str());
}

boost::shared_ptr<Port>
PortManager::register_port (DataType dtype, const string& portname, bool input, bool async)
{
	boost::shared_ptr<Port> newport;

	try {
		if (dtype == DataType::AUDIO) {
			DEBUG_TRACE (DEBUG::Ports, string_compose ("registering AUDIO port %1, input %2\n",
								   portname, input));
			newport.reset (new AudioPort (portname, (input ? IsInput : IsOutput)));
		} else if (dtype == DataType::MIDI) {
			if (async) {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("registering ASYNC MIDI port %1, input %2\n",
									   portname, input));
				newport.reset (new AsyncMIDIPort (portname, (input ? IsInput : IsOutput)));
			} else {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("registering MIDI port %1, input %2\n",
									   portname, input));
				newport.reset (new MidiPort (portname, (input ? IsInput : IsOutput)));
			}
		} else {
			throw PortRegistrationFailure("unable to create port (unknown type)");
		}

		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->insert (make_pair (make_port_name_relative (portname), newport));

		/* writer goes out of scope, forces update */

	}

	catch (PortRegistrationFailure& err) {
		throw err;
	} catch (std::exception& e) {
		throw PortRegistrationFailure(string_compose(
				_("unable to create port: %1"), e.what()).c_str());
	} catch (...) {
		throw PortRegistrationFailure("unable to create port (unknown error)");
	}

	DEBUG_TRACE (DEBUG::Ports, string_compose ("\t%2 port registration success, ports now = %1\n", ports.reader()->size(), this));
	return newport;
}

boost::shared_ptr<Port>
PortManager::register_input_port (DataType type, const string& portname, bool async)
{
	return register_port (type, portname, true, async);
}

boost::shared_ptr<Port>
PortManager::register_output_port (DataType type, const string& portname, bool async)
{
	return register_port (type, portname, false, async);
}

int
PortManager::unregister_port (boost::shared_ptr<Port> port)
{
	/* caller must hold process lock */

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		Ports::iterator x = ps->find (make_port_name_relative (port->name()));

		if (x != ps->end()) {
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
	if (!_impl) {
		return false;
	}

	PortEngine::PortHandle handle = _impl->get_port_by_name (port_name);

	if (!handle) {
		return false;
	}

	return _impl->connected (handle);
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
		/* neither port is known to us, and this API isn't intended for use as a general patch bay */
		ret = -1;
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
		/* neither port is known to us, and this API isn't intended for use as a general patch bay */
		ret = -1;
	}
	return ret;
}

int
PortManager::disconnect (boost::shared_ptr<Port> port)
{
	return port->disconnect_all ();
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
			cerr << string_compose (_("Re-establising port %1 failed"), i->second->name()) << endl;
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

	PortConnectedOrDisconnected (
		port_a, a,
		port_b, b,
		conn
		); /* EMIT SIGNAL */
}	

void
PortManager::registration_callback ()
{
	if (!_port_remove_in_progress) {
		PortRegisteredOrUnregistered (); /* EMIT SIGNAL */
	}
}

bool
PortManager::can_request_input_monitoring () const
{
	if (!_impl) {
		return false;
	}

	return _impl->can_monitor_input ();
}
 
void
PortManager::request_input_monitoring (const string& name, bool yn) const
{
	if (!_impl) {
		return;
	}

	PortEngine::PortHandle ph = _impl->get_port_by_name (name);

	if (ph) {
		_impl->request_input_monitoring (ph, yn);
	}
}
 
void
PortManager::ensure_input_monitoring (const string& name, bool yn) const
{
	if (!_impl) {
		return;
	}

	PortEngine::PortHandle ph = _impl->get_port_by_name (name);

	if (ph) {
		_impl->ensure_input_monitoring (ph, yn);
	}
}

uint32_t
PortManager::port_name_size() const
{
	if (!_impl) {
		return 0;
	}
	
	return _impl->port_name_size ();
}

string
PortManager::my_name() const
{
	if (!_impl) {
		return string();
	}
	
	return _impl->my_name();
}

int
PortManager::graph_order_callback ()
{
	if (!_port_remove_in_progress) {
		GraphReordered(); /* EMIT SIGNAL */
	}

	return 0;
}

void
PortManager::cycle_start (pframes_t nframes)
{
	Port::set_global_port_buffer_offset (0);
        Port::set_cycle_framecnt (nframes);

	_cycle_ports = ports.reader ();

	for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
		p->second->cycle_start (nframes);
	}
}

void
PortManager::cycle_end (pframes_t nframes)
{
	for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
		p->second->cycle_end (nframes);
	}

	for (Ports::iterator p = _cycle_ports->begin(); p != _cycle_ports->end(); ++p) {
		p->second->flush_buffers (nframes);
	}

	_cycle_ports.reset ();

	/* we are done */
}

void
PortManager::silence (pframes_t nframes)
{
	for (Ports::iterator i = _cycle_ports->begin(); i != _cycle_ports->end(); ++i) {
		if (i->second->sends_output()) {
			i->second->get_buffer(nframes).silence(nframes);
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
PortManager::fade_out (gain_t base_gain, gain_t gain_step, pframes_t nframes)
{
	for (Ports::iterator i = _cycle_ports->begin(); i != _cycle_ports->end(); ++i) {
		
		if (i->second->sends_output()) {
			
			boost::shared_ptr<AudioPort> ap = boost::dynamic_pointer_cast<AudioPort> (i->second);
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
}
