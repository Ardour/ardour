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

#ifndef __ardour_connection_h__
#define __ardour_connection_h__

#include <vector>
#include <string>
#include <sigc++/signal.h>
#include <glibmm/thread.h>
#include <pbd/stateful.h> 

using std::vector;
using std::string;

namespace ARDOUR {

class Connection : public Stateful, public sigc::trackable {
  public:
	Connection (string name, bool sdep = false) : _name (name), _sysdep(sdep) {}
	~Connection() {}

	typedef vector<string> PortList;

	void set_name (string name, void *src);
	string name() const { return _name; }
	
	bool system_dependent() const { return _sysdep; }

	uint32_t nports () const { return _ports.size(); }
	const PortList& port_connections (int port) const;

	void add_connection (int port, string portname);
	void remove_connection (int port, string portname);
	
	void add_port ();
	void remove_port (int port);
	void clear ();

	sigc::signal<void,void*> NameChanged;
	sigc::signal<void>       ConfigurationChanged;
	sigc::signal<void,int>   ConnectionsChanged;

	bool operator==(const Connection& other) const;

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

  protected:
	Connection (const XMLNode&);

  private:
	mutable Glib::Mutex port_lock;
	vector<PortList> _ports;
	string _name;
	bool   _sysdep;

	int set_connections (const string& str);
	int parse_io_string (const string& str, vector<string>& ports);
};

class InputConnection : public Connection {
  public:
	InputConnection (string name, bool sdep = false) : Connection (name, sdep) {}
	InputConnection (const XMLNode&);
};

class OutputConnection : public Connection {
  public:
	OutputConnection (string name, bool sdep = false) : Connection (name, sdep) {}
	OutputConnection (const XMLNode&);
};

}

#endif /* __ardour_connection_h__ */

