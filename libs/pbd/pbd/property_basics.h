/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __libpbd_property_basics_h__
#define __libpbd_property_basics_h__

#include <glib.h>
#include <set>
#include <vector>

#include "pbd/libpbd_visibility.h"
#include "pbd/xml++.h"

class Command;

namespace PBD {

class LIBPBD_API PropertyList;
class LIBPBD_API StatefulDiffCommand;	

/** A unique identifier for a property of a Stateful object */
typedef GQuark PropertyID;

template<typename T>
struct /*LIBPBD_API*/ PropertyDescriptor {
	PropertyDescriptor () : property_id (0) {}
	PropertyDescriptor (PropertyID pid) : property_id (pid) {}
	
	PropertyID property_id;
	typedef T value_type;
};

/** A list of IDs of Properties that have changed in some situation or other */
class /*LIBPBD_API*/ PropertyChange : public std::set<PropertyID>
{
public:
	LIBPBD_API  PropertyChange() {}
	LIBPBD_API ~PropertyChange() {}

	template<typename T> PropertyChange(PropertyDescriptor<T> p);

	LIBPBD_API PropertyChange(const PropertyChange& other) : std::set<PropertyID> (other) {}

	LIBPBD_API PropertyChange operator=(const PropertyChange& other) {
		clear ();
		insert (other.begin (), other.end ());
		return *this;
	}

	template<typename T> PropertyChange operator=(PropertyDescriptor<T> p);
	template<typename T> bool contains (PropertyDescriptor<T> p) const;

	LIBPBD_API bool contains (const PropertyChange& other) const {
		for (const_iterator x = other.begin (); x != other.end (); ++x) {
			if (find (*x) != end ()) {
				return true;
			}
		}
		return false;
	}

	LIBPBD_API void add (PropertyID id)               { insert (id); }
	LIBPBD_API void add (const PropertyChange& other) { insert (other.begin (), other.end ()); }
	template<typename T> void add (PropertyDescriptor<T> p);
};

/** Base (non template) part of Property
 *  Properties are used for two main reasons:
 *    - to handle current state (when serializing Stateful objects)
 *    - to handle history since some operation was started (when making StatefulDiffCommands for undo)
 */
class LIBPBD_API PropertyBase
{
public:
	PropertyBase (PropertyID pid)
		: _property_id (pid)
	{}

	virtual ~PropertyBase () {}


	/* MANAGEMENT OF Stateful STATE */

	/** Set the value of this property from a Stateful node.
	 *  @return true if the value was set.
	 */
	virtual bool set_value (XMLNode const &) = 0;

	/** Get this property's value and put it into a Stateful node */
	virtual void get_value (XMLNode& node) const = 0;


	/* MANAGEMENT OF HISTORY */

	/** Forget about any old changes to this property's value */
	virtual void clear_changes () = 0;

	/** Tell any things we own to forget about their old values */
	virtual void clear_owned_changes () {}

	/** @return true if this property has changed in value since construction or since
	 *  the last call to clear_changes (), whichever was more recent.
	 */
	virtual bool changed () const = 0;

	/** Invert the changes in this property */
	virtual void invert () = 0;
	

	/* TRANSFERRING HISTORY TO / FROM A StatefulDiffCommand */

        /** Get any changes in this property as XML and add them to a
	 *  StatefulDiffCommand node.
	 */
	virtual void get_changes_as_xml (XMLNode *) const = 0;
	
        /** If this Property has changed, clone it and add it to a given list.
	 *  Used for making StatefulDiffCommands.
	 */
	virtual void get_changes_as_properties (PropertyList& changes, Command *) const = 0;

	/** Collect StatefulDiffCommands for changes to anything that we own */
	virtual void rdiff (std::vector<Command*> &) const {}

	/** Look in an XML node written by get_changes_as_xml and, if XML from this property
	 *  is found, create a property with the changes from the XML.
	 */
        virtual PropertyBase* clone_from_xml (const XMLNode &) const { return 0; }


	/* VARIOUS */
	
	virtual PropertyBase* clone () const = 0;

	/** Set this property's current state from another */
	virtual void apply_changes (PropertyBase const *) = 0;

	const gchar* property_name () const { return g_quark_to_string (_property_id); }
	PropertyID   property_id () const   { return _property_id; }

	bool operator==(PropertyID pid) const {
		return _property_id == pid;
	}

protected:	
	/* copy construction only by subclasses */
	PropertyBase (PropertyBase const & b)
		: _property_id (b._property_id)
	{}
	
private:
	PropertyID _property_id;

};

}

#endif /* __libpbd_property_basics_h__ */
