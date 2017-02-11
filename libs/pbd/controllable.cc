/*
    Copyright (C) 2000-2007 Paul Davis

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

#include "pbd/controllable.h"
#include "pbd/enumwriter.h"
#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/locale_guard.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace std;

PBD::Signal1<void,Controllable*> Controllable::Destroyed;
PBD::Signal1<bool,Controllable*> Controllable::StartLearning;
PBD::Signal1<void,Controllable*> Controllable::StopLearning;
PBD::Signal3<void,Controllable*,int,int> Controllable::CreateBinding;
PBD::Signal1<void,Controllable*> Controllable::DeleteBinding;

Glib::Threads::RWLock Controllable::registry_lock;
Controllable::Controllables Controllable::registry;
PBD::ScopedConnectionList* registry_connections = 0;
const std::string Controllable::xml_node_name = X_("Controllable");

Controllable::Controllable (const string& name, Flag f)
	: _name (name)
	, _flags (f)
	, _touching (false)
{
	add (*this);
}

void
Controllable::add (Controllable& ctl)
{
	using namespace boost;

	Glib::Threads::RWLock::WriterLock lm (registry_lock);
	registry.insert (&ctl);

	if (!registry_connections) {
		registry_connections = new ScopedConnectionList;
	}

	/* Controllable::remove() is static - no need to manage this connection */

	ctl.DropReferences.connect_same_thread (*registry_connections, boost::bind (&Controllable::remove, &ctl));
}

void
Controllable::remove (Controllable* ctl)
{
	Glib::Threads::RWLock::WriterLock lm (registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i) == ctl) {
			registry.erase (i);
			break;
		}
	}
}

Controllable*
Controllable::by_id (const ID& id)
{
	Glib::Threads::RWLock::ReaderLock lm (registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i)->id() == id) {
			return (*i);
		}
	}
	return 0;
}

Controllable*
Controllable::by_name (const string& str)
{
	Glib::Threads::RWLock::ReaderLock lm (registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i)->_name == str) {
			return (*i);
		}
	}
	return 0;
}

XMLNode&
Controllable::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name);
	LocaleGuard lg;
	char buf[64];

	/* Waves' "Pressure3" has a parameter called "µ-iness"
	 * which causes a  parser error : Input is not proper UTF-8, indicate encoding !
	 *  Bytes: 0xB5 0x2D 0x69 0x6E
	 *          <Controllable name="�-iness" id="2391" flags="" value="0.000000000000" p
	 */

	// this is not reloaded from XML, but it must be present because it is
	// used to find and identify XML nodes by various Controllable-derived objects

	node->add_property (X_("name"), _name);

	id().print (buf, sizeof (buf));
	node->add_property (X_("id"), buf);
	node->add_property (X_("flags"), enum_2_string (_flags));
	snprintf (buf, sizeof (buf), "%2.12f", get_save_value());
        node->add_property (X_("value"), buf);

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return *node;
}

int
Controllable::set_state (const XMLNode& node, int /*version*/)
{
	LocaleGuard lg;
	const XMLProperty* prop;

	Stateful::save_extra_xml (node);

	set_id (node);

	if ((prop = node.property (X_("flags"))) != 0) {
		_flags = (Flag) string_2_enum (prop->value(), _flags);
	}

	if ((prop = node.property (X_("value"))) != 0) {
		float val;

		if (sscanf (prop->value().c_str(), "%f", &val) == 1) {
			set_value (val, NoGroup);
		}
        }

        return 0;
}

void
Controllable::set_flags (Flag f)
{
	_flags = f;
}
