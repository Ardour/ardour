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

#include "pbd/stateful.h"
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
{
	_extra_xml = 0;
	_instant_xml = 0;
}

Stateful::~Stateful ()
{
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
	for (OwnedPropertyList::iterator i = _properties.begin(); i != _properties.end(); ++i) {
		i->second->clear_history ();
	}
}

/** @return A pair of XMLNodes representing state that has changed since the last time clear_history
 *  was called on this object; the first is the state before, the second the state after.
 *
 *  It is the caller's responsibility to delete the returned XMLNodes.
 */
pair<XMLNode *, XMLNode *>
Stateful::diff () const
{
	XMLNode* old = new XMLNode (_xml_node_name);
	XMLNode* current = new XMLNode (_xml_node_name);

	for (OwnedPropertyList::const_iterator i = _properties.begin(); i != _properties.end(); ++i) {
		i->second->diff (old, current);
	}

	return make_pair (old, current);
}

/** Modifies PropertyChange @param c to indicate what properties have changed since the last
    time clear_history was called on this object. Note that not all properties have change
    values - if this object has any such Property members, they will never show up in
    the value of @param c. Note also that @param c is not cleared by this function.
*/
void
Stateful::changed (PropertyChange& c) const
{
	for (OwnedPropertyList::const_iterator i = _properties.begin(); i != _properties.end(); ++i) {
		i->second->diff (c);
	}
}	
	
/** Set state of some/all _properties from an XML node.
 *  @param node Node.
 *  @return PropertyChanges made.
 */
PropertyChange
Stateful::set_properties (XMLNode const & node)
{
	PropertyChange c;

	for (OwnedPropertyList::iterator i = _properties.begin(); i != _properties.end(); ++i) {
		if (i->second->set_state (node)) {
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

	for (OwnedPropertyList::iterator i = _properties.begin(); i != _properties.end(); ++i) {
		if ((p = property_list.find (i->first)) != property_list.end()) {
			if (set_property (*p->second)) {
				c.add (i->first);
			}
		}
	}
	
	post_set ();

	return c;
}


/** Add property states to an XML node.
 *  @param node Node.
 */
void
Stateful::add_properties (XMLNode & node)
{
	for (OwnedPropertyList::iterator i = _properties.begin(); i != _properties.end(); ++i) {
		i->second->add_state (node);
	}
}

} // namespace PBD
