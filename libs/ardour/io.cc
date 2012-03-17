/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <fstream>
#include <algorithm>
#include <cmath>

#include <unistd.h>
#include <locale.h>
#include <errno.h>

#include <glibmm.h>
#include <glibmm/thread.h>

#include "pbd/xml++.h"
#include "pbd/replace_all.h"
#include "pbd/unknown_type.h"
#include "pbd/enumwriter.h"

#include "ardour/audioengine.h"
#include "ardour/buffer.h"
#include "ardour/debug.h"
#include "ardour/io.h"
#include "ardour/route.h"
#include "ardour/port.h"
#include "ardour/audio_port.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/cycle_timer.h"
#include "ardour/buffer_set.h"
#include "ardour/meter.h"
#include "ardour/amp.h"
#include "ardour/user_bundle.h"

#include "i18n.h"

#define BLOCK_PROCESS_CALLBACK() Glib::Mutex::Lock em (AudioEngine::instance()->process_lock())

using namespace std;
using namespace ARDOUR;
using namespace PBD;

const string                 IO::state_node_name = "IO";
bool                         IO::connecting_legal = false;
PBD::Signal0<int>            IO::ConnectingLegal;
PBD::Signal1<void,ChanCount> IO::PortCountChanged;

/** @param default_type The type of port that will be created by ensure_io
 * and friends if no type is explicitly requested (to avoid breakage).
 */
IO::IO (Session& s, const string& name, Direction dir, DataType default_type)
	: SessionObject (s, name)
	, _direction (dir)
	, _default_type (default_type)
{
	_active = true;
	Port::PostDisconnect.connect_same_thread (*this, boost::bind (&IO::disconnect_check, this, _1, _2));
	pending_state_node = 0;
	setup_bundle ();
}

IO::IO (Session& s, const XMLNode& node, DataType dt)
	: SessionObject(s, "unnamed io")
	, _direction (Input)
	, _default_type (dt)
{
	_active = true;
	pending_state_node = 0;
	Port::PostDisconnect.connect_same_thread (*this, boost::bind (&IO::disconnect_check, this, _1, _2));

	set_state (node, Stateful::loading_state_version);
	setup_bundle ();
}

IO::~IO ()
{
	Glib::Mutex::Lock lm (io_lock);

	BLOCK_PROCESS_CALLBACK ();

	for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {
		_session.engine().unregister_port (*i);
	}
}

void
IO::disconnect_check (boost::shared_ptr<Port> a, boost::shared_ptr<Port> b)
{
	/* this could be called from within our own ::disconnect() method(s)
	   or from somewhere that operates directly on a port. so, we don't
	   know for sure if we can take this lock or not. if we fail,
	   we assume that its safely locked by our own ::disconnect().
	*/

	Glib::Mutex::Lock tm (io_lock, Glib::TRY_LOCK);

	if (tm.locked()) {
		/* we took the lock, so we cannot be here from inside
		 * ::disconnect()
		 */
		if (_ports.contains (a) || _ports.contains (b)) {
			changed (IOChange (IOChange::ConnectionsChanged), this); /* EMIT SIGNAL */		
		}
	} else {
		/* we didn't get the lock, so assume that we're inside
		 * ::disconnect(), and it will call changed() appropriately.
		 */
	}
}

void
IO::increment_port_buffer_offset (pframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

        if (_direction == Output) {
                for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {
                        i->increment_port_buffer_offset (offset);
                }
        }
}

void
IO::silence (framecnt_t nframes)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {
		i->get_buffer(nframes).silence (nframes);
	}
}

/** Set _bundles_connected to those bundles that are connected such that every
 *  port on every bundle channel x is connected to port x in _ports.
 */
void
IO::check_bundles_connected ()
{
	std::vector<UserBundleInfo*> new_list;

	for (std::vector<UserBundleInfo*>::iterator i = _bundles_connected.begin(); i != _bundles_connected.end(); ++i) {

		uint32_t const N = (*i)->bundle->nchannels().n_total();

		if (_ports.num_ports() < N) {
			continue;
		}

		bool ok = true;

		for (uint32_t j = 0; j < N; ++j) {
			/* Every port on bundle channel j must be connected to our input j */
			Bundle::PortList const pl = (*i)->bundle->channel_ports (j);
			for (uint32_t k = 0; k < pl.size(); ++k) {
				if (_ports.port(j)->connected_to (pl[k]) == false) {
					ok = false;
					break;
				}
			}

			if (ok == false) {
				break;
			}
		}

		if (ok) {
			new_list.push_back (*i);
		} else {
			delete *i;
		}
	}

	_bundles_connected = new_list;
}


int
IO::disconnect (boost::shared_ptr<Port> our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

        {
                Glib::Mutex::Lock lm (io_lock);

                /* check that our_port is really one of ours */

                if ( ! _ports.contains(our_port)) {
                        return -1;
                }

                /* disconnect it from the source */

                if (our_port->disconnect (other_port)) {
                        error << string_compose(_("IO: cannot disconnect port %1 from %2"), our_port->name(), other_port) << endmsg;
                        return -1;
                }

                check_bundles_connected ();
        }

        changed (IOChange (IOChange::ConnectionsChanged), src); /* EMIT SIGNAL */

	_session.set_dirty ();

	return 0;
}

int
IO::connect (boost::shared_ptr<Port> our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

	{
		Glib::Mutex::Lock lm (io_lock);

		/* check that our_port is really one of ours */

		if ( ! _ports.contains(our_port) ) {
			return -1;
		}

		/* connect it to the source */

		if (our_port->connect (other_port)) {
			return -1;
		}
	}
	changed (IOChange (IOChange::ConnectionsChanged), src); /* EMIT SIGNAL */
	_session.set_dirty ();
	return 0;
}

int
IO::remove_port (boost::shared_ptr<Port> port, void* src)
{
	ChanCount before = _ports.count ();
	ChanCount after = before;
	after.set (port->type(), after.get (port->type()) - 1);

	bool const r = PortCountChanging (after); /* EMIT SIGNAL */
	if (r) {
		return -1;
	}

	IOChange change;

	{
		BLOCK_PROCESS_CALLBACK ();

		{
			Glib::Mutex::Lock lm (io_lock);

			if (_ports.remove(port)) {
				change.type = IOChange::Type (change.type | IOChange::ConfigurationChanged);
				change.before = before;
				change.after = _ports.count ();

				if (port->connected()) {
					change.type = IOChange::Type (change.type | IOChange::ConnectionsChanged);
				}

				_session.engine().unregister_port (port);
				check_bundles_connected ();
			}
		}

		PortCountChanged (n_ports()); /* EMIT SIGNAL */

		if (change.type != IOChange::NoChange) {
			changed (change, src);
			_buffers.attach_buffers (_ports);
		}
	}

	if (change.type & IOChange::ConfigurationChanged) {
		setup_bundle ();
	}

	if (change.type == IOChange::NoChange) {
		return -1;
	}

	_session.set_dirty ();
	
	return 0;
}

/** Add a port.
 *
 * @param destination Name of port to connect new port to.
 * @param src Source for emitted ConfigurationChanged signal.
 * @param type Data type of port.  Default value (NIL) will use this IO's default type.
 */
int
IO::add_port (string destination, void* src, DataType type)
{
	boost::shared_ptr<Port> our_port;

	if (type == DataType::NIL) {
		type = _default_type;
	}

	ChanCount before = _ports.count ();
	ChanCount after = before;
	after.set (type, after.get (type) + 1);
	
	bool const r = PortCountChanging (after); /* EMIT SIGNAL */
	if (r) {
		return -1;
	}
	
	IOChange change;

	{
		BLOCK_PROCESS_CALLBACK ();


		{
			Glib::Mutex::Lock lm (io_lock);

			/* Create a new port */

			string portname = build_legal_port_name (type);
			
			if (_direction == Input) {
				if ((our_port = _session.engine().register_input_port (type, portname)) == 0) {
					error << string_compose(_("IO: cannot register input port %1"), portname) << endmsg;
					return -1;
				}
			} else {
				if ((our_port = _session.engine().register_output_port (type, portname)) == 0) {
					error << string_compose(_("IO: cannot register output port %1"), portname) << endmsg;
					return -1;
				}
			}

			change.before = _ports.count ();
			_ports.add (our_port);
		}
		
		PortCountChanged (n_ports()); /* EMIT SIGNAL */
		change.type = IOChange::ConfigurationChanged;
		change.after = _ports.count ();
		changed (change, src); /* EMIT SIGNAL */
		_buffers.attach_buffers (_ports);
	}

	if (!destination.empty()) {
		if (our_port->connect (destination)) {
			return -1;
		}
	}

	setup_bundle ();
	_session.set_dirty ();

	return 0;
}

int
IO::disconnect (void* src)
{
	{
		Glib::Mutex::Lock lm (io_lock);

		for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {
			i->disconnect_all ();
		}

		check_bundles_connected ();
	}

	changed (IOChange (IOChange::ConnectionsChanged), src); /* EMIT SIGNAL */

	return 0;
}

/** Caller must hold process lock */
int
IO::ensure_ports_locked (ChanCount count, bool clear, bool& changed)
{
	assert (!AudioEngine::instance()->process_lock().trylock());

	boost::shared_ptr<Port> port;

	changed    = false;

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

		const size_t n = count.get(*t);

		/* remove unused ports */
		for (size_t i = n_ports().get(*t); i > n; --i) {
			port = _ports.port(*t, i-1);

			assert(port);
			_ports.remove(port);
			_session.engine().unregister_port (port);

			changed = true;
		}

		/* create any necessary new ports */
		while (n_ports().get(*t) < n) {

			string portname = build_legal_port_name (*t);

			try {

				if (_direction == Input) {
					if ((port = _session.engine().register_input_port (*t, portname)) == 0) {
						error << string_compose(_("IO: cannot register input port %1"), portname) << endmsg;
						return -1;
					}
				} else {
					if ((port = _session.engine().register_output_port (*t, portname)) == 0) {
						error << string_compose(_("IO: cannot register output port %1"), portname) << endmsg;
						return -1;
					}
				}
			}

			catch (AudioEngine::PortRegistrationFailure& err) {
				/* pass it on */
				throw;
			}

			_ports.add (port);
			changed = true;
		}
	}

	if (changed) {
		check_bundles_connected ();
		PortCountChanged (n_ports()); /* EMIT SIGNAL */
		_session.set_dirty ();
	}

	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
		for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {
			i->disconnect_all ();
		}
	}

	return 0;
}

/** Caller must hold process lock */
int
IO::ensure_ports (ChanCount count, bool clear, void* src)
{
	assert (!AudioEngine::instance()->process_lock().trylock());

	bool changed = false;

	if (count == n_ports() && !clear) {
		return 0;
	}

	IOChange change;

	change.before = _ports.count ();

	{
		Glib::Mutex::Lock im (io_lock);
		if (ensure_ports_locked (count, clear, changed)) {
			return -1;
		}
	}

	if (changed) {
		change.after = _ports.count ();
		change.type = IOChange::ConfigurationChanged;
		this->changed (change, src); /* EMIT SIGNAL */
		_buffers.attach_buffers (_ports);
		setup_bundle ();
		_session.set_dirty ();
	}

	return 0;
}

/** Caller must hold process lock */
int
IO::ensure_io (ChanCount count, bool clear, void* src)
{
	assert (!AudioEngine::instance()->process_lock().trylock());

	return ensure_ports (count, clear, src);
}

XMLNode&
IO::get_state ()
{
	return state (true);
}

XMLNode&
IO::state (bool /*full_state*/)
{
	XMLNode* node = new XMLNode (state_node_name);
	char buf[64];
	string str;
	vector<string>::iterator ci;
	int n;
	LocaleGuard lg (X_("POSIX"));
	Glib::Mutex::Lock lm (io_lock);

	node->add_property("name", _name);
	id().print (buf, sizeof (buf));
	node->add_property("id", buf);
	node->add_property ("direction", enum_2_string (_direction));
	node->add_property ("default-type", _default_type.to_string());

	for (std::vector<UserBundleInfo*>::iterator i = _bundles_connected.begin(); i != _bundles_connected.end(); ++i) {
		XMLNode* n = new XMLNode ("Bundle");
		n->add_property ("name", (*i)->bundle->name ());
		node->add_child_nocopy (*n);
	}

	for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {

		vector<string> connections;

		XMLNode* pnode = new XMLNode (X_("Port"));
		pnode->add_property (X_("type"), i->type().to_string());
		pnode->add_property (X_("name"), i->name());

		if (i->get_connections (connections)) {

			for (n = 0, ci = connections.begin(); ci != connections.end(); ++ci, ++n) {

				/* if its a connection to our own port,
				   return only the port name, not the
				   whole thing. this allows connections
				   to be re-established even when our
				   client name is different.
				*/

				XMLNode* cnode = new XMLNode (X_("Connection"));

				cnode->add_property (X_("other"), _session.engine().make_port_name_relative (*ci));
				pnode->add_child_nocopy (*cnode);
			}
		}

		node->add_child_nocopy (*pnode);
	}

	snprintf (buf, sizeof (buf), "%" PRId64, _user_latency);
	node->add_property (X_("user-latency"), buf);
	
	return *node;
}

int
IO::set_state (const XMLNode& node, int version)
{
	/* callers for version < 3000 need to call set_state_2X directly, as A3 IOs
	 * are input OR output, not both, so the direction needs to be specified
	 * by the caller.
	 */
	assert (version >= 3000);

	const XMLProperty* prop;
	XMLNodeConstIterator iter;
	LocaleGuard lg (X_("POSIX"));

	/* force use of non-localized representation of decimal point,
	   since we use it a lot in XML files and so forth.
	*/

	if (node.name() != state_node_name) {
		error << string_compose(_("incorrect XML node \"%1\" passed to IO object"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property ("name")) != 0) {
		set_name (prop->value());
	}

	if ((prop = node.property (X_("default-type"))) != 0) {
		_default_type = DataType(prop->value());
		assert(_default_type != DataType::NIL);
	}

	set_id (node);

	if ((prop = node.property ("direction")) != 0) {
		_direction = (Direction) string_2_enum (prop->value(), _direction);
	}

	if (create_ports (node, version)) {
		return -1;
	}

	if (connecting_legal) {

		if (make_connections (node, version, false)) {
			return -1;
		}

	} else {

		pending_state_node = new XMLNode (node);
		pending_state_node_version = version;
		pending_state_node_in = false;
		ConnectingLegal.connect_same_thread (connection_legal_c, boost::bind (&IO::connecting_became_legal, this));
	}

	if ((prop = node.property ("user-latency")) != 0) {
		_user_latency = atoi (prop->value ());
	}

	return 0;
}

int
IO::set_state_2X (const XMLNode& node, int version, bool in)
{
	const XMLProperty* prop;
	XMLNodeConstIterator iter;
	LocaleGuard lg (X_("POSIX"));

	/* force use of non-localized representation of decimal point,
	   since we use it a lot in XML files and so forth.
	*/

	if (node.name() != state_node_name) {
		error << string_compose(_("incorrect XML node \"%1\" passed to IO object"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property ("name")) != 0) {
		set_name (prop->value());
	}

	if ((prop = node.property (X_("default-type"))) != 0) {
		_default_type = DataType(prop->value());
		assert(_default_type != DataType::NIL);
	}

	set_id (node);

	_direction = in ? Input : Output;

	if (create_ports (node, version)) {
		return -1;
	}

	if (connecting_legal) {

		if (make_connections_2X (node, version, in)) {
			return -1;
		}

	} else {

		pending_state_node = new XMLNode (node);
		pending_state_node_version = version;
		pending_state_node_in = in;
		ConnectingLegal.connect_same_thread (connection_legal_c, boost::bind (&IO::connecting_became_legal, this));
	}

	return 0;
}

int
IO::connecting_became_legal ()
{
	int ret;

	assert (pending_state_node);

	connection_legal_c.disconnect ();

	ret = make_connections (*pending_state_node, pending_state_node_version, pending_state_node_in);

	delete pending_state_node;
	pending_state_node = 0;

	return ret;
}

boost::shared_ptr<Bundle>
IO::find_possible_bundle (const string &desired_name)
{
	static const string digits = "0123456789";
	const string &default_name = (_direction == Input ? _("in") : _("out"));
	const string &bundle_type_name = (_direction == Input ? _("input") : _("output"));

	boost::shared_ptr<Bundle> c = _session.bundle_by_name (desired_name);

	if (!c) {
		int bundle_number, mask;
		string possible_name;
		bool stereo = false;
		string::size_type last_non_digit_pos;

		error << string_compose(_("Unknown bundle \"%1\" listed for %2 of %3"), desired_name, bundle_type_name, _name)
		      << endmsg;

		// find numeric suffix of desired name
		bundle_number = 0;

		last_non_digit_pos = desired_name.find_last_not_of(digits);

		if (last_non_digit_pos != string::npos) {
			stringstream s;
			s << desired_name.substr(last_non_digit_pos);
			s >> bundle_number;
		}

		// see if it's a stereo connection e.g. "in 3+4"

		if (last_non_digit_pos > 1 && desired_name[last_non_digit_pos] == '+') {
			string::size_type left_last_non_digit_pos;

			left_last_non_digit_pos = desired_name.find_last_not_of(digits, last_non_digit_pos-1);

			if (left_last_non_digit_pos != string::npos) {
				int left_bundle_number = 0;
				stringstream s;
				s << desired_name.substr(left_last_non_digit_pos, last_non_digit_pos-1);
				s >> left_bundle_number;

				if (left_bundle_number > 0 && left_bundle_number + 1 == bundle_number) {
					bundle_number--;
					stereo = true;
				}
			}
		}

		// make 0-based
		if (bundle_number)
			bundle_number--;

		// find highest set bit
		mask = 1;
		while ((mask <= bundle_number) && (mask <<= 1)) {}

		// "wrap" bundle number into largest possible power of 2
		// that works...

		while (mask) {

			if (bundle_number & mask) {
				bundle_number &= ~mask;

				stringstream s;
				s << default_name << " " << bundle_number + 1;

				if (stereo) {
					s << "+" << bundle_number + 2;
				}

				possible_name = s.str();

				if ((c = _session.bundle_by_name (possible_name)) != 0) {
					break;
				}
			}
			mask >>= 1;
		}
		if (c) {
			info << string_compose (_("Bundle %1 was not available - \"%2\" used instead"), desired_name, possible_name)
			     << endmsg;
		} else {
			error << string_compose(_("No %1 bundles available as a replacement"), bundle_type_name)
			      << endmsg;
		}

	}

	return c;

}

int
IO::get_port_counts_2X (XMLNode const & node, int /*version*/, ChanCount& n, boost::shared_ptr<Bundle>& /*c*/)
{
	XMLProperty const * prop;
	XMLNodeList children = node.children ();

	uint32_t n_audio = 0;

	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {

		if ((prop = node.property ("inputs")) != 0 && _direction == Input) {
			n_audio = count (prop->value().begin(), prop->value().end(), '{');
		} else if ((prop = node.property ("input-connection")) != 0 && _direction == Input) {
			n_audio = 1;
		} else if ((prop = node.property ("outputs")) != 0 && _direction == Output) {
			n_audio = count (prop->value().begin(), prop->value().end(), '{');
		} else if ((prop = node.property ("output-connection")) != 0 && _direction == Output) {
			n_audio = 2;
		}
	}

	ChanCount cnt;
	cnt.set_audio (n_audio);
	n = ChanCount::max (n, cnt);

	return 0;
}

int
IO::get_port_counts (const XMLNode& node, int version, ChanCount& n, boost::shared_ptr<Bundle>& c)
{
	if (version < 3000) {
		return get_port_counts_2X (node, version, n, c);
	}

	XMLProperty const * prop;
	XMLNodeConstIterator iter;
	uint32_t n_audio = 0;
	uint32_t n_midi = 0;
	ChanCount cnt;

	n = n_ports();

	if ((prop = node.property ("connection")) != 0) {

		if ((c = find_possible_bundle (prop->value())) != 0) {
			n = ChanCount::max (n, c->nchannels());
		}
		return 0;
	}

	for (iter = node.children().begin(); iter != node.children().end(); ++iter) {

		if ((*iter)->name() == X_("Bundle")) {
			if ((c = find_possible_bundle (prop->value())) != 0) {
				n = ChanCount::max (n, c->nchannels());
				return 0;
			} else {
				return -1;
			}
		}

		if ((*iter)->name() == X_("Port")) {
			prop = (*iter)->property (X_("type"));

			if (!prop) {
				continue;
			}

			if (prop->value() == X_("audio")) {
				cnt.set_audio (++n_audio);
			} else if (prop->value() == X_("midi")) {
				cnt.set_midi (++n_midi);
			}
		}
	}

	n = ChanCount::max (n, cnt);
	return 0;
}

int
IO::create_ports (const XMLNode& node, int version)
{
	ChanCount n;
	boost::shared_ptr<Bundle> c;

	get_port_counts (node, version, n, c);

	{
		Glib::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

		if (ensure_ports (n, true, this)) {
			error << string_compose(_("%1: cannot create I/O ports"), _name) << endmsg;
			return -1;
		}
	}

	/* XXX use c */

	return 0;
}

int
IO::make_connections (const XMLNode& node, int version, bool in)
{
	if (version < 3000) {
		return make_connections_2X (node, version, in);
	}

	const XMLProperty* prop;

	for (XMLNodeConstIterator i = node.children().begin(); i != node.children().end(); ++i) {

		if ((*i)->name() == "Bundle") {
			XMLProperty const * prop = (*i)->property ("name");
			if (prop) {
				boost::shared_ptr<Bundle> b = find_possible_bundle (prop->value());
				if (b) {
					connect_ports_to_bundle (b, this);
				}
			}

			return 0;
		}

		if ((*i)->name() == "Port") {

			prop = (*i)->property (X_("name"));

			if (!prop) {
				continue;
			}

			boost::shared_ptr<Port> p = port_by_name (prop->value());

			if (p) {
				for (XMLNodeConstIterator c = (*i)->children().begin(); c != (*i)->children().end(); ++c) {

					XMLNode* cnode = (*c);

					if (cnode->name() != X_("Connection")) {
						continue;
					}

					if ((prop = cnode->property (X_("other"))) == 0) {
						continue;
					}

					if (prop) {
                                                connect (p, prop->value(), this);
					}
				}
			}
		}
	}

	return 0;
}

void
IO::prepare_for_reset (XMLNode& node, const std::string& name)
{
	/* reset name */
	node.add_property ("name", name);

	/* now find connections and reset the name of the port
	   in one so that when we re-use it it will match
	   the name of the thing we're applying it to.
	*/

	XMLProperty* prop;
	XMLNodeList children = node.children();

	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() == "Port") {
			
			prop = (*i)->property (X_("name"));
			
			if (prop) {
				string new_name;
				string old = prop->value();
				string::size_type slash = old.find ('/');

				if (slash != string::npos) {
					/* port name is of form: <IO-name>/<port-name> */
					
					new_name = name;
					new_name += old.substr (old.find ('/'));
					
					prop->set_value (new_name);
				}
			}
		}
	}
}


int
IO::make_connections_2X (const XMLNode& node, int /*version*/, bool in)
{
	const XMLProperty* prop;

	/* XXX: bundles ("connections" as was) */

	if ((prop = node.property ("inputs")) != 0 && in) {

		string::size_type ostart = 0;
		string::size_type start = 0;
		string::size_type end = 0;
		int i = 0;
		int n;
		vector<string> ports;

		string const str = prop->value ();

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
					/* XXX: this is a bit of a hack; need to check if it's always valid */
					string::size_type const p = ports[x].find ("/out");
					if (p != string::npos) {
						ports[x].replace (p, 4, "/audio_out");
					}
					nth(i)->connect (ports[x]);
				}
			}

			ostart = end+1;
			i++;
		}

	}

	if ((prop = node.property ("outputs")) != 0 && !in) {

		string::size_type ostart = 0;
		string::size_type start = 0;
		string::size_type end = 0;
		int i = 0;
		int n;
		vector<string> ports;

		string const str = prop->value ();

		while ((start = str.find_first_of ('{', ostart)) != string::npos) {
			start += 1;

			if ((end = str.find_first_of ('}', start)) == string::npos) {
				error << string_compose(_("IO: badly formed string in XML node for outputs \"%1\""), str) << endmsg;
				return -1;
			}

			if ((n = parse_io_string (str.substr (start, end - start), ports)) < 0) {
				error << string_compose(_("IO: bad output string in XML node \"%1\""), str) << endmsg;

				return -1;

			} else if (n > 0) {

				for (int x = 0; x < n; ++x) {
					/* XXX: this is a bit of a hack; need to check if it's always valid */
					string::size_type const p = ports[x].find ("/in");
					if (p != string::npos) {
						ports[x].replace (p, 3, "/audio_in");
					}
					nth(i)->connect (ports[x]);
				}
			}

			ostart = end+1;
			i++;
		}
	}

	return 0;
}

int
IO::set_ports (const string& str)
{
	vector<string> ports;
	int i;
	int n;
	uint32_t nports;

	if ((nports = count (str.begin(), str.end(), '{')) == 0) {
		return 0;
	}

	{
		Glib::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

		// FIXME: audio-only
		if (ensure_ports (ChanCount(DataType::AUDIO, nports), true, this)) {
			return -1;
		}
	}

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
				connect (nth (i), ports[x], this);
			}
		}

		ostart = end+1;
		i++;
	}

	return 0;
}

int
IO::parse_io_string (const string& str, vector<string>& ports)
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

int
IO::parse_gain_string (const string& str, vector<string>& ports)
{
	string::size_type pos, opos;

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

bool
IO::set_name (const string& requested_name)
{
	string name = requested_name;

	if (_name == name) {
		return true;
	}

	/* replace all colons in the name. i wish we didn't have to do this */

	replace_all (name, ":", "-");

	for (PortSet::iterator i = _ports.begin(); i != _ports.end(); ++i) {
		string current_name = i->name();
		current_name.replace (current_name.find (_name), _name.val().length(), name);
		i->set_name (current_name);
	}

	bool const r = SessionObject::set_name (name);

	setup_bundle ();

	return r;
}

framecnt_t
IO::latency () const
{
	framecnt_t max_latency;
	framecnt_t latency;

	max_latency = 0;

	/* io lock not taken - must be protected by other means */

	for (PortSet::const_iterator i = _ports.begin(); i != _ports.end(); ++i) {
		if ((latency = i->private_latency_range (_direction == Output).max) > max_latency) {
                        DEBUG_TRACE (DEBUG::Latency, string_compose ("port %1 has %2 latency of %3 - use\n",
                                                                     name(),
                                                                     ((_direction == Output) ? "PLAYBACK" : "CAPTURE"),
                                                                     latency));
			max_latency = latency;
		}
	}

        DEBUG_TRACE (DEBUG::Latency, string_compose ("%1: max %4 latency from %2 ports = %3\n",
                                                     name(), _ports.num_ports(), max_latency,
                                                     ((_direction == Output) ? "PLAYBACK" : "CAPTURE")));
	return max_latency;
}

int
IO::connect_ports_to_bundle (boost::shared_ptr<Bundle> c, void* src)
{
	BLOCK_PROCESS_CALLBACK ();

	{
		Glib::Mutex::Lock lm2 (io_lock);

		c->connect (_bundle, _session.engine());

		/* If this is a UserBundle, make a note of what we've done */

		boost::shared_ptr<UserBundle> ub = boost::dynamic_pointer_cast<UserBundle> (c);
		if (ub) {

			/* See if we already know about this one */
			std::vector<UserBundleInfo*>::iterator i = _bundles_connected.begin();
			while (i != _bundles_connected.end() && (*i)->bundle != ub) {
				++i;
			}

			if (i == _bundles_connected.end()) {
				/* We don't, so make a note */
				_bundles_connected.push_back (new UserBundleInfo (this, ub));
			}
		}
	}

	changed (IOChange (IOChange::ConnectionsChanged), src); /* EMIT SIGNAL */
	return 0;
}

int
IO::disconnect_ports_from_bundle (boost::shared_ptr<Bundle> c, void* src)
{
	BLOCK_PROCESS_CALLBACK ();

	{
		Glib::Mutex::Lock lm2 (io_lock);

		c->disconnect (_bundle, _session.engine());

		/* If this is a UserBundle, make a note of what we've done */

		boost::shared_ptr<UserBundle> ub = boost::dynamic_pointer_cast<UserBundle> (c);
		if (ub) {

			std::vector<UserBundleInfo*>::iterator i = _bundles_connected.begin();
			while (i != _bundles_connected.end() && (*i)->bundle != ub) {
				++i;
			}

			if (i != _bundles_connected.end()) {
				delete *i;
				_bundles_connected.erase (i);
			}
		}
	}

	changed (IOChange (IOChange::ConnectionsChanged), src); /* EMIT SIGNAL */
	return 0;
}


int
IO::disable_connecting ()
{
	connecting_legal = false;
	return 0;
}

int
IO::enable_connecting ()
{
	Glib::Mutex::Lock lm (AudioEngine::instance()->process_lock());
	connecting_legal = true;
	boost::optional<int> r = ConnectingLegal ();
	return r.get_value_or (0);
}

void
IO::bundle_changed (Bundle::Change /*c*/)
{
	/* XXX */
//	connect_input_ports_to_bundle (_input_bundle, this);
}


string
IO::build_legal_port_name (DataType type)
{
	const int name_size = jack_port_name_size();
	int limit;
	string suffix;

	if (type == DataType::AUDIO) {
		suffix = _("audio");
	} else if (type == DataType::MIDI) {
		suffix = _("midi");
	} else {
		throw unknown_type();
	}

	/* note that if "in" or "out" are translated it will break a session
	   across locale switches because a port's connection list will
	   show (old) translated names, but the current port name will
	   use the (new) translated name.
	*/

	if (_direction == Input) {
		suffix += X_("_in");
	} else {
		suffix += X_("_out");
	}

	// allow up to 4 digits for the output port number, plus the slash, suffix and extra space

	limit = name_size - _session.engine().client_name().length() - (suffix.length() + 5);

	char buf1[name_size+1];
	char buf2[name_size+1];

	/* colons are illegal in port names, so fix that */

	string nom = _name.val();
	replace_all (nom, ":", ";");

	snprintf (buf1, name_size+1, ("%.*s/%s"), limit, nom.c_str(), suffix.c_str());

	int port_number = find_port_hole (buf1);
	snprintf (buf2, name_size+1, "%s %d", buf1, port_number);

	return string (buf2);
}

int32_t
IO::find_port_hole (const char* base)
{
	/* CALLER MUST HOLD IO LOCK */

	uint32_t n;

	if (_ports.empty()) {
		return 1;
	}

	/* we only allow up to 4 characters for the port number
	 */

	for (n = 1; n < 9999; ++n) {
		char buf[jack_port_name_size()];
		PortSet::iterator i = _ports.begin();

		snprintf (buf, jack_port_name_size(), _("%s %u"), base, n);

		for ( ; i != _ports.end(); ++i) {
			if (i->name() == buf) {
				break;
			}
		}

		if (i == _ports.end()) {
			break;
		}
	}
	return n;
}


boost::shared_ptr<AudioPort>
IO::audio(uint32_t n) const
{
	return _ports.nth_audio_port (n);

}

boost::shared_ptr<MidiPort>
IO::midi(uint32_t n) const
{
	return _ports.nth_midi_port (n);
}

/**
 *  Setup a bundle that describe our inputs or outputs. Also creates the bundle if necessary.
 */

void
IO::setup_bundle ()
{
        char buf[32];

	if (!_bundle) {
		_bundle.reset (new Bundle (_direction == Input));
	}

	_bundle->suspend_signals ();

	_bundle->remove_channels ();

	if (_direction == Input) {
		snprintf(buf, sizeof (buf), _("%s in"), _name.val().c_str());
	} else {
		snprintf(buf, sizeof (buf), _("%s out"), _name.val().c_str());
	}
        _bundle->set_name (buf);

	int c = 0;
	for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {

		uint32_t const N = _ports.count().get (*i);
		for (uint32_t j = 0; j < N; ++j) {
			_bundle->add_channel (bundle_channel_name (j, N, *i), *i);
			_bundle->set_port (c, _session.engine().make_port_name_non_relative (_ports.port(*i, j)->name()));
			++c;
		}

	}

	_bundle->resume_signals ();
}

/** @return Bundles connected to our ports */
BundleList
IO::bundles_connected ()
{
	BundleList bundles;

	/* User bundles */
	for (std::vector<UserBundleInfo*>::iterator i = _bundles_connected.begin(); i != _bundles_connected.end(); ++i) {
		bundles.push_back ((*i)->bundle);
	}

	/* Session bundles */
	boost::shared_ptr<ARDOUR::BundleList> b = _session.bundles ();
	for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
		if ((*i)->connected_to (_bundle, _session.engine())) {
			bundles.push_back (*i);
		}
	}

	/* Route bundles */

	boost::shared_ptr<ARDOUR::RouteList> r = _session.get_routes ();

	if (_direction == Input) {
		for (ARDOUR::RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			if ((*i)->output()->bundle()->connected_to (_bundle, _session.engine())) {
				bundles.push_back ((*i)->output()->bundle());
			}
		}
	} else {
		for (ARDOUR::RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			if ((*i)->input()->bundle()->connected_to (_bundle, _session.engine())) {
				bundles.push_back ((*i)->input()->bundle());
			}
		}
	}

	return bundles;
}


IO::UserBundleInfo::UserBundleInfo (IO* io, boost::shared_ptr<UserBundle> b)
{
	bundle = b;
	b->Changed.connect_same_thread (changed, boost::bind (&IO::bundle_changed, io, _1));
}

std::string
IO::bundle_channel_name (uint32_t c, uint32_t n, DataType t) const
{
	char buf[32];

	if (t == DataType::AUDIO) {

		switch (n) {
		case 1:
			return _("mono");
		case 2:
			return c == 0 ? _("L") : _("R");
		default:
			snprintf (buf, sizeof(buf), _("%d"), (c + 1));
			return buf;
		}

	} else {

		snprintf (buf, sizeof(buf), _("%d"), (c + 1));
		return buf;

	}

	return "";
}

string
IO::name_from_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("name")) != 0) {
		return prop->value();
	}

	return string();
}

void
IO::set_name_in_state (XMLNode& node, const string& new_name)
{
	node.add_property (X_("name"), new_name);
	XMLNodeList children = node.children ();
	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("Port")) {
			string const old_name = (*i)->property(X_("name"))->value();
			string const old_name_second_part = old_name.substr (old_name.find_first_of ("/") + 1);
			(*i)->add_property (X_("name"), string_compose ("%1/%2", new_name, old_name_second_part));
		}
	}
}

bool
IO::connected () const
{
        /* do we have any connections at all? */

        for (PortSet::const_iterator p = _ports.begin(); p != _ports.end(); ++p) {
                if (p->connected()) {
                        return true;
                }
        }

        return false;
}

bool
IO::connected_to (boost::shared_ptr<const IO> other) const
{
	if (!other) {
                return connected ();
	}

	assert (_direction != other->direction());

	uint32_t i, j;
	uint32_t no = n_ports().n_total();
	uint32_t ni = other->n_ports ().n_total();

	for (i = 0; i < no; ++i) {
		for (j = 0; j < ni; ++j) {
			if (nth(i)->connected_to (other->nth(j)->name())) {
				return true;
			}
		}
	}

	return false;
}

bool
IO::connected_to (const string& str) const
{
	for (PortSet::const_iterator i = _ports.begin(); i != _ports.end(); ++i) {
		if (i->connected_to (str)) {
			return true;
		}
	}

	return false;
}

/** Call a processor's ::run() method, giving it our buffers
 *  Caller must hold process lock.
 */
void
IO::process_input (boost::shared_ptr<Processor> proc, framepos_t start_frame, framepos_t end_frame, pframes_t nframes)
{
	/* don't read the data into new buffers - just use the port buffers directly */

	if (n_ports().n_total() == 0) {
		/* We have no ports, so nothing to process */
		return;
	}

	_buffers.get_jack_port_addresses (_ports, nframes);
	proc->run (_buffers, start_frame, end_frame, nframes, true);
}

void
IO::collect_input (BufferSet& bufs, pframes_t nframes, ChanCount offset)
{
	assert(bufs.available() >= _ports.count());

	if (_ports.count() == ChanCount::ZERO) {
		return;
	}

	bufs.set_count (_ports.count());

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		PortSet::iterator   i = _ports.begin(*t);
		BufferSet::iterator b = bufs.begin(*t);

		for (uint32_t off = 0; off < offset.get(*t); ++off, ++b) {
			if (b == bufs.end(*t)) {
				continue;
			}
		}

		for ( ; i != _ports.end(*t); ++i, ++b) {
			Buffer& bb (i->get_buffer (nframes));
			b->read_from (bb, nframes);
		}
	}
}

void
IO::copy_to_outputs (BufferSet& bufs, DataType type, pframes_t nframes, framecnt_t offset)
{
	// Copy any buffers 1:1 to outputs

	PortSet::iterator o = _ports.begin(type);
	BufferSet::iterator i = bufs.begin(type);
	BufferSet::iterator prev = i;

	while (i != bufs.end(type) && o != _ports.end (type)) {
		Buffer& port_buffer (o->get_buffer (nframes));
		port_buffer.read_from (*i, nframes, offset);
		prev = i;
		++i;
		++o;
	}

	// Copy last buffer to any extra outputs

	while (o != _ports.end(type)) {
		Buffer& port_buffer (o->get_buffer (nframes));
		port_buffer.read_from (*prev, nframes, offset);
		++o;
	}
}

boost::shared_ptr<Port>
IO::port_by_name (const std::string& str) const
{
	/* to be called only from ::set_state() - no locking */

	for (PortSet::const_iterator i = _ports.begin(); i != _ports.end(); ++i) {

		if (i->name() == str) {
			return boost::const_pointer_cast<Port> (*i);
		}
	}

	return boost::shared_ptr<Port> ();
}

bool
IO::physically_connected () const
{
	for (PortSet::const_iterator i = _ports.begin(); i != _ports.end(); ++i) {
                if (i->physically_connected()) {
                        return true;
                }
        }

        return false;
}

bool
IO::has_port (boost::shared_ptr<Port> p) const
{
	Glib::Mutex::Lock lm (io_lock);
	return _ports.contains (p);
}
