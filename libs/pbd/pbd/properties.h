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

#ifndef __pbd_properties_h__
#define __pbd_properties_h__

#include <string>
#include <sstream>
#include <list>
#include <set>
#include <glib.h>

#include "pbd/xml++.h"
#include "pbd/property_basics.h"
#include "pbd/property_list.h"

namespace PBD {

/** Parent class for classes which represent a single scalar property in a Stateful object 
 */
template<class T>
class PropertyTemplate : public PropertyBase
{
public:
	PropertyTemplate (PropertyDescriptor<T> p, T const& v)
		: PropertyBase (p.property_id)
		, _have_old (false)
		, _current (v)
	{}

	PropertyTemplate<T>& operator=(PropertyTemplate<T> const& s) {
		/* XXX: isn't there a nicer place to do this? */
		_have_old    = s._have_old;
		_property_id = s._property_id;

		_current = s._current;
		_old     = s._old;
		return *this;
	}

	T & operator=(T const& v) {
		set (v);
		return _current;
	}

	T & operator+=(T const& v) {
		set (_current + v);
		return _current;
	}

	bool operator==(const T& other) const {
		return _current == other;
	}

	bool operator!=(const T& other) const {
		return _current != other;
	}

	operator T const &() const {
		return _current;
	}

	T const& val () const {
		return _current;
	}

	void clear_history () {
		_have_old = false;
	}

	void add_history_state (XMLNode* history_node) const {
		/* We can get to the current state of a scalar property like this one simply
		   by knowing what the new state is.
		*/
                history_node->add_property (property_name(), to_string (_current));
	}

	/** Try to set state from the property of an XML node.
	 *  @param node XML node.
	 *  @return true if the value of the property is changed
	 */
	bool set_state_from_owner_state (XMLNode const& owner_state) {

		XMLProperty const* p = owner_state.property (property_name());

		if (p) {
			T const v = from_string (p->value ());

			if (v != _current) {
				set (v);
				return true;
			}
		}

		return false;
	}

	void add_state_to_owner_state (XMLNode& owner_state) const {
                owner_state.add_property (property_name(), to_string (_current));
	}

	bool changed () const { return _have_old; }
	void set_state_from_property (PropertyBase const * p) {
		T v = dynamic_cast<const PropertyTemplate<T>* > (p)->val ();
		if (v != _current) {
			set (v);
		}
	}

protected:
        /** Constructs a PropertyTemplate with a default
            value for _old and _current.
        */

	PropertyTemplate (PropertyDescriptor<T> p)
		: PropertyBase (p.property_id)
	{}

	void set (T const& v) {
                if (!_have_old) {
                        _old      = _current;
                        _have_old = true;
                }
		_current  = v;
	}

	virtual std::string to_string (T const& v) const             = 0;
	virtual T           from_string (std::string const& s) const = 0;

	bool _have_old;
	T _current;
	T _old;
};

template<class T>
std::ostream & operator<<(std::ostream& os, PropertyTemplate<T> const& s)
{
	return os << s.val ();
}

/** Representation of a single piece of scalar state in a Stateful; for use
 *  with types that can be written to / read from stringstreams.
 */
template<class T>
class Property : public PropertyTemplate<T>
{
public:
	Property (PropertyDescriptor<T> q, T const& v)
		: PropertyTemplate<T> (q, v)
	{}
        
        void diff (PropertyList& undo, PropertyList& redo) const {
                if (this->_have_old) {
                        undo.add (new Property<T> (this->property_id(), this->_old));
                        redo.add (new Property<T> (this->property_id(), this->_current));
                }
        }

        Property<T>* maybe_clone_self_if_found_in_history_node (const XMLNode& node) const {
                const XMLProperty* prop = node.property (this->property_name());
                if (!prop) {
                        return 0;
                }
                return new Property<T> (this->property_id(), from_string (prop->value()));
        }

	T & operator=(T const& v) {
		this->set (v);
		return this->_current;
	}

private:
        friend class PropertyFactory;

	Property (PropertyDescriptor<T> q)
		: PropertyTemplate<T> (q)
	{}

	/* Note that we do not set a locale for the streams used
	 * in to_string() or from_string(), because we want the
	 * format to be portable across locales (i.e. C or
	 * POSIX). Also, there is the small matter of
	 * std::locale aborting on OS X if used with anything
	 * other than C or POSIX locales.
	 */
	std::string to_string (T const& v) const {
		std::stringstream s;
		s.precision (12); // in case its floating point
		s << v;
		return s.str ();
	}

	T from_string (std::string const& s) const {
		std::stringstream t (s);
		T                 v;
		t >> v;
		return v;
	}

};

/** Specialization, for std::string which is common and special (see to_string() and from_string()
 *  Using stringstream to read from a std::string is easy to get wrong because of whitespace
 *  separators, etc.
 */
template<>
class Property<std::string> : public PropertyTemplate<std::string>
{
public:
	Property (PropertyDescriptor<std::string> q, std::string const& v)
		: PropertyTemplate<std::string> (q, v)
	{}

        void diff (PropertyList& before, PropertyList& after) const {
                if (this->_have_old) {
                        before.add (new Property<std::string> (PropertyDescriptor<std::string> (this->property_id()), this->_old));
                        after.add (new Property<std::string> (PropertyDescriptor<std::string> (this->property_id()), this->_current));
                }
        }

	std::string & operator=(std::string const& v) {
		this->set (v);
		return this->_current;
	}

private:
	std::string to_string (std::string const& v) const {
		return _current;
	}

	std::string from_string (std::string const& s) const {
		return s;
	}

};

} /* namespace PBD */

#include "pbd/property_list_impl.h"
#include "pbd/property_basics_impl.h"

#endif /* __pbd_properties_h__ */
