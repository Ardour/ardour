/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
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

#ifndef __pbd_property_list_h__
#define __pbd_property_list_h__

#include <map>

#include "pbd/libpbd_visibility.h"
#include "pbd/property_basics.h"

class XMLNode;

namespace PBD {

/** A list of properties, mapped using their ID */
class LIBPBD_API PropertyList : public std::map<PropertyID, PropertyBase*>
{
public:
	PropertyList ();
	PropertyList (PropertyList const &);

	virtual ~PropertyList();

	void get_changes_as_xml (XMLNode*);
	void invert ();

	/** Add a property (of some kind) to the list.
	 *
	 * Used when
	 * constructing PropertyLists that describe a change/operation.
	 */
	bool add (PropertyBase* prop);

	/** Construct a new Property List
	 *
	 * Code that is constructing a property list for use
	 * in setting the state of an object uses this.
	 *
	 * Defined below, once we have Property<T>
	 */
	template<typename T, typename V> bool add (PropertyDescriptor<T> pid, const V& v);

protected:
	bool _property_owner;
};

/** Persistent Property List
 *
 * A variant of PropertyList that does not delete its
 * property list in its destructor. Objects with their
 * own Properties store them in an OwnedPropertyList
 * to avoid having them deleted at the wrong time.
 */
class LIBPBD_API OwnedPropertyList : public PropertyList
{
public:
	OwnedPropertyList();

	/** Add a property to the List
	 *
	 * Classes that own property lists use this to add their
	 * property members to their plists. Note that it takes
	 * a reference argument rather than a pointer like
	 * one of the add() methods in PropertyList.
	 */
	bool add (PropertyBase& p);
};

}

#endif /* __pbd_property_list_h__ */
