/*
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include "pbd/controllable.h"
#include "pbd/enumwriter.h"
#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/types_convert.h"
#include "pbd/string_convert.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace std;

PBD::Signal1<bool, boost::weak_ptr<PBD::Controllable> > Controllable::StartLearning;
PBD::Signal1<void, boost::weak_ptr<PBD::Controllable> > Controllable::StopLearning;
PBD::Signal1<void, boost::weak_ptr<PBD::Controllable> > Controllable::GUIFocusChanged;

Glib::Threads::RWLock Controllable::registry_lock;
Controllable::Controllables Controllable::registry;
PBD::ScopedConnectionList Controllable::registry_connections;

const std::string Controllable::xml_node_name = X_("Controllable");

Controllable::Controllable (const string& name, Flag f)
	: _name (name)
	, _flags (f)
	, _touching (false)
{
	add (*this);
}

XMLNode&
Controllable::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name);

	/* Waves' "Pressure3" has a parameter called "µ-iness"
	 * which causes a  parser error : Input is not proper UTF-8, indicate encoding !
	 *  Bytes: 0xB5 0x2D 0x69 0x6E
	 *          <Controllable name="�-iness" id="2391" flags="" value="0.000000000000" p
	 */

	// this is not reloaded from XML, but it must be present because it is
	// used to find and identify XML nodes by various Controllable-derived objects

	node->set_property (X_("name"), _name);
	node->set_property (X_("id"), id());
	node->set_property (X_("flags"), _flags);
	node->set_property (X_("value"), get_save_value());

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return *node;
}

int
Controllable::set_state (const XMLNode& node, int /*version*/)
{
	Stateful::save_extra_xml (node);

	set_id (node);

	if (node.get_property (X_("flags"), _flags)) {
		_flags = Flag(_flags | (_flags & Controllable::RealTime));
	}

	float val;
	if (node.get_property (X_("value"), val)) {
			set_value (val, NoGroup);
	}
	return 0;
}

void
Controllable::set_flags (Flag f)
{
	_flags = f;
}

void
Controllable::set_flag (Flag f)
{
	_flags = Flag ((int)_flags | f);
}

void
Controllable::clear_flag (Flag f)
{
	_flags = Flag ((int)_flags & ~f);
}

void
Controllable::add (Controllable& ctl)
{
	Glib::Threads::RWLock::WriterLock lm (registry_lock);
	registry.insert (&ctl);
	ctl.DropReferences.connect_same_thread (registry_connections, boost::bind (&Controllable::remove, &ctl));
	ctl.Destroyed.connect_same_thread (registry_connections, boost::bind (&Controllable::remove, &ctl));
}

void
Controllable::remove (Controllable* ctl)
{
	Glib::Threads::RWLock::WriterLock lm (registry_lock);
	Controllables::iterator i = std::find (registry.begin(), registry.end(), ctl);
	if (i != registry.end()) {
		registry.erase (i);
	}
}

boost::shared_ptr<Controllable>
Controllable::by_id (const ID& id)
{
	Glib::Threads::RWLock::ReaderLock lm (registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i)->id() == id) {
			return (*i)->shared_from_this ();
		}
	}
	return boost::shared_ptr<Controllable>();
}

void
Controllable::dump_registry ()
{
	Glib::Threads::RWLock::ReaderLock lm (registry_lock);
	if (registry.size() == 0) {
		return;
	}
	unsigned int cnt = 0;
	cout << "-- List Of Registered Controllables\n";
	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i, ++cnt) {
		cout << "CTRL: " << (*i)->name() << "\n";
	}
	cout << "Total number of registered sontrollables: " << cnt << "\n";
}
