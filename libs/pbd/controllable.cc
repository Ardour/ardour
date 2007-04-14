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

#include <pbd/controllable.h>
#include <pbd/xml++.h>
#include <pbd/error.h>

#include "i18n.h"

using namespace PBD;

sigc::signal<void,Controllable*> Controllable::Destroyed;
sigc::signal<bool,Controllable*> Controllable::StartLearning;
sigc::signal<void,Controllable*> Controllable::StopLearning;

Glib::Mutex* Controllable::registry_lock = 0;
Controllable::Controllables Controllable::registry;

Controllable::Controllable (std::string name)
	: _name (name)
{
	if (registry_lock == 0) {
		registry_lock = new Glib::Mutex;
	}

	add ();
}

void
Controllable::add ()
{
	Glib::Mutex::Lock lm (*registry_lock);
	registry.insert (this);
	this->GoingAway.connect (mem_fun (this, &Controllable::remove));
}

void
Controllable::remove ()
{
	Glib::Mutex::Lock lm (*registry_lock);
	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i) == this) {
			registry.erase (i);
			break;
		}
	}
}

Controllable*
Controllable::by_id (const ID& id)
{
	Glib::Mutex::Lock lm (*registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i)->id() == id) {
			return (*i);
		}
	}
	return 0;
}


Controllable*
Controllable::by_name (const std::string& str)
{
	Glib::Mutex::Lock lm (*registry_lock);

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
	XMLNode* node = new XMLNode (X_("controllable"));
	char buf[64];

	node->add_property (X_("name"), _name); // not reloaded from XML state, just there to look at
	_id.print (buf, sizeof (buf));
	node->add_property (X_("id"), buf);
	return *node;
}

int
Controllable::set_state (const XMLNode& node)
{
	const XMLProperty* prop = node.property (X_("id"));

	if (prop) {
		_id = prop->value();
		return 0;
	} else {
		error << _("Controllable state node has no ID property") << endmsg;
		return -1;
	}
}
