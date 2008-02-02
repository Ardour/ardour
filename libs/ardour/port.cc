/*
    Copyright (C) 2002-2006 Paul Davis 

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

#include <ardour/port.h>

using namespace ARDOUR;
using namespace std;

AudioEngine* Port::engine = 0;

Port::Port (const std::string& name, Flags flgs)
	: _flags (flgs)
	, _name (name)
	, _metering (0)
	, _last_monitor (false)
{
}

Port::~Port ()
{
	disconnect_all ();
}

void
Port::reset ()
{
	_last_monitor = false;
}

void
Port::set_engine (AudioEngine* e) 
{
	engine = e;
}

int
Port::connect (Port& other)
{
	/* caller must hold process lock */

	pair<set<Port*>::iterator,bool> result;

	result = _connections.insert (&other);

	if (result.second) {
		return 0;
	} else {
		return 1;
	}
}

int
Port::disconnect (Port& other)
{
	/* caller must hold process lock */
	
	for (set<Port*>::iterator i = _connections.begin(); i != _connections.end(); ++i) {
		if ((*i) == &other) {
			_connections.erase (i);
			return 0;
		}
	}

	return -1;
}


int
Port::disconnect_all ()
{
	/* caller must hold process lock */

	_connections.clear ();
	return 0;
}

void
Port::set_latency (nframes_t val)
{
	_latency = val;
}

bool
Port::connected() const
{
	/* caller must hold process lock */
	return !_connections.empty();
}

bool
Port::connected_to (const string& portname) const
{
	/* caller must hold process lock */

	for (set<Port*>::const_iterator p = _connections.begin(); p != _connections.end(); ++p) {
		if ((*p)->name() == portname) {
			return true;
		}
	}

	return false;
}

int
Port::get_connections (vector<string>& names) const
{
	/* caller must hold process lock */
	int i = 0;
	set<Port*>::const_iterator p;

	for (i = 0, p = _connections.begin(); p != _connections.end(); ++p, ++i) {
		names.push_back ((*p)->name());
	}

	return i;
}


//-------------------------------------

int
PortFacade::set_name (const std::string& str)
{
	int ret;

	if (_ext_port) {
		if ((ret = _ext_port->set_name (str)) == 0) {
			_name = _ext_port->name();
		}
	} else {
		_name = str;
		ret = 0;
	}

	return ret;
}

string
PortFacade::short_name ()  const
{
	if (_ext_port) {
		return _ext_port->short_name(); 
	} else {
		return _name;
	}
}


int
PortFacade::reestablish ()
{
	if (_ext_port) {
		return _ext_port->reestablish ();
	} else {
		return 0;
	}
}


int
PortFacade::reconnect()
{
	if (_ext_port) {
		return _ext_port->reconnect ();
	} else {
		return 0;
	}
}

void
PortFacade::set_latency (nframes_t val)
{
	if (_ext_port) {
		_ext_port->set_latency (val);
	} else {
		_latency = val;
	}
}

nframes_t
PortFacade::latency() const
{
	if (_ext_port) {
		return _ext_port->latency();
	} else {
		return _latency;
	}
}

nframes_t
PortFacade::total_latency() const
{
	if (_ext_port) {
		return _ext_port->total_latency();
	} else {
		return _latency;
	}
}

bool
PortFacade::monitoring_input() const
{
	if (_ext_port) {
		return _ext_port->monitoring_input ();
	} else {
		return false;
	}
}

void
PortFacade::ensure_monitor_input (bool yn)
{
	if (_ext_port) {
		_ext_port->ensure_monitor_input (yn);
	}
}

void
PortFacade::request_monitor_input (bool yn)
{
	if (_ext_port) {
		_ext_port->request_monitor_input (yn);
	} 
}

int
PortFacade::connect (Port& other)
{
	int ret;
	
	if (_ext_port) {
		ret = _ext_port->connect (other);
	} else {
		ret = 0;
	}

	if (ret == 0) {
		ret = Port::connect (other);
	}

	return ret;
}

int
PortFacade::connect (const std::string& other)
{
	PortConnectableByName* pcn;

	if (!_ext_port) {
		return -1;
	}
		
	pcn = dynamic_cast<PortConnectableByName*>(_ext_port);

	if (pcn) {
		return pcn->connect (other);
	} else {
		return -1;
	}
}


int
PortFacade::disconnect (Port& other)
{
	int reta;
	int retb;
	
	if (_ext_port) {
		reta = _ext_port->disconnect (other);
	} else {
		reta = 0;
	}

	retb = Port::disconnect (other);

	return reta || retb;
}

int 
PortFacade::disconnect_all ()
{
	int reta = 0;
	int retb = 0;

	if (_ext_port) {
		reta = _ext_port->disconnect_all ();
	} 

	retb = Port::disconnect_all ();

	return reta || retb;
}
		
int
PortFacade::disconnect (const std::string& other)
{
	PortConnectableByName* pcn;

	if (!_ext_port) {
		return -1;
	}
		
	pcn = dynamic_cast<PortConnectableByName*>(_ext_port);

	if (pcn) {
		return pcn->disconnect (other);
	} else {
		return -1;
	}
}

bool
PortFacade::connected () const 
{
	if (Port::connected()) {
		return true;
	}

	if (_ext_port) {
		return _ext_port->connected();
	}

	return false;
}
bool
PortFacade::connected_to (const std::string& portname) const 
{
	if (Port::connected_to (portname)) {
		return true;
	}

	if (_ext_port) {
		return _ext_port->connected_to (portname);
	}

	return false;

}

int
PortFacade::get_connections (vector<string>& names) const 
{
	int i = 0;

	if (_ext_port) {
		i = _ext_port->get_connections (names);
	}

	i += Port::get_connections (names);

	return i;
}

void
PortFacade::reset ()
{
	Port::reset ();

	if (_ext_port) {
		_ext_port->reset ();
	}
}
