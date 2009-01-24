/*
    Copyright (C) 2002-2007 Paul Davis 

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

#ifndef __ardour_bundle_h__
#define __ardour_bundle_h__

#include <string>
#include <vector>
#include <glibmm/thread.h>
#include <sigc++/signal.h>
#include "ardour/data_type.h"

namespace ARDOUR {
  
/** A set of `channels', each of which is associated with 0 or more ports.
 *  Each channel has a name which can be anything useful.
 *  Intended for grouping things like, for example, a buss' outputs.
 *  `Channel' is a rather overloaded term but I can't think of a better
 *  one right now.
 */
class Bundle : public sigc::trackable
{
  public:

	/// List of ports associated with a channel.  We can't use a
	/// PortSet because we might want to involve non-Ardour ports
	/// (ie those without a Port object)
	typedef std::vector<std::string> PortList;

	struct Channel {
		Channel (std::string n) : name (n) {}

		bool operator== (Channel const &o) const {
			return name == o.name && ports == o.ports;
		}
		
		std::string name;
		PortList ports;
	};

	/** Construct an audio bundle.
	 *  @param i true if ports are inputs, otherwise false.
	 */
	Bundle (bool i = true) : _type (DataType::AUDIO), _ports_are_inputs (i) {}

	/** Construct an audio bundle.
	 *  @param n Name.
	 *  @param i true if ports are inputs, otherwise false.
	 */
	Bundle (std::string const & n, bool i = true) : _name (n), _type (DataType::AUDIO), _ports_are_inputs (i) {}

	/** Construct a bundle.
	 *  @param n Name.
	 *  @param t Type.
	 *  @param i true if ports are inputs, otherwise false.
	 */
	Bundle (std::string const & n, DataType t, bool i = true) : _name (n), _type (t), _ports_are_inputs (i) {}

	virtual ~Bundle() {}

	/** @return Number of channels that this Bundle has */
	uint32_t nchannels () const;

	/** @param Channel index.
	 *  @return Ports associated with this channel.
	 */
	PortList const & channel_ports (uint32_t) const;

	void add_channel (std::string const &);
	std::string channel_name (uint32_t) const;
	void set_channel_name (uint32_t, std::string const &);
	void add_port_to_channel (uint32_t, std::string);
	void set_port (uint32_t, std::string);
	void remove_port_from_channel (uint32_t, std::string);
	bool port_attached_to_channel (uint32_t, std::string);
	bool uses_port (std::string) const;
	bool offers_port_alone (std::string) const;
	void remove_channel (uint32_t);
	void remove_channels ();

	/** Set the name.
	 *  @param n New name.
	 */
	void set_name (std::string const & n) {
		_name = n;
		NameChanged ();
	}

	/** @return Bundle name */
	std::string name () const { return _name; }

	/** Set the type of the ports in this Bundle.
	 *  @param t New type.
	 */
	void set_type (DataType t) { _type = t; }

	/** @return Type of the ports in this Bundle. */
	DataType type () const { return _type; }

	void set_ports_are_inputs () { _ports_are_inputs = true; }
	void set_ports_are_outputs () { _ports_are_inputs = false; }
	bool ports_are_inputs () const { return _ports_are_inputs; }
	bool ports_are_outputs () const { return !_ports_are_inputs; }

	bool operator== (Bundle const &) const;

	/** Emitted when the bundle name or a channel name has changed */
	sigc::signal<void> NameChanged;
	/** The number of channels has changed */
	sigc::signal<void> ConfigurationChanged;
	/** The port list associated with one of our channels has changed */
	sigc::signal<void, int> PortsChanged;

  protected:
	
	/// mutex for _channel_ports and _channel_names
	/// XXX: is this necessary?
	mutable Glib::Mutex _channel_mutex;
	std::vector<Channel> _channel;

  private:
	int set_channels (std::string const &);
	int parse_io_string (std::string const &, std::vector<std::string> &);
	
	std::string _name;
	ARDOUR::DataType _type;
	bool _ports_are_inputs;
};

}

#endif /* __ardour_bundle_h__ */
