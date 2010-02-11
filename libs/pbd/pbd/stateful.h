/*
    Copyright (C) 2000-2010 Paul Davis 

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

#ifndef __pbd_stateful_h__
#define __pbd_stateful_h__

#include <string>
#include <cassert>
#include "pbd/id.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

class XMLNode;

namespace PBD {

namespace sys {
	class path;
}

enum Change {
	range_guarantee = ~0
};

Change new_change ();

/** Base (non template) part of State */	
class StateBase
{
public:
	StateBase (std::string const & p, Change c)
		: _have_old (false)
		, _xml_property_name (p)
		, _change (c)
	{

	}

	/** Forget about any old value for this state */
	void clear_history () {
		_have_old = false;
	}

	virtual void diff (XMLNode *, XMLNode *) const = 0;
	virtual Change set_state (XMLNode const &) = 0;
	virtual void add_state (XMLNode &) const = 0;

protected:
	bool _have_old;
	std::string _xml_property_name;
	Change _change;
};

/** Parent class for classes which represent a single piece of state in a Stateful object */
template <class T>
class StateTemplate : public StateBase
{
public:
	StateTemplate (std::string const & p, Change c, T const & v)
		: StateBase (p, c)
		, _current (v)
	{

	}

	StateTemplate<T> & operator= (StateTemplate<T> const & s) {
		/* XXX: isn't there a nicer place to do this? */
		_have_old = s._have_old;
		_xml_property_name = s._xml_property_name;
		_change = s._change;
		
		_current = s._current;
		_old = s._old;
		return *this;
	}

	T & operator= (T const & v) {
		set (v);
		return _current;
	}

	T & operator+= (T const & v) {
		set (_current + v);
		return _current;
	}

	bool operator== (std::string const & o) const {
		return o == to_string (_current);
	}

	bool operator!= (std::string const & o) const {
		return o != to_string (_current);
	}

	operator T const & () const {
		return _current;
	}

	T const & get () const {
		return _current;
	}

	void diff (XMLNode* old, XMLNode* current) const {
		if (_have_old) {
			old->add_property (_xml_property_name.c_str(), to_string (_old));
			current->add_property (_xml_property_name.c_str(), to_string (_current));
		}
	}

	/** Try to set state from the property of an XML node.
	 *  @param node XML node.
	 *  @return Change effected, or 0.
	 */
	Change set_state (XMLNode const & node) {
		XMLProperty const * p = node.property (_xml_property_name.c_str());

		if (p) {
			T const v = from_string (p->value ());

			if (v == _current) {
				return Change (0);
			}

			set (v);
			return _change;
		}

		return Change (0);
	}

	void add_state (XMLNode & node) const {
		node.add_property (_xml_property_name.c_str(), to_string (_current));
	}

protected:
	void set (T const & v) {
		_old = _current;
		_have_old = true;
		_current = v;
	}

	virtual std::string to_string (T const & v) const = 0;
	virtual T from_string (std::string const & s) const = 0;
		
	T _current;
	T _old;
};

template<class T>	
std::ostream& operator<< (std::ostream& os, StateTemplate<T> const & s)
{
	os << s.get();
	return os;
}

/** Representation of a single piece of state in a Stateful; for use
 *  with types that can be written to / read from stringstreams.
 */
template <class T>
class State : public StateTemplate<T>
{
public:
	State (std::string const & p, Change c, T const & v)
		: StateTemplate<T> (p, c, v)
	{

	}
	
	T & operator= (T const & v) {
		this->set (v);
		return this->_current;
	}
	
private:	
	std::string to_string (T const & v) const {
		std::stringstream s;
		s << v;
		return s.str ();
	}

	T from_string (std::string const & s) const {
		std::stringstream t (s);
		T v;
		t >> v;
		return v;
	}
};

template <class T>
class EnumState : public StateTemplate<T>
{
public:
	EnumState (std::string const & p, Change c, T const & v)
		: StateTemplate<T> (p, c, v)
	{

	}
	
	T & operator= (T const & v) {
		this->set (v);
		return this->_current;
	}

private:
	std::string to_string (T const & v) const {
		return enum_2_string (v);
	}

	T from_string (std::string const & v) const {
		return T (string_2_enum (v, this->_current));
	}
};

/** Base class for objects with saveable and undoable state */
class Stateful {
  public:
	Stateful ();
	virtual ~Stateful();

	virtual XMLNode& get_state (void) = 0;

	virtual int set_state (const XMLNode&, int version) = 0;

	void add_state (StateBase & s) {
		_states.push_back (&s);
	}

	/* Extra XML nodes */

	void add_extra_xml (XMLNode&);
	XMLNode *extra_xml (const std::string& str);

	const PBD::ID& id() const { return _id; }

	void clear_history ();
	std::pair<XMLNode *, XMLNode*> diff ();

	static int current_state_version;
	static int loading_state_version;

  protected:

	void add_instant_xml (XMLNode&, const sys::path& directory_path);
	XMLNode *instant_xml (const std::string& str, const sys::path& directory_path);
	Change set_state_using_states (XMLNode const &);
	void add_states (XMLNode &);

	XMLNode *_extra_xml;
	XMLNode *_instant_xml;
	PBD::ID _id;

	std::string _xml_node_name; ///< name of node to use for this object in XML
	std::list<StateBase*> _states; ///< state variables that this object has
};

} // namespace PBD

#endif /* __pbd_stateful_h__ */

