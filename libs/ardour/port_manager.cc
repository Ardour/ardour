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

#include "midi++/manager.h"

#include "ardour/port_manager.h"
#include "ardour/audio_port.h"
#include "ardour/midi_port.h"

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
	PortEngine::PortHandle ph = _impl->get_port_by_name (portname);
	if (!ph) {
		return false;
	}

	return _impl->port_is_physical (ph);
}

/** @param name Full or short name of port
 *  @return Corresponding Port or 0.
 */

boost::shared_ptr<Port>
PortManager::get_port_by_name (const string& portname)
{
	if (!_impl->connected()) {
		fatal << _("get_port_by_name() called before engine was started") << endmsg;
		/*NOTREACHED*/
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
PortManager::get_ports (const string& port_name_pattern, DataType type, PortFlags flags, vector<string>& s)
{
	return _impl->get_ports (port_name_pattern, type, flags, s);
}

void
PortManager::port_registration_failure (const std::string& portname)
{
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
PortManager::register_port (DataType dtype, const string& portname, bool input)
{
	boost::shared_ptr<Port> newport;

	try {
		if (dtype == DataType::AUDIO) {
			newport.reset (new AudioPort (portname, (input ? IsInput : IsOutput)));
		} else if (dtype == DataType::MIDI) {
			newport.reset (new MidiPort (portname, (input ? IsInput : IsOutput)));
		} else {
			throw PortRegistrationFailure("unable to create port (unknown type)");
		}

		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->insert (make_pair (make_port_name_relative (portname), newport));

		/* writer goes out of scope, forces update */

		return newport;
	}

	catch (PortRegistrationFailure& err) {
		throw err;
	} catch (std::exception& e) {
		throw PortRegistrationFailure(string_compose(
				_("unable to create port: %1"), e.what()).c_str());
	} catch (...) {
		throw PortRegistrationFailure("unable to create port (unknown error)");
	}
}

boost::shared_ptr<Port>
PortManager::register_input_port (DataType type, const string& portname)
{
	return register_port (type, portname, true);
}

boost::shared_ptr<Port>
PortManager::register_output_port (DataType type, const string& portname)
{
	return register_port (type, portname, false);
}

int
PortManager::unregister_port (boost::shared_ptr<Port> port)
{
	/* caller must hold process lock */

	if (!_impl->connected()) {
		/* probably happening when the engine has been halted by JACK,
		   in which case, there is nothing we can do here.
		   */
		return 0;
	}

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

	if (!_impl->connected()) {
		return -1;
	}

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

	if (!_impl->connected()) {
		return -1;
	}

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

	for (i = p->begin(); i != p->end(); ++i) {
		if (i->second->reestablish ()) {
			break;
		}
	}

	if (i != p->end()) {
		/* failed */
		remove_all_ports ();
		return -1;
	}

	MIDI::Manager::instance()->reestablish ();

	return 0;
}

int
PortManager::reconnect_ports ()
{
	boost::shared_ptr<Ports> p = ports.reader ();

	/* re-establish connections */
	
	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		i->second->reconnect ();
	}

	MIDI::Manager::instance()->reconnect ();

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
