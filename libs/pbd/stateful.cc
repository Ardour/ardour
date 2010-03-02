/*
    Copyright (C) 2000-2001 Paul Davis 

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

    $Id: stateful.cc 629 2006-06-21 23:01:03Z paul $
*/

#include <unistd.h>

#include "pbd/debug.h"
#include "pbd/stateful.h"
#include "pbd/property_list.h"
#include "pbd/properties.h"
#include "pbd/destructible.h"
#include "pbd/filesystem.h"
#include "pbd/xml++.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace std;

namespace PBD {

int Stateful::current_state_version = 0;
int Stateful::loading_state_version = 0;

Stateful::Stateful ()
        : _frozen (0)
        , _no_property_changes (false)
        , _properties (new OwnedPropertyList)
{
	_extra_xml = 0;
	_instant_xml = 0;
}

Stateful::~Stateful ()
{
        delete _properties;

	// Do not delete _extra_xml.  The use of add_child_nocopy() 
	// means it needs to live on indefinately.

	delete _instant_xml;
}

void
Stateful::add_extra_xml (XMLNode& node)
{
	if (_extra_xml == 0) {
		_extra_xml = new XMLNode ("Extra");
	}

	_extra_xml->remove_nodes (node.name());
	_extra_xml->add_child_nocopy (node);
}

XMLNode *
Stateful::extra_xml (const string& str)
{
	if (_extra_xml == 0) {
		return 0;
	}

	const XMLNodeList& nlist = _extra_xml->children();
	XMLNodeConstIterator i;

	for (i = nlist.begin(); i != nlist.end(); ++i) {
		if ((*i)->name() == str) {
			return (*i);
		}
	}

	return 0;
}

void
Stateful::add_instant_xml (XMLNode& node, const sys::path& directory_path)
{
	sys::create_directories (directory_path); // may throw

	if (_instant_xml == 0) {
		_instant_xml = new XMLNode ("instant");
	}

	_instant_xml->remove_nodes_and_delete (node.name());
	_instant_xml->add_child_copy (node);

	sys::path instant_xml_path(directory_path);

	instant_xml_path /= "instant.xml";
	
	XMLTree tree;
	tree.set_filename(instant_xml_path.to_string());

	/* Important: the destructor for an XMLTree deletes
	   all of its nodes, starting at _root. We therefore
	   cannot simply hand it our persistent _instant_xml 
	   node as its _root, because we will lose it whenever
	   the Tree goes out of scope.

	   So instead, copy the _instant_xml node (which does 
	   a deep copy), and hand that to the tree.
	*/

	XMLNode* copy = new XMLNode (*_instant_xml);
	tree.set_root (copy);

	if (!tree.write()) {
		error << string_compose(_("Error: could not write %1"), instant_xml_path.to_string()) << endmsg;
	}
}

XMLNode *
Stateful::instant_xml (const string& str, const sys::path& directory_path)
{
	if (_instant_xml == 0) {

		sys::path instant_xml_path(directory_path);
		instant_xml_path /= "instant.xml";

		if (exists(instant_xml_path)) {
			XMLTree tree;
			if (tree.read(instant_xml_path.to_string())) {
				_instant_xml = new XMLNode(*(tree.root()));
			} else {
				warning << string_compose(_("Could not understand XML file %1"), instant_xml_path.to_string()) << endmsg;
				return 0;
			}
		} else {
			return 0;
		}
	}

	const XMLNodeList& nlist = _instant_xml->children();
	XMLNodeConstIterator i;

	for (i = nlist.begin(); i != nlist.end(); ++i) {
		if ((*i)->name() == str) {
			return (*i);
		}
	}

	return 0;
}

/** Forget about any old state for this object */	
void
Stateful::clear_history ()
{
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->clear_history ();
	}
}

void
Stateful::diff (PropertyList& before, PropertyList& after) const
{
	for (OwnedPropertyList::const_iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->diff (before, after);
	}
}

/** Set state of some/all _properties from an XML node.
 *  @param node Node.
 *  @return PropertyChanges made.
 */
PropertyChange
Stateful::set_properties (XMLNode const & owner_state)
{
	PropertyChange c;
        
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		if (i->second->set_state_from_owner_state (owner_state)) {
			c.add (i->first);
		}
	}

	post_set ();

	return c;
}

PropertyChange
Stateful::set_properties (const PropertyList& property_list)
{
	PropertyChange c;
	PropertyList::const_iterator p;

        DEBUG_TRACE (DEBUG::Stateful, string_compose ("Stateful %1 setting properties from list of %2\n", this, property_list.size()));

        for (PropertyList::const_iterator pp = property_list.begin(); pp != property_list.end(); ++pp) {
                DEBUG_TRACE (DEBUG::Stateful, string_compose ("in plist: %1\n", pp->second->property_name()));
        }
        
        for (PropertyList::const_iterator i = property_list.begin(); i != property_list.end(); ++i) {
                if ((p = _properties->find (i->first)) != _properties->end()) {
                        DEBUG_TRACE (DEBUG::Stateful, string_compose ("actually setting property %1\n", p->second->property_name()));
			if (set_property (*i->second)) {
				c.add (i->first);
			}
		} else {
                        DEBUG_TRACE (DEBUG::Stateful, string_compose ("passed in property %1 not found in own property list\n",
                                                                      i->second->property_name()));
                }
	}
	
	post_set ();

	return c;
}

/** Add property states to an XML node.
 *  @param node Node.
 */
void
Stateful::add_properties (XMLNode& owner_state)
{
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->add_state_to_owner_state (owner_state);
	}
}

void
Stateful::add_property (PropertyBase& s)
{
        _properties->add (s);
}

void
Stateful::send_change (const PropertyChange& what_changed)
{
	if (what_changed.empty()) {
		return;
	}

	{
		Glib::Mutex::Lock lm (_lock);
		if (_frozen) {
			_pending_changed.add (what_changed);
			return;
		}
	}

	PropertyChanged (what_changed);
}

void
Stateful::suspend_property_changes ()
{
        _frozen++;
}

void
Stateful::resume_property_changes ()
{
	PropertyChange what_changed;

	{
		Glib::Mutex::Lock lm (_lock);

		if (_frozen && --_frozen > 0) {
			return;
		}

		if (!_pending_changed.empty()) {
			what_changed = _pending_changed;
			_pending_changed.clear ();
		}
	}

        mid_thaw (what_changed);

        send_change (what_changed);
}


} // namespace PBD
