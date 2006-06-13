/*
    Copyright (C) 2002 Paul Davis 

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

    $Id$
*/

#include <algorithm>

#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/connection.h>
#include <pbd/xml++.h>

#include "i18n.h"

using namespace ARDOUR;

Connection::Connection (const XMLNode& node)
{
	if (set_state (node)) {
		throw failed_constructor();
	}
}

InputConnection::InputConnection (const XMLNode& node)
	: Connection (node)
{
}

OutputConnection::OutputConnection (const XMLNode& node)
	: Connection (node)
{
}

void
Connection::set_name (string name, void *src)
{
	_name = name;
	NameChanged (src);
}

void
Connection::add_connection (int port, string portname)
{
	{
		Glib::Mutex::Lock lm (port_lock);
		_ports[port].push_back (portname);
	}
	 ConnectionsChanged (port); /* EMIT SIGNAL */
}

void
Connection::remove_connection (int port, string portname)
{
	bool changed = false;

	{
		Glib::Mutex::Lock lm (port_lock);
		PortList& pl = _ports[port];
		PortList::iterator i = find (pl.begin(), pl.end(), portname);
		
		if (i != pl.end()) {
			pl.erase (i);
			changed = true;
		}
	}

	if (changed) {
		 ConnectionsChanged (port); /* EMIT SIGNAL */
	}
}

const Connection::PortList&
Connection::port_connections (int port) const
{
	Glib::Mutex::Lock lm (port_lock);
	return _ports[port];
}

bool
Connection::operator== (const Connection& other) const
{
	return other._ports == _ports;
}

void
Connection::add_port ()
{
	{
		Glib::Mutex::Lock lm (port_lock);
		_ports.push_back (PortList());
	}
	 ConfigurationChanged(); /* EMIT SIGNAL */
}

void
Connection::remove_port (int which_port)
{
	bool changed = false;

	{
		Glib::Mutex::Lock lm (port_lock);
		vector<PortList>::iterator i;
		int n;
		
		for (n = 0, i = _ports.begin(); i != _ports.end() && n < which_port; ++i, ++n);

		if (i != _ports.end()) {
			_ports.erase (i);
			changed = true;
		}
	}

	if (changed) {
		 ConfigurationChanged(); /* EMIT SIGNAL */
	}
}

void 
Connection::clear ()
{
	{
		Glib::Mutex::Lock lm (port_lock);
		_ports.clear ();
	}

	 ConfigurationChanged(); /* EMIT SIGNAL */
}

XMLNode&
Connection::get_state ()
{
	XMLNode *node;
	string str;

	if (dynamic_cast<InputConnection *> (this)) {
		node = new XMLNode ("InputConnection");
	} else {
		node = new XMLNode ("OutputConnection");
	}

	node->add_property ("name", _name);

	for (vector<PortList>::iterator i = _ports.begin(); i != _ports.end(); ++i) {

		str += '{';

		for (vector<string>::iterator ii = (*i).begin(); ii != (*i).end(); ++ii) {
			if (ii != (*i).begin()) {
				str += ',';
			}
			str += *ii;
		}
		str += '}';
	}

	node->add_property ("connections", str);

	return *node;
}

int
Connection::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	if ((prop = node.property ("name")) == 0) {
		error << _("Node for Connection has no \"name\" property") << endmsg;
		return -1;
	}

	_name = prop->value();
	_sysdep = false;
	
	if ((prop = node.property ("connections")) == 0) {
		error << _("Node for Connection has no \"connections\" property") << endmsg;
		return -1;
	}
	
	set_connections (prop->value());

	return 0;
}

int
Connection::set_connections (const string& str)
{
	vector<string> ports;
	int i;
	int n;
	int nports;
	
	if ((nports = count (str.begin(), str.end(), '{')) == 0) {
		return 0;
	}

	for (n = 0; n < nports; ++n) {
		add_port ();
	}

	string::size_type start, end, ostart;

	ostart = 0;
	start = 0;
	end = 0;
	i = 0;

	while ((start = str.find_first_of ('{', ostart)) != string::npos) {
		start += 1;

		if ((end = str.find_first_of ('}', start)) == string::npos) {
			error << string_compose(_("IO: badly formed string in XML node for inputs \"%1\""), str) << endmsg;
			return -1;
		}

		if ((n = parse_io_string (str.substr (start, end - start), ports)) < 0) {
			error << string_compose(_("bad input string in XML node \"%1\""), str) << endmsg;

			return -1;
			
		} else if (n > 0) {

			for (int x = 0; x < n; ++x) {
				add_connection (i, ports[x]);
			}
		}

		ostart = end+1;
		i++;
	}

	return 0;
}

int
Connection::parse_io_string (const string& str, vector<string>& ports)
{
	string::size_type pos, opos;

	if (str.length() == 0) {
		return 0;
	}

	pos = 0;
	opos = 0;

	ports.clear ();

	while ((pos = str.find_first_of (',', opos)) != string::npos) {
		ports.push_back (str.substr (opos, pos - opos));
		opos = pos + 1;
	}
	
	if (opos < str.length()) {
		ports.push_back (str.substr(opos));
	}

	return ports.size();
}

