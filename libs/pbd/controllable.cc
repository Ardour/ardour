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
#include "pbd/xml++.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace PBD;
using namespace std;

sigc::signal<void,Controllable*> Controllable::Destroyed;
sigc::signal<bool,Controllable*> Controllable::StartLearning;
sigc::signal<void,Controllable*> Controllable::StopLearning;
sigc::signal<void,Controllable*,int,int> Controllable::CreateBinding;
sigc::signal<void,Controllable*> Controllable::DeleteBinding;

Glib::StaticRWLock Controllable::registry_lock = GLIBMM_STATIC_RW_LOCK_INIT;
Controllable::Controllables Controllable::registry;
Controllable::ControllablesByURI Controllable::registry_by_uri;

Controllable::Controllable (const string& name, const string& uri)
	: _name (name)
	, _uri (uri)
	, _touching (false)
{
	add ();
}

void
Controllable::add ()
{
	Glib::RWLock::WriterLock lm (registry_lock);
	registry.insert (this);

	if (!_uri.empty()) {
		pair<string,Controllable*> newpair;
		newpair.first = _uri;
		newpair.second = this;
		registry_by_uri.insert (newpair);
	}

	this->GoingAway.connect (mem_fun (this, &Controllable::remove));
}

void
Controllable::remove ()
{
	Glib::RWLock::WriterLock lm (registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i) == this) {
			registry.erase (i);
			break;
		}
	}

	if (!_uri.empty()) {
		ControllablesByURI::iterator i = registry_by_uri.find (_uri);
		if (i != registry_by_uri.end()) {
			registry_by_uri.erase (i);
		}
	}


}

void
Controllable::set_uri (const string& new_uri)
{
	Glib::RWLock::WriterLock lm (registry_lock);

	if (!_uri.empty()) {
		ControllablesByURI::iterator i = registry_by_uri.find (_uri);
		if (i != registry_by_uri.end()) {
			registry_by_uri.erase (i);
		}
	}

	_uri = new_uri;

	if (!_uri.empty()) {
		pair<string,Controllable*> newpair;
		newpair.first = _uri;
		newpair.second = this;
		registry_by_uri.insert (newpair);
	}
}

Controllable*
Controllable::by_id (const ID& id)
{
	Glib::RWLock::ReaderLock lm (registry_lock);

	for (Controllables::iterator i = registry.begin(); i != registry.end(); ++i) {
		if ((*i)->id() == id) {
			return (*i);
		}
	}
	return 0;
}

Controllable*
Controllable::by_uri (const string& uri)
{
	Glib::RWLock::ReaderLock lm (registry_lock);
	ControllablesByURI::iterator i;

	if ((i = registry_by_uri.find (uri)) != registry_by_uri.end()) {
		return i->second;
	}
	return 0;
}

Controllable*
Controllable::by_name (const string& str)
{
	Glib::RWLock::ReaderLock lm (registry_lock);

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
	XMLNode* node = new XMLNode (X_("Controllable"));
	char buf[64];

	node->add_property (X_("name"), _name); // not reloaded from XML state, just there to look at
	_id.print (buf, sizeof (buf));
	node->add_property (X_("id"), buf);

	if (!_uri.empty()) {
		node->add_property (X_("uri"), _uri);
	}
		
	return *node;
}

int
Controllable::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property (X_("id"))) != 0) {
		_id = prop->value();
		return 0;
	} else {
		error << _("Controllable state node has no ID property") << endmsg;
		return -1;
	}

	if ((prop = node.property (X_("uri"))) != 0) {
		set_uri (prop->value());
	}
}
