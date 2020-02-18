/*
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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

#include "ardour/bundle.h"
#include "ardour/audioengine.h"
#include "ardour/port.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/** Construct an audio bundle.
 *  @param i true if ports are inputs, otherwise false.
 */
Bundle::Bundle (bool i)
	: _ports_are_inputs (i),
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
	  _ports_are_inputs (i),
	  _signals_suspended (false),
	  _pending_change (Change (0))
{

}

Bundle::Bundle (boost::shared_ptr<Bundle> other)
	: _channel (other->_channel),
	  _name (other->_name),
	  _ports_are_inputs (other->_ports_are_inputs),
	  _signals_suspended (other->_signals_suspended),
	  _pending_change (other->_pending_change)
{

}

ChanCount
Bundle::nchannels () const
{
	Glib::Threads::Mutex::Lock lm (_channel_mutex);

	ChanCount c;
	for (vector<Channel>::const_iterator i = _channel.begin(); i != _channel.end(); ++i) {
		c.set (i->type, c.get (i->type) + 1);
	}

	return c;
}

uint32_t
Bundle::n_total () const
{
    /* Simpler and far more efficient than nchannels.n_total() */
    return _channel.size();
}

Bundle::PortList const &
Bundle::channel_ports (uint32_t c) const
{
	assert (c < n_total());

	Glib::Threads::Mutex::Lock lm (_channel_mutex);
	return _channel[c].ports;
}

/** Add an association between one of our channels and a port.
 *  @param ch Channel index.
 *  @param portname full port name to associate with (including prefix).
 */
void
Bundle::add_port_to_channel (uint32_t ch, string portname)
{
	assert (ch < n_total());
	assert (portname.find_first_of (':') != string::npos);

	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
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
	assert (ch < n_total());

	bool changed = false;

	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
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
	assert (ch < n_total());
	assert (portname.find_first_of (':') != string::npos);

	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
		_channel[ch].ports.clear ();
		_channel[ch].ports.push_back (portname);
	}

	emit_changed (PortsChanged);
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n, DataType t)
{
	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n, t));
	}

	emit_changed (ConfigurationChanged);
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n, DataType t, PortList p)
{
	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n, t, p));
	}

	emit_changed (ConfigurationChanged);
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n, DataType t, std::string const & p)
{
	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n, t, p));
	}

	emit_changed (ConfigurationChanged);
}

bool
Bundle::port_attached_to_channel (uint32_t ch, std::string portname)
{
	assert (ch < n_total());

	Glib::Threads::Mutex::Lock lm (_channel_mutex);
	return (std::find (_channel[ch].ports.begin (), _channel[ch].ports.end (), portname) != _channel[ch].ports.end ());
}

/** Remove a channel.
 *  @param ch Channel.
 */
void
Bundle::remove_channel (uint32_t ch)
{
	assert (ch < n_total());

	Glib::Threads::Mutex::Lock lm (_channel_mutex);
	_channel.erase (_channel.begin () + ch);

	lm.release();
	emit_changed (ConfigurationChanged);
}

/** Remove all channels */
void
Bundle::remove_channels ()
{
	Glib::Threads::Mutex::Lock lm (_channel_mutex);

	_channel.clear ();

	lm.release();
	emit_changed (ConfigurationChanged);
}

/** @param p Port name.
 *  @return true if any channel is associated with p.
 */
bool
Bundle::offers_port (std::string p) const
{
	Glib::Threads::Mutex::Lock lm (_channel_mutex);

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
	Glib::Threads::Mutex::Lock lm (_channel_mutex);

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
	assert (ch < n_total());

	Glib::Threads::Mutex::Lock lm (_channel_mutex);
	return _channel[ch].name;
}

/** Set the name of a channel.
 *  @param ch Channel.
 *  @param n New name.
 */
void
Bundle::set_channel_name (uint32_t ch, std::string const & n)
{
	assert (ch < n_total());

	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
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
	uint32_t const ch = n_total();

	for (uint32_t i = 0; i < other->n_total(); ++i) {

		std::stringstream s;
		s << other->name() << " " << other->channel_name(i);

		add_channel (s.str(), other->channel_type(i));

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
 *  @param allow_partial whether to allow leaving unconnected channels types,
 *              or require that the ChanCounts match exactly (default false).
 */
void
Bundle::connect (boost::shared_ptr<Bundle> other, AudioEngine & engine,
                 bool allow_partial)
{
	ChanCount our_count = nchannels();
	ChanCount other_count = other->nchannels();

	if (!allow_partial && our_count != other_count) {
		assert (our_count == other_count);
		return;
	}

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t N = our_count.n(*t);
		if (N != other_count.n(*t))
			continue;
		for (uint32_t i = 0; i < N; ++i) {
			Bundle::PortList const & our_ports =
				channel_ports (type_channel_to_overall(*t, i));
			Bundle::PortList const & other_ports =
				other->channel_ports (other->type_channel_to_overall(*t, i));

			for (Bundle::PortList::const_iterator j = our_ports.begin();
						j != our_ports.end(); ++j) {
				for (Bundle::PortList::const_iterator k = other_ports.begin();
							k != other_ports.end(); ++k) {
					engine.connect (*j, *k);
				}
			}
		}
	}
}

void
Bundle::disconnect (boost::shared_ptr<Bundle> other, AudioEngine & engine)
{
	ChanCount our_count = nchannels();
	ChanCount other_count = other->nchannels();

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t N = min(our_count.n(*t), other_count.n(*t));
		for (uint32_t i = 0; i < N; ++i) {
			Bundle::PortList const & our_ports =
				channel_ports (type_channel_to_overall(*t, i));
			Bundle::PortList const & other_ports =
				other->channel_ports (other->type_channel_to_overall(*t, i));

			for (Bundle::PortList::const_iterator j = our_ports.begin();
						j != our_ports.end(); ++j) {
				for (Bundle::PortList::const_iterator k = other_ports.begin();
							k != other_ports.end(); ++k) {
					engine.disconnect (*j, *k);
				}
			}
		}
	}
}

/** Remove all ports from all channels */
void
Bundle::remove_ports_from_channels ()
{
	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
		for (uint32_t c = 0; c < n_total(); ++c) {
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
	assert (ch < n_total());

	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);
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

/** This must not be called in code executed as a response to a backend event,
 *  as it may query the backend in the same thread where it's waiting for us.
 * @return true if a Bundle is connected to another.
 * @param type: if not NIL, restrict the check to channels of that type.
 * @param exclusive: if true, additionally check if the bundle is connected
 *                   only to |other|, and return false if not. */
bool
Bundle::connected_to (boost::shared_ptr<Bundle> other, AudioEngine & engine,
                      DataType type, bool exclusive)
{
	if (_ports_are_inputs == other->_ports_are_inputs)
		return false;

	if (type == DataType::NIL) {
		for (DataType::iterator t = DataType::begin();
		                        t != DataType::end(); ++t) {
			if (!connected_to(other, engine, *t, exclusive))
				return false;
		}
		return true;
	}

	uint32_t N = nchannels().n(type);
	if (other->nchannels().n(type) != N)
		return false;

	vector<string> port_connections;

	for (uint32_t i = 0; i < N; ++i) {
		Bundle::PortList const & our_ports =
			channel_ports (type_channel_to_overall(type, i));
		Bundle::PortList const & other_ports =
			other->channel_ports (other->type_channel_to_overall(type, i));

		for (Bundle::PortList::const_iterator j = our_ports.begin(); j != our_ports.end(); ++j) {

			boost::shared_ptr<Port> p = engine.get_port_by_name(*j);

			for (Bundle::PortList::const_iterator k = other_ports.begin();
			                                   k != other_ports.end(); ++k) {
				boost::shared_ptr<Port> q = engine.get_port_by_name(*k);

				if (!p && !q) {
					return false;
				}

				if (p && !p->connected_to (*k)) {
					return false;
				} else if (q && !q->connected_to (*j)) {
					return false;
				}
			}

			if (exclusive && p) {
				port_connections.clear();
				p->get_connections(port_connections);
				if (port_connections.size() != other_ports.size())
					return false;
			}
		}
	}

	return true;
}

/** This must not be called in code executed as a response to a backend event,
 *  as it uses the backend port_get_all_connections().
 *  @return true if any of this bundle's channels are connected to anything.
 */
bool
Bundle::connected_to_anything (AudioEngine& engine)
{
	PortManager& pm (engine);

	for (uint32_t i = 0; i < n_total(); ++i) {
		Bundle::PortList const & ports = channel_ports (i);

		for (uint32_t j = 0; j < ports.size(); ++j) {

			/* ports[j] may not be an Ardour port, so use the port manager directly
			   rather than doing it with Port.
			*/

			if (pm.connected (ports[j])) {
				return true;
			}
		}
	}

	return false;
}

void
Bundle::set_ports_are_inputs ()
{
	_ports_are_inputs = true;
	emit_changed (DirectionChanged);
}

void
Bundle::set_ports_are_outputs ()
{
	_ports_are_inputs = false;
	emit_changed (DirectionChanged);
}

/** Set the name.
 *  @param n New name.
 */
void
Bundle::set_name (string const & n)
{
	_name = n;
	emit_changed (NameChanged);
}

/** @param b Other bundle.
 *  @return true if b has the same number of channels as this bundle, and those channels have corresponding ports.
 */
bool
Bundle::has_same_ports (boost::shared_ptr<Bundle> b) const
{
	ChanCount our_count = nchannels();
	ChanCount other_count = b->nchannels();

	if (our_count != other_count)
		return false;

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t N = our_count.n(*t);
		for (uint32_t i = 0; i < N; ++i) {
			Bundle::PortList const & our_ports =
				channel_ports (type_channel_to_overall(*t, i));
			Bundle::PortList const & other_ports =
				b->channel_ports (b->type_channel_to_overall(*t, i));

			if (our_ports != other_ports)
				return false;
		}
	}

	return true;
}

DataType
Bundle::channel_type (uint32_t c) const
{
	assert (c < n_total());

	Glib::Threads::Mutex::Lock lm (_channel_mutex);
	return _channel[c].type;
}

ostream &
operator<< (ostream& os, Bundle const & b)
{
	os << "BUNDLE " << b.nchannels() << " channels: ";
	for (uint32_t i = 0; i < b.n_total(); ++i) {
		os << "( ";
		Bundle::PortList const & pl = b.channel_ports (i);
		for (Bundle::PortList::const_iterator j = pl.begin(); j != pl.end(); ++j) {
			os << *j << " ";
		}
		os << ") ";
	}

	return os;
}

bool
Bundle::operator== (Bundle const & other)
{
	return _channel == other._channel;
}

/** Given an index of a channel as the nth channel of a particular type,
 *  return an index of that channel when considering channels of all types.
 *
 *  e.g. given a bundle with channels:
 *          fred   [audio]
 *          jim    [audio]
 *          sheila [midi]
 *
 * If t == MIDI and c == 0, then we would return 2, as 2 is the index of the
 * 0th MIDI channel.
 *
 * If t == NIL, we just return c.
 */

uint32_t
Bundle::type_channel_to_overall (DataType t, uint32_t c) const
{
	if (t == DataType::NIL) {
		return c;
	}

	Glib::Threads::Mutex::Lock lm (_channel_mutex);

	vector<Channel>::const_iterator i = _channel.begin ();

	uint32_t o = 0;

	while (1) {

		assert (i != _channel.end ());

		if (i->type != t) {
			++i;
		} else {
			if (c == 0) {
				return o;
			}
			--c;
		}

		++o;
	}

	abort(); /* NOTREACHED */
	return -1;
}

/** Perform the reverse of type_channel_to_overall */
uint32_t
Bundle::overall_channel_to_type (DataType t, uint32_t c) const
{
	if (t == DataType::NIL) {
		return c;
	}

	Glib::Threads::Mutex::Lock lm (_channel_mutex);

	uint32_t s = 0;

	vector<Channel>::const_iterator i = _channel.begin ();
	for (uint32_t j = 0; j < c; ++j) {
		if (i->type == t) {
			++s;
		}
		++i;
	}

	return s;
}
