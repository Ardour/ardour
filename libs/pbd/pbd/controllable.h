/*
    Copyright (C) 2000-2007 Paul Davis 

    This program is free software; you can redistribute it and/or modify
v    it under the terms of the GNU General Public License as published by
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

#ifndef __pbd_controllable_h__
#define __pbd_controllable_h__

#include <string>
#include <set>
#include <map>

#include <boost/signals2.hpp>
#include <glibmm/thread.h>

#include "pbd/statefuldestructible.h"

class XMLNode;

namespace PBD {

class Controllable : public PBD::StatefulDestructible {
  public:
	Controllable (const std::string& name, const std::string& uri);
	virtual ~Controllable() { Destroyed (this); }

	void set_uri (const std::string&);
	const std::string& uri() const { return _uri; }

	virtual void set_value (float) = 0;
	virtual float get_value (void) const = 0;

	boost::signals2::signal<void()> LearningFinished;
	static boost::signals2::signal<void(PBD::Controllable*,int,int)> CreateBinding;
	static boost::signals2::signal<void(PBD::Controllable*)> DeleteBinding;

	static boost::signals2::signal<bool(PBD::Controllable*)> StartLearning;
	static boost::signals2::signal<void(PBD::Controllable*)> StopLearning;

	static boost::signals2::signal<void(Controllable*)> Destroyed;
	
	boost::signals2::signal<void()> Changed;

	int set_state (const XMLNode&, int version);
	XMLNode& get_state ();

	std::string name()      const { return _name; }
	bool        touching () const { return _touching; }
	
	void set_touching (bool yn) { _touching = yn; }

	static Controllable* by_id (const PBD::ID&);
	static Controllable* by_name (const std::string&);
	static Controllable* by_uri (const std::string&);

  private:
	std::string _name;
	std::string _uri;
	bool        _touching;

	static void add (Controllable&);
	static void remove (Controllable&);

	typedef std::set<PBD::Controllable*> Controllables;
	typedef std::map<std::string,PBD::Controllable*> ControllablesByURI;
	static Glib::StaticRWLock registry_lock;
	static Controllables registry;
	static ControllablesByURI registry_by_uri;
};

/* a utility class for the occasions when you need but do not have
   a Controllable
*/

class IgnorableControllable : public Controllable 
{
  public: 
	IgnorableControllable () : PBD::Controllable ("ignoreMe", std::string()) {}
	~IgnorableControllable () {}
    
	void set_value (float /*v*/) {}
	float get_value () const { return 0.0; }
};

}

#endif /* __pbd_controllable_h__ */
