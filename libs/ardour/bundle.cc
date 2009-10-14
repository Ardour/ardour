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

#include "pbd/failed_constructor.h"
#include "ardour/ardour.h"
#include "ardour/bundle.h"
#include "ardour/audioengine.h"
#include "ardour/port.h"
#include "pbd/xml++.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/** Construct an audio bundle.
 *  @param i true if ports are inputs, otherwise false.
 */
Bundle::Bundle (bool i)
	: _type (DataType::AUDIO),
	  _ports_are_inputs (i),
	  _signals_suspended (false),
	  _pending_change (Change (0))
{

}


/** Construct an audio bundle.
 *  @param n Name.
 *  @param i true if ports are inputs, otherwise false.
 */
Bundle::Bundle (std::string const & n, bool i)
	: _name (n),
	  _type (DataType::AUDIO),
	  _ports_are_inputs (i),
	  _signals_suspended (false),
	  _pending_change (Change (0))
{

}


/** Construct a bundle.
 *  @param n Name.
 *  @param t Type.
 *  @param i true if ports are inputs, otherwise false.
 */
Bundle::Bundle (std::string const & n, DataType t, bool i)
	: _name (n),
	  _type (t),
	  _ports_are_inputs (i),
	  _signals_suspended (false),
	  _pending_change (Change (0))
{

}


Bundle::Bundle (boost::shared_ptr<Bundle> other)
	: _channel (other->_channel),
	  _name (other->_name),
	  _type (other->_type),
	  _ports_are_inputs (other->_ports_are_inputs),
	  _signals_suspended (other->_signals_suspended),
	  _pending_change (other->_pending_change)
{

}

uint32_t
Bundle::nchannels () const
{
	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel.size ();
}

Bundle::PortList const &
Bundle::channel_ports (uint32_t c) const
{
	assert (c < nchannels());

	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel[c].ports;
}

/** Add an association between one of our channels and a port.
 *  @param ch Channel index.
 *  @param portname full port name to associate with (including prefix).
 */
void
Bundle::add_port_to_channel (uint32_t ch, string portname)
{
	assert (ch < nchannels());
	assert (portname.find_first_of (':') != string::npos);

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].ports.push_back (portname);
	}

	emit_changed (PortsChanged);
}

/** Disassociate a port from one of our channels.
 *  @param ch Channel index.
 *  @param portname port name to disassociate from.
 */
void
Bundle::remove_port_from_channel (uint32_t ch, string portname)
{
	assert (ch < nchannels());

	bool changed = false;

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		PortList& pl = _channel[ch].ports;
		PortList::iterator i = find (pl.begin(), pl.end(), portname);

		if (i != pl.end()) {
			pl.erase (i);
			changed = true;
		}
	}

	if (changed) {
		emit_changed (PortsChanged);
	}
}

/** Set a single port to be associated with a channel, removing any others.
 *  @param ch Channel.
 *  @param portname Full port name, including prefix.
 */
void
Bundle::set_port (uint32_t ch, string portname)
{
	assert (ch < nchannels());
	assert (portname.find_first_of (':') != string::npos);

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].ports.clear ();
		_channel[ch].ports.push_back (portname);
	}

	emit_changed (PortsChanged);
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n)
{
	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n));
	}

	emit_changed (ConfigurationChanged);
}

bool
Bundle::port_attached_to_channel (uint32_t ch, std::string portname)
{
	assert (ch < nchannels());

	Glib::Mutex::Lock lm (_channel_mutex);
	return (std::find (_channel[ch].ports.begin (), _channel[ch].ports.end (), portname) != _channel[ch].ports.end ());
}

/** Remove a channel.
 *  @param ch Channel.
 */
void
Bundle::remove_channel (uint32_t ch)
{
	assert (ch < nchannels ());

	Glib::Mutex::Lock lm (_channel_mutex);
	_channel.erase (_channel.begin () + ch);
}

/** Remove all channels */
void
Bundle::remove_channels ()
{
	Glib::Mutex::Lock lm (_channel_mutex);

	_channel.clear ();
}

/** @param p Port name.
 *  @return true if any channel is associated with p.
 */
bool
Bundle::uses_port (std::string p) const
{
	Glib::Mutex::Lock lm (_channel_mutex);

	for (std::vector<Channel>::const_iterator i = _channel.begin(); i != _channel.end(); ++i) {
		for (PortList::const_iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
			if (*j == p) {
				return true;
			}
		}
	}

	return false;
}

/** @param p Port name.
 *  @return true if this bundle offers this port on its own on a channel.
 */
bool
Bundle::offers_port_alone (std::string p) const
{
	Glib::Mutex::Lock lm (_channel_mutex);

	for (std::vector<Channel>::const_iterator i = _channel.begin(); i != _channel.end(); ++i) {
		if (i->ports.size() == 1 && i->ports[0] == p) {
			return true;
		}
	}

	return false;
}


/** @param ch Channel.
 *  @return Channel name.
 */
std::string
Bundle::channel_name (uint32_t ch) const
{
	assert (ch < nchannels());

	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel[ch].name;
}

/** Set the name of a channel.
 *  @param ch Channel.
 *  @param n New name.
 */
void
Bundle::set_channel_name (uint32_t ch, std::string const & n)
{
	assert (ch < nchannels());

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].name = n;
	}

	emit_changed (NameChanged);
}

/** Take the channels from another bundle and add them to this bundle,
 *  so that channels from other are added to this (with their ports)
 *  and are named "<other_bundle_name> <other_channel_name>".
 */
void
Bundle::add_channels_from_bundle (boost::shared_ptr<Bundle> other)
{
	uint32_t const ch = nchannels ();

	for (uint32_t i = 0; i < other->nchannels(); ++i) {

		std::stringstream s;
		s << other->name() << " " << other->channel_name(i);

		add_channel (s.str());

		PortList const& pl = other->channel_ports (i);
		for (uint32_t j = 0; j < pl.size(); ++j) {
			add_port_to_channel (ch + i, pl[j]);
		}
	}
}

/** Connect the ports associated with our channels to the ports associated
 *  with another bundle's channels.
 *  @param other Other bundle.
 *  @param engine AudioEngine to use to make the connections.
 */
void
Bundle::connect (boost::shared_ptr<Bundle> other, AudioEngine & engine)
{
	uint32_t const N = nchannels ();
	assert (N == other->nchannels ());

	for (uint32_t i = 0; i < N; ++i) {
		Bundle::PortList const & our_ports = channel_ports (i);
		Bundle::PortList const & other_ports = other->channel_ports (i);

		for (Bundle::PortList::const_iterator j = our_ports.begin(); j != our_ports.end(); ++j) {
			for (Bundle::PortList::const_iterator k = other_ports.begin(); k != other_ports.end(); ++k) {
				engine.connect (*j, *k);
			}
		}
	}
}

void
Bundle::disconnect (boost::shared_ptr<Bundle> other, AudioEngine & engine)
{
	uint32_t const N = nchannels ();
	assert (N == other->nchannels ());

	for (uint32_t i = 0; i < N; ++i) {
		Bundle::PortList const & our_ports = channel_ports (i);
		Bundle::PortList const & other_ports = other->channel_ports (i);

		for (Bundle::PortList::const_iterator j = our_ports.begin(); j != our_ports.end(); ++j) {
			for (Bundle::PortList::const_iterator k = other_ports.begin(); k != other_ports.end(); ++k) {
				engine.disconnect (*j, *k);
			}
		}
	}
}

/** Remove all ports from all channels */
void
Bundle::remove_ports_from_channels ()
{
	{
		Glib::Mutex::Lock lm (_channel_mutex);
		for (uint32_t c = 0; c < _channel.size(); ++c) {
			_channel[c].ports.clear ();
		}

	}

	emit_changed (PortsChanged);
}

/** Remove all ports from a given channel.
 *  @param ch Channel.
 */
void
Bundle::remove_ports_from_channel (uint32_t ch)
{
	assert (ch < nchannels ());

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].ports.clear ();
	}

	emit_changed (PortsChanged);
}

void
Bundle::suspend_signals ()
{
	_signals_suspended = true;
}

void
Bundle::resume_signals ()
{
	if (_pending_change) {
		Changed (_pending_change);
		_pending_change = Change (0);
	}

	_signals_suspended = false;
}

void
Bundle::emit_changed (Change c)
{
	if (_signals_suspended) {
		_pending_change = Change (int (_pending_change) | int (c));
	} else {
		Changed (c);
	}
}

bool
Bundle::connected_to (boost::shared_ptr<Bundle> other, AudioEngine & engine)
{
	if (_ports_are_inputs == other->_ports_are_inputs ||
	    _type != other->_type ||
	    nchannels() != other->nchannels ()) {

		return false;
	}

	for (uint32_t i = 0; i < nchannels(); ++i) {
		Bundle::PortList const & A = channel_ports (i);
		Bundle::PortList const & B = other->channel_ports (i);

		for (uint32_t j = 0; j < A.size(); ++j) {
			for (uint32_t k = 0; k < B.size(); ++k) {

				Port* p = engine.get_port_by_name (A[j]);
				Port* q = engine.get_port_by_name (B[k]);

				if (!p && !q) {
					return false;
				}

				if (p && !p->connected_to (B[k])) {
					return false;
				} else if (q && !q->connected_to (A[j])) {
					return false;
				}
			}
		}
	}

	return true;
}
