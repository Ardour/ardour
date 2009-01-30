/*
    Copyright (C) 2009 Paul Davis 

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

#include "ardour/port.h"
#include "ardour/audioengine.h"
#include "ardour/i18n.h"
#include "pbd/failed_constructor.h"
#include "pbd/error.h"
#include "pbd/compose.h"
#include <stdexcept>

using namespace std;
using namespace ARDOUR;

AudioEngine* Port::_engine = 0;

/** @param n Port short name */
Port::Port (std::string const & n, DataType t, Flags f, bool e) 
	: _jack_port (0)
	, _last_monitor (false)
	, _latency (0)
	, _name (n)
	, _flags (f)
{

	/* Unfortunately we have to pass the DataType into this constructor so that we can
	   create the right kind of JACK port; aside from this we'll use the virtual function type ()
	   to establish type. 
	*/

	assert (_name.find_first_of (':') == std::string::npos);
	
	if (e) {
		try {
			cerr << "NEW PORT " << _name << " ext = " << e << endl;
			do_make_external (t);
		}
		catch (...) {
			throw failed_constructor ();
		}
	}
}

/** Port destructor */
Port::~Port ()
{
	if (_jack_port) {
		jack_port_unregister (_engine->jack (), _jack_port);
	}
}

/** Make this port externally visible by setting it up to use a JACK port.
 * @param t Data type, so that we can call this method from the constructor.
 */
void
Port::do_make_external (DataType t)
{
	if (_jack_port) {
		/* already external */
		return;
	}
	
	if ((_jack_port = jack_port_register (_engine->jack (), _name.c_str (), t.to_jack_type (), _flags, 0)) == 0) {
		throw std::runtime_error ("Could not register JACK port");
	}
}

void
Port::make_external ()
{
	do_make_external (type ());
}

/** @return true if this port is connected to anything */
bool
Port::connected () const
{
	if (!_connections.empty ()) {
		/* connected to a Port* */
		return true;
	}

	if (_jack_port == 0) {
		/* not using a JACK port, so can't be connected to anything else */
		return false;
	}
	
	return (jack_port_connected (_jack_port) != 0);
}

/** @return true if this port is connected to anything via an external port */
bool
Port::externally_connected () const
{
	if (_jack_port == 0) {
		/* not using a JACK port, so can't be connected to anything else */
		return false;
	}
	
	return (jack_port_connected (_jack_port) != 0);
}

int
Port::disconnect_all ()
{
	/* Disconnect from Port* connections */
	for (std::set<Port*>::iterator i = _connections.begin (); i != _connections.end (); ++i) {
		(*i)->_connections.erase (this);
	}

	_connections.clear ();

	/* And JACK connections */
	jack_port_disconnect (_engine->jack(), _jack_port);
	_named_connections.clear ();

	check_buffer_status ();

	return 0;
}

/** @param o Port name
 * @return true if this port is connected to o, otherwise false.
 */
bool
Port::connected_to (std::string const & o) const
{
	std::string const full = _engine->make_port_name_non_relative (o);
	std::string const shrt = _engine->make_port_name_non_relative (o);
	
	if (_jack_port && jack_port_connected_to (_jack_port, full.c_str ())) {
		/* connected via JACK */
		return true;
	}

	for (std::set<Port*>::iterator i = _connections.begin (); i != _connections.end (); ++i) {
		if ((*i)->name () == shrt) {
			/* connected internally */
			return true;
		}
	}

	return false;
}

/** @param o Filled in with port full names of ports that we are connected to */
int
Port::get_connections (std::vector<std::string> & c) const
{
	int n = 0;

	/* JACK connections */
	if (_jack_port) {
		const char** jc = jack_port_get_connections (_jack_port);
		if (jc) {
			for (int i = 0; jc[i]; ++i) {
				c.push_back (jc[i]);
				++n;
			}
		}
	}

	/* Internal connections */
	for (std::set<Port*>::iterator i = _connections.begin (); i != _connections.end (); ++i) {
		std::string const full = _engine->make_port_name_non_relative ((*i)->name());
		c.push_back (full);
		++n;
	}

	return n;
}

int
Port::connect (std::string const & other)
{
	/* caller must hold process lock */

	std::string const other_shrt = _engine->make_port_name_non_relative (other);
	
	Port* p = _engine->get_port_by_name_locked (other_shrt);
	
	int r;
	
	if (p && !p->external ()) {
		/* non-external Ardour port; connect using Port* */
		r = connect (p);
	} else {
		/* connect using name */

		/* for this to work, we must be an external port */
		if (!external ()) {
			make_external ();
		}

		std::string const this_shrt = _engine->make_port_name_non_relative (_name);

		if (sends_output ()) {
			r = jack_connect (_engine->jack (), this_shrt.c_str (), other_shrt.c_str ());
		} else {
			r = jack_connect (_engine->jack (), other_shrt.c_str (), this_shrt.c_str());
		}

		if (r == 0) {
			_named_connections.insert (other);
		}
	}

	check_buffer_status ();

	return r;
}

int
Port::disconnect (std::string const & other)
{
	/* caller must hold process lock */

	std::string const other_shrt = _engine->make_port_name_non_relative (other);
	
	Port* p = _engine->get_port_by_name_locked (other_shrt);
	int r;

	if (p && !p->external ()) {
		/* non-external Ardour port; disconnect using Port* */
		r = disconnect (p);
	} else {
		/* disconnect using name */

		std::string const this_shrt = _engine->make_port_name_non_relative (_name);

		if (sends_output ()) {
			r = jack_disconnect (_engine->jack (), this_shrt.c_str (), other_shrt.c_str ());
		} else {
			r = jack_disconnect (_engine->jack (), other_shrt.c_str (), this_shrt.c_str ());
		}

		if (r == 0) {
			_named_connections.erase (other);
		}

		check_buffer_status ();
	}

	return r;
}


bool
Port::connected_to (Port* o) const
{
	return connected_to (o->name ());
}

int
Port::connect (Port* o)
{
	/* caller must hold process lock */

	if (external () && o->external ()) {
		/* we're both external; connect using name */
		return connect (o->name ());
	}

	/* otherwise connect by Port* */
	_connections.insert (o);
	o->_connections.insert (this);

	check_buffer_status ();
	o->check_buffer_status ();

	return 0;
}

int
Port::disconnect (Port* o)
{
	if (external () && o->external ()) {
		/* we're both external; try disconnecting using name */
		int const r = disconnect (o->name ());
		if (r == 0) {
			return 0;
		}
	}
	
	_connections.erase (o);
	o->_connections.erase (this);

	check_buffer_status ();
	o->check_buffer_status ();

	return 0;
}

void
Port::set_engine (AudioEngine* e)
{
	_engine = e;
}

void
Port::ensure_monitor_input (bool yn)
{
	if (_jack_port) {
		jack_port_ensure_monitor (_jack_port, yn);
	}
}

bool
Port::monitoring_input () const
{
	if (_jack_port) {
		return jack_port_monitoring_input (_jack_port);
	} else {
		return false;
	}
}

void
Port::reset ()
{
	_last_monitor = false;

	// XXX
	// _metering = 0;
	// reset_meters ();
}

void
Port::recompute_total_latency () const
{
#ifdef HAVE_JACK_RECOMPUTE_LATENCY	
	if (_jack_port) {
		jack_recompute_total_latency (_engine->jack (), _jack_port);
	}
#endif	
}

nframes_t
Port::total_latency () const
{
	if (_jack_port) {
		return jack_port_get_total_latency (_engine->jack (), _jack_port);
	} else {
		return _latency;
	}
}

int
Port::reestablish ()
{
	if (!_jack_port) {
		return 0;
	}

	_jack_port = jack_port_register (_engine->jack(), _name.c_str(), type().to_jack_type(), _flags, 0);

	if (_jack_port == 0) {
		PBD::error << string_compose (_("could not reregister %1"), _name) << endmsg;
		return -1;
	}

	reset ();

	return 0;
}


int
Port::reconnect ()
{
	/* caller must hold process lock; intended to be used only after reestablish() */

	if (!_jack_port) {
		return 0;
	}
	
	for (std::set<string>::iterator i = _named_connections.begin(); i != _named_connections.end(); ++i) {
		if (connect (*i)) {
			return -1;
		}
	}

	return 0;
}

/** @param n Short name */
int
Port::set_name (std::string const & n)
{
	assert (_name.find_first_of (':') == std::string::npos);

	int r = 0;
	
	if (_jack_port) {
		r = jack_port_set_name (_jack_port, n.c_str());
		if (r == 0) {
			_name = n;
		}
	} else {
		_name = n;
	}

	return r;
}

void
Port::set_latency (nframes_t n)
{
	_latency = n;
}

void
Port::request_monitor_input (bool yn)
{
	if (_jack_port) {
		jack_port_request_monitor (_jack_port, yn);
	}
}

void
Port::check_buffer_status ()
{
	if (external() && receives_input()) {
		if (!externally_connected()) {
			if (!_connections.empty()) {

				/* There are no external connections, so the
				   external port buffer will be the silent buffer. We cannot write into it.
				   But we have to write somewhere because there is at least one internal
				   connection that is supplying us with data.
				*/

				if (!using_internal_data()) {
					use_internal_data ();
				}

			} else {
				
				/* There are no external connections and no internal ones
				   either, so we can revert to use the externally supplied
				   buffer which will be silent (whatever the semantics of 
				   that are for a particular data type.
				*/

				if (using_internal_data()) {
					use_external_data ();
				}
			}
		} 
	}
}
