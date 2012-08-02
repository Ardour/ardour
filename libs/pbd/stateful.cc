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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/debug.h"
#include "pbd/stateful.h"
#include "pbd/property_list.h"
#include "pbd/properties.h"
#include "pbd/destructible.h"
#include "pbd/xml++.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace std;

namespace PBD {

int Stateful::current_state_version = 0;
int Stateful::loading_state_version = 0;

Stateful::Stateful ()
	: _properties (new OwnedPropertyList)
	, _stateful_frozen (0)
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
Stateful::extra_xml (const string& str, bool add_if_missing)
{
	XMLNode* node = 0;

	if (_extra_xml) {
		node = _extra_xml->child (str.c_str());
	}

	if (!node && add_if_missing) {
		node = new XMLNode (str);
		add_extra_xml (*node);
	} 

	return node;
}

void
Stateful::save_extra_xml (const XMLNode& node)
{
	/* Looks for the child node called "Extra" and makes _extra_xml 
	   point to a copy of it. Will delete any existing node pointed
	   to by _extra_xml if a new Extra node is found, but not
	   otherwise.
	*/
	
	const XMLNode* xtra = node.child ("Extra");

	if (xtra) {
		delete _extra_xml;
		_extra_xml = new XMLNode (*xtra);
	}
}

void
Stateful::add_instant_xml (XMLNode& node, const std::string& directory_path)
{
	if (!Glib::file_test (directory_path, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (directory_path.c_str(), 0755) != 0) {
			error << string_compose(_("Error: could not create directory %1"), directory_path) << endmsg;
			return;
		}
	}

	if (_instant_xml == 0) {
		_instant_xml = new XMLNode ("instant");
	}

	_instant_xml->remove_nodes_and_delete (node.name());
	_instant_xml->add_child_copy (node);

	std::string instant_xml_path = Glib::build_filename (directory_path, "instant.xml");
	
	XMLTree tree;
	tree.set_filename(instant_xml_path);

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
		error << string_compose(_("Error: could not write %1"), instant_xml_path) << endmsg;
	}
}

XMLNode *
Stateful::instant_xml (const string& str, const std::string& directory_path)
{
	if (_instant_xml == 0) {

		std::string instant_xml_path = Glib::build_filename (directory_path, "instant.xml");

		if (Glib::file_test (instant_xml_path, Glib::FILE_TEST_EXISTS)) {
			XMLTree tree;
			if (tree.read(instant_xml_path)) {
				_instant_xml = new XMLNode(*(tree.root()));
			} else {
				warning << string_compose(_("Could not understand XML file %1"), instant_xml_path) << endmsg;
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

/** Forget about any changes to this object's properties */
void
Stateful::clear_changes ()
{
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->clear_changes ();
	}
}

PropertyList *
Stateful::get_changes_as_properties (Command* cmd) const
{
	PropertyList* pl = new PropertyList;
	
	for (OwnedPropertyList::const_iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->get_changes_as_properties (*pl, cmd);
	}

	return pl;
}

/** Set our property values from an XML node.
 *  Derived types can call this from set_state() (or elsewhere)
 *  to get basic property setting done.
 *  @return IDs of properties that were changed.
 */
PropertyChange
Stateful::set_values (XMLNode const & node)
{
	PropertyChange c;
	
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		if (i->second->set_value (node)) {
			c.add (i->first);
		}
	}

	post_set (c);

	return c;
}

PropertyChange
Stateful::apply_changes (const PropertyList& property_list)
{
	PropertyChange c;
	PropertyList::const_iterator p;

	DEBUG_TRACE (DEBUG::Stateful, string_compose ("Stateful %1 setting properties from list of %2\n", this, property_list.size()));

	for (PropertyList::const_iterator pp = property_list.begin(); pp != property_list.end(); ++pp) {
		DEBUG_TRACE (DEBUG::Stateful, string_compose ("in plist: %1\n", pp->second->property_name()));
	}
	
	for (PropertyList::const_iterator i = property_list.begin(); i != property_list.end(); ++i) {
		if ((p = _properties->find (i->first)) != _properties->end()) {

			DEBUG_TRACE (
				DEBUG::Stateful,
				string_compose ("actually setting property %1 using %2\n", p->second->property_name(), i->second->property_name())
				);
			
			if (apply_changes (*i->second)) {
				c.add (i->first);
			}
		} else {
			DEBUG_TRACE (DEBUG::Stateful, string_compose ("passed in property %1 not found in own property list\n",
			                                              i->second->property_name()));
		}
	}
	
	post_set (c);

	send_change (c);

	return c;
}

/** Add property states to an XML node.
 *  @param owner_state Node.
 */
void
Stateful::add_properties (XMLNode& owner_state)
{
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->get_value (owner_state);
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
		Glib::Threads::Mutex::Lock lm (_lock);
		if (property_changes_suspended ()) {
			_pending_changed.add (what_changed);
			return;
		}
	}

	PropertyChanged (what_changed);
}

void
Stateful::suspend_property_changes ()
{
	g_atomic_int_add (&_stateful_frozen, 1);
}

void
Stateful::resume_property_changes ()
{
	PropertyChange what_changed;

	{
		Glib::Threads::Mutex::Lock lm (_lock);

		if (property_changes_suspended() && g_atomic_int_dec_and_test (&_stateful_frozen) == FALSE) {
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

bool
Stateful::changed() const  
{
	for (OwnedPropertyList::const_iterator i = _properties->begin(); i != _properties->end(); ++i) {
		if (i->second->changed()) {
			return true;
		}
	}

	return false;
}

bool
Stateful::apply_changes (const PropertyBase& prop)
{
	OwnedPropertyList::iterator i = _properties->find (prop.property_id());
	if (i == _properties->end()) {
		return false;
	}

	i->second->apply_changes (&prop);
	return true;
}

PropertyList*
Stateful::property_factory (const XMLNode& history_node) const
{
	PropertyList* prop_list = new PropertyList;

	for (OwnedPropertyList::const_iterator i = _properties->begin(); i != _properties->end(); ++i) {
		PropertyBase* prop = i->second->clone_from_xml (history_node);

		if (prop) {
			prop_list->add (prop);
		}
	}

	return prop_list;
}

void
Stateful::rdiff (vector<Command*>& cmds) const
{
	for (OwnedPropertyList::const_iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->rdiff (cmds);
	}
}

void
Stateful::clear_owned_changes ()
{
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->clear_owned_changes ();
	}
}
  
bool
Stateful::set_id (const XMLNode& node) 
{
	const XMLProperty* prop;

	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
		return true;
	} 

	return false;
}

void
Stateful::reset_id ()
{
	_id = ID ();
}

void
Stateful::set_id (const string& str)
{
	_id = str;
}

} // namespace PBD
