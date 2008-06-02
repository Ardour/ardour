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

*/

#include <algorithm>

#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/bundle.h>
#include <pbd/xml++.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

/** Construct a Bundle from an XML node.
 * @param node XML node.
 */
Bundle::Bundle (const XMLNode& node)
{
	if (set_state (node)) {
		throw failed_constructor();
	}
}

/** Construct an InputBundle from an XML node.
 * @param node XML node.
 */
InputBundle::InputBundle (const XMLNode& node)
	: Bundle (node)
{
  
}

/** Construct an OutputBundle from an XML node.
 * @param node XML node.
 */
OutputBundle::OutputBundle (const XMLNode& node)
	: Bundle (node)
{
  
}

/** Set the name.
 * @param name New name.
 */
void
Bundle::set_name (string name, void *src)
{
	_name = name;
	NameChanged (src);
}

/** Add an association between one of our channels and a JACK port.
 * @param ch Channel index.
 * @param portname JACK port name to associate with.
 */
void
Bundle::add_port_to_channel (int ch, string portname)
{
	{
		Glib::Mutex::Lock lm (channels_lock);
		_channels[ch].push_back (portname);
	}
	
	PortsChanged (ch); /* EMIT SIGNAL */
}

/** Disassociate a JACK port from one of our channels.
 * @param ch Channel index.
 * @param portname JACK port name to disassociate from.
 */

void
Bundle::remove_port_from_channel (int ch, string portname)
{
	bool changed = false;

	{
		Glib::Mutex::Lock lm (channels_lock);
		PortList& pl = _channels[ch];
		PortList::iterator i = find (pl.begin(), pl.end(), portname);
		
		if (i != pl.end()) {
			pl.erase (i);
			changed = true;
		}
	}

	if (changed) {
		 PortsChanged (ch); /* EMIT SIGNAL */
	}
}

/**
 * @param ch Channel index.
 * @return List of JACK ports that this channel is connected to.
 */
const Bundle::PortList&
Bundle::channel_ports (int ch) const
{
	Glib::Mutex::Lock lm (channels_lock);
	return _channels[ch];
}

/** operator== for Bundles; they are equal if their channels are the same.
 * @param other Bundle to compare with this one.
 */
bool
Bundle::operator== (const Bundle& other) const
{
	return other._channels == _channels;
}


/** Set the number of channels.
 * @param n New number of channels.
 */

void
Bundle::set_nchannels (int n)
{
	{
		Glib::Mutex::Lock lm (channels_lock);
		_channels.clear ();
		for (int i = 0; i < n; ++i) {
			_channels.push_back (PortList());
		}
	}

	ConfigurationChanged (); /* EMIT SIGNAL */
}

XMLNode&
Bundle::get_state ()
{
	XMLNode *node;
	string str;

	if (dynamic_cast<InputBundle *> (this)) {
		node = new XMLNode ("InputConnection");
	} else {
		node = new XMLNode ("OutputConnection");
	}

	node->add_property ("name", _name);

	for (vector<PortList>::iterator i = _channels.begin(); i != _channels.end(); ++i) {

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
Bundle::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	if ((prop = node.property ("name")) == 0) {
		error << _("Node for Connection has no \"name\" property") << endmsg;
		return -1;
	}

	_name = prop->value();
	_dynamic = false;
	
	if ((prop = node.property ("connections")) == 0) {
		error << _("Node for Connection has no \"connections\" property") << endmsg;
		return -1;
	}
	
	set_channels (prop->value());

	return 0;
}

/** Set up channels from an XML property string.
 * @param str String.
 * @return 0 on success, -1 on error.
 */
int
Bundle::set_channels (const string& str)
{
	vector<string> ports;
	int i;
	int n;
	int nchannels;
	
	if ((nchannels = count (str.begin(), str.end(), '{')) == 0) {
		return 0;
	}

	set_nchannels (nchannels);

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
				add_port_to_channel (i, ports[x]);
			}
		}

		ostart = end+1;
		i++;
	}

	return 0;
}

int
Bundle::parse_io_string (const string& str, vector<string>& ports)
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

