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


#include "ardour/port_manager.h"

using namespace ARDOUR;

PortManager::PortManager ()
	, ports (new Ports)
{
}

void
AudioEngine::remove_all_ports ()
{
	/* make sure that JACK callbacks that will be invoked as we cleanup
	 * ports know that they have nothing to do.
	 */

	port_remove_in_progress = true;

	/* process lock MUST be held by caller
	*/

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}

	/* clear dead wood list in RCU */

	ports.flush ();

	port_remove_in_progress = false;
}


string
AudioEngine::make_port_name_relative (const string& portname) const
{
	string::size_type len;
	string::size_type n;

	len = portname.length();

	for (n = 0; n < len; ++n) {
		if (portname[n] == ':') {
			break;
		}
	}

	if ((n != len) && (portname.substr (0, n) == jack_client_name)) {
		return portname.substr (n+1);
	}

	return portname;
}

string
AudioEngine::make_port_name_non_relative (const string& portname) const
{
	string str;

	if (portname.find_first_of (':') != string::npos) {
		return portname;
	}

	str  = jack_client_name;
	str += ':';
	str += portname;

	return str;
}

bool
AudioEngine::port_is_mine (const string& portname) const
{
	if (portname.find_first_of (':') != string::npos) {
		if (portname.substr (0, jack_client_name.length ()) != jack_client_name) {
                        return false;
                }
        }
        return true;
}

bool
AudioEngine::port_is_physical (const std::string& portname) const
{
        GET_PRIVATE_JACK_POINTER_RET(_jack, false);

        jack_port_t *port = jack_port_by_name (_priv_jack, portname.c_str());

        if (!port) {
                return false;
        }

        return jack_port_flags (port) & JackPortIsPhysical;
}

ChanCount
AudioEngine::n_physical (unsigned long flags) const
{
	ChanCount c;

	GET_PRIVATE_JACK_POINTER_RET (_jack, c);

	const char ** ports = jack_get_ports (_priv_jack, NULL, NULL, JackPortIsPhysical | flags);
	if (ports == 0) {
		return c;
	}

	for (uint32_t i = 0; ports[i]; ++i) {
		if (!strstr (ports[i], "Midi-Through")) {
			DataType t (jack_port_type (jack_port_by_name (_jack, ports[i])));
			c.set (t, c.get (t) + 1);
		}
	}

	free (ports);

	return c;
}

ChanCount
AudioEngine::n_physical_inputs () const
{
	return n_physical (JackPortIsInput);
}

ChanCount
AudioEngine::n_physical_outputs () const
{
	return n_physical (JackPortIsOutput);
}

void
AudioEngine::get_physical (DataType type, unsigned long flags, vector<string>& phy)
{
	GET_PRIVATE_JACK_POINTER (_jack);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, type.to_jack_type(), JackPortIsPhysical | flags)) == 0) {
		return;
	}

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
                        if (strstr (ports[i], "Midi-Through")) {
                                continue;
                        }
			phy.push_back (ports[i]);
		}
		free (ports);
	}
}

/** Get physical ports for which JackPortIsOutput is set; ie those that correspond to
 *  a physical input connector.
 */
void
AudioEngine::get_physical_inputs (DataType type, vector<string>& ins)
{
	get_physical (type, JackPortIsOutput, ins);
}

/** Get physical ports for which JackPortIsInput is set; ie those that correspond to
 *  a physical output connector.
 */
void
AudioEngine::get_physical_outputs (DataType type, vector<string>& outs)
{
	get_physical (type, JackPortIsInput, outs);
}


bool
AudioEngine::can_request_hardware_monitoring ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,false);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortCanMonitor)) == 0) {
		return false;
	}

	free (ports);

	return true;
}


/** @param name Full or short name of port
 *  @return Corresponding Port or 0.
 */

boost::shared_ptr<Port>
AudioEngine::get_port_by_name (const string& portname)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("get_port_by_name() called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			boost::shared_ptr<Port> ();
		}
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
		const std::string check = make_port_name_relative (jack_port_name (x->second->jack_port()));
		if (check != rel) {
			x->second->set_name (check);
		}
		return x->second;
	}

        return boost::shared_ptr<Port> ();
}

void
AudioEngine::port_renamed (const std::string& old_relative_name, const std::string& new_relative_name)
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

const char **
AudioEngine::get_ports (const string& port_name_pattern, const string& type_name_pattern, uint32_t flags)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,0);
	if (!_running) {
		if (!_has_run) {
			fatal << _("get_ports called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return 0;
		}
	}
	return jack_get_ports (_priv_jack, port_name_pattern.c_str(), type_name_pattern.c_str(), flags);
}

void
AudioEngine::port_registration_failure (const std::string& portname)
{
	GET_PRIVATE_JACK_POINTER (_jack);
	string full_portname = jack_client_name;
	full_portname += ':';
	full_portname += portname;


	jack_port_t* p = jack_port_by_name (_priv_jack, full_portname.c_str());
	string reason;

	if (p) {
		reason = string_compose (_("a port with the name \"%1\" already exists: check for duplicated track/bus names"), portname);
	} else {
		reason = string_compose (_("No more JACK ports are available. You will need to stop %1 and restart JACK with more ports if you need this many tracks."), PROGRAM_NAME);
	}

	throw PortRegistrationFailure (string_compose (_("AudioEngine: cannot register port \"%1\": %2"), portname, reason).c_str());
}

boost::shared_ptr<Port>
AudioEngine::register_port (DataType dtype, const string& portname, bool input)
{
	boost::shared_ptr<Port> newport;

	try {
		if (dtype == DataType::AUDIO) {
			newport.reset (new AudioPort (portname, (input ? Port::IsInput : Port::IsOutput)));
		} else if (dtype == DataType::MIDI) {
			newport.reset (new MidiPort (portname, (input ? Port::IsInput : Port::IsOutput)));
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
AudioEngine::register_input_port (DataType type, const string& portname)
{
	return register_port (type, portname, true);
}

boost::shared_ptr<Port>
AudioEngine::register_output_port (DataType type, const string& portname)
{
	return register_port (type, portname, false);
}

int
AudioEngine::unregister_port (boost::shared_ptr<Port> port)
{
	/* caller must hold process lock */

	if (!_running) {
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
AudioEngine::connect (const string& source, const string& destination)
{
	int ret;

	if (!_running) {
		if (!_has_run) {
			fatal << _("connect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
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
AudioEngine::disconnect (const string& source, const string& destination)
{
	int ret;

	if (!_running) {
		if (!_has_run) {
			fatal << _("disconnect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
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
AudioEngine::disconnect (boost::shared_ptr<Port> port)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,-1);

	if (!_running) {
		if (!_has_run) {
			fatal << _("disconnect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
	}

	return port->disconnect_all ();
}

