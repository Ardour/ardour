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

#include "pbd/xml++.h"

class Command;

namespace PBD {

class PropertyList;
class StatefulDiffCommand;	

/** A unique identifier for a property of a Stateful object */
typedef GQuark PropertyID;

template<typename T>
struct PropertyDescriptor {
    PropertyDescriptor () : property_id (0) {}
    PropertyDescriptor (PropertyID pid) : property_id (pid) {}

    PropertyID property_id;
    typedef T value_type;
};

/** A list of IDs of Properties that have changed in some situation or other */
class PropertyChange : public std::set<PropertyID>
{
public:
	PropertyChange() {}

	template<typename T> PropertyChange(PropertyDescriptor<T> p);

	PropertyChange(const PropertyChange& other) : std::set<PropertyID> (other) {}

	PropertyChange operator=(const PropertyChange& other) {
		clear ();
		insert (other.begin (), other.end ());
		return *this;
	}

	template<typename T> PropertyChange operator=(PropertyDescriptor<T> p);
	template<typename T> bool contains (PropertyDescriptor<T> p) const;

	bool contains (const PropertyChange& other) const {
		for (const_iterator x = other.begin (); x != other.end (); ++x) {
			if (find (*x) != end ()) {
				return true;
			}
		}
		return false;
	}

	void add (PropertyID id)               { insert (id); }
	void add (const PropertyChange& other) { insert (other.begin (), other.end ()); }
	template<typename T> void add (PropertyDescriptor<T> p);
};

/** Base (non template) part of Property */
class PropertyBase
{
public:
	PropertyBase (PropertyID pid)
		: _property_id (pid)
	{}

	virtual ~PropertyBase () {}

	virtual PropertyBase* clone () const = 0;
        
	/** Forget about any old value for this state */
	virtual void clear_history () = 0;

	/** Tell any things we own to forget about their old values */
	virtual void clear_owned_history () {}

        /** Get any changes in this property as XML and add it to a node */
	virtual void get_changes_as_xml (XMLNode *) const = 0;
	
	virtual void get_changes_as_properties (PropertyList& changes, Command *) const = 0;

	/** Collect StatefulDiffCommands for changes to anything that we own */
	virtual void rdiff (std::vector<StatefulDiffCommand*> &) const {}
	
        virtual PropertyBase* maybe_clone_self_if_found_in_history_node (const XMLNode&) const { return 0; }

	/** Set our value from an XML node.
	 *  @return true if the value was set.
	 */
	virtual bool set_value (XMLNode const &) = 0;

	/** Get our value and put it into an XML node */
	virtual void get_value (XMLNode& node) const = 0;

	/** @return true if this property has changed in value since construction or since
	 *  the last call to clear_history(), whichever was more recent.
	 */
	virtual bool changed() const = 0;

	/** Apply changes contained in another Property to this one */
	virtual void apply_changes (PropertyBase const *) = 0;

	/** Invert the changes in this property */
	virtual void invert () = 0;

	const gchar*property_name () const { return g_quark_to_string (_property_id); }
	PropertyID  property_id () const   { return _property_id; }

	bool operator==(PropertyID pid) const {
		return _property_id == pid;
	}

protected:
	PropertyID _property_id;
};

}

#endif /* __libpbd_property_basics_h__ */
