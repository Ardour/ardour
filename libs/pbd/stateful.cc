/*
 * Copyright (C) 2000-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/debug.h"
#include "pbd/stateful.h"
#include "pbd/types_convert.h"
#include "pbd/property_list.h"
#include "pbd/destructible.h"
#include "pbd/xml++.h"
#include "pbd/error.h"

#include "pbd/i18n.h"

using namespace std;

namespace PBD {

int Stateful::current_state_version = 0;
int Stateful::loading_state_version = 0;

Glib::Threads::Private<bool> Stateful::_regenerate_xml_or_string_ids;

Stateful::Stateful ()
	: _extra_xml (nullptr)
	, _instant_xml (nullptr)
	, _properties (new OwnedPropertyList)
{
	g_atomic_int_set (&_stateful_frozen, 0);
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
	if (_extra_xml == nullptr) {
		_extra_xml = new XMLNode ("Extra");
	}

	_extra_xml->remove_nodes_and_delete (node.name());
	_extra_xml->add_child_nocopy (node);
}

XMLNode *
Stateful::extra_xml (const string& str, bool add_if_missing)
{
	XMLNode* node = nullptr;

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

	if (_instant_xml == nullptr) {
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

	auto* copy = new XMLNode (*_instant_xml);
	tree.set_root (copy);

	if (!tree.write()) {
		error << string_compose(_("Error: could not write %1"), instant_xml_path) << endmsg;
	}
}

XMLNode *
Stateful::instant_xml (const string& str, const std::string& directory_path)
{
	if (_instant_xml == nullptr) {
		std::string instant_xml_path = Glib::build_filename (directory_path, "instant.xml");

		if (Glib::file_test (instant_xml_path, Glib::FILE_TEST_EXISTS)) {
			XMLTree tree;
			if (tree.read(instant_xml_path)) {
				_instant_xml = new XMLNode(*(tree.root()));
			} else {
				warning << string_compose(_("Could not understand XML file %1"), instant_xml_path) << endmsg;
				return nullptr;
			}
		} else {
			return nullptr;
		}
	}

	const XMLNodeList& nlist = _instant_xml->children();
	XMLNodeConstIterator i;

	for (i = nlist.begin(); i != nlist.end(); ++i) {
		if ((*i)->name() == str) {
			return (*i);
		}
	}

	return nullptr;
}

/** Forget about any changes to this object's properties */
void
Stateful::clear_changes ()
{
	for (const auto& _property : *_properties) {
		_property.second->clear_changes ();
	}
	_pending_changed.clear ();
}

PropertyList *
Stateful::get_changes_as_properties (Command* cmd) const
{
	auto* pl = new PropertyList;

	for (const auto& _property : *_properties) {
		_property.second->get_changes_as_properties (*pl, cmd);
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

	for (const auto& _property : *_properties) {
		if (_property.second->set_value (node)) {
			c.add (_property.first);
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

	for (auto pp = property_list.begin(); pp != property_list.end(); ++pp) {
		DEBUG_TRACE (DEBUG::Stateful, string_compose ("in plist: %1\n", pp->second->property_name()));
	}

	for (auto i = property_list.begin(); i != property_list.end(); ++i) {
		if ((p = _properties->find (i->first)) != _properties->end()) {

			DEBUG_TRACE (
				DEBUG::Stateful,
				string_compose ("actually setting property %1 using %2\n", p->second->property_name(), i->second->property_name())
				);

			if (apply_change (*i->second)) {
				DEBUG_TRACE (DEBUG::Stateful, string_compose ("applying change succeeded, add %1 to change list\n", p->second->property_name()));
				c.add (i->first);
			} else {
				DEBUG_TRACE (DEBUG::Stateful, string_compose ("applying change failed for %1\n", p->second->property_name()));
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
Stateful::add_properties (XMLNode& owner_state) const
{
	for (const auto& _property : *_properties) {
		_property.second->get_value (owner_state);
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
	for (const auto& _property : *_properties) {
		if (_property.second->changed ()) {
			return true;
		}
	}

	return false;
}

bool
Stateful::apply_change (const PropertyBase& prop)
{
	OwnedPropertyList::iterator i = _properties->find (prop.property_id());
	if (i == _properties->end()) {
		return false;
	}

	i->second->apply_change (&prop);
	return true;
}

PropertyList*
Stateful::property_factory (const XMLNode& history_node) const
{
	auto* prop_list = new PropertyList;

	for (const auto& _property : *_properties) {
		PropertyBase* prop = _property.second->clone_from_xml (history_node);

		if (prop) {
			prop_list->add (prop);
		}
	}

	return prop_list;
}

void
Stateful::rdiff (vector<Command*>& cmds) const
{
	for (const auto& _property : *_properties) {
		_property.second->rdiff (cmds);
	}
}

void
Stateful::clear_owned_changes ()
{
	for (const auto& _property : *_properties) {
		_property.second->clear_owned_changes ();
	}
}

bool
Stateful::set_id (const XMLNode& node)
{
	bool* regen = _regenerate_xml_or_string_ids.get();

	if (regen && *regen) {
		reset_id ();
		return true;
	}

	if (node.get_property ("id", _id)) {
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
	bool* regen = _regenerate_xml_or_string_ids.get();

	if (regen && *regen) {
		reset_id ();
	} else {
		_id = str;
	}
}

bool
Stateful::regenerate_xml_or_string_ids () const
{
	bool* regen = _regenerate_xml_or_string_ids.get();
	if (regen && *regen) {
		return true;
	} else {
		return false;
	}
}

void
Stateful::set_regenerate_xml_and_string_ids_in_this_thread (bool yn)
{
	bool* val = new bool (yn);
	_regenerate_xml_or_string_ids.set (val);
}

} // namespace PBD
