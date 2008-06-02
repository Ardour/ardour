/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __ardour_user_bundle_h__
#define __ardour_user_bundle_h__

#include <vector>
#include <glibmm/thread.h>
#include "pbd/stateful.h"
#include "ardour/bundle.h"

namespace ARDOUR {

class Session;

class UserBundle : public Bundle, public PBD::Stateful {

  public:
	UserBundle (std::string const &);
	UserBundle (XMLNode const &, bool);

	uint32_t nchannels () const;
	const ARDOUR::PortList& channel_ports (uint32_t) const;

	void add_channel ();
	void set_channels (uint32_t);
	void remove_channel (uint32_t);
	void add_port_to_channel (uint32_t, std::string const &);
	void remove_port_from_channel (uint32_t, std::string const &);
	bool port_attached_to_channel (uint32_t, std::string const &) const;
	XMLNode& get_state ();

	/// The number of channels is about to change
	sigc::signal<void> ConfigurationWillChange;
	/// The number of channels has changed
	sigc::signal<void> ConfigurationHasChanged;
	/// The port set associated with one of our channels is about to change
	/// Parameter is the channel number
	sigc::signal<void, int> PortsWillChange;
	/// The port set associated with one of our channels has changed
	/// Parameter is the channel number
	sigc::signal<void, int> PortsHaveChanged;
	
  private:

	int set_state (const XMLNode &);

	/// mutex for _ports;
	/// XXX: is this necessary?
	mutable Glib::Mutex _ports_mutex; 
	std::vector<PortList> _ports;
};

}
	
#endif
