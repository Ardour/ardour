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
	Glib::Mutex::Lock lm (_channel_mutex);

	ChanCount c;
	for (vector<Channel>::const_iterator i = _channel.begin(); i != _channel.end(); ++i) {
		c.set (i->type, c.get (i->type) + 1);
	}

	return c;
}

Bundle::PortList const &
Bundle::channel_ports (uint32_t c) const
{
	assert (c < nchannels().n_total());

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
	assert (ch < nchannels().n_total());
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
	assert (ch < nchannels().n_total());

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
	assert (ch < nchannels().n_total());
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
Bundle::add_channel (std::string const & n, DataType t)
{
	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n, t));
	}

	emit_changed (ConfigurationChanged);
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n, DataType t, PortList p)
{
	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n, t, p));
	}

	emit_changed (ConfigurationChanged);
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n, DataType t, std::string const & p)
{
	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n, t, p));
	}

	emit_changed (ConfigurationChanged);
}

bool
Bundle::port_attached_to_channel (uint32_t ch, std::string portname)
{
	assert (ch < nchannels().n_total());

	Glib::Mutex::Lock lm (_channel_mutex);
	return (std::find (_channel[ch].ports.begin (), _channel[ch].ports.end (), portname) != _channel[ch].ports.end ());
}

/** Remove a channel.
 *  @param ch Channel.
 */
void
Bundle::remove_channel (uint32_t ch)
{
	assert (ch < nchannels().n_total());

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
Bundle::offers_port (std::string p) const
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
	assert (ch < nchannels().n_total());

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
	assert (ch < nchannels().n_total());

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
	uint32_t const ch = nchannels().n_total();

	for (uint32_t i = 0; i < other->nchannels().n_total(); ++i) {

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
 */
void
Bundle::connect (boost::shared_ptr<Bundle> other, AudioEngine & engine)
{
	uint32_t const N = nchannels().n_total();
	assert (N == other->nchannels().n_total());

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
	uint32_t const N = nchannels().n_total();
	assert (N == other->nchannels().n_total());

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
	assert (ch < nchannels().n_total());

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
	if (_ports_are_inputs == other->_ports_are_inputs || nchannels() != other->nchannels()) {
		return false;
	}

	for (uint32_t i = 0; i < nchannels().n_total(); ++i) {
		Bundle::PortList const & A = channel_ports (i);
		Bundle::PortList const & B = other->channel_ports (i);

		for (uint32_t j = 0; j < A.size(); ++j) {
			for (uint32_t k = 0; k < B.size(); ++k) {

				boost::shared_ptr<Port> p = engine.get_port_by_name (A[j]);
				boost::shared_ptr<Port> q = engine.get_port_by_name (B[k]);

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

/** This must not be called in code executed as a response to a JACK event,
 *  as it uses jack_port_get_all_connections().
 *  @return true if any of this bundle's channels are connected to anything.
 */
bool
Bundle::connected_to_anything (AudioEngine& engine)
{
	for (uint32_t i = 0; i < nchannels().n_total(); ++i) {
		Bundle::PortList const & ports = channel_ports (i);

		for (uint32_t j = 0; j < ports.size(); ++j) {
			/* ports[j] may not be an Ardour port, so use JACK directly
			   rather than doing it with Port.
			*/
			jack_port_t* jp = jack_port_by_name (engine.jack(), ports[j].c_str());
			if (jp) {
				const char ** c = jack_port_get_all_connections (engine.jack(), jp);
				if (c) {
					jack_free (c);
					return true;
				}
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
	uint32_t const N = nchannels().n_total();

	if (b->nchannels().n_total() != N) {
		return false;
	}

	/* XXX: probably should sort channel port lists before comparing them */

	for (uint32_t i = 0; i < N; ++i) {
		if (channel_ports (i) != b->channel_ports (i)) {
			return false;
		}
	}

	return true;
}

DataType
Bundle::channel_type (uint32_t c) const
{
	assert (c < nchannels().n_total());

	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel[c].type;
}

ostream &
operator<< (ostream& os, Bundle const & b)
{
	os << "BUNDLE " << b.nchannels() << " channels: ";
	for (uint32_t i = 0; i < b.nchannels().n_total(); ++i) {
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
	
	Glib::Mutex::Lock lm (_channel_mutex);

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

	/* NOTREACHED */
	return -1;
}

/** Perform the reverse of type_channel_to_overall */
uint32_t
Bundle::overall_channel_to_type (DataType t, uint32_t c) const
{
	if (t == DataType::NIL) {
		return c;
	}
	
	Glib::Mutex::Lock lm (_channel_mutex);

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
