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

#include "pbd/signals.h"
#include <glibmm/thread.h>

#include "pbd/statefuldestructible.h"

class XMLNode;

namespace PBD {

class Controllable : public PBD::StatefulDestructible {
  public:
	enum Flag {
		Toggle = 0x1,
		Discrete = 0x2,
		GainLike = 0x4,
		IntegerOnly = 0x8
	};

	Controllable (const std::string& name, Flag f = Flag (0));
	virtual ~Controllable() { Destroyed (this); }

	virtual void set_value (float) = 0;
	virtual float get_value (void) const = 0;

	PBD::Signal0<void> LearningFinished;
	static PBD::Signal3<void,PBD::Controllable*,int,int> CreateBinding;
	static PBD::Signal1<void,PBD::Controllable*> DeleteBinding;

	static PBD::Signal1<bool,PBD::Controllable*> StartLearning;
	static PBD::Signal1<void,PBD::Controllable*> StopLearning;

	static PBD::Signal1<void,Controllable*> Destroyed;
	
	PBD::Signal0<void> Changed;

	int set_state (const XMLNode&, int version);
	XMLNode& get_state ();

	std::string name()      const { return _name; }

	bool touching () const { return _touching; }
	void set_touching (bool yn) { _touching = yn; }

	bool is_toggle() const { return _flags & Toggle; }
	bool is_discrete() const { return _flags & Discrete; }
	bool is_gain_like() const { return _flags & GainLike; }
	bool is_integral_only() const { return _flags & IntegerOnly; }

        virtual float lower() const { return 0.0f; }
        virtual float upper() const { return 1.0f; }

	Flag flags() const { return _flags; }
	void set_flags (Flag f);

	virtual uint32_t get_discrete_values (std::list<float>&) { return 0; /* no values returned */ }

	static Controllable* by_id (const PBD::ID&);
	static Controllable* by_name (const std::string&);

  private:
	std::string _name;

	Flag        _flags;
	bool        _touching;

	static void add (Controllable&);
	static void remove (Controllable*);

	typedef std::set<PBD::Controllable*> Controllables;
	static Glib::StaticRWLock registry_lock;
	static Controllables registry;
};

/* a utility class for the occasions when you need but do not have
   a Controllable
*/

class IgnorableControllable : public Controllable 
{
  public: 
	IgnorableControllable () : PBD::Controllable ("ignoreMe") {}
	~IgnorableControllable () {}
    
	void set_value (float /*v*/) {}
	float get_value () const { return 0.0; }
};

}

#endif /* __pbd_controllable_h__ */
