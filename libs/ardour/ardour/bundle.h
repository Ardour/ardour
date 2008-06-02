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
#include <sigc++/signal.h>
#include "ardour/data_type.h"

namespace ARDOUR {

typedef std::vector<std::string> PortList;
  
/**
 *  A set of `channels', each of which is associated with 0 or more JACK ports.
 */

class Bundle {
  public:
	Bundle () : _type (DataType::AUDIO) {}
	Bundle (bool i) : _type (DataType::AUDIO), _ports_are_inputs (i) {}
	Bundle (std::string const & n, bool i = true) : _name (n), _type (DataType::AUDIO), _ports_are_inputs (i) {}
	virtual ~Bundle() {}

	/**
	 *  @return Number of channels that this Bundle has.
	 */
	virtual uint32_t nchannels () const = 0;
	virtual const PortList& channel_ports (uint32_t) const = 0;

	void set_name (std::string const & n) {
		_name = n;
		NameChanged ();
	}
	
	std::string name () const { return _name; }

	sigc::signal<void> NameChanged;

	void set_type (DataType t) { _type = t; }
	DataType type () const { return _type; }

	void set_ports_are_inputs () { _ports_are_inputs = true; }
	void set_ports_are_outputs () { _ports_are_inputs = false; }
	bool ports_are_inputs () const { return _ports_are_inputs; }
	bool ports_are_outputs () const { return !_ports_are_inputs; }

  private:
	std::string _name;
	ARDOUR::DataType _type;
	bool _ports_are_inputs;
};

}

#endif /* __ardour_bundle_h__ */
